#include <atomic>
#include <chrono>
#include <future>
#include <optional>
#include <string>
#include <stdexcept>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "server/core/runtime_metrics.hpp"
#include "server/core/storage/db_worker_pool.hpp"

using namespace std::chrono_literals;
using server::core::runtime_metrics::snapshot;
using server::core::storage::DbWorkerPool;
using server::core::storage::IConnectionPool;
using server::core::storage::IUnitOfWork;
using server::core::storage::IUserRepository;
using server::core::storage::IRoomRepository;
using server::core::storage::IMessageRepository;
using server::core::storage::IMembershipRepository;
using server::core::storage::ISessionRepository;
using server::core::storage::User;
using server::core::storage::Room;
using server::core::storage::Message;
using server::core::storage::Membership;
using server::core::storage::Session;

namespace {

struct NullUserRepository final : IUserRepository {
    std::optional<User> find_by_id(const std::string&) override { return std::nullopt; }
    std::vector<User> find_by_name_ci(const std::string&, std::size_t) override { return {}; }
    User create_guest(const std::string&) override { return {}; }
    void update_last_login(const std::string&, const std::string&) override {}
};

struct NullRoomRepository final : IRoomRepository {
    std::optional<Room> find_by_id(const std::string&) override { return std::nullopt; }
    std::vector<Room> search_by_name_ci(const std::string&, std::size_t) override { return {}; }
    std::optional<Room> find_by_name_exact_ci(const std::string&) override { return std::nullopt; }
    Room create(const std::string&, bool) override { return {}; }
};

struct NullMessageRepository final : IMessageRepository {
    std::vector<Message> fetch_recent_by_room(const std::string&, std::uint64_t, std::size_t) override { return {}; }
    Message create(const std::string&, const std::string&, const std::optional<std::string>&, const std::string&) override {
        return {};
    }
    std::uint64_t get_last_id(const std::string&) override { return 0; }
};

struct NullMembershipRepository final : IMembershipRepository {
    void upsert_join(const std::string&, const std::string&, const std::string&) override {}
    void update_last_seen(const std::string&, const std::string&, std::uint64_t) override {}
    void leave(const std::string&, const std::string&) override {}
    std::optional<std::uint64_t> get_last_seen(const std::string&, const std::string&) override { return std::nullopt; }
};

struct NullSessionRepository final : ISessionRepository {
    std::optional<Session> find_by_token_hash(const std::string&) override { return std::nullopt; }
    Session create(const std::string&, const std::chrono::system_clock::time_point&,
                   const std::optional<std::string>&, const std::optional<std::string>&,
                   const std::string&) override {
        return {};
    }
    void revoke(const std::string&) override {}
};

struct FakeUnitState {
    std::atomic<int> commit_calls{0};
    std::atomic<int> rollback_calls{0};
};

class FakeUnitOfWork final : public IUnitOfWork {
public:
    explicit FakeUnitOfWork(std::shared_ptr<FakeUnitState> state)
        : state_(std::move(state)) {}

    void commit() override { state_->commit_calls.fetch_add(1); }
    void rollback() override { state_->rollback_calls.fetch_add(1); }

    IUserRepository& users() override { return user_repo_; }
    IRoomRepository& rooms() override { return room_repo_; }
    IMessageRepository& messages() override { return message_repo_; }
    ISessionRepository& sessions() override { return session_repo_; }
    IMembershipRepository& memberships() override { return membership_repo_; }

private:
    std::shared_ptr<FakeUnitState> state_;
    NullUserRepository user_repo_;
    NullRoomRepository room_repo_;
    NullMessageRepository message_repo_;
    NullSessionRepository session_repo_;
    NullMembershipRepository membership_repo_;
};

class FakeConnectionPool final : public IConnectionPool {
public:
    explicit FakeConnectionPool(std::shared_ptr<FakeUnitState> state)
        : state_(std::move(state)) {}

    std::unique_ptr<IUnitOfWork> make_unit_of_work() override {
        return std::make_unique<FakeUnitOfWork>(state_);
    }

    bool health_check() override { return true; }

private:
    std::shared_ptr<FakeUnitState> state_;
};

} // namespace

TEST(DbWorkerPoolTests, ProcessesJobWithAutoCommit) {
    auto state = std::make_shared<FakeUnitState>();
    auto pool = std::make_shared<FakeConnectionPool>(state);
    DbWorkerPool workers(pool);

    auto before = snapshot();
    workers.start(1);

    std::promise<void> completion;
    auto future = completion.get_future();
    workers.submit([&](IUnitOfWork&) { completion.set_value(); }, true);

    ASSERT_EQ(future.wait_for(200ms), std::future_status::ready);

    workers.stop();

    EXPECT_EQ(state->commit_calls.load(), 1);
    EXPECT_EQ(state->rollback_calls.load(), 0);

    auto after = snapshot();
    EXPECT_EQ(after.db_job_processed_total, before.db_job_processed_total + 1);
    EXPECT_EQ(after.db_job_failed_total, before.db_job_failed_total);
    EXPECT_EQ(after.db_job_queue_depth, 0u);
}

TEST(DbWorkerPoolTests, ProcessesJobWithoutAutoCommit) {
    auto state = std::make_shared<FakeUnitState>();
    auto pool = std::make_shared<FakeConnectionPool>(state);
    DbWorkerPool workers(pool);

    auto before = snapshot();
    workers.start(1);

    std::promise<void> completion;
    auto future = completion.get_future();
    workers.submit([&](IUnitOfWork&) { completion.set_value(); }, false);

    ASSERT_EQ(future.wait_for(200ms), std::future_status::ready);

    workers.stop();

    EXPECT_EQ(state->commit_calls.load(), 0);
    EXPECT_EQ(state->rollback_calls.load(), 1);

    auto after = snapshot();
    EXPECT_EQ(after.db_job_processed_total, before.db_job_processed_total + 1);
    EXPECT_EQ(after.db_job_failed_total, before.db_job_failed_total);
}

TEST(DbWorkerPoolTests, JobExceptionTriggersRollbackAndFailureMetric) {
    auto state = std::make_shared<FakeUnitState>();
    auto pool = std::make_shared<FakeConnectionPool>(state);
    DbWorkerPool workers(pool);

    auto before = snapshot();
    workers.start(1);

    std::atomic<bool> invoked{false};
    workers.submit([&](IUnitOfWork&) {
        invoked.store(true);
        throw std::runtime_error("fail");
    });

    for (int i = 0; i < 50 && !invoked.load(); ++i) {
        std::this_thread::sleep_for(5ms);
    }

    std::this_thread::sleep_for(20ms);
    workers.stop();

    EXPECT_GE(state->rollback_calls.load(), 1);
    EXPECT_EQ(state->commit_calls.load(), 0);

    auto after = snapshot();
    EXPECT_EQ(after.db_job_failed_total, before.db_job_failed_total + 1);
    EXPECT_EQ(after.db_job_processed_total, before.db_job_processed_total);
}

TEST(DbWorkerPoolTests, SubmitThrowsWhenNotRunning) {
    auto state = std::make_shared<FakeUnitState>();
    auto pool = std::make_shared<FakeConnectionPool>(state);
    DbWorkerPool workers(pool);

    EXPECT_THROW(workers.submit([](IUnitOfWork&) {}, true), std::runtime_error);
}
