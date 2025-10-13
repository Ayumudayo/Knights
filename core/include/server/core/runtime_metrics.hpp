#pragma once

#include <chrono>
#include <cstdint>
#include <cstddef>
#include <vector>
#include <utility>

namespace server::core::runtime_metrics {

struct Snapshot {
    std::uint64_t accept_total{0};
    std::uint64_t session_started_total{0};
    std::uint64_t session_stopped_total{0};
    std::uint64_t session_active{0};
    std::uint64_t session_timeout_total{0};
    std::uint64_t heartbeat_timeout_total{0};
    std::uint64_t send_queue_drop_total{0};
    std::uint64_t frame_total{0};
    std::uint64_t frame_error_total{0};
    std::uint64_t frame_payload_sum_bytes{0};
    std::uint64_t frame_payload_count{0};
    std::uint64_t frame_payload_max_bytes{0};
    std::uint64_t dispatch_total{0};
    std::uint64_t dispatch_unknown_total{0};
    std::uint64_t dispatch_exception_total{0};
    std::uint64_t dispatch_latency_sum_ns{0};
    std::uint64_t dispatch_latency_count{0};
    std::uint64_t dispatch_latency_last_ns{0};
    std::uint64_t dispatch_latency_max_ns{0};
    std::uint64_t job_queue_depth{0};
    std::uint64_t job_queue_depth_peak{0};
    std::uint64_t db_job_queue_depth{0};
    std::uint64_t db_job_queue_depth_peak{0};
    std::uint64_t db_job_processed_total{0};
    std::uint64_t db_job_failed_total{0};
    std::uint64_t memory_pool_capacity{0};
    std::uint64_t memory_pool_in_use{0};
    std::uint64_t memory_pool_in_use_peak{0};
    std::vector<std::pair<std::uint16_t, std::uint64_t>> opcode_counts;
};

void record_accept();
void record_session_start();
void record_session_stop();
void record_frame_ok();
void record_frame_error();
void record_dispatch_attempt(bool handler_found, std::chrono::nanoseconds elapsed);
void record_dispatch_exception();
void record_session_timeout();
void record_heartbeat_timeout();
void record_send_queue_drop();
void record_frame_payload(std::size_t bytes);
void record_dispatch_opcode(std::uint16_t opcode);
void record_job_queue_depth(std::size_t depth);
void record_db_job_queue_depth(std::size_t depth);
void record_db_job_processed();
void record_db_job_failed();
void register_memory_pool_capacity(std::size_t capacity);
void record_memory_pool_acquire();
void record_memory_pool_release();
Snapshot snapshot();

} // namespace server::core::runtime_metrics
