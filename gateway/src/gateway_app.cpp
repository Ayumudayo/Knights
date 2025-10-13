#include "gateway/gateway_app.hpp"

#include <chrono>
#include <string>
#include <thread>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>

#include "server/core/util/log.hpp"

namespace gateway {

namespace {
constexpr std::uint16_t kSmokeTestPort = 0; // let the OS choose an available port
}

using tcp = boost::asio::ip::tcp;

GatewayApp::GatewayApp()
    : hive_(std::make_shared<server::core::net::Hive>(io_))
    , stop_timer_(io_) {}

bool GatewayApp::run_smoke_test() {
    payload_received_.store(false, std::memory_order_relaxed);
    watchdog_fired_.store(false, std::memory_order_relaxed);
    last_payload_.clear();

    const auto port = setup_listener(kSmokeTestPort);
    arm_stop_timer();

    std::thread worker([this]() { hive_->run(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    boost::asio::io_context client_io;
    tcp::socket client_socket(client_io);
    boost::system::error_code ec;
    tcp::endpoint destination(boost::asio::ip::address_v4::loopback(), port);
    client_socket.connect(destination, ec);
    if (!ec) {
        const std::string message = "gateway-smoke";
        boost::asio::write(client_socket, boost::asio::buffer(message), ec);
    } else {
        server::core::log::warn(std::string("GatewayApp smoke client failed to connect: ") + ec.message());
    }
    client_socket.close();

    worker.join();

    if (listener_) {
        listener_->stop();
        listener_.reset();
    }

    std::vector<std::uint8_t> payload_copy;
    {
        std::lock_guard<std::mutex> lock(payload_mutex_);
        payload_copy = last_payload_;
    }
    const std::string payload_str(payload_copy.begin(), payload_copy.end());
    const bool payload_ok = payload_received_.load(std::memory_order_relaxed) && payload_str == "gateway-smoke";
    const bool ok = payload_ok && !watchdog_fired_.load(std::memory_order_relaxed);
    if (ok) {
        server::core::log::info("GatewayApp Hive/Connection smoke test completed");
    } else {
        server::core::log::warn("GatewayApp smoke test failed (payload_ok="
            + std::string(payload_ok ? "true" : "false") + ", watchdog="
            + std::string(watchdog_fired_.load(std::memory_order_relaxed) ? "true" : "false") + ")");
    }
    return ok;
}

void GatewayApp::arm_stop_timer() {
    using namespace std::chrono_literals;
    stop_timer_.expires_after(200ms);
    stop_timer_.async_wait([this](const boost::system::error_code& ec) {
        if (ec == boost::asio::error::operation_aborted) {
            return;
        }
        if (ec) {
            server::core::log::warn(std::string("GatewayApp timer error: ") + ec.message());
        }
        watchdog_fired_.store(true, std::memory_order_relaxed);
        hive_->stop();
    });
}

std::uint16_t GatewayApp::setup_listener(std::uint16_t port) {
    auto payload_callback = [this](std::vector<std::uint8_t> payload) {
        payload_received_.store(true, std::memory_order_relaxed);
        {
            std::lock_guard<std::mutex> lock(payload_mutex_);
            last_payload_ = std::move(payload);
        }
        stop_timer_.cancel();
        hive_->stop();
    };

    tcp::endpoint endpoint{tcp::v4(), port};
    listener_ = std::make_shared<server::core::net::Listener>(
        hive_,
        endpoint,
        [payload_callback](std::shared_ptr<server::core::net::Hive> hive) {
            return std::make_shared<GatewayConnection>(std::move(hive), payload_callback);
        });
    listener_->start();

    auto bound_endpoint = listener_->local_endpoint();
    server::core::log::info("GatewayApp listener bound to port " + std::to_string(bound_endpoint.port()));
    return bound_endpoint.port();
}

} // namespace gateway
