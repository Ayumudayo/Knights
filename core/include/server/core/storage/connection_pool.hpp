#pragma once

#include <cstddef>
#include <memory>
#include <string>

#include "server/core/storage/unit_of_work.hpp"

namespace server::core::storage {

struct PoolOptions {
    std::size_t min_size{1};
    std::size_t max_size{10};
    std::uint32_t connect_timeout_ms{5000};
    std::uint32_t query_timeout_ms{5000};
    bool prepare_statements{true};
};

// 데이터베이스 연결 생명주기를 관리하는 SPI 인터페이스
class IConnectionPool {
public:
    virtual ~IConnectionPool() = default;

    // 트랜잭션 단위를 생성하고, 소멸 시 연결 반환과 롤백을 책임진다.
    virtual std::unique_ptr<IUnitOfWork> make_unit_of_work() = 0;

    // 연결이 유효한지 여부를 점검한다(선택 사항).
    virtual bool health_check() = 0;
};

} // namespace server::core::storage
