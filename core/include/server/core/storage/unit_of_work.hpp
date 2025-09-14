#pragma once

#include <memory>
#include "server/core/storage/repositories.hpp"

namespace server::core::storage {

// 트랜잭션 경계. 생성 시점에 시작되고, commit/rollback으로 종료한다.
class IUnitOfWork {
public:
    virtual ~IUnitOfWork() = default;

    // 성공 시 호출. 호출 전 예외 발생 시 rollback이 권장된다.
    virtual void commit() = 0;
    // 명시적 취소. commit 미호출 상태에서 소멸 시 구현이 자동 롤백할 수 있다.
    virtual void rollback() = 0;

    // 저장소 접근자
    virtual IUserRepository& users() = 0;
    virtual IRoomRepository& rooms() = 0;
    virtual IMessageRepository& messages() = 0;
    virtual ISessionRepository& sessions() = 0;
};

} // namespace server::core::storage

