#pragma once

#include <atomic>
#include <cstddef>

namespace server::core {

struct SharedState {
    std::atomic<std::size_t> connection_count{0};
    std::size_t max_connections = 10'000; // 0이면 최대 연결 수를 제한하지 않는다.
    std::atomic<std::uint32_t> next_session_id{1};
};

} // namespace server::core

