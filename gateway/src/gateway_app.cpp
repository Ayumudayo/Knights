#include "gateway/gateway_app.hpp"

#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <grpcpp/grpcpp.h>

#include "gateway/gateway_connection.hpp"
#include "server/core/config/dotenv.hpp"
#include "server/core/util/paths.hpp"
#include "server/core/util/log.hpp"

namespace gateway {

namespace {

constexpr const char* kEnvGatewayListen = "GATEWAY_LISTEN";
constexpr const char* kEnvGatewayId = "GATEWAY_ID";
constexpr const char* kEnvLbEndpoint = "LB_GRPC_ENDPOINT";
constexpr const char* kDefaultGatewayListen = "0.0.0.0:6000";
constexpr const char* kDefaultGatewayId = "gateway-default";
constexpr const char* kDefaultLbEndpoint = "127.0.0.1:7001";

std::pair<std::string, std::uint16_t> parse_listen(std::string_view value, std::uint16_t fallback_port) {
    if (value.empty()) {
        return {"0.0.0.0", fallback_port};
    }

    auto delimiter = value.find(':');
    if (delimiter == std::string_view::npos) {
        return {std::string(value), fallback_port};
    }

    std::string host(value.substr(0, delimiter));
    std::string_view port_view = value.substr(delimiter + 1);
    std::uint16_t port = fallback_port;
    if (!port_view.empty()) {
        try {
            port = static_cast<std::uint16_t>(std::stoul(std::string(port_view)));
        } catch (...) {
            server::core::log::warn("GatewayApp invalid port in GATEWAY_LISTEN; falling back to default");
            port = fallback_port;
        }
    }
    return {std::move(host), port};
}

} // namespace

GatewayApp::LbSession::LbSession(GatewayApp& app,
                                 std::string session_id,
                                 std::string client_id,
                                 std::weak_ptr<GatewayConnection> connection)
    : app_(app)
    , session_id_(std::move(session_id))
    , client_id_(std::move(client_id))
    , connection_(std::move(connection)) {}

GatewayApp::LbSession::~LbSession() {
    stop();
}

bool GatewayApp::LbSession::start() {
    if (!app_.lb_stub_) {
        return false;
    }
    context_ = std::make_unique<grpc::ClientContext>();
    stream_ = app_.lb_stub_->Stream(context_.get());
    if (!stream_) {
        server::core::log::error("GatewayApp failed to open LB gRPC stream");
        return false;
    }
    reader_thread_ = std::thread([weak_self = std::weak_ptr<LbSession>(shared_from_this())]() {
        if (auto self = weak_self.lock()) {
            self->read_loop();
        }
    });
    return true;
}

bool GatewayApp::LbSession::send(gateway::lb::RouteMessageKind kind,
                                 const std::vector<std::uint8_t>& payload) {
    if (stopped_.load(std::memory_order_relaxed) || !stream_) {
        return false;
    }

    gateway::lb::RouteMessage message;
    message.set_session_id(session_id_);
    message.set_gateway_id(app_.gateway_id_);
    message.set_client_id(client_id_);
    message.set_kind(kind);
    if (!payload.empty()) {
        message.set_payload(reinterpret_cast<const char*>(payload.data()), static_cast<int>(payload.size()));
    }

    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        if (!stream_->Write(message)) {
            stopped_.store(true, std::memory_order_relaxed);
            return false;
        }
        if (kind == gateway::lb::ROUTE_KIND_CLIENT_CLOSE) {
            stream_->WritesDone();
        }
    }
    return true;
}

void GatewayApp::LbSession::stop() {
    bool expected = false;
    if (!stopped_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        if (stream_) {
            stream_->WritesDone();
        }
    }

    if (context_) {
        context_->TryCancel();
    }

    if (reader_thread_.joinable()) {
        reader_thread_.join();
    }
    stream_.reset();
    context_.reset();
}

const std::string& GatewayApp::LbSession::session_id() const {
    return session_id_;
}

void GatewayApp::LbSession::read_loop() {
    gateway::lb::RouteMessage response;
    while (!stopped_.load(std::memory_order_relaxed) && stream_ && stream_->Read(&response)) {
        switch (response.kind()) {
        case gateway::lb::ROUTE_KIND_SERVER_PAYLOAD: {
            auto payload = response.payload();
            if (auto connection = connection_.lock()) {
                std::vector<std::uint8_t> data(payload.begin(), payload.end());
                connection->handle_backend_payload(std::move(data));
            }
            break;
        }
        case gateway::lb::ROUTE_KIND_SERVER_CLOSE:
        case gateway::lb::ROUTE_KIND_SERVER_ERROR: {
            const std::string reason = response.error();
            if (auto connection = connection_.lock()) {
                connection->handle_backend_close(reason);
            }
            stopped_.store(true, std::memory_order_relaxed);
            return;
        }
        default:
            break;
        }
    }

    if (auto connection = connection_.lock()) {
        connection->handle_backend_close("load balancer stream ended");
    }
    stopped_.store(true, std::memory_order_relaxed);
}

GatewayApp::GatewayApp()
    : hive_(std::make_shared<server::core::net::Hive>(io_))
    , signals_(io_)
    , authenticator_(std::make_shared<auth::NoopAuthenticator>()) {
    load_environment();
    configure_gateway();
    configure_load_balancer();
}

