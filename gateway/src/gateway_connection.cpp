#include "gateway/gateway_connection.hpp"

#include <string>

#include "server/core/util/log.hpp"

namespace gateway {

namespace {
std::string extract_token(const std::string& message, std::string& client_id) {
    auto pos = message.find(':');
    if (pos == std::string::npos) {
        client_id.clear();
        return message;
    }
    client_id = message.substr(0, pos);
    return message.substr(pos + 1);
}
} // namespace

GatewayConnection::GatewayConnection(std::shared_ptr<server::core::net::Hive> hive,
                                     std::shared_ptr<auth::IAuthenticator> authenticator,
                                     PayloadCallback on_payload)
    : server::core::net::Connection(std::move(hive))
    , authenticator_(std::move(authenticator))
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
    if (length == 0) {
        return;
    }

    if (!authenticated_) {
        std::string message(reinterpret_cast<const char*>(data), length);
        auth::AuthRequest request{};
        request.token = extract_token(message, request.client_id);
        try {
            request.remote_address = socket().remote_endpoint().address().to_string();
        } catch (...) {
            request.remote_address.clear();
        }

        if (authenticator_) {
            last_auth_result_ = authenticator_->authenticate(request);
            authenticated_ = last_auth_result_.success;
        } else {
            authenticated_ = true;
            last_auth_result_.success = true;
            last_auth_result_.subject = request.client_id.empty() ? "anonymous" : request.client_id;
        }

        if (!authenticated_) {
            server::core::log::warn(std::string("GatewayConnection authentication failed: ")
                + last_auth_result_.failure_reason);
            stop();
        }
        return;
    }

    if (!on_payload_) {
        return;
    }
    std::vector<std::uint8_t> payload(data, data + length);
    on_payload_(std::move(payload));
}

void GatewayConnection::on_error(const boost::system::error_code& ec) {
    server::core::log::warn(std::string("GatewayConnection error: ") + ec.message());
}

} // namespace gateway
