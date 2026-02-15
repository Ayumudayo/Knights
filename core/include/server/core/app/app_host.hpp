#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

#include "server/core/app/termination_signals.hpp"
#include "server/core/metrics/http_server.hpp"

namespace server::core::app {

// Small, reusable process host for service binaries.
//
// Responsibilities (intentionally minimal for now):
// - Track process lifecycle flags (stop/healthy/ready)
// - Start an admin HTTP server (metrics + healthz/readyz) on the METRICS_PORT
// - Install graceful shutdown hooks (SIGINT/SIGTERM via boost::asio::signal_set)
class AppHost {
public:
    enum class DependencyRequirement : std::uint8_t {
        kRequired,
        kOptional,
    };

    explicit AppHost(std::string name);
    ~AppHost();

    AppHost(const AppHost&) = delete;
    AppHost& operator=(const AppHost&) = delete;

    bool request_stop() noexcept;
    bool stop_requested() const noexcept;

    void set_healthy(bool healthy) noexcept;
    bool healthy() const noexcept;

    void set_ready(bool ready) noexcept;
    bool ready() const noexcept;

    // Declare a dependency that must be healthy for readiness.
    // Required dependencies default to "not OK" until set_dependency_ok() marks them OK.
    void declare_dependency(std::string name,
                            DependencyRequirement requirement = DependencyRequirement::kRequired);

    // Updates a dependency status. If the dependency was not declared yet, it is
    // declared as required.
    void set_dependency_ok(std::string_view name, bool ok);

    bool dependencies_ok() const noexcept;

    void start_admin_http(unsigned short port,
                          server::core::metrics::MetricsHttpServer::MetricsCallback metrics_callback);
    void stop_admin_http();

    // Installs async signal handlers on the given io_context.
    // `on_shutdown` should stop app components and eventually stop the io_context.
    void install_asio_termination_signals(boost::asio::io_context& io,
                                         std::function<void()> on_shutdown);

private:
    struct DependencyRegistry;

    std::string name_;
    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> healthy_{true};
    std::atomic<bool> ready_base_{false};
    std::atomic<bool> deps_ok_{true};

    std::unique_ptr<DependencyRegistry> deps_;

    std::unique_ptr<server::core::metrics::MetricsHttpServer> admin_http_;
    std::unique_ptr<boost::asio::signal_set> signals_;
};

} // namespace server::core::app
