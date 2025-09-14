// UTF-8, 한국어 주석
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <cstdlib>

#include "server/storage/redis/client.hpp"
#include "server/core/util/log.hpp"

// 최소 Write-behind 워커 스켈레톤: Redis Streams를 폴링/구독하는 실제 구현은 추후 보강
// 현재는 환경만 읽고 Redis 헬스체크 후 대기 루프를 도는 형태입니다.

int main(int, char**) {
    using server::core::log::info;
    using server::core::log::warn;
    try {
        const char* ruri = std::getenv("REDIS_URI");
        if (!ruri || !*ruri) {
            std::cerr << "WB worker: REDIS_URI not set" << std::endl;
            return 2;
        }
        server::storage::redis::Options ropts{};
        if (const char* v = std::getenv("REDIS_POOL_MAX")) ropts.pool_max = static_cast<std::size_t>(std::strtoul(v, nullptr, 10));
        auto redis = server::storage::redis::make_redis_client(ruri, ropts);
        if (!redis || !redis->health_check()) {
            std::cerr << "WB worker: Redis health check failed" << std::endl;
            return 3;
        }
        info("WB worker started (skeleton). Waiting for future stream processing...");
        // TODO: Streams (XREADGROUP) 로직 연결
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    } catch (const std::exception& e) {
        std::cerr << "WB worker error: " << e.what() << std::endl;
        return 1;
    }
}

