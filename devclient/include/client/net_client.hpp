// UTF-8, 한국어 주석
#pragma once
#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <span>

#include <boost/asio.hpp>

#include "server/core/protocol/frame.hpp"

class NetClient {
public:
    using OnHello = std::function<void(std::uint16_t caps)>;
    using OnErr = std::function<void(std::uint16_t code, std::string msg)>;
    using OnLoginRes = std::function<void(std::string effective_user, std::uint32_t sid)>;
    using OnBroadcast = std::function<void(std::string room, std::string sender, std::string text, std::uint16_t flags, std::uint32_t sender_sid)>;
    using OnRoomUsers = std::function<void(std::string room, std::vector<std::string> users)>;
    using OnSnapshot = std::function<void(std::string current, std::vector<std::string> rooms, std::vector<std::string> users, std::vector<bool> locked)>;
    using OnWhisper = std::function<void(std::string sender, std::string recipient, std::string text, bool outgoing)>;
    using OnWhisperResult = std::function<void(bool ok, std::string reason)>;
    using OnDisconnected = std::function<void(std::string reason)>;

    NetClient();
    ~NetClient();

    void set_on_hello(OnHello f) { on_hello_ = std::move(f); }
    void set_on_err(OnErr f) { on_err_ = std::move(f); }
    void set_on_login_res(OnLoginRes f) { on_login_ = std::move(f); }
    void set_on_broadcast(OnBroadcast f) { on_bcast_ = std::move(f); }
    void set_on_room_users(OnRoomUsers f) { on_room_users_ = std::move(f); }
    void set_on_snapshot(OnSnapshot f) { on_snapshot_ = std::move(f); }
    void set_on_whisper(OnWhisper f) { on_whisper_ = std::move(f); }
    void set_on_whisper_result(OnWhisperResult f) { on_whisper_result_ = std::move(f); }
    void set_on_disconnected(OnDisconnected f) { on_disconnected_ = std::move(f); }

    bool connect(const std::string& host, unsigned short port);
    void close();
    bool is_connected() const { return connected_.load(); }

    void send_login(const std::string& user, const std::string& token);
    void send_join(const std::string& room, const std::string& password = std::string());
    void send_leave(const std::string& room = std::string());
    void send_chat(const std::string& room, const std::string& text);
    void send_refresh(const std::string& current_room);
    void send_who(const std::string& room);
    void send_rooms(const std::string& current_room);
    void send_whisper(const std::string& user, const std::string& text);

private:
    using Tcp = boost::asio::ip::tcp;
    using Strand = boost::asio::strand<boost::asio::io_context::executor_type>;
    using Timer = boost::asio::steady_timer;
    using WorkGuard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;

    void start_read_header();
    void start_read_body(const server::core::protocol::FrameHeader& header);
    void handle_frame(const server::core::protocol::FrameHeader& header, std::span<const std::uint8_t> body);
    void enqueue_frame(std::uint16_t msg_id, std::uint16_t flags, std::vector<std::uint8_t> payload = {});
    void drain_send_queue();
    void schedule_ping();
    void handle_disconnect(const boost::system::error_code& ec, const char* context);
    void reset_state();

    boost::asio::io_context io_;
    Tcp::resolver resolver_{io_};
    Tcp::socket socket_{io_};
    Strand strand_{io_.get_executor()};
    std::deque<std::shared_ptr<std::vector<std::uint8_t>>> write_queue_;
    std::unique_ptr<Timer> ping_timer_;
    std::unique_ptr<WorkGuard> work_guard_;
    std::thread io_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> connected_{false};
    std::vector<std::uint8_t> read_header_;
    std::vector<std::uint8_t> read_body_;
    std::uint32_t seq_{1};

    OnHello on_hello_;
    OnErr on_err_;
    OnLoginRes on_login_;
    OnBroadcast on_bcast_;
    OnRoomUsers on_room_users_;
    OnSnapshot on_snapshot_;
    OnWhisper on_whisper_;
    OnWhisperResult on_whisper_result_;
    OnDisconnected on_disconnected_;
};

