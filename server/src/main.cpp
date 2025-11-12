// 진입점은 모든 초기화를 server::app::run_server로 위임한다.
#include "server/app/bootstrap.hpp"

int main(int argc, char** argv) {
    return server::app::run_server(argc, argv);
}
