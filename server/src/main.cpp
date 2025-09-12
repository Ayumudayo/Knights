// Entry point delegates to bootstrap module.
#include "server/app/bootstrap.hpp"

int main(int argc, char** argv) {
    return server::app::run_server(argc, argv);
}
