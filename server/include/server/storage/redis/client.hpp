#pragma once

#include <memory>
#include <string>

#include "server/core/storage/redis/client.hpp"

namespace server::storage::redis {
using server::core::storage::redis::Options;
using server::core::storage::redis::IRedisClient;

/**
 * @brief Redis 클라이언트/풀 구현체를 생성합니다.
 * @param uri Redis 접속 URI
 * @param opts 연결 풀/기능 옵션
 * @return 생성된 Redis 클라이언트
 */
std::shared_ptr<IRedisClient> make_redis_client(const std::string& uri, const Options& opts);

} // namespace server::storage::redis
