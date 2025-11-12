#include "server/core/util/log.hpp"

#include <atomic>
#include <cstddef>
#include <chrono>
#include <ctime>
#include <deque>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <vector>

namespace server::core::log {

namespace {
std::atomic<level> g_level{level::info};
std::mutex g_mu;
std::deque<std::string> g_buffer;
std::size_t g_buffer_capacity = 256; // 최근 로그를 /metrics, /debug 등에 노출하기 위한 ring buffer

const char* to_cstr(level lv) {
    switch (lv) {
    case level::trace: return "TRACE";
    case level::debug: return "DEBUG";
    case level::info:  return "INFO";
    case level::warn:  return "WARN";
    case level::error: return "ERROR";
    }
    return "INFO";
}

std::string format_line(level lv, const std::string& msg) {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%F %T") << " [" << to_cstr(lv) << "] " << msg;
    return oss.str();
}

void push_buffer(const std::string& line) {
    // g_buffer는 diagnostics 용도로만 사용되므로, capacity가 0이면 완전히 비활성화한다.
    if (g_buffer_capacity == 0) return;
    g_buffer.push_back(line);
    if (g_buffer.size() > g_buffer_capacity) {
        g_buffer.pop_front();
    }
}

void emit(level lv, const std::string& msg) {
    if (static_cast<int>(lv) < static_cast<int>(g_level.load())) return;
    std::lock_guard<std::mutex> lk(g_mu);
    // 로그 라인은 포맷팅 후 stderr로 바로 내보내고, 필요 시 buffer에 보관한다.
    std::string line = format_line(lv, msg);
    push_buffer(line);
    std::cerr << line << std::endl;
}
} // namespace

void set_level(level lv) { g_level.store(lv); }

void set_buffer_capacity(std::size_t capacity) {
    std::lock_guard<std::mutex> lk(g_mu);
    g_buffer_capacity = capacity;
    while (g_buffer.size() > g_buffer_capacity) {
        g_buffer.pop_front();
    }
}

std::vector<std::string> recent(std::size_t limit) {
    std::lock_guard<std::mutex> lk(g_mu);
    if (limit == 0 || limit > g_buffer.size()) {
        limit = g_buffer.size();
    }
    std::vector<std::string> snapshot;
    snapshot.reserve(limit);
    auto begin = g_buffer.end() - static_cast<std::ptrdiff_t>(limit);
    snapshot.insert(snapshot.end(), begin, g_buffer.end());
    return snapshot;
}

void trace(const std::string& msg) { emit(level::trace, msg); }
void debug(const std::string& msg) { emit(level::debug, msg); }
void info(const std::string& msg)  { emit(level::info, msg); }
void warn(const std::string& msg)  { emit(level::warn, msg); }
void error(const std::string& msg) { emit(level::error, msg); }

} // namespace server::core::log
