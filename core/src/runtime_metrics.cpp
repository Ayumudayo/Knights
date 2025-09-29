#include "server/core/runtime_metrics.hpp"

#include <atomic>
#include <algorithm>

namespace server::core::runtime_metrics {

namespace {
struct RuntimeCounters {
    std::atomic<std::uint64_t> accept_total{0};
    std::atomic<std::uint64_t> session_started_total{0};
    std::atomic<std::uint64_t> session_stopped_total{0};
    std::atomic<std::uint64_t> session_active{0};
    std::atomic<std::uint64_t> frame_total{0};
    std::atomic<std::uint64_t> frame_error_total{0};
    std::atomic<std::uint64_t> dispatch_total{0};
    std::atomic<std::uint64_t> dispatch_unknown_total{0};
    std::atomic<std::uint64_t> dispatch_exception_total{0};
    std::atomic<std::uint64_t> dispatch_latency_sum_ns{0};
    std::atomic<std::uint64_t> dispatch_latency_count{0};
    std::atomic<std::uint64_t> dispatch_latency_last_ns{0};
    std::atomic<std::uint64_t> dispatch_latency_max_ns{0};
};

RuntimeCounters& counters() {
    static RuntimeCounters c;
    return c;
}

} // namespace

void record_accept() {
    counters().accept_total.fetch_add(1, std::memory_order_relaxed);
}

void record_session_start() {
    counters().session_started_total.fetch_add(1, std::memory_order_relaxed);
    counters().session_active.fetch_add(1, std::memory_order_relaxed);
}

void record_session_stop() {
    counters().session_stopped_total.fetch_add(1, std::memory_order_relaxed);
    counters().session_active.fetch_sub(1, std::memory_order_relaxed);
}

void record_frame_ok() {
    counters().frame_total.fetch_add(1, std::memory_order_relaxed);
}

void record_frame_error() {
    counters().frame_error_total.fetch_add(1, std::memory_order_relaxed);
}

void record_dispatch_attempt(bool handler_found, std::chrono::nanoseconds elapsed) {
    counters().dispatch_total.fetch_add(1, std::memory_order_relaxed);
    if (!handler_found) {
        counters().dispatch_unknown_total.fetch_add(1, std::memory_order_relaxed);
    }
    const auto ns = static_cast<std::uint64_t>(elapsed.count());
    counters().dispatch_latency_sum_ns.fetch_add(ns, std::memory_order_relaxed);
    counters().dispatch_latency_count.fetch_add(1, std::memory_order_relaxed);
    counters().dispatch_latency_last_ns.store(ns, std::memory_order_relaxed);

    auto& max_ref = counters().dispatch_latency_max_ns;
    std::uint64_t current_max = max_ref.load(std::memory_order_relaxed);
    while (current_max < ns && !max_ref.compare_exchange_weak(current_max, ns, std::memory_order_relaxed)) {
        // retry until successfully updated or observed newer max
    }
}

void record_dispatch_exception() {
    counters().dispatch_exception_total.fetch_add(1, std::memory_order_relaxed);
}

Snapshot snapshot() {
    Snapshot snap{};
    auto& c = counters();
    snap.accept_total = c.accept_total.load(std::memory_order_relaxed);
    snap.session_started_total = c.session_started_total.load(std::memory_order_relaxed);
    snap.session_stopped_total = c.session_stopped_total.load(std::memory_order_relaxed);
    snap.session_active = c.session_active.load(std::memory_order_relaxed);
    snap.frame_total = c.frame_total.load(std::memory_order_relaxed);
    snap.frame_error_total = c.frame_error_total.load(std::memory_order_relaxed);
    snap.dispatch_total = c.dispatch_total.load(std::memory_order_relaxed);
    snap.dispatch_unknown_total = c.dispatch_unknown_total.load(std::memory_order_relaxed);
    snap.dispatch_exception_total = c.dispatch_exception_total.load(std::memory_order_relaxed);
    snap.dispatch_latency_sum_ns = c.dispatch_latency_sum_ns.load(std::memory_order_relaxed);
    snap.dispatch_latency_count = c.dispatch_latency_count.load(std::memory_order_relaxed);
    snap.dispatch_latency_last_ns = c.dispatch_latency_last_ns.load(std::memory_order_relaxed);
    snap.dispatch_latency_max_ns = c.dispatch_latency_max_ns.load(std::memory_order_relaxed);
    return snap;
}

} // namespace server::core::runtime_metrics

