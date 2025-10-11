#pragma once

#include <memory>

#include "server/core/storage/repositories.hpp"

namespace server::core::storage {

// 트랜잭션 단위. commit/rollback 책임을 외부에 명확히 전달한다.
class IUnitOfWork {
public:
    virtual ~IUnitOfWork() = default;

    // 작업을 확정한다. 실패 시 예외를 던지며, 호출자는 rollback 을 시도해야 한다.
    virtual void commit() = 0;
    // 트랜잭션을 취소한다. commit 이전에는 여러 번 호출되어도 무방하다.
    virtual void rollback() = 0;

    // 하위 Repository 접근자
    virtual IUserRepository& users() = 0;
    virtual IRoomRepository& rooms() = 0;
    virtual IMessageRepository& messages() = 0;
    virtual ISessionRepository& sessions() = 0;
    virtual IMembershipRepository& memberships() = 0;
};

} // namespace server::core::storage

