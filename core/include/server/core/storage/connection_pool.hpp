#pragma once

#include <memory>
#include <cstddef>
#include <string>
#include <optional>
#include "server/core/storage/unit_of_work.hpp"

namespace server::core::storage {

struct PoolOptions {
    std::size_t min_size{1};
    std::size_t max_size{10};
    std::uint32_t connect_timeout_ms{5000};
    std::uint32_t query_timeout_ms{5000};
    bool prepare_statements{true};
};

// 커넥션 풀 SPI. 구현체는 DB 드라이버에 의존한다.
class IConnectionPool {
public:
    virtual ~IConnectionPool() = default;

    // 트랜잭션 단위를 생성한다. 구현은 내부적으로 커넥션 할당/반납을 관리한다.
    virtual std::unique_ptr<IUnitOfWork> make_unit_of_work() = 0;

    // 헬스체크/간단 쿼리 수행(옵션)
    virtual bool health_check() = 0;
};

} // namespace server::core::storage

