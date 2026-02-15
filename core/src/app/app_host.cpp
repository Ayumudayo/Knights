#include "server/core/app/app_host.hpp"

#include "server/core/util/log.hpp"

namespace server::core::app {

namespace corelog = server::core::log;

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
    ready_.store(ready, std::memory_order_relaxed);
}

bool AppHost::ready() const noexcept {
    return ready_.load(std::memory_order_relaxed);
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
