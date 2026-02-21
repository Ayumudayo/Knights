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

/** @brief Callback invoked when a core session is closed. */
using SessionCloseCallback = std::function<void(std::shared_ptr<server::core::Session>)>;

/** @brief Start/stop callable bundle for a session listener. */
struct SessionListenerHandle {
    std::function<void()> start;
    std::function<void()> stop;
};

/**
 * @brief Creates runtime connection counters shared by sessions.
 * @return New `ConnectionRuntimeState` instance.
 */
std::shared_ptr<server::core::net::ConnectionRuntimeState> make_connection_runtime_state();

/**
 * @brief Creates a session listener adapter backed by server_core networking.
 * @param io Asio IO context for accept loop.
 * @param port TCP listen port.
 * @param dispatcher Packet dispatcher used by accepted sessions.
 * @param buffer_manager Shared buffer manager for session IO.
 * @param options Session option set applied to accepted sessions.
 * @param state Shared runtime state for connection counters.
 * @param on_session_close Callback installed on each accepted session.
 * @return Listener handle exposing `start` and `stop` closures.
 */
std::shared_ptr<SessionListenerHandle> make_session_listener_handle(
    boost::asio::io_context& io,
    unsigned short port,
    server::core::Dispatcher& dispatcher,
    server::core::BufferManager& buffer_manager,
    const std::shared_ptr<server::core::SessionOptions>& options,
    const std::shared_ptr<server::core::net::ConnectionRuntimeState>& state,
    SessionCloseCallback on_session_close);

/**
 * @brief Creates a Postgres connection pool using server_core storage options.
 * @param db_uri Postgres connection URI.
 * @param min_size Minimum pool size.
 * @param max_size Maximum pool size.
 * @param connect_timeout_ms Connect timeout in milliseconds.
 * @param query_timeout_ms Query timeout in milliseconds.
 * @param prepare_statements Whether prepared statements are enabled.
 * @return Shared connection pool interface.
 */
std::shared_ptr<server::core::storage::IConnectionPool> make_postgres_connection_pool(
    const std::string& db_uri,
    std::size_t min_size,
    std::size_t max_size,
    std::uint32_t connect_timeout_ms,
    std::uint32_t query_timeout_ms,
    bool prepare_statements);

/**
 * @brief Performs a safe health check for a connection pool.
 * @param connection_pool Connection pool to probe.
 * @return `true` if pool is present and healthy.
 */
bool connection_pool_health_check(
    const std::shared_ptr<server::core::storage::IConnectionPool>& connection_pool) noexcept;

/**
 * @brief Creates a DB worker pool bound to a connection pool.
 * @param connection_pool Connection pool used by workers.
 * @param queue_capacity Max queued jobs in worker pool.
 * @return Shared DB worker pool instance.
 */
std::shared_ptr<server::core::storage::DbWorkerPool> make_db_worker_pool(
    const std::shared_ptr<server::core::storage::IConnectionPool>& connection_pool,
    std::size_t queue_capacity);

/**
 * @brief Starts DB worker threads if the pool exists.
 * @param worker_pool Target DB worker pool.
 * @param thread_count Number of worker threads.
 */
void start_db_worker_pool(
    const std::shared_ptr<server::core::storage::DbWorkerPool>& worker_pool,
    std::size_t thread_count);

/**
 * @brief Stops DB worker pool with noexcept guard.
 * @param worker_pool Target DB worker pool.
 */
void stop_db_worker_pool(
    const std::shared_ptr<server::core::storage::DbWorkerPool>& worker_pool) noexcept;

/**
 * @brief Reads active connection count from runtime state.
 * @param state Runtime state that owns connection counters.
 * @return Current active connection count.
 */
std::uint64_t connection_count(
    const std::shared_ptr<server::core::net::ConnectionRuntimeState>& state) noexcept;

/**
 * @brief Registers runtime connection state into service registry.
 * @param state Runtime connection state service instance.
 */
void register_connection_runtime_state_service(
    const std::shared_ptr<server::core::net::ConnectionRuntimeState>& state);

/**
 * @brief Registers DB connection pool into service registry.
 * @param connection_pool Connection pool service instance.
 */
void register_connection_pool_service(
    const std::shared_ptr<server::core::storage::IConnectionPool>& connection_pool);

/**
 * @brief Registers DB worker pool into service registry.
 * @param worker_pool DB worker pool service instance.
 */
void register_db_worker_pool_service(
    const std::shared_ptr<server::core::storage::DbWorkerPool>& worker_pool);

/** @brief Installs process crash handler from server_core utility. */
void install_crash_handler();

} // namespace server::app::core_internal
