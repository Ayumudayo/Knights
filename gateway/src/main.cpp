#include <exception>

#include "gateway/gateway_app.hpp"
#include "server/core/util/log.hpp"

int main() {
    try {
        // GatewayApp 인스턴스를 생성하고 메인 루프를 시작합니다.
        // 예외가 발생하면 로그를 남기고 종료합니다.
        gateway::GatewayApp app;
        return app.run();
    } catch (const std::exception& ex) {
        server::core::log::error(std::string("GatewayApp fatal error: ") + ex.what());
    } catch (...) {
        server::core::log::error("GatewayApp fatal error: unknown exception");
    }
    return 1;
}
