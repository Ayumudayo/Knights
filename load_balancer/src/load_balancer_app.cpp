#include "load_balancer/load_balancer_app.hpp"
#include "load_balancer/grpc_service_impl.hpp"
#include "load_balancer/backend_registry.hpp"
#include "load_balancer/config.hpp"
#include "load_balancer/backend_refresher.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>

#include "load_balancer/session_directory.hpp"
#include "server/core/util/log.hpp"
#include "server/storage/redis/client.hpp"

namespace {
    std::uint64_t to_ms(std::chrono::steady_clock::time_point tp) {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count());
    }
} // namespace

namespace load_balancer {

    LoadBalancerApp::LoadBalancerApp()
        : config_(Config::load())
        , hive_(std::make_shared<server::core::net::Hive>(io_))
        , heartbeat_timer_(io_)
        , signals_(io_)
        , backend_registry_(config_.backend_failure_threshold, config_.backend_retry_cooldown) {
        
        state_backend_ = create_backend();
        
        if (config_.dynamic_backends_active) {
            backend_refresher_ = std::make_unique<BackendRefresher>(
                io_, config_, backend_registry_, state_backend_.get());
        } else {
             // Load static backends immediately
             std::vector<BackendEndpoint> static_backends;
             for (size_t i = 0; i < config_.static_backend_endpoints.size(); ++i) {
                 // Simple parsing as in refresher
                 std::string_view val = config_.static_backend_endpoints[i];
                 std::string host = "127.0.0.1";
                 std::uint16_t port = 5000;
                 
                 auto delim = val.find(':');
                 if (delim != std::string_view::npos) {
                     host = std::string(val.substr(0, delim));
                     auto p = val.substr(delim + 1);
                     if (!p.empty()) port = static_cast<std::uint16_t>(std::stoul(std::string(p)));
                 } else if (!val.empty()) {
                     host = std::string(val);
                 }
                 
                 BackendEndpoint ep;
                 ep.id = "backend-" + std::to_string(i);
                 ep.host = host;
                 ep.port = port;
                 static_backends.push_back(std::move(ep));
            }
            backend_registry_.set_backends(std::move(static_backends));
        }

        configure();

        if (const char* port_env = std::getenv("METRICS_PORT")) {
            metrics_port_ = static_cast<std::uint16_t>(std::stoi(port_env));
        }

        metrics_server_ = std::make_unique<server::core::metrics::MetricsHttpServer>(metrics_port_, [this]() {
            std::ostringstream stream;
            stream << "# TYPE lb_backends_active gauge\n";
            stream << "lb_backends_active " << backend_registry_.size() << "\n";
            stream << "# TYPE lb_backend_idle_close_total counter\n";
            stream << "lb_backend_idle_close_total " << backend_idle_close_total_.load() << "\n";
            return stream.str();
        });
        metrics_server_->start();
    }

    LoadBalancerApp::~LoadBalancerApp() {
        stop();
    }

    int LoadBalancerApp::run() {
        start_grpc_server();
        schedule_heartbeat();
        if (backend_refresher_) {
            backend_refresher_->start();
        }
        handle_signals();

        server::core::log::info("LoadBalancerApp entering run loop");
        hive_->run();
        server::core::log::info("LoadBalancerApp stopped");

        stop_grpc_server();
        return 0;
    }

    void LoadBalancerApp::stop() {
        heartbeat_timer_.cancel();
        if (backend_refresher_) {
            backend_refresher_->stop();
        }
        if (metrics_server_) {
            metrics_server_->stop();
        }
        stop_grpc_server();
        if (hive_) {
            hive_->stop();
        }
        io_.stop();
    }

    void LoadBalancerApp::configure() {
        if (redis_client_) {
            session_directory_ = std::make_unique<SessionDirectory>(redis_client_, "gateway/session", config_.session_binding_ttl);
        } else {
            session_directory_.reset();
        }

        std::size_t active_backends = backend_registry_.size();
        server::core::log::info("LoadBalancerApp grpc_listen=" + config_.grpc_listen_address
            + " instance_id=" + config_.instance_id
            + " backends=" + std::to_string(active_backends)
            + " dynamic_backends=" + std::string(config_.dynamic_backends_active ? "1" : "0")
            + " idle_timeout=" + std::to_string(config_.backend_idle_timeout.count()) + "s");
    }

