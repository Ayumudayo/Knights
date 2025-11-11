#include "client/app/application.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {
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
    (void)argc;
    (void)argv;
    client::app::Application app("127.0.0.1", 6000, true);
#endif
    return app.Run();
}
