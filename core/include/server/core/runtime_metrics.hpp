#pragma once

#include <chrono>
#include <cstdint>

namespace server::core::runtime_metrics {

struct Snapshot {
    std::uint64_t accept_total{0};
    std::uint64_t session_started_total{0};
    std::uint64_t session_stopped_total{0};
    std::uint64_t session_active{0};
    std::uint64_t frame_total{0};
    std::uint64_t frame_error_total{0};
    std::uint64_t dispatch_total{0};
    std::uint64_t dispatch_unknown_total{0};
    std::uint64_t dispatch_exception_total{0};
    std::uint64_t dispatch_latency_sum_ns{0};
    std::uint64_t dispatch_latency_count{0};
    std::uint64_t dispatch_latency_last_ns{0};
    std::uint64_t dispatch_latency_max_ns{0};
};

void record_accept();
void record_session_start();
void record_session_stop();
void record_frame_ok();
void record_frame_error();
void record_dispatch_attempt(bool handler_found, std::chrono::nanoseconds elapsed);
void record_dispatch_exception();
Snapshot snapshot();

} // namespace server::core::runtime_metrics
