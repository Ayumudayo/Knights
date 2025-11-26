#include "load_balancer/grpc_service_impl.hpp"
#include "load_balancer/load_balancer_app.hpp"

namespace load_balancer {

GrpcServiceImpl::GrpcServiceImpl(LoadBalancerApp& owner)
    : owner_(owner) {
}

grpc::Status GrpcServiceImpl::Forward(
    grpc::ServerContext*, const gateway::lb::RouteRequest*, gateway::lb::RouteResponse* response) {
    response->set_accepted(false);
    response->set_reason("Use Stream RPC");
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Stream RPC required");
}

grpc::Status GrpcServiceImpl::Stream(
    grpc::ServerContext* context,
    grpc::ServerReaderWriter<gateway::lb::RouteMessage, gateway::lb::RouteMessage>* stream) {
    return owner_.handle_stream(context, stream);
}

} // namespace load_balancer