GatewayApp::~GatewayApp() {
    stop();
}

int GatewayApp::run() {
    start_listener();
    handle_signals();

    server::core::log::info("GatewayApp starting main loop");
    hive_->run();
    server::core::log::info("GatewayApp stopped");
    return 0;
}

void GatewayApp::stop() {
    if (listener_) {
        listener_->stop();
    }

    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        for (auto& [_, state] : sessions_) {
            if (state.session) {
                state.session->stop();
            }
        }
        sessions_.clear();
    }

    if (hive_) {
        hive_->stop();
    }
    io_.stop();
}

GatewayApp::LbSessionPtr GatewayApp::create_lb_session(const std::string& client_id,
                                                       std::weak_ptr<GatewayConnection> connection) {
    if (!lb_stub_) {
        server::core::log::error("GatewayApp load balancer stub unavailable");
        return nullptr;
    }

    const auto seq = session_counter_.fetch_add(1, std::memory_order_relaxed) + 1;
    std::string session_id = gateway_id_ + "-" + std::to_string(seq);

    auto session = std::make_shared<LbSession>(*this, session_id, client_id, std::move(connection));
    if (!session->start()) {
        return nullptr;
    }

    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        sessions_[session_id] = SessionState{session};
    }

    return session;
}

void GatewayApp::close_lb_session(const std::string& session_id) {
    LbSessionPtr session;
    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        auto it = sessions_.find(session_id);
        if (it != sessions_.end()) {
            session = it->second.session;
            sessions_.erase(it);
        }
    }
    if (session) {
        session->stop();
    }
}

void GatewayApp::configure_gateway() {
    const char* listen_env = std::getenv(kEnvGatewayListen);
    const auto [host, port] = parse_listen(listen_env ? std::string_view(listen_env) : std::string_view(kDefaultGatewayListen),
                                           listen_port_);
    listen_host_ = host;
    listen_port_ = port;

    const char* id_env = std::getenv(kEnvGatewayId);
    if (id_env && *id_env) {
        gateway_id_ = id_env;
    } else {
        gateway_id_ = kDefaultGatewayId;
    }

    server::core::log::info("GatewayApp configured: gateway_id=" + gateway_id_
        + " listen=" + listen_host_ + ":" + std::to_string(listen_port_));
}

void GatewayApp::configure_load_balancer() {
    const char* lb_env = std::getenv(kEnvLbEndpoint);
    if (lb_env && *lb_env) {
        lb_endpoint_ = lb_env;
    } else {
        lb_endpoint_ = kDefaultLbEndpoint;
    }

    auto channel = grpc::CreateChannel(lb_endpoint_, grpc::InsecureChannelCredentials());
    lb_stub_ = gateway::lb::LoadBalancerService::NewStub(channel);

    server::core::log::info("GatewayApp LB endpoint: " + lb_endpoint_);
}

void GatewayApp::start_listener() {
    using tcp = boost::asio::ip::tcp;

    boost::system::error_code ec;
    boost::asio::ip::address address = boost::asio::ip::address_v4::any();
    if (!listen_host_.empty()) {
        auto parsed = boost::asio::ip::make_address(listen_host_, ec);
        if (!ec) {
            address = parsed;
        } else {
            server::core::log::warn("GatewayApp failed to parse listen address; defaulting to 0.0.0.0");
        }
    }

    tcp::endpoint endpoint{address, listen_port_};
    listener_ = std::make_shared<server::core::net::Listener>(
        hive_,
        endpoint,
        [authenticator = authenticator_, this](std::shared_ptr<server::core::net::Hive> hive) {
            return std::make_shared<GatewayConnection>(std::move(hive), authenticator, *this);
        });

    if (listener_->is_stopped()) {
        throw std::runtime_error("GatewayApp listener failed to start");
    }

    listener_->start();
    auto bound = listener_->local_endpoint();
    server::core::log::info("GatewayApp listening on " + bound.address().to_string() + ":" + std::to_string(bound.port()));
}

void GatewayApp::handle_signals() {
#if defined(SIGINT)
    signals_.add(SIGINT);
#endif
#if defined(SIGTERM)
    signals_.add(SIGTERM);
#endif
    signals_.async_wait([this](const boost::system::error_code& ec, int) {
        if (ec == boost::asio::error::operation_aborted) {
            return;
        }
        if (!ec) {
            server::core::log::info("GatewayApp received shutdown signal");
            stop();
        }
    });
}

void GatewayApp::load_environment() {
    namespace paths = server::core::util::paths;
    try {
        auto exe_dir = paths::executable_dir();
        auto exe_env = exe_dir / ".env";
        if (std::filesystem::exists(exe_env)) {
            server::core::config::load_dotenv(exe_env.string(), true);
            return;
        }
    } catch (const std::exception& ex) {
        server::core::log::warn(std::string("GatewayApp executable dir detection failed: ") + ex.what());
    }

    std::filesystem::path repo_env{".env"};
    if (std::filesystem::exists(repo_env)) {
        server::core::config::load_dotenv(repo_env.string(), true);
    }
}

} // namespace gateway
