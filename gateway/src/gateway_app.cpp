#include "gateway/gateway_app.hpp"

#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <random>
#include <algorithm>
#include <deque>

#include <boost/asio/connect.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "gateway/gateway_connection.hpp"
#include "server/core/util/paths.hpp"
#include "server/core/util/log.hpp"
#include "server/storage/redis/client.hpp"
#include "server/state/instance_registry.hpp"

namespace gateway {

namespace {

constexpr const char* kEnvGatewayListen = "GATEWAY_LISTEN";
constexpr const char* kEnvGatewayId = "GATEWAY_ID";
constexpr const char* kEnvRedisUri = "REDIS_URI";
constexpr const char* kEnvServerRegistryPrefix = "SERVER_REGISTRY_PREFIX";
constexpr const char* kEnvServerRegistryTtl = "SERVER_REGISTRY_TTL";
constexpr const char* kDefaultGatewayListen = "0.0.0.0:6000";
constexpr const char* kDefaultGatewayId = "gateway-default";
constexpr const char* kDefaultRedisUri = "tcp://127.0.0.1:6379";
constexpr const char* kDefaultServerRegistryPrefix = "gateway/instances/";

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

// --- BackendSession Implementation ---

// Re-implementing GatewayApp::BackendSession methods directly

GatewayApp::BackendSession::BackendSession(GatewayApp& app,
                                           std::string session_id,
                                           std::string client_id,
                                           std::weak_ptr<GatewayConnection> connection)
    : app_(app)
    , session_id_(std::move(session_id))
    , client_id_(std::move(client_id))
    , connection_(std::move(connection))
    , socket_(app.io_) {
}

GatewayApp::BackendSession::~BackendSession() {
    close();
}

void GatewayApp::BackendSession::connect(const std::string& host, std::uint16_t port) {
    if (closed_.load(std::memory_order_relaxed)) return;
    do_connect(host, port);
}

void GatewayApp::BackendSession::do_connect(const std::string& host, std::uint16_t port) {
    auto self = shared_from_this();
    auto resolver = std::make_shared<boost::asio::ip::tcp::resolver>(app_.io_);
    
    resolver->async_resolve(host, std::to_string(port),
        [self, this, resolver](const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::results_type results) {
            if (ec) {
                server::core::log::error("BackendSession resolve failed: " + ec.message());
                if (auto conn = connection_.lock()) {
                    conn->handle_backend_close("resolve failed");
                }
                return;
            }
            
            boost::asio::async_connect(socket_, results,
                [self, this](const boost::system::error_code& ec, const boost::asio::ip::tcp::endpoint& /*endpoint*/) {
                    if (ec) {
                        server::core::log::error("BackendSession connect failed: " + ec.message());
                        if (auto conn = connection_.lock()) {
                            conn->handle_backend_close("connect failed");
                        }
                        return;
                    }
                    
                    {
                        std::lock_guard<std::mutex> lock(send_mutex_);
                        connected_ = true;
                        if (!write_queue_.empty()) {
                            do_write();
                        }
                    }
                    do_read();
                });
        });
}

void GatewayApp::BackendSession::send(const std::vector<std::uint8_t>& payload) {
    std::lock_guard<std::mutex> lock(send_mutex_);
    if (closed_) return;

    write_queue_.push_back(payload);
    if (connected_ && !write_in_progress_) {
        do_write();
    }
}

void GatewayApp::BackendSession::do_write() {
    if (write_queue_.empty()) {
        write_in_progress_ = false;
        return;
    }

    write_in_progress_ = true;
    auto& msg = write_queue_.front();
    
    auto self = shared_from_this();
    boost::asio::async_write(socket_, boost::asio::buffer(msg),
        [self, this](const boost::system::error_code& ec, std::size_t /*bytes_transferred*/) {
            if (ec) {
                close();
                return;
            }

            std::lock_guard<std::mutex> lock(send_mutex_);
            write_queue_.pop_front();
            if (!write_queue_.empty()) {
                do_write();
            } else {
                write_in_progress_ = false;
            }
        });
}

void GatewayApp::BackendSession::do_read() {
    auto self = shared_from_this();
    socket_.async_read_some(boost::asio::buffer(buffer_),
        [self, this](const boost::system::error_code& ec, std::size_t bytes_transferred) {
            on_read(ec, bytes_transferred);
        });
}

void GatewayApp::BackendSession::on_read(const boost::system::error_code& ec, std::size_t bytes_transferred) {
    if (ec) {
        if (ec != boost::asio::error::operation_aborted) {
            if (auto conn = connection_.lock()) {
                conn->handle_backend_close(ec.message());
            }
            close();
        }
        return;
    }

    if (bytes_transferred > 0) {
        if (auto conn = connection_.lock()) {
            std::vector<std::uint8_t> data(buffer_.begin(), buffer_.begin() + bytes_transferred);
            conn->handle_backend_payload(std::move(data));
        }
        do_read();
    }
}

void GatewayApp::BackendSession::close() {
    bool expected = false;
    if (!closed_.compare_exchange_strong(expected, true)) return;

    boost::system::error_code ignored;
    if (socket_.is_open()) {
        socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored);
        socket_.close(ignored);
    }
}

const std::string& GatewayApp::BackendSession::session_id() const {
    return session_id_;
}

// --- GatewayApp Implementation ---

GatewayApp::GatewayApp()
    : hive_(std::make_shared<server::core::net::Hive>(io_))
    , signals_(io_)
    , authenticator_(std::make_shared<auth::NoopAuthenticator>()) {
    
    configure_gateway();
    configure_infrastructure();

    if (const char* port_env = std::getenv("METRICS_PORT")) {
        metrics_port_ = static_cast<std::uint16_t>(std::stoi(port_env));
    }

     metrics_server_ = std::make_unique<server::core::metrics::MetricsHttpServer>(metrics_port_, [this]() {
         std::ostringstream stream;
         stream << "# TYPE gateway_sessions_active gauge\n";
         {
             std::lock_guard<std::mutex> lock(session_mutex_);
             stream << "gateway_sessions_active " << sessions_.size() << "\n";
         }
         stream << "# TYPE gateway_connections_total counter\n";
         stream << "gateway_connections_total " << connections_total_.load(std::memory_order_relaxed) << "\n";
         return stream.str();
     });
     metrics_server_->start();
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
    if (metrics_server_) {
        metrics_server_->stop();
    }

    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        for (auto& [_, state] : sessions_) {
            if (state.session) {
                state.session->close();
            }
        }
        sessions_.clear();
    }

