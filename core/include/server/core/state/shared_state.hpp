#pragma once

#include <atomic>
#include <cstddef>
#include <random>

namespace server::core {

struct SharedState {
    std::atomic<std::size_t> connection_count{0};
    std::size_t max_connections = 10'000; // 0이면 최대 연결 수를 제한하지 않는다.
    std::atomic<std::uint32_t> next_session_id;
    
    // 생성자: 랜덤 오프셋으로 session_id를 초기화하여 
    // 서버 재시작/다중 인스턴스 간 충돌 방지
    SharedState() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<std::uint32_t> dis(1000, 0xFFFFFF00u);
        next_session_id.store(dis(gen));
    }
};

} // namespace server::core

