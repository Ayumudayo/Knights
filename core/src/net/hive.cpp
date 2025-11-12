#include "server/core/net/hive.hpp"

namespace server::core::net {

Hive::Hive(io_context& io)
    : io_(io)
    , guard_(boost::asio::make_work_guard(io_)) {}

Hive::~Hive() {
    stop();
}

Hive::io_context& Hive::context() {
    return io_;
}

void Hive::run() {
    // work_guard가 살아 있는 동안 run()은 큐가 빌 때까지 블로킹된다.
    stopped_.store(false, std::memory_order_relaxed);
    io_.run();
}

void Hive::stop() {
    bool expected = false;
    if (!stopped_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;
    }

    // guard를 해제해야 io_context::run이 빠져나와 스레드가 종료된다.
    guard_.reset();
    io_.stop();
}

bool Hive::is_stopped() const {
    return stopped_.load(std::memory_order_relaxed);
}

} // namespace server::core::net
