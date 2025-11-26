#include "client/app/application.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

// CLI 인자에서 포트를 파싱해 유효성을 검사한다.
unsigned short ParsePort(const char* value) {
    if (!value || !*value) {
        return 0;
    }
    char* end = nullptr;
    auto parsed = std::strtoul(value, &end, 10);
    if (value == end || *end != '\0' || parsed == 0 || parsed > 65535) {
        return 0;
    }
    return static_cast<unsigned short>(parsed);
}

} // namespace

int main(int argc, char** argv) {
#ifdef NDEBUG
    // Release 빌드: 명시적인 host/port 인자를 요구한다.
    // 배포 환경에서는 실수로 로컬호스트에 접속하는 것을 방지하기 위함입니다.
    if (argc < 3) {
        std::cerr << "usage: dev_chat_cli <host> <port>\n";
        return 1;
    }
    std::string host = argv[1];
    unsigned short port = ParsePort(argv[2]);
    if (port == 0) {
        std::cerr << "invalid port: " << argv[2] << "\n";
        return 1;
    }
    client::app::Application app(std::move(host), port, false);
#else
    // Debug 빌드: 기본 loopback 환경을 사용해 빠르게 실행한다.
    // 개발 편의성을 위해 인자 없이 실행하면 로컬 서버(127.0.0.1:6000)로 접속합니다.
    (void)argc;
    (void)argv;
    client::app::Application app("127.0.0.1", 6000, true);
#endif
    return app.Run();
}
