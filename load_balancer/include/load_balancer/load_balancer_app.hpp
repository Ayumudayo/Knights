#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>
#include <grpcpp/grpcpp.h>

#include "gateway_lb.grpc.pb.h"
#include "server/core/net/hive.hpp"
#include "server/state/instance_registry.hpp"

namespace load_balancer {

class LoadBalancerApp {
public:
    LoadBalancerApp();
    ~LoadBalancerApp();

    int run();
    void stop();

private:
    struct BackendEndpoint {
        std::string id;
        std::string host;
        std::uint16_t port{0};
    };

    class GrpcServiceImpl final : public gateway::lb::LoadBalancerService::Service {
    public:
        explicit GrpcServiceImpl(LoadBalancerApp& owner);
        grpc::Status Forward(grpc::ServerContext*, const gateway::lb::RouteRequest*,
                             gateway::lb::RouteResponse*) override;
        grpc::Status Stream(grpc::ServerContext*,
                            grpc::ServerReaderWriter<gateway::lb::RouteMessage, gateway::lb::RouteMessage>*) override;
    private:
        LoadBalancerApp& owner_;
    };

    void configure();
    void configure_backends(std::string_view list);
    void schedule_heartbeat();
    void publish_heartbeat();
    std::unique_ptr<server::state::IInstanceStateBackend> create_backend();
    std::optional<BackendEndpoint> select_backend();
    grpc::Status handle_stream(grpc::ServerContext* context,
                               grpc::ServerReaderWriter<gateway::lb::RouteMessage, gateway::lb::RouteMessage>* stream);
    bool connect_backend(const BackendEndpoint& endpoint,
                         boost::asio::ip::tcp::socket& socket,
                         std::string& error) const;
    void start_grpc_server();
    void stop_grpc_server();
    void handle_signals();
    void load_environment();

    boost::asio::io_context io_;
    std::shared_ptr<server::core::net::Hive> hive_;
    boost::asio::steady_timer heartbeat_timer_;
    boost::asio::signal_set signals_;
    std::unique_ptr<server::state::IInstanceStateBackend> state_backend_;

    std::string grpc_listen_address_;
    std::unique_ptr<GrpcServiceImpl> grpc_service_;
    std::unique_ptr<grpc::Server> grpc_server_;
    std::thread grpc_thread_;
    int grpc_selected_port_{0};

    std::string instance_id_;
    std::vector<BackendEndpoint> backends_;
    std::atomic<std::size_t> backend_index_{0};
    std::chrono::seconds heartbeat_interval_{std::chrono::seconds{5}};
    std::chrono::seconds backend_state_ttl_{std::chrono::seconds{30}};
};

} // namespace load_balancer
