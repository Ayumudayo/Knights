#include "server/core/app/app_host.hpp"

#include "server/core/util/log.hpp"

#include <mutex>
#include <vector>

namespace server::core::app {

namespace corelog = server::core::log;

struct AppHost::DependencyRegistry {
    struct Entry {
        std::string name;
        DependencyRequirement requirement{DependencyRequirement::kRequired};
        bool ok{false};
    };

    std::mutex mutex;
    std::vector<Entry> entries;

    bool compute_ok() const {
        for (const auto& e : entries) {
            if (e.requirement == DependencyRequirement::kRequired && !e.ok) {
                return false;
            }
        }
        return true;
    }
};

AppHost::AppHost(std::string name)
    : name_(std::move(name)) {
}

AppHost::~AppHost() {
    stop_admin_http();
}

bool AppHost::request_stop() noexcept {
    return !stop_requested_.exchange(true, std::memory_order_acq_rel);
}

bool AppHost::stop_requested() const noexcept {
    return stop_requested_.load(std::memory_order_relaxed) || termination_signal_received();
}

void AppHost::set_healthy(bool healthy) noexcept {
    healthy_.store(healthy, std::memory_order_relaxed);
}

bool AppHost::healthy() const noexcept {
    return healthy_.load(std::memory_order_relaxed);
}

void AppHost::set_ready(bool ready) noexcept {
    ready_base_.store(ready, std::memory_order_relaxed);
}

bool AppHost::ready() const noexcept {
    return ready_base_.load(std::memory_order_relaxed) && deps_ok_.load(std::memory_order_relaxed);
}

void AppHost::declare_dependency(std::string name, DependencyRequirement requirement) {
    if (name.empty()) {
        corelog::warn(name_ + " declare_dependency called with empty name");
        return;
    }
    if (!deps_) {
        deps_ = std::make_unique<DependencyRegistry>();
    }

    std::lock_guard<std::mutex> lock(deps_->mutex);

    for (auto& e : deps_->entries) {
        if (e.name == name) {
            e.requirement = requirement;
            deps_ok_.store(deps_->compute_ok(), std::memory_order_relaxed);
            return;
        }
    }

    DependencyRegistry::Entry entry;
    entry.name = std::move(name);
    entry.requirement = requirement;
    entry.ok = false;
    deps_->entries.emplace_back(std::move(entry));
    deps_ok_.store(deps_->compute_ok(), std::memory_order_relaxed);
}

void AppHost::set_dependency_ok(std::string_view name, bool ok) {
    if (name.empty()) {
        corelog::warn(name_ + " set_dependency_ok called with empty name");
        return;
    }
    if (!deps_) {
        deps_ = std::make_unique<DependencyRegistry>();
    }

    std::lock_guard<std::mutex> lock(deps_->mutex);

    for (auto& e : deps_->entries) {
        if (e.name == name) {
            e.ok = ok;
            deps_ok_.store(deps_->compute_ok(), std::memory_order_relaxed);
            return;
        }
    }

    corelog::warn(name_ + " set_dependency_ok used before declare_dependency: " + std::string(name));
    DependencyRegistry::Entry entry;
    entry.name = std::string(name);
    entry.requirement = DependencyRequirement::kRequired;
    entry.ok = ok;
    deps_->entries.emplace_back(std::move(entry));
    deps_ok_.store(deps_->compute_ok(), std::memory_order_relaxed);
}

bool AppHost::dependencies_ok() const noexcept {
    return deps_ok_.load(std::memory_order_relaxed);
}

void AppHost::start_admin_http(unsigned short port,
                               server::core::metrics::MetricsHttpServer::MetricsCallback metrics_callback) {
    if (port == 0) {
        return;
    }
    if (admin_http_) {
        return;
    }

    admin_http_ = std::make_unique<server::core::metrics::MetricsHttpServer>(
        port,
        std::move(metrics_callback),
        [this]() {
            // Keep health simple and cheap.
            return healthy() && !stop_requested();
        },
        [this]() {
            // Readiness implies health.
            return ready() && healthy() && !stop_requested();
        });
    admin_http_->start();

    corelog::info(name_ + " admin http enabled on :" + std::to_string(port));
}

void AppHost::stop_admin_http() {
    if (!admin_http_) {
        return;
    }
    admin_http_->stop();
    admin_http_.reset();
}

void AppHost::install_asio_termination_signals(boost::asio::io_context& io,
                                              std::function<void()> on_shutdown) {
    if (signals_) {
        return;
    }

    // Always install the process-wide (polling) handler too. It's cheap and helps
    // non-asio loops share the same shutdown behavior.
    install_termination_signal_handlers();

    signals_ = std::make_unique<boost::asio::signal_set>(io);
#if defined(SIGINT)
    signals_->add(SIGINT);
#endif
#if defined(SIGTERM)
    signals_->add(SIGTERM);
#endif

    signals_->async_wait([this, on_shutdown = std::move(on_shutdown)](const boost::system::error_code& ec, int) {
        if (ec == boost::asio::error::operation_aborted) {
            return;
        }
        if (ec) {
            return;
        }
        if (!request_stop()) {
            return;
        }

        set_ready(false);
        corelog::info(name_ + " received shutdown signal");
        try {
            if (on_shutdown) {
                on_shutdown();
            }
        } catch (const std::exception& ex) {
            corelog::error(name_ + " shutdown callback exception: " + std::string(ex.what()));
        } catch (...) {
            corelog::error(name_ + " shutdown callback unknown exception");
        }
    });
}

} // namespace server::core::app
