#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

namespace gateway {

/**
 * @brief Tracks repeated UDP bind failures per endpoint and applies temporary blocks.
 *
 * The guard maintains a failure window and block duration to reduce repeated
 * abuse attempts against bind ticket validation.
 */
class UdpBindAbuseGuard {
public:
    /** @brief Current block state for an endpoint. */
    struct BlockState {
        bool blocked{false};             ///< Whether endpoint is currently blocked.
        std::uint64_t retry_after_ms{0}; ///< Remaining block time in milliseconds.
    };

    UdpBindAbuseGuard() = default;

    /**
     * @brief Configures failure window and block thresholds.
     * @param fail_window_ms Rolling window length in milliseconds.
     * @param fail_limit Failures required to trigger a block.
     * @param block_ms Block duration in milliseconds.
     */
    void configure(std::uint32_t fail_window_ms,
                   std::uint32_t fail_limit,
                   std::uint32_t block_ms) {
        fail_window_ms_ = fail_window_ms;
        fail_limit_ = fail_limit;
        block_ms_ = block_ms;
    }

    /**
     * @brief Returns current block state for an endpoint.
     * @param endpoint_key Endpoint identity key (`ip:port`).
     * @param now_ms Current unix timestamp in milliseconds.
     * @return Block state with remaining retry delay when blocked.
     */
    BlockState block_state(std::string_view endpoint_key, std::uint64_t now_ms) {
        auto it = entries_.find(std::string(endpoint_key));
        if (it == entries_.end()) {
            return {};
        }

        auto& entry = it->second;
        if (entry.blocked_until_ms <= now_ms) {
            entry.blocked_until_ms = 0;
            return {};
        }

        return {true, entry.blocked_until_ms - now_ms};
    }

    /**
     * @brief Records a bind failure and updates block status.
     * @param endpoint_key Endpoint identity key (`ip:port`).
     * @param now_ms Current unix timestamp in milliseconds.
     * @return `true` when this failure triggered a new block.
     */
    bool record_failure(std::string_view endpoint_key, std::uint64_t now_ms) {
        auto& entry = entries_[std::string(endpoint_key)];
        if (entry.blocked_until_ms > now_ms) {
            return false;
        }

        if (entry.window_started_ms == 0 || now_ms < entry.window_started_ms ||
            (now_ms - entry.window_started_ms) > fail_window_ms_) {
            entry.window_started_ms = now_ms;
            entry.failures_in_window = 0;
        }

        ++entry.failures_in_window;
        if (entry.failures_in_window < fail_limit_) {
            return false;
        }

        entry.failures_in_window = 0;
        entry.window_started_ms = 0;
        entry.blocked_until_ms = now_ms + block_ms_;
        return true;
    }

    /**
     * @brief Resets failure counters after a successful bind.
     * @param endpoint_key Endpoint identity key (`ip:port`).
     */
    void record_success(std::string_view endpoint_key) {
        auto it = entries_.find(std::string(endpoint_key));
        if (it == entries_.end()) {
            return;
        }
        it->second.failures_in_window = 0;
        it->second.window_started_ms = 0;
    }

private:
    /** @brief Endpoint별 실패 윈도우/차단 시각을 보관하는 내부 상태입니다. */
    struct Entry {
        std::uint64_t window_started_ms{0};
        std::uint32_t failures_in_window{0};
        std::uint64_t blocked_until_ms{0};
    };

    std::uint32_t fail_window_ms_{10000};
    std::uint32_t fail_limit_{5};
    std::uint32_t block_ms_{60000};
    std::unordered_map<std::string, Entry> entries_;
};

} // namespace gateway
