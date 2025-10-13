#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "server/core/net/connection.hpp"

namespace gateway {

class GatewayConnection : public server::core::net::Connection {
public:
    using PayloadCallback = std::function<void(std::vector<std::uint8_t>)>;

    GatewayConnection(std::shared_ptr<server::core::net::Hive> hive,
                      PayloadCallback on_payload);

protected:
    void on_connect() override;
    void on_disconnect() override;
    void on_read(const std::uint8_t* data, std::size_t length) override;
    void on_error(const boost::system::error_code& ec) override;

private:
    PayloadCallback on_payload_;
};

} // namespace gateway