    void LoadBalancerApp::schedule_heartbeat() {
        heartbeat_timer_.expires_after(config_.heartbeat_interval);
        heartbeat_timer_.async_wait([this](const boost::system::error_code& ec) {
            if (ec == boost::asio::error::operation_aborted) return;
            if (ec) {
                server::core::log::warn(std::string("LoadBalancerApp heartbeat timer error: ") + ec.message());
            } else {
                publish_heartbeat();
            }
            schedule_heartbeat();
        });
    }

    void LoadBalancerApp::publish_heartbeat() {
        if (!state_backend_) return;
        
        server::state::InstanceRecord record{};
        record.instance_id = config_.instance_id;
        
        // Parse listen address for heartbeat
        std::string host = "127.0.0.1";
        std::uint16_t port = 7001;
        auto delim = config_.grpc_listen_address.find(':');
        if (delim != std::string::npos) {
            host = config_.grpc_listen_address.substr(0, delim);
            auto p = config_.grpc_listen_address.substr(delim + 1);
            if (!p.empty()) port = static_cast<std::uint16_t>(std::stoul(p));
        } else if (!config_.grpc_listen_address.empty()) {
            host = config_.grpc_listen_address;
        }

        if (grpc_selected_port_ > 0) {
            port = static_cast<std::uint16_t>(grpc_selected_port_);
        }
        record.host = std::move(host);
        record.port = port;
        record.role = "load_balancer";
        record.capacity = static_cast<std::uint32_t>(backend_registry_.size());
        record.active_sessions = 0;
        record.last_heartbeat_ms = to_ms(std::chrono::steady_clock::now());

        if (!state_backend_->upsert(record)) {
            server::core::log::warn("LoadBalancerApp heartbeat upsert failed");
        }
    }

    std::unique_ptr<server::state::IInstanceStateBackend> LoadBalancerApp::create_backend() {
        if (!config_.redis_uri.empty()) {
            try {
                server::storage::redis::Options opts;
                auto client = server::storage::redis::make_redis_client(config_.redis_uri, opts);
                if (client) {
                    redis_client_ = client;
                    auto state_client = server::state::make_redis_state_client(client);
                    server::core::log::info("LoadBalancerApp using Redis state backend");
                    return std::make_unique<server::state::RedisInstanceStateBackend>(
                        state_client,
                        config_.backend_registry_prefix,
                        config_.backend_state_ttl);
                }
            } catch (const std::exception& ex) {
                redis_client_.reset();
                server::core::log::warn(std::string("LoadBalancerApp Redis backend init failed: ") + ex.what());
            }
        }

        server::core::log::warn("LoadBalancerApp falling back to in-memory state backend");
        redis_client_.reset();
        return std::make_unique<server::state::InMemoryStateBackend>();
    }

    bool LoadBalancerApp::connect_backend(const BackendEndpoint& endpoint,
        boost::asio::ip::tcp::socket& socket,
        std::string& error) const {
        boost::asio::ip::tcp::resolver resolver(socket.get_executor());
        boost::system::error_code ec;
        auto results = resolver.resolve(endpoint.host, std::to_string(endpoint.port), ec);
        if (ec) {
            error = ec.message();
            return false;
        }
        boost::asio::connect(socket, results, ec);
        if (ec) {
            error = ec.message();
            return false;
        }
        return true;
    }

