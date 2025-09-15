#pragma once

#include <memory>
#include <string>
#include <cstddef>
#include <functional>

namespace server::storage::redis {

struct Options {
    std::size_t pool_max{10};
    bool use_streams{false};
};

class IRedisClient {
public:
    virtual ~IRedisClient() = default;
    virtual bool health_check() = 0;
    virtual bool lpush_trim(const std::string& key, const std::string& value, std::size_t maxlen) = 0;
    virtual bool sadd(const std::string& key, const std::string& member) = 0;
    virtual bool srem(const std::string& key, const std::string& member) = 0;
    // 키 삭제
    virtual bool del(const std::string& key) = 0;
    // 패턴 스캔 후 일괄 삭제(naive): SCAN pattern -> DEL
    virtual bool scan_del(const std::string& pattern) = 0;
    // TTL을 가진 키 설정(초 단위)
    virtual bool setex(const std::string& key, const std::string& value, unsigned int ttl_sec) = 0;
    // Pub/Sub publish
    virtual bool publish(const std::string& channel, const std::string& message) = 0;
    // Pub/Sub 구독(패턴): 백그라운드로 consume 루프를 돌며 콜백 호출
    virtual bool start_psubscribe(const std::string& pattern,
                                  std::function<void(const std::string& channel, const std::string& message)> on_message) = 0;
    // 구독 중지(소유 스레드 종료)
    virtual void stop_psubscribe() = 0;
};

// Redis 클라이언트/풀 팩토리(스켈레톤)
std::shared_ptr<IRedisClient> make_redis_client(const std::string& uri, const Options& opts);

} // namespace server::storage::redis
