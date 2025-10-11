#pragma once

#include <cstddef>

namespace server::core {

struct SessionOptions {
    std::size_t recv_max_payload = 32 * 1024;   // 수신 payload 최대 크기(바이트, 0이면 제한 없음)
    std::size_t send_queue_max   = 256 * 1024;  // 송신 큐 누적량 상한(바이트, 0이면 제한 없음)
    unsigned    heartbeat_interval_ms = 10'000; // heartbeat 주기(ms, 0이면 heartbeat 비활성)
    unsigned    read_timeout_ms       = 15'000; // 수신 타임아웃(ms, 0이면 감시하지 않음)
    unsigned    write_timeout_ms      = 15'000; // 송신 타임아웃(ms, 0이면 감시하지 않음)
};

} // namespace server::core

