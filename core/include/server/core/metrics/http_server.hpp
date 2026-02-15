#pragma once

#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace server::core::metrics {

class MetricsHttpServer {
public:
    using MetricsCallback = std::function<std::string()>;
    using StatusCallback = std::function<bool()>;

    MetricsHttpServer(unsigned short port,
                      MetricsCallback metrics_callback,
                      StatusCallback health_callback = {},
                      StatusCallback ready_callback = {});
    ~MetricsHttpServer();

    void start();
    void stop();

private:
    void do_accept();

    unsigned short port_;
    MetricsCallback callback_;
    StatusCallback health_callback_;
    StatusCallback ready_callback_;
    std::shared_ptr<boost::asio::io_context> io_context_;
    std::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
    std::unique_ptr<std::thread> thread_;
    std::atomic<bool> stopped_{false};
};

} // namespace server::core::metrics
