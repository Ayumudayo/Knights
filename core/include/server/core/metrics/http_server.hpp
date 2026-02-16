#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <atomic>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace server::core::metrics {

class MetricsHttpServer {
public:
    struct RouteResponse {
        std::string status;
        std::string content_type;
        std::string body;
    };

    using MetricsCallback = std::function<std::string()>;
    using StatusCallback = std::function<bool()>;
    using StatusBodyCallback = std::function<std::string(bool ok)>;
    using LogsCallback = std::function<std::string()>;
    using RouteCallback = std::function<std::optional<RouteResponse>(std::string_view method,
                                                                     std::string_view target)>;

    MetricsHttpServer(unsigned short port,
                      MetricsCallback metrics_callback,
                      StatusCallback health_callback = {},
                      StatusCallback ready_callback = {},
                      LogsCallback logs_callback = {},
                      StatusBodyCallback health_body_callback = {},
                      StatusBodyCallback ready_body_callback = {},
                      RouteCallback route_callback = {});
    ~MetricsHttpServer();

    void start();
    void stop();

private:
    void do_accept();

    unsigned short port_;
    MetricsCallback callback_;
    StatusCallback health_callback_;
    StatusCallback ready_callback_;
    LogsCallback logs_callback_;
    StatusBodyCallback health_body_callback_;
    StatusBodyCallback ready_body_callback_;
    RouteCallback route_callback_;
    std::shared_ptr<boost::asio::io_context> io_context_;
    std::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
    std::unique_ptr<std::thread> thread_;
    std::atomic<bool> stopped_{false};
};

} // namespace server::core::metrics
