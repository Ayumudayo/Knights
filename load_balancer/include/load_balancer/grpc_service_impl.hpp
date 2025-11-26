#pragma once

#include "gateway_lb.grpc.pb.h"
#include <grpcpp/grpcpp.h>

namespace load_balancer {

class LoadBalancerApp;

class GrpcServiceImpl final : public gateway::lb::LoadBalancerService::Service {
public:
    explicit GrpcServiceImpl(LoadBalancerApp& owner);
    grpc::Status Forward(grpc::ServerContext*, const gateway::lb::RouteRequest*,
                         gateway::lb::RouteResponse*) override;
    grpc::Status Stream(grpc::ServerContext* context,
                        grpc::ServerReaderWriter<gateway::lb::RouteMessage, gateway::lb::RouteMessage>* stream) override;
private:
    LoadBalancerApp& owner_;
};

} // namespace load_balancer
