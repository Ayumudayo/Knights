#include "server/core/metrics/metrics.hpp"
#include <unordered_map>
#include <mutex>

namespace server::core::metrics {

namespace {
struct NoopCounter final : Counter { void inc(double, Labels) override {} };
struct NoopGauge final : Gauge { void set(double, Labels) override {} void inc(double, Labels) override {} void dec(double, Labels) override {} };
struct NoopHistogram final : Histogram { void observe(double, Labels) override {} };

std::mutex& mu() { static std::mutex m; return m; }
std::unordered_map<std::string, NoopCounter>& counters() { static std::unordered_map<std::string, NoopCounter> m; return m; }
std::unordered_map<std::string, NoopGauge>& gauges() { static std::unordered_map<std::string, NoopGauge> m; return m; }
std::unordered_map<std::string, NoopHistogram>& histos() { static std::unordered_map<std::string, NoopHistogram> m; return m; }
}

// Prometheus exporter가 붙지 않은 초기 단계에서도 공통 metrics API를 호출할 수 있도록
// Noop 객체를 미리 준비해두고, 실제 exporter가 연결되면 즉시 교체되는 구조다.
Counter& get_counter(const std::string& name) {
    std::lock_guard<std::mutex> lk(mu());
    return counters().try_emplace(name).first->second;
}

Gauge& get_gauge(const std::string& name) {
    std::lock_guard<std::mutex> lk(mu());
    return gauges().try_emplace(name).first->second;
}

Histogram& get_histogram(const std::string& name) {
    std::lock_guard<std::mutex> lk(mu());
    return histos().try_emplace(name).first->second;
}

} // namespace server::core::metrics
