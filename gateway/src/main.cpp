#include <exception>

#include "gateway/gateway_app.hpp"
#include "server/core/util/log.hpp"

int main() {
    try {
        gateway::GatewayApp app;
        return app.run();
    } catch (const std::exception& ex) {
        server::core::log::error(std::string("GatewayApp fatal error: ") + ex.what());
    } catch (...) {
        server::core::log::error("GatewayApp fatal error: unknown exception");
    }
    return 1;
}
