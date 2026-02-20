#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace boost::asio {
class io_context;
}

namespace server::core {
class Dispatcher;
class BufferManager;
struct SessionOptions;
class Session;
}

namespace server::core::storage {
class IConnectionPool;
class DbWorkerPool;
}

namespace server::core::net {
struct ConnectionRuntimeState;
}

namespace server::app::core_internal {

using SessionCloseCallback = std::function<void(std::shared_ptr<server::core::Session>)>;

struct SessionListenerHandle {
    std::function<void()> start;
    std::function<void()> stop;
};

std::shared_ptr<server::core::net::ConnectionRuntimeState> make_connection_runtime_state();

std::shared_ptr<SessionListenerHandle> make_session_listener_handle(
    boost::asio::io_context& io,
    unsigned short port,
    server::core::Dispatcher& dispatcher,
    server::core::BufferManager& buffer_manager,
    const std::shared_ptr<server::core::SessionOptions>& options,
    const std::shared_ptr<server::core::net::ConnectionRuntimeState>& state,
    SessionCloseCallback on_session_close);

std::shared_ptr<server::core::storage::IConnectionPool> make_postgres_connection_pool(
    const std::string& db_uri,
    std::size_t min_size,
    std::size_t max_size,
    std::uint32_t connect_timeout_ms,
    std::uint32_t query_timeout_ms,
    bool prepare_statements);

bool connection_pool_health_check(
    const std::shared_ptr<server::core::storage::IConnectionPool>& connection_pool) noexcept;

std::shared_ptr<server::core::storage::DbWorkerPool> make_db_worker_pool(
    const std::shared_ptr<server::core::storage::IConnectionPool>& connection_pool,
    std::size_t queue_capacity);

void start_db_worker_pool(
    const std::shared_ptr<server::core::storage::DbWorkerPool>& worker_pool,
    std::size_t thread_count);

void stop_db_worker_pool(
    const std::shared_ptr<server::core::storage::DbWorkerPool>& worker_pool) noexcept;

std::uint64_t connection_count(
    const std::shared_ptr<server::core::net::ConnectionRuntimeState>& state) noexcept;

void register_connection_runtime_state_service(
    const std::shared_ptr<server::core::net::ConnectionRuntimeState>& state);

void register_connection_pool_service(
    const std::shared_ptr<server::core::storage::IConnectionPool>& connection_pool);

void register_db_worker_pool_service(
    const std::shared_ptr<server::core::storage::DbWorkerPool>& worker_pool);

void install_crash_handler();

} // namespace server::app::core_internal
