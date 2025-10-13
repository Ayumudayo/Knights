#include "gateway/gateway_connection.hpp"

#include <string>

#include "server/core/util/log.hpp"

namespace gateway {

GatewayConnection::GatewayConnection(std::shared_ptr<server::core::net::Hive> hive,
                                     PayloadCallback on_payload)
    : server::core::net::Connection(std::move(hive))
    , on_payload_(std::move(on_payload)) {}

void GatewayConnection::on_connect() {
    try {
        const auto remote = socket().remote_endpoint();
        server::core::log::info("GatewayConnection accepted from " + remote.address().to_string());
    } catch (const std::exception& ex) {
        server::core::log::warn(std::string("GatewayConnection remote endpoint unknown: ") + ex.what());
    }
}

void GatewayConnection::on_disconnect() {
    server::core::log::info("GatewayConnection disconnected");
}

void GatewayConnection::on_read(const std::uint8_t* data, std::size_t length) {
    if (!on_payload_ || length == 0) {
        return;
    }
    std::vector<std::uint8_t> payload(data, data + length);
    on_payload_(std::move(payload));
}

void GatewayConnection::on_error(const boost::system::error_code& ec) {
    server::core::log::warn(std::string("GatewayConnection error: ") + ec.message());
}

} // namespace gateway
