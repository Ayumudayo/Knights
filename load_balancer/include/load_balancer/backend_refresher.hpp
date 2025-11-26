#pragma once

#include <memory>
#include <string>
#include <vector>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/io_context.hpp>

#include "load_balancer/config.hpp"
#include "load_balancer/backend_registry.hpp"
#include "server/state/instance_registry.hpp"

namespace load_balancer {

class BackendRefresher {
public:
    BackendRefresher(boost::asio::io_context& io,
                     const Config& config,
                     BackendRegistry& registry,
                     server::state::IInstanceStateBackend* state_backend);

    void start();
    void stop();
    void refresh();

private:
    void schedule_refresh();

    boost::asio::steady_timer timer_;
    const Config& config_;
    BackendRegistry& registry_;
    server::state::IInstanceStateBackend* state_backend_;
    bool running_{false};
};

} // namespace load_balancer
