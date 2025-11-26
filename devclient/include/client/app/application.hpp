#pragma once

#include <memory>
#include <string>

class NetClient;

namespace client::app {

class AppState;
class CommandProcessor;
class NetworkRouter;
class UiBuilder;

// 클라이언트 애플리케이션의 진입점 클래스입니다.
// 네트워크 클라이언트, UI 빌더, 커맨드 프로세서 등을 초기화하고 메인 루프를 실행합니다.
class Application {
public:
    Application(std::string host, unsigned short port, bool allow_env_override);
    Application();
    ~Application();

    int Run();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace client::app

