#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "gateway/auth/authenticator.hpp"
#include "gateway/gateway_app.hpp"
#include "gateway_lb.grpc.pb.h"
#include "server/core/net/connection.hpp"

namespace gateway {

class GatewayApp;

class GatewayConnection : public server::core::net::Connection {
public:
    GatewayConnection(std::shared_ptr<server::core::net::Hive> hive,
                      std::shared_ptr<auth::IAuthenticator> authenticator,
                      GatewayApp& app);

    void handle_backend_payload(std::vector<std::uint8_t> payload);
    void handle_backend_close(const std::string& reason);

protected:
    void on_connect() override;
    void on_disconnect() override;
    void on_read(const std::uint8_t* data, std::size_t length) override;
    void on_error(const boost::system::error_code& ec) override;

private:
    void open_lb_session(const std::string& handshake_payload);
    void send_to_lb(const std::vector<std::uint8_t>& payload, gateway::lb::RouteMessageKind kind);

    std::shared_ptr<auth::IAuthenticator> authenticator_;
    GatewayApp& app_;
    std::string session_id_;
    std::string client_id_;
    GatewayApp::LbSessionPtr lb_session_;
    bool authenticated_{false};
    auth::AuthResult last_auth_result_{};
    std::atomic<bool> closing_{false};
};

} // namespace gateway