    if (hive_) {
        hive_->stop();
    }
    io_.stop();
}

GatewayApp::BackendSessionPtr GatewayApp::create_backend_session(const std::string& client_id,
                                                                 std::weak_ptr<GatewayConnection> connection) {
    auto target = select_best_server(client_id);
    if (!target) {
        server::core::log::error("GatewayApp: No available backend server found");
        return nullptr;
    }

    // 고유 세션 ID 생성
    // 더 나은 생성기를 사용할 수 있지만, 현재는 원자적 카운터로 충분합니다.
    static std::atomic<std::uint64_t> counter{0};
    std::string session_id = gateway_id_ + "-" + std::to_string(++counter);

    auto session = std::make_shared<BackendSession>(*this, session_id, client_id, std::move(connection));
    
    server::core::log::info("GatewayApp connecting session " + session_id + " to " + target->first + ":" + std::to_string(target->second));
    session->connect(target->first, target->second);

    // Binding session if authenticated
    if (session_directory_ && !client_id.empty() && client_id != "anonymous") {
        // 호스트/포트로 백엔드 ID를 찾습니다.
        // 참고: 이상적으로는 select_best_server가 InstanceRecord를 반환해야 하지만, 
        // 현재는 호스트:포트 일관성에 의존하거나 조회합니다.
        // 더 간단하게 구현하기 위해 필요하다면 호스트:포트를 ID로 사용할 수 있습니다.
        // 추후 select_best_server가 InstanceRecord를 반환하도록 개선할 예정입니다.
        
        // 사실, 연결 성공 후 바인딩을 보장하는 것이 더 좋습니다.
        // 하지만 Instance ID가 필요합니다.
    }

    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        sessions_[session_id] = SessionState{session};
    }

    return session;
}

void GatewayApp::close_backend_session(const std::string& session_id) {
    BackendSessionPtr session;
    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        auto it = sessions_.find(session_id);
        if (it != sessions_.end()) {
            session = it->second.session;
            sessions_.erase(it);
        }
    }
    if (session) {
        session->close();
    }
}

