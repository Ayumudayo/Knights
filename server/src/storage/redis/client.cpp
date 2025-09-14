#include "server/storage/redis/client.hpp"

#include <memory>
#include <utility>

namespace server::storage::redis {

class RedisClientStub final : public IRedisClient {
public:
    explicit RedisClientStub(std::string uri, Options opts)
        : uri_(std::move(uri)), opts_(opts) {}

    bool health_check() override {
        // TODO: PING in real implementation
        (void)uri_; (void)opts_;
        return true;
    }

private:
    std::string uri_;
    Options opts_{};
};

std::shared_ptr<IRedisClient> make_redis_client(const std::string& uri, const Options& opts) {
    return std::make_shared<RedisClientStub>(uri, opts);
}

} // namespace server::storage::redis

