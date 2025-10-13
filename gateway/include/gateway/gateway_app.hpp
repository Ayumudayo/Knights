#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>

#include "gateway/auth/authenticator.hpp"
#include "gateway/gateway_connection.hpp"
#include "server/core/net/hive.hpp"
#include "server/core/net/listener.hpp"

namespace gateway {

class GatewayApp {
public:
    GatewayApp();
    ~GatewayApp() = default;

    // Smoke test to verify Hive/Connection pipeline and graceful stop flow
    bool run_smoke_test();

private:
    void arm_stop_timer();
    std::uint16_t setup_listener(std::uint16_t port);

    boost::asio::io_context io_;
    std::shared_ptr<server::core::net::Hive> hive_;
    boost::asio::steady_timer stop_timer_;
    std::shared_ptr<server::core::net::Listener> listener_;
    std::shared_ptr<auth::IAuthenticator> authenticator_;
    std::atomic<bool> payload_received_{false};
    std::atomic<bool> watchdog_fired_{false};
    std::mutex payload_mutex_;
    std::vector<std::uint8_t> last_payload_;
};

} // namespace gateway
