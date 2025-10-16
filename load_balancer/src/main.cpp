#include <exception>

#include "load_balancer/load_balancer_app.hpp"
#include "server/core/util/log.hpp"

int main() {
    try {
        load_balancer::LoadBalancerApp app;
        return app.run();
    } catch (const std::exception& ex) {
        server::core::log::error(std::string("LoadBalancerApp fatal error: ") + ex.what());
    } catch (...) {
        server::core::log::error("LoadBalancerApp fatal error: unknown exception");
    }
    return 1;
}