    grpc::Status LoadBalancerApp::handle_stream(
        grpc::ServerContext*,
        grpc::ServerReaderWriter<gateway::lb::RouteMessage, gateway::lb::RouteMessage>* stream) {
        gateway::lb::RouteMessage request;
        if (!stream->Read(&request)) {
            return grpc::Status::OK;
        }

        auto session_id = request.session_id();
        auto gateway_id = request.gateway_id();
        auto client_id = request.client_id();

        auto backend_opt = backend_registry_.select_backend(client_id);
        if (!backend_opt) {
            gateway::lb::RouteMessage error_msg;
            error_msg.set_kind(gateway::lb::ROUTE_KIND_SERVER_ERROR);
            error_msg.set_session_id(session_id);
            error_msg.set_gateway_id(gateway_id);
            error_msg.set_error("no backend available");
            stream->Write(error_msg);
            return grpc::Status(grpc::StatusCode::UNAVAILABLE, "no backend");
        }
        auto backend = *backend_opt;
        std::string assigned_backend_id = backend.id;

        std::atomic<bool> running{ true };
        std::mutex write_mutex;
        bool session_bound = false;

        auto release_session = [&]() {
            if (session_bound && session_directory_ && !client_id.empty()) {
                session_directory_->release_backend(client_id, assigned_backend_id);
                session_bound = false;
            }
        };

        auto now = std::chrono::steady_clock::now();
        if (session_directory_ && !client_id.empty() && client_id != "anonymous") {
            bool resolved = false;
            if (auto existing = session_directory_->find_backend(client_id)) {
                if (auto mapped = backend_registry_.find_backend_by_id(*existing)) {
                    if (backend_registry_.is_available(*mapped, now)) {
                        backend = *mapped;
                        assigned_backend_id = backend.id;
                        session_directory_->refresh_backend(client_id, assigned_backend_id);
                        resolved = true;
                    } else {
                        session_directory_->release_backend(client_id, *existing);
                    }
                } else {
                    session_directory_->release_backend(client_id, *existing);
                }
            }
            if (!resolved) {
                // Try to ensure backend binding
                if (auto resolved_id = session_directory_->ensure_backend(client_id, backend.id)) {
                     // Check if resolved backend is valid/healthy
                     if (auto mapped = backend_registry_.find_backend_by_id(*resolved_id)) {
                         if (backend_registry_.is_available(*mapped, now)) {
                             backend = *mapped;
                             assigned_backend_id = backend.id;
                             resolved = true;
                         } else {
                             session_directory_->release_backend(client_id, *resolved_id);
                         }
                     } else {
                         session_directory_->release_backend(client_id, *resolved_id);
                     }
                     
                     // Retry once if failed
                     if (!resolved) {
                         if (auto retry_id = session_directory_->ensure_backend(client_id, backend.id)) {
                             if (auto mapped_retry = backend_registry_.find_backend_by_id(*retry_id)) {
                                 if (backend_registry_.is_available(*mapped_retry, now)) {
                                     backend = *mapped_retry;
                                     assigned_backend_id = backend.id;
                                     resolved = true;
                                 } else {
                                     session_directory_->release_backend(client_id, *retry_id);
                                 }
                             } else {
                                 session_directory_->release_backend(client_id, *retry_id);
                             }
                         }
                     }
                }
            }
            session_bound = resolved;
        }

        boost::asio::io_context backend_io;
        boost::asio::ip::tcp::socket backend_socket(backend_io);
        std::atomic<std::chrono::steady_clock::time_point> last_backend_activity{ std::chrono::steady_clock::now() };
        std::string connect_error;
        if (!connect_backend(backend, backend_socket, connect_error)) {
            backend_registry_.mark_failure(assigned_backend_id);
            gateway::lb::RouteMessage error_msg;
            error_msg.set_kind(gateway::lb::ROUTE_KIND_SERVER_ERROR);
            error_msg.set_session_id(session_id);
            error_msg.set_gateway_id(gateway_id);
            error_msg.set_backend_id(assigned_backend_id);
            error_msg.set_error(connect_error);
            stream->Write(error_msg);
            release_session();
            return grpc::Status(grpc::StatusCode::UNAVAILABLE, connect_error);
        }

        backend_registry_.mark_success(assigned_backend_id);
        server::core::log::info("LoadBalancerApp routed session=" + session_id
            + " gateway=" + gateway_id + " client=" + client_id
            + " backend=" + backend.host + ":" + std::to_string(backend.port)
            + " sticky=" + (session_bound ? "1" : "0"));
        last_backend_activity.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);

        auto send_to_gateway = [&](gateway::lb::RouteMessage message) {
            message.set_session_id(session_id);
            message.set_gateway_id(gateway_id);
            message.set_backend_id(assigned_backend_id);
            std::lock_guard<std::mutex> lock(write_mutex);
            if (!stream->Write(message)) {
                running.store(false, std::memory_order_relaxed);
                return false;
            }
            return true;
        };

