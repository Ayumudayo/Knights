#pragma once

#include <memory>
#include <string>

class NetClient;

namespace client::app {

class AppState;
class CommandProcessor;
class NetworkRouter;
class UiBuilder;

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

