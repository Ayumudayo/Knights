#include "server/storage/postgres/connection_pool.hpp"

#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "server/core/storage/connection_pool.hpp"
#include "server/core/storage/unit_of_work.hpp"
#include "server/core/storage/repositories.hpp"

namespace server::storage::postgres {

using server::core::storage::IConnectionPool;
using server::core::storage::IUnitOfWork;
using server::core::storage::PoolOptions;
using server::core::storage::IUserRepository;
using server::core::storage::IRoomRepository;
using server::core::storage::IMessageRepository;
using server::core::storage::ISessionRepository;
using server::core::storage::User;
using server::core::storage::Room;
using server::core::storage::Message;
using server::core::storage::Session;

class PgUserRepository final : public IUserRepository {
public:
    std::optional<User> find_by_id(const std::string& /*user_id*/) override {
        return std::nullopt; // TODO: implement
    }
    std::vector<User> find_by_name_ci(const std::string& /*name*/, std::size_t /*limit*/) override {
        return {}; // TODO: implement
    }
};

class PgRoomRepository final : public IRoomRepository {
public:
    std::optional<Room> find_by_id(const std::string& /*room_id*/) override {
        return std::nullopt; // TODO: implement
    }
    std::vector<Room> search_by_name_ci(const std::string& /*query*/, std::size_t /*limit*/) override {
        return {}; // TODO: implement
    }
};

class PgMessageRepository final : public IMessageRepository {
public:
    std::vector<Message> fetch_recent_by_room(const std::string& /*room_id*/,
                                              std::uint64_t /*since_id*/,
                                              std::size_t /*limit*/) override {
        return {}; // TODO: implement
    }
    Message create(const std::string& /*room_id*/,
                   const std::optional<std::string>& /*user_id*/,
                   const std::string& /*content*/) override {
        return {}; // TODO: implement
    }
};

class PgSessionRepository final : public ISessionRepository {
public:
    std::optional<Session> find_by_token_hash(const std::string& /*token_hash*/) override {
        return std::nullopt; // TODO: implement
    }
    Session create(const std::string& /*user_id*/,
                   const std::chrono::system_clock::time_point& /*expires_at*/,
                   const std::optional<std::string>& /*client_ip*/,
                   const std::optional<std::string>& /*user_agent*/,
                   const std::string& /*token_hash*/) override {
        return {}; // TODO: implement
    }
    void revoke(const std::string& /*session_id*/) override {
        // TODO: implement
    }
};

class PgUnitOfWork final : public IUnitOfWork {
public:
    PgUnitOfWork() = default;
    ~PgUnitOfWork() override = default;

    void commit() override {
        // TODO: implement transaction commit
    }
    void rollback() override {
        // TODO: implement transaction rollback
    }

    IUserRepository& users() override { return users_; }
    IRoomRepository& rooms() override { return rooms_; }
    IMessageRepository& messages() override { return messages_; }
    ISessionRepository& sessions() override { return sessions_; }

private:
    PgUserRepository users_{};
    PgRoomRepository rooms_{};
    PgMessageRepository messages_{};
    PgSessionRepository sessions_{};
};

class PgConnectionPool final : public IConnectionPool {
public:
    PgConnectionPool(std::string db_uri, PoolOptions opts)
        : db_uri_(std::move(db_uri)), opts_(opts) {}

    std::unique_ptr<IUnitOfWork> make_unit_of_work() override {
        // TODO: allocate/borrow a connection and start a transaction
        return std::make_unique<PgUnitOfWork>();
    }

    bool health_check() override {
        // TODO: run lightweight query (e.g., SELECT 1)
        return true;
    }

private:
    std::string db_uri_;
    PoolOptions opts_{};
};

std::shared_ptr<IConnectionPool>
make_connection_pool(const std::string& db_uri, const PoolOptions& opts) {
    return std::make_shared<PgConnectionPool>(db_uri, opts);
}

} // namespace server::storage::postgres

