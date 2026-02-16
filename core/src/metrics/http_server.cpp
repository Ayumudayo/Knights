#include "server/core/metrics/http_server.hpp"
#include "server/core/util/log.hpp"
#include <algorithm>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>
#include <cctype>
#include <iostream>
#include <sstream>

namespace server::core::metrics {

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
namespace corelog = server::core::log;

std::string trim_ascii(std::string_view value) {
    std::size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
        ++begin;
    }

    std::size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }
    return std::string(value.substr(begin, end - begin));
}

std::string to_lower_ascii(std::string_view value) {
    std::string out(value);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

MetricsHttpServer::MetricsHttpServer(unsigned short port,
                                     MetricsCallback metrics_callback,
                                     StatusCallback health_callback,
                                     StatusCallback ready_callback,
                                     LogsCallback logs_callback,
                                     StatusBodyCallback health_body_callback,
                                     StatusBodyCallback ready_body_callback,
                                     RouteCallback route_callback)
    : port_(port)
    , callback_(std::move(metrics_callback))
    , health_callback_(std::move(health_callback))
    , ready_callback_(std::move(ready_callback))
    , logs_callback_(std::move(logs_callback))
    , health_body_callback_(std::move(health_body_callback))
    , ready_body_callback_(std::move(ready_body_callback))
    , route_callback_(std::move(route_callback)) {
    io_context_ = std::make_shared<asio::io_context>();
    acceptor_ = std::make_shared<tcp::acceptor>(*io_context_, tcp::endpoint(tcp::v4(), port_));
}

MetricsHttpServer::~MetricsHttpServer() {
    stop();
}

void MetricsHttpServer::start() {
    corelog::info(std::string("MetricsHttpServer listening on :") + std::to_string(port_));
    do_accept();
    thread_ = std::make_unique<std::thread>([this]() {
        try {
            io_context_->run();
        } catch (const std::exception& e) {
            corelog::error(std::string("MetricsHttpServer exception: ") + e.what());
        }
    });
}

void MetricsHttpServer::stop() {
    if (stopped_.exchange(true)) {
        return;
    }
    if (io_context_) {
        io_context_->stop();
    }
    if (thread_ && thread_->joinable()) {
        thread_->join();
    }
}

void MetricsHttpServer::do_accept() {
    auto sock = std::make_shared<tcp::socket>(*io_context_);
    acceptor_->async_accept(*sock, [this, sock](const boost::system::error_code& ec) {
        if (!ec) {
            asio::post(io_context_->get_executor(), [this, sock]() {
                try {
                    asio::streambuf request_buf;
                    boost::system::error_code read_ec;
                    asio::read_until(*sock, request_buf, "\r\n\r\n", read_ec);
                    if (read_ec && read_ec != asio::error::eof) {
                        try {
                            sock->close();
                        } catch (...) {
                        }
                        return;
                    }

                    std::istream request_stream(&request_buf);
                    std::string request_line;
                    std::getline(request_stream, request_line);
                    if (!request_line.empty() && request_line.back() == '\r') request_line.pop_back();

                    MetricsHttpServer::HttpRequest request;
                    {
                        std::istringstream line_stream(request_line);
                        line_stream >> request.method >> request.target;
                    }
                    if (request.target.empty()) request.target = "/metrics";

                    std::string header_line;
                    while (std::getline(request_stream, header_line)) {
                        if (!header_line.empty() && header_line.back() == '\r') {
                            header_line.pop_back();
                        }
                        if (header_line.empty()) {
                            break;
                        }

                        const auto colon = header_line.find(':');
                        if (colon == std::string::npos) {
                            continue;
                        }

                        const std::string name = to_lower_ascii(trim_ascii(std::string_view(header_line).substr(0, colon)));
                        const std::string value = trim_ascii(std::string_view(header_line).substr(colon + 1));
                        if (!name.empty()) {
                            request.headers[name] = value;
                        }
                    }

                    request.source_ip = "unknown";
                    boost::system::error_code endpoint_ec;
                    const auto remote = sock->remote_endpoint(endpoint_ec);
                    if (!endpoint_ec) {
                        const auto ip = remote.address().to_string();
                        if (!ip.empty()) {
                            request.source_ip = ip;
                        }
                    }

                    const std::string& target = request.target;

                    std::string body;
                    std::string status = "200 OK";
                    std::string content_type = "text/plain; version=0.0.4";

                    if (target == "/logs") {
                        if (logs_callback_) {
                            content_type = "text/plain; charset=utf-8";
                            body = logs_callback_();
                        } else {
                            status = "404 Not Found";
                            content_type = "text/plain; charset=utf-8";
                            body = "Not Found\r\n";
                        }
                    } else if (target == "/metrics" || target == "/") {
                        if (callback_) {
                            body = callback_();
                        } else {
                            body = "# No callback registered\n";
                        }
                    } else if (target == "/healthz" || target == "/health") {
                        const bool ok = health_callback_ ? health_callback_() : true;
                        status = ok ? "200 OK" : "503 Service Unavailable";
                        content_type = "text/plain; charset=utf-8";
                        if (health_body_callback_) {
                            body = health_body_callback_(ok);
                        } else {
                            body = ok ? "ok\n" : "unhealthy\n";
                        }
                    } else if (target == "/readyz" || target == "/ready") {
                        const bool ok = ready_callback_ ? ready_callback_() : true;
                        status = ok ? "200 OK" : "503 Service Unavailable";
                        content_type = "text/plain; charset=utf-8";
                        if (ready_body_callback_) {
                            body = ready_body_callback_(ok);
                        } else {
                            body = ok ? "ready\n" : "not ready\n";
                        }
                    } else {
                        bool handled_custom = false;
                        if (route_callback_) {
                            if (auto custom = route_callback_(request)) {
                                handled_custom = true;
                                status = custom->status.empty() ? "200 OK" : std::move(custom->status);
                                content_type = custom->content_type.empty()
                                    ? "text/plain; charset=utf-8"
                                    : std::move(custom->content_type);
                                body = std::move(custom->body);
                            }
                        }
                        if (!handled_custom) {
                            status = "404 Not Found";
                            content_type = "text/plain; charset=utf-8";
                            body = "Not Found\r\n";
                        }
                    }

                    std::string hdr = "HTTP/1.1 " + status + "\r\nContent-Type: " + content_type +
                        "\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n";
                    std::vector<asio::const_buffer> bufs;
                    bufs.emplace_back(asio::buffer(hdr));
                    if (!body.empty()) {
                        bufs.emplace_back(asio::buffer(body));
                    }
                    asio::write(*sock, bufs);
                    try {
                        sock->shutdown(tcp::socket::shutdown_both);
                    } catch (...) {
                    }
                    try {
                        sock->close();
                    } catch (...) {
                    }
                } catch (const std::exception& e) {
                    corelog::error(std::string("MetricsHttpServer request handling exception: ") + e.what());
                    try {
                        sock->shutdown(tcp::socket::shutdown_both);
                    } catch (...) {
                    }
                    try {
                        sock->close();
                    } catch (...) {
                    }
                } catch (...) {
                    corelog::error("MetricsHttpServer request handling unknown exception");
                    try {
                        sock->shutdown(tcp::socket::shutdown_both);
                    } catch (...) {
                    }
                    try {
                        sock->close();
                    } catch (...) {
                    }
                }
            });
        }
        
        if (!stopped_) {
            do_accept();
        }
    });
}

} // namespace server::core::metrics