std::optional<std::pair<std::string, std::uint16_t>> GatewayApp::select_best_server(const std::string& client_id) {
    if (!backend_registry_) return std::nullopt;

    auto instances = backend_registry_->list_instances();
    if (instances.empty()) return std::nullopt;

    // 1. 세션 스티키니스 (Session Stickiness)
    // 클라이언트가 이전에 연결했던 백엔드가 있다면 해당 서버로 다시 연결을 시도합니다.
    if (session_directory_ && !client_id.empty() && client_id != "anonymous") {
        if (auto backend_id = session_directory_->find_backend(client_id)) {
            auto it = std::find_if(instances.begin(), instances.end(), [&](const auto& rec) {
                return rec.instance_id == *backend_id;
            });
            if (it != instances.end()) {
                 // 활성 상태인 스티키 백엔드를 찾았습니다.
                 return std::make_pair(it->host, it->port);
            }
            // 백엔드가 사라졌거나 비활성화되었으므로 바인딩을 해제합니다.
            session_directory_->release_backend(client_id, *backend_id);
        }
    }

    // 2. 새로운 백엔드 선택 (최소 연결 수, Least Connections)
    std::vector<server::state::InstanceRecord> candidates;
    std::copy_if(instances.begin(), instances.end(), std::back_inserter(candidates), [](const auto& rec) {
        return !rec.host.empty() && rec.port > 0;
    });

    if (candidates.empty()) return std::nullopt;

    // 활성 세션 수(active_sessions)를 기준으로 오름차순 정렬
    std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
        return a.active_sessions < b.active_sessions;
    });

    // 가장 부하가 적은 서버를 선택합니다.
    // 만약 최소 연결 수를 가진 서버가 여러 대라면 랜덤하게 선택하여 편중(thundering herd)을 막을 수 있지만,
    // 현재는 단순하게 가장 첫 번째 서버를 선택합니다.
    const auto& selected = candidates.front();

    // 3. 새로운 백엔드 바인딩 (세션 고정)
    // 다음에 이 클라이언트가 다시 접속하면 동일한 서버로 연결되도록 기록합니다.
    if (session_directory_ && !client_id.empty() && client_id != "anonymous") {
        session_directory_->refresh_backend(client_id, selected.instance_id);
    }
    
    return std::make_pair(selected.host, selected.port);
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

void GatewayApp::configure_infrastructure() {
    const char* redis_env = std::getenv(kEnvRedisUri);
    redis_uri_ = redis_env ? redis_env : kDefaultRedisUri;

    try {
        server::storage::redis::Options opts;
        redis_client_ = server::storage::redis::make_redis_client(redis_uri_, opts);
        
        if (redis_client_) {
             std::string registry_prefix = kDefaultServerRegistryPrefix;
             if (const char* v = std::getenv(kEnvServerRegistryPrefix); v && *v) {
                 registry_prefix = v;
             }

             std::chrono::seconds registry_ttl{30};
             if (const char* v = std::getenv(kEnvServerRegistryTtl); v && *v) {
                 try {
                     auto parsed = std::stoul(v);
                     if (parsed > 0) {
                         registry_ttl = std::chrono::seconds{static_cast<long long>(parsed)};
                     }
                 } catch (...) {
                     server::core::log::warn("GatewayApp invalid SERVER_REGISTRY_TTL; using default");
                 }
             }

              auto state_client = server::state::make_redis_state_client(redis_client_);
              backend_registry_ = std::make_unique<server::state::RedisInstanceStateBackend>(
                  state_client,
                  std::move(registry_prefix),
                  registry_ttl
              );
              
              session_directory_ = std::make_unique<SessionDirectory>(
                  redis_client_,
                  "gateway/session/",
                  std::chrono::seconds(600) // 10 minutes session stickiness
              );

             server::core::log::info("GatewayApp connected to Redis at " + redis_uri_);
        } else {
            server::core::log::error("GatewayApp failed to create Redis client at " + redis_uri_);
        }
    } catch (const std::exception& e) {
        server::core::log::error(std::string("GatewayApp infrastructure init failed: ") + e.what());
    }
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

} // namespace gateway
