#pragma once

#include <memory>
#include <string>
#include <cstddef>

namespace server::storage::redis {

struct Options {
    std::size_t pool_max{10};
    bool use_streams{false};
};

class IRedisClient {
public:
    virtual ~IRedisClient() = default;
    virtual bool health_check() = 0;
};

// Redis 클라이언트/풀 팩토리(스켈레톤)
std::shared_ptr<IRedisClient> make_redis_client(const std::string& uri, const Options& opts);

} // namespace server::storage::redis

