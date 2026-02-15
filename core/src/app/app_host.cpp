#include "server/core/app/app_host.hpp"

#include "server/core/util/log.hpp"

#include <mutex>
#include <utility>
#include <vector>
#include <sstream>

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

struct AppHost::ShutdownRegistry {
    using Step = std::pair<std::string, std::function<void()>>;

    std::mutex mutex;
    std::vector<Step> steps;
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

std::string AppHost::health_body(bool ok) const {
    if (ok) {
        return "ok\n";
    }
    if (stop_requested()) {
        return "stopping\n";
    }
    return "unhealthy\n";
}

std::string AppHost::readiness_body(bool ok) const {
    if (ok) {
        return "ready\n";
    }

    std::ostringstream oss;
    oss << "not ready";

    std::vector<std::string> reasons;
    if (stop_requested()) {
        reasons.emplace_back("stopping");
    }
    if (!healthy()) {
        reasons.emplace_back("unhealthy");
    }
    if (!ready_base_.load(std::memory_order_relaxed)) {
        reasons.emplace_back("starting");
    }

    std::vector<std::string> missing;
    if (deps_ && !dependencies_ok()) {
        std::lock_guard<std::mutex> lock(deps_->mutex);
        for (const auto& e : deps_->entries) {
            if (e.requirement == DependencyRequirement::kRequired && !e.ok) {
                missing.emplace_back(e.name);
            }
        }
    }
    if (!missing.empty()) {
        std::ostringstream dep;
        dep << "deps=";
        for (std::size_t i = 0; i < missing.size(); ++i) {
            if (i != 0) dep << ',';
            dep << missing[i];
        }
        reasons.emplace_back(dep.str());
    }

    if (!reasons.empty()) {
        oss << ": ";
        for (std::size_t i = 0; i < reasons.size(); ++i) {
            if (i != 0) oss << ", ";
            oss << reasons[i];
        }
    }
    oss << "\n";
    return oss.str();
}

std::string AppHost::dependency_metrics_text() const {
    std::ostringstream out;
    out << "# TYPE knights_dependency_ready gauge\n";

    if (deps_) {
        std::lock_guard<std::mutex> lock(deps_->mutex);
        for (const auto& e : deps_->entries) {
            const char* required = (e.requirement == DependencyRequirement::kRequired) ? "true" : "false";
            out << "knights_dependency_ready{name=\"" << e.name << "\",required=\"" << required << "\"} "
                << (e.ok ? 1 : 0) << "\n";
        }
    }

    out << "# TYPE knights_dependencies_ok gauge\n";
    out << "knights_dependencies_ok " << (dependencies_ok() ? 1 : 0) << "\n";
    return out.str();
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
        },
        server::core::metrics::MetricsHttpServer::LogsCallback{},
        [this](bool ok) { return health_body(ok); },
        [this](bool ok) { return readiness_body(ok); });
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

void AppHost::add_shutdown_step(std::string name, std::function<void()> step) {
    if (!step) {
        return;
    }
    if (name.empty()) {
        name = "(unnamed)";
    }
    if (!shutdown_) {
        shutdown_ = std::make_unique<ShutdownRegistry>();
    }

    std::lock_guard<std::mutex> lock(shutdown_->mutex);
    shutdown_->steps.emplace_back(std::move(name), std::move(step));
}

void AppHost::run_shutdown_steps() noexcept {
    if (shutdown_ran_.exchange(true, std::memory_order_acq_rel)) {
        return;
    }
    if (!shutdown_ || shutdown_->steps.empty()) {
        return;
    }

    std::vector<ShutdownRegistry::Step> steps;
    {
        std::lock_guard<std::mutex> lock(shutdown_->mutex);
        steps.swap(shutdown_->steps);
    }

    for (auto it = steps.rbegin(); it != steps.rend(); ++it) {
        const auto& name = it->first;
        try {
            it->second();
        } catch (const std::exception& ex) {
            corelog::error(name_ + " shutdown step failed: " + name + ": " + ex.what());
        } catch (...) {
            corelog::error(name_ + " shutdown step failed: " + name + ": unknown exception");
        }
    }
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

        run_shutdown_steps();
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