        auto forward_to_backend = [&](const std::string& payload) {
            boost::system::error_code write_ec;
            boost::asio::write(backend_socket, boost::asio::buffer(payload), write_ec);
            if (write_ec) {
                gateway::lb::RouteMessage error_msg;
                error_msg.set_kind(gateway::lb::ROUTE_KIND_SERVER_ERROR);
                error_msg.set_error(write_ec.message());
                backend_registry_.mark_failure(assigned_backend_id);
                send_to_gateway(std::move(error_msg));
                running.store(false, std::memory_order_relaxed);
                return false;
            }
            last_backend_activity.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);
            return true;
        };

        std::thread backend_reader([&]() {
            std::array<std::uint8_t, 8192> buffer{};
            while (running.load(std::memory_order_relaxed)) {
                boost::system::error_code read_ec;
                std::size_t bytes = backend_socket.read_some(boost::asio::buffer(buffer), read_ec);
                if (read_ec) {
                    if (running.load(std::memory_order_relaxed)) {
                        gateway::lb::RouteMessage close_msg;
                        if (read_ec == boost::asio::error::eof) {
                            close_msg.set_kind(gateway::lb::ROUTE_KIND_SERVER_CLOSE);
                        } else {
                            close_msg.set_kind(gateway::lb::ROUTE_KIND_SERVER_ERROR);
                            close_msg.set_error(read_ec.message());
                        }
                        send_to_gateway(std::move(close_msg));
                    }
                    running.store(false, std::memory_order_relaxed);
                    break;
                }

                if (bytes > 0) {
                    gateway::lb::RouteMessage payload_msg;
                    payload_msg.set_kind(gateway::lb::ROUTE_KIND_SERVER_PAYLOAD);
                    payload_msg.set_payload(reinterpret_cast<const char*>(buffer.data()), static_cast<int>(bytes));
                    if (!send_to_gateway(std::move(payload_msg))) {
                        break;
                    }
                }
            }
        });

        gateway::lb::RouteMessage response;
        while (running.load(std::memory_order_relaxed) && stream->Read(&response)) {
            switch (response.kind()) {
            case gateway::lb::ROUTE_KIND_CLIENT_PAYLOAD: {
                auto payload = response.payload();
                if (!forward_to_backend(payload)) {
                    running.store(false, std::memory_order_relaxed);
                }
                break;
            }
            case gateway::lb::ROUTE_KIND_CLIENT_CLOSE:
                running.store(false, std::memory_order_relaxed);
                break;
            default:
                break;
            }
        }

        running.store(false, std::memory_order_relaxed);
        if (backend_socket.is_open()) {
            boost::system::error_code ignored;
            backend_socket.close(ignored);
        }
        if (backend_reader.joinable()) {
            backend_reader.join();
        }

        release_session();
        return grpc::Status::OK;
    }

    void LoadBalancerApp::start_grpc_server() {
        std::string server_address = config_.grpc_listen_address;
        grpc::ServerBuilder builder;
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials(), &grpc_selected_port_);

        grpc_service_ = std::make_unique<GrpcServiceImpl>(*this);
        builder.RegisterService(grpc_service_.get());

        grpc_server_ = builder.BuildAndStart();
        server::core::log::info("LoadBalancerApp gRPC server listening on " + server_address);

        grpc_thread_ = std::thread([this]() {
            if (grpc_server_) {
                grpc_server_->Wait();
            }
        });
    }

    void LoadBalancerApp::stop_grpc_server() {
        if (grpc_server_) {
            grpc_server_->Shutdown();
            grpc_server_.reset();
        }
        if (grpc_thread_.joinable()) {
            grpc_thread_.join();
        }
    }

    void LoadBalancerApp::handle_signals() {
        signals_.add(SIGINT);
        signals_.add(SIGTERM);
        signals_.async_wait([this](const boost::system::error_code& error, int signal_number) {
            if (!error) {
                server::core::log::info("LoadBalancerApp received signal " + std::to_string(signal_number));
                stop();
            }
        });
    }

} // namespace load_balancer