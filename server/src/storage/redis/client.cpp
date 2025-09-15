#include "server/storage/redis/client.hpp"

#include <memory>
#include <utility>
#include <thread>
#include <atomic>
#include <functional>

#if defined(HAVE_REDIS_PLUS_PLUS)
#include <sw/redis++/redis++.h>
#endif
#include "server/core/util/log.hpp"

namespace server::storage::redis {

#if defined(HAVE_REDIS_PLUS_PLUS)
class RedisClientImpl final : public IRedisClient {
public:
    explicit RedisClientImpl(const std::string& uri, Options opts) {
        (void)opts;
        // 일부 배포판에선 ConnectionOptions.uri 필드가 없을 수 있음. 간단한 URI 생성자를 사용.
        redis_ = std::make_unique<sw::redis::Redis>(uri);
    }

    bool health_check() override {
        try {
            auto pong = redis_->ping();
            return !pong.empty();
        } catch (const std::exception& e) {
            server::core::log::warn(std::string("Redis PING failed: ") + e.what());
            return false;
        } catch (...) {
            server::core::log::warn("Redis PING failed: unknown exception");
            return false;
        }
    }

    bool lpush_trim(const std::string& key, const std::string& value, std::size_t maxlen) override {
        try {
            redis_->lpush(key, value);
            if (maxlen > 0) redis_->ltrim(key, 0, static_cast<long long>(maxlen - 1));
            return true;
        } catch (const std::exception& e) {
            server::core::log::warn(std::string("Redis LPUSH/LTRIM failed: ") + e.what());
            return false;
        } catch (...) {
            server::core::log::warn("Redis LPUSH/LTRIM failed: unknown exception");
            return false;
        }
    }

    bool sadd(const std::string& key, const std::string& member) override {
        try { redis_->sadd(key, member); return true; } catch (const std::exception& e) { server::core::log::warn(std::string("Redis SADD failed: ") + e.what()); return false; } catch (...) { server::core::log::warn("Redis SADD failed: unknown"); return false; }
    }
    bool srem(const std::string& key, const std::string& member) override {
        try { redis_->srem(key, member); return true; } catch (const std::exception& e) { server::core::log::warn(std::string("Redis SREM failed: ") + e.what()); return false; } catch (...) { server::core::log::warn("Redis SREM failed: unknown"); return false; }
    }

    bool setex(const std::string& key, const std::string& value, unsigned int ttl_sec) override {
        try { redis_->setex(key, static_cast<long long>(ttl_sec), value); return true; } catch (const std::exception& e) { server::core::log::warn(std::string("Redis SETEX failed: ") + e.what()); return false; } catch (...) { server::core::log::warn("Redis SETEX failed: unknown"); return false; }
    }

    bool publish(const std::string& channel, const std::string& message) override {
        try { redis_->publish(channel, message); return true; } catch (const std::exception& e) { server::core::log::warn(std::string("Redis PUBLISH failed: ") + e.what()); return false; } catch (...) { server::core::log::warn("Redis PUBLISH failed: unknown"); return false; }
    }

    bool start_psubscribe(const std::string& pattern,
                          std::function<void(const std::string& channel, const std::string& message)> on_message) override {
        try {
            if (sub_running_) return true;
            subscriber_ = std::make_unique<sw::redis::Subscriber>(redis_->subscriber());
            subscriber_->on_pmessage([cb = std::move(on_message)](std::string /*pat*/, std::string channel, std::string msg) mutable {
                if (cb) cb(channel, msg);
            });
            subscriber_->psubscribe(pattern);
            sub_running_ = true;
            sub_thread_ = std::thread([this]() {
                while (sub_running_) {
                    try { subscriber_->consume(); } catch (...) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
                }
            });
            return true;
        } catch (const std::exception& e) {
            server::core::log::warn(std::string("Redis PSUBSCRIBE failed: ") + e.what());
            return false;
        } catch (...) { server::core::log::warn("Redis PSUBSCRIBE failed: unknown"); return false; }
    }

    void stop_psubscribe() override {
        try {
            sub_running_ = false;
            if (subscriber_) { try { subscriber_->punsubscribe(); } catch (...) {} }
            if (sub_thread_.joinable()) sub_thread_.join();
            subscriber_.reset();
        } catch (...) {}
    }

    bool xgroup_create_mkstream(const std::string& /*key*/, const std::string& /*group*/) override { return true; }\n\n    bool xadd(const std::string& /*key*/, const std::vector<std::pair<std::string, std::string>>& /*fields*/, std::string* /*out_id*/) override { return true; }\n\n    bool xreadgroup(const std::string& /*key*/, const std::string& /*group*/, const std::string& /*consumer*/, long long /*block_ms*/, std::size_t /*count*/, std::vector<StreamEntry>& /*out*/) override { return true; }\n\n    bool xack(const std::string& /*key*/, const std::string& /*group*/, const std::string& /*id*/) override { return true; }\n
    bool del(const std::string& key) override {
        try { redis_->del(key); return true; } catch (const std::exception& e) { server::core::log::warn(std::string("Redis DEL failed: ") + e.what()); return false; } catch (...) { server::core::log::warn("Redis DEL failed: unknown"); return false; }
    }

    bool scan_del(const std::string& pattern) override {
        try {
            long long cursor = 0;
            do {
                std::vector<std::string> keys;
                cursor = redis_->scan(cursor, pattern, 100, std::back_inserter(keys));
                if (!keys.empty()) {
                    redis_->del(keys.begin(), keys.end());
                }
            } while (cursor != 0);
            return true;
        } catch (const std::exception& e) {
            server::core::log::warn(std::string("Redis SCAN/DEL failed: ") + e.what());
            return false;
        } catch (...) {
            server::core::log::warn("Redis SCAN/DEL failed: unknown");
            return false;
        }
    }

private:
    std::unique_ptr<sw::redis::Redis> redis_;
    std::unique_ptr<sw::redis::Subscriber> subscriber_;
    std::thread sub_thread_;
    std::atomic<bool> sub_running_{false};
};
#else
class RedisClientStub final : public IRedisClient {
public:
    explicit RedisClientStub(std::string uri, Options opts)
        : uri_(std::move(uri)), opts_(opts) {}
    bool health_check() override { (void)uri_; (void)opts_; return true; }
    bool lpush_trim(const std::string& key, const std::string& value, std::size_t maxlen) override { (void)key; (void)value; (void)maxlen; return true; }
    bool sadd(const std::string& key, const std::string& member) override { (void)key; (void)member; return true; }
    bool srem(const std::string& key, const std::string& member) override { (void)key; (void)member; return true; }
    bool setex(const std::string& key, const std::string& value, unsigned int ttl_sec) override { (void)key; (void)value; (void)ttl_sec; return true; }
    bool publish(const std::string& channel, const std::string& message) override { (void)channel; (void)message; return true; }
    bool start_psubscribe(const std::string& pattern, std::function<void(const std::string&, const std::string&)> on_message) override { (void)pattern; (void)on_message; return true; }
    void stop_psubscribe() override {}
    bool xgroup_create_mkstream(const std::string& key, const std::string& group) override { (void)key; (void)group; return true; }
    bool xadd(const std::string& key, const std::vector<std::pair<std::string, std::string>>& fields, std::string* out_id) override { (void)key; (void)fields; (void)out_id; return true; }
    bool xreadgroup(const std::string& key, const std::string& group, const std::string& consumer, long long block_ms, std::size_t count, std::vector<StreamEntry>& out) override { (void)key; (void)group; (void)consumer; (void)block_ms; (void)count; (void)out; return true; }
    bool xack(const std::string& /*key*/, const std::string& /*group*/, const std::string& /*id*/) override { return true; }\n
} // namespace server::storage::redis

