#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio.hpp>

namespace loadgen {

struct TransportStats {
    std::uint64_t connect_failures{0};
    std::uint64_t read_timeouts{0};
    std::uint64_t disconnects{0};
};

struct ClientOptions {
    std::uint32_t connect_timeout_ms{5000};
    std::uint32_t read_timeout_ms{5000};
};

struct LoginResult {
    std::string effective_user;
    std::uint32_t session_id{0};
    bool is_admin{false};
};

struct SnapshotResult {
    std::string current_room;
};

class SessionClient {
public:
    explicit SessionClient(ClientOptions options);
    ~SessionClient();

    SessionClient(const SessionClient&) = delete;
    SessionClient& operator=(const SessionClient&) = delete;

    bool connect(const std::string& host, unsigned short port);
    bool login(const std::string& user, const std::string& token, LoginResult* result = nullptr);
    bool join(const std::string& room, const std::string& password, SnapshotResult* result = nullptr);
    bool send_chat_and_wait_echo(const std::string& room, const std::string& text);
    bool send_ping_and_wait_pong();
    void close();

    bool is_connected() const noexcept { return connected_.load(); }
    std::string last_error() const;
    TransportStats transport_stats() const noexcept;

private:
    enum class EventType {
        kHello,
        kLoginRes,
        kSnapshot,
        kPong,
        kSelfChat,
        kError,
    };

    struct Event {
        EventType type{EventType::kError};
        std::uint16_t capabilities{0};
        std::uint16_t flags{0};
        LoginResult login;
        SnapshotResult snapshot;
        std::string room;
        std::string sender;
        std::string text;
        std::uint16_t error_code{0};
        std::string error_message;
    };

    using Tcp = boost::asio::ip::tcp;
    using Strand = boost::asio::strand<boost::asio::io_context::executor_type>;
    using WorkGuard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;

    void reset_state();
    void start_read_header();
    void start_read_body(std::uint16_t payload_size, std::uint16_t msg_id, std::uint16_t flags);
    void handle_frame(std::uint16_t msg_id, std::uint16_t flags, const std::vector<std::uint8_t>& payload);
    void enqueue_packet(std::uint16_t msg_id, std::uint16_t flags, std::vector<std::uint8_t> payload = {});
    void drain_send_queue();
    void handle_disconnect(const boost::system::error_code& ec, const char* context);
    void push_event(Event event);
    bool wait_for_event(std::chrono::milliseconds timeout,
                        const std::function<bool(const Event&)>& predicate,
                        const char* wait_label,
                        Event* out = nullptr);
    void set_last_error_locked(std::string message);

    ClientOptions options_;
    boost::asio::io_context io_;
    Tcp::resolver resolver_{io_};
    Tcp::socket socket_{io_};
    Strand strand_{io_.get_executor()};
    std::unique_ptr<WorkGuard> work_guard_;
    std::thread io_thread_;

    std::deque<std::shared_ptr<std::vector<std::uint8_t>>> write_queue_;
    std::vector<std::uint8_t> read_header_;
    std::vector<std::uint8_t> read_body_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> closing_{false};
    std::uint32_t seq_{1};

    mutable std::mutex mu_;
    std::condition_variable cv_;
    std::deque<Event> events_;
    std::string last_error_;
    std::string current_user_;
    TransportStats transport_;
};

}  // namespace loadgen
