#include <exception>

#include "load_balancer/load_balancer_app.hpp"
#include "server/core/util/log.hpp"

int main() {
    try {
        // LoadBalancerApp 인스턴스를 생성하고 메인 루프를 시작합니다.
        // 예외가 발생하면 로그를 남기고 종료합니다.
        load_balancer::LoadBalancerApp app;
        return app.run();
    } catch (const std::exception& ex) {
        server::core::log::error(std::string("LoadBalancerApp fatal error: ") + ex.what());
    } catch (...) {
        server::core::log::error("LoadBalancerApp fatal error: unknown exception");
    }
    return 1;
}
