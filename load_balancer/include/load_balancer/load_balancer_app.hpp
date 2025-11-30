#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>
#include <grpcpp/grpcpp.h>

#include "gateway_lb.grpc.pb.h"
#include "load_balancer/session_directory.hpp"
#include "load_balancer/backend_registry.hpp"
#include "load_balancer/config.hpp"
#include "load_balancer/backend_refresher.hpp"
#include "server/core/net/hive.hpp"
#include "server/state/instance_registry.hpp"
#include "server/core/metrics/http_server.hpp"

namespace load_balancer {

class GrpcServiceImpl;

/**
 * @brief 로드 밸런서 애플리케이션 클래스
 * 
 * Gateway와 백엔드 서버(ServerApp) 사이에서 트래픽을 분산시키는 역할을 합니다.
 * gRPC를 통해 Gateway로부터 요청을 받고, TCP를 통해 백엔드 서버로 전달합니다.
 */
class LoadBalancerApp {
public:
    LoadBalancerApp();
    ~LoadBalancerApp();

    int run();
    void stop();

    // gRPC 스트림 핸들러 (메인 루프) - GrpcServiceImpl에서 호출
    grpc::Status handle_stream(grpc::ServerContext* context,
                               grpc::ServerReaderWriter<gateway::lb::RouteMessage, gateway::lb::RouteMessage>* stream);

private:
    void configure();
    
    void schedule_heartbeat();
    void publish_heartbeat();
    std::unique_ptr<server::state::IInstanceStateBackend> create_backend();
    
    // 백엔드 TCP 연결
    bool connect_backend(const BackendEndpoint& endpoint,
                         boost::asio::ip::tcp::socket& socket,
                         std::string& error) const;
                         
    void start_grpc_server();
    void stop_grpc_server();
    void handle_signals();

    Config config_;
    boost::asio::io_context io_;
    std::shared_ptr<server::core::net::Hive> hive_;
    boost::asio::steady_timer heartbeat_timer_;
    boost::asio::signal_set signals_;
    
    std::unique_ptr<server::state::IInstanceStateBackend> state_backend_;
    std::shared_ptr<server::storage::redis::IRedisClient> redis_client_;
    std::unique_ptr<SessionDirectory> session_directory_; // Sticky Session 관리자

    std::unique_ptr<GrpcServiceImpl> grpc_service_;
    std::unique_ptr<grpc::Server> grpc_server_;
    std::thread grpc_thread_;
    int grpc_selected_port_{0};

    // 백엔드 레지스트리 (상태 및 선택 로직 관리)
    BackendRegistry backend_registry_;
    
    // 백엔드 리프레셔 (주기적 업데이트)
    std::unique_ptr<BackendRefresher> backend_refresher_;
    
    std::atomic<std::uint64_t> backend_idle_close_total_{0};
    
    std::unique_ptr<server::core::metrics::MetricsHttpServer> metrics_server_;
    std::uint16_t metrics_port_{7002};
};

} // namespace load_balancer
