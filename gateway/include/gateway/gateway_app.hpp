#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

#include "gateway/auth/authenticator.hpp"
#include "gateway_lb.grpc.pb.h"
#include "server/core/net/hive.hpp"
#include "server/core/net/listener.hpp"
#include "server/core/metrics/http_server.hpp"

namespace gateway {

class GatewayConnection;

// Gateway 애플리케이션의 메인 클래스입니다.
// TCP 리스너를 구동하여 클라이언트 연결을 수락하고,
// 각 연결에 대해 로드 밸런서(LB)와의 gRPC 세션을 관리합니다.
class GatewayApp {
public:
    // 로드 밸런서와의 개별 세션을 관리하는 내부 클래스입니다.
    // gRPC의 양방향 스트리밍(Bi-directional streaming)을 사용하여
    // GatewayConnection(TCP)과 로드 밸런서 간의 데이터를 실시간으로 중계합니다.
    class LbSession : public std::enable_shared_from_this<LbSession> {
    public:
        LbSession(GatewayApp& app,
                  std::string session_id,
                  std::string client_id,
                  std::weak_ptr<GatewayConnection> connection);
        ~LbSession();

        bool start();
        bool send(gateway::lb::RouteMessageKind kind, const std::vector<std::uint8_t>& payload);
        void stop();
        const std::string& session_id() const;

    private:
        void read_loop();

        GatewayApp& app_;
        std::string session_id_;
        std::string client_id_;
        std::weak_ptr<GatewayConnection> connection_;
        std::unique_ptr<grpc::ClientContext> context_;
        std::unique_ptr<grpc::ClientReaderWriter<gateway::lb::RouteMessage, gateway::lb::RouteMessage>> stream_;
        std::thread reader_thread_;
        std::mutex write_mutex_;
        std::atomic<bool> stopped_{false};
    };
    using LbSessionPtr = std::shared_ptr<LbSession>;

    GatewayApp();
    ~GatewayApp();

    int run();
    void stop();

    LbSessionPtr create_lb_session(const std::string& client_id,
                                   std::weak_ptr<GatewayConnection> connection);
    void close_lb_session(const std::string& session_id);
    std::string gateway_id() const { return gateway_id_; }

    boost::asio::io_context io_;
    std::shared_ptr<server::core::net::Hive> hive_;
    std::shared_ptr<server::core::net::Listener> listener_;
    boost::asio::signal_set signals_;
    std::shared_ptr<auth::IAuthenticator> authenticator_;
    std::string gateway_id_;
    std::string lb_endpoint_;
    std::string listen_host_;
    std::uint16_t listen_port_{6000};

private:
    void configure_gateway();
    void configure_load_balancer();
    void start_listener();
    void handle_signals();
    void load_environment();

    struct SessionState {
        LbSessionPtr session;
    };
    std::mutex session_mutex_;
    std::unordered_map<std::string, SessionState> sessions_;
    std::shared_ptr<gateway::lb::LoadBalancerService::Stub> lb_stub_;
    std::atomic<std::uint64_t> session_counter_{0};
    std::unique_ptr<server::core::metrics::MetricsHttpServer> metrics_server_;
    std::uint16_t metrics_port_{6001};
};

} // namespace gateway
