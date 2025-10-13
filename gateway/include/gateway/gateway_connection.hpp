#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "gateway/auth/authenticator.hpp"
#include "server/core/net/connection.hpp"

namespace gateway {

class GatewayConnection : public server::core::net::Connection {
public:
    using PayloadCallback = std::function<void(std::vector<std::uint8_t>)>;

    GatewayConnection(std::shared_ptr<server::core::net::Hive> hive,
                      std::shared_ptr<auth::IAuthenticator> authenticator,
                      PayloadCallback on_payload);

protected:
    void on_connect() override;
    void on_disconnect() override;
    void on_read(const std::uint8_t* data, std::size_t length) override;
    void on_error(const boost::system::error_code& ec) override;

private:
    std::shared_ptr<auth::IAuthenticator> authenticator_;
    PayloadCallback on_payload_;
    bool authenticated_{false};
    auth::AuthResult last_auth_result_{};
};

} // namespace gateway
