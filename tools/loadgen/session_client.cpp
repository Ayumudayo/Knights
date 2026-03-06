#include "session_client.hpp"

#include "server/core/protocol/packet.hpp"
#include "server/core/protocol/protocol_flags.hpp"
#include "server/core/protocol/system_opcodes.hpp"
#include "server/core/protocol/version.hpp"
#include "server/protocol/game_opcodes.hpp"
#include "server/wire/codec.hpp"
#include "wire.pb.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <future>
#include <span>
#include <utility>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
namespace proto = server::core::protocol;
namespace game_proto = server::protocol;

namespace loadgen {

namespace {

struct AsyncConnectResult {
    std::promise<boost::system::error_code> promise;
    std::atomic<bool> fulfilled{false};
};

void complete_async_result(const std::shared_ptr<AsyncConnectResult>& result,
                           const boost::system::error_code& ec) {
    bool expected = false;
    if (result->fulfilled.compare_exchange_strong(expected, true)) {
        result->promise.set_value(ec);
    }
}

}  // namespace

SessionClient::SessionClient(ClientOptions options)
    : options_(options) {
    read_header_.resize(proto::k_header_bytes);
}

SessionClient::~SessionClient() {
    close();
}

bool SessionClient::connect(const std::string& host, unsigned short port) {
    close();

    reset_state();
    work_guard_ = std::make_unique<WorkGuard>(asio::make_work_guard(io_));
    const auto connect_result = std::make_shared<AsyncConnectResult>();
    auto future = connect_result->promise.get_future();

    resolver_.async_resolve(
        host,
        std::to_string(port),
        [this, connect_result](const boost::system::error_code& ec, tcp::resolver::results_type results) {
            if (ec) {
                complete_async_result(connect_result, ec);
                return;
            }

            asio::async_connect(
                socket_,
                results,
                asio::bind_executor(
                    strand_,
                    [this, connect_result](const boost::system::error_code& connect_ec, const tcp::endpoint&) {
                        if (connect_ec) {
                            complete_async_result(connect_result, connect_ec);
                            return;
                        }

                        boost::system::error_code option_ec;
                        socket_.set_option(tcp::no_delay(true), option_ec);
                        if (option_ec) {
                            complete_async_result(connect_result, option_ec);
                            return;
                        }

                        connected_.store(true);
                        start_read_header();
                        complete_async_result(connect_result, {});
                    }));
        });

    io_thread_ = std::thread([this] { io_.run(); });

    if (future.wait_for(std::chrono::milliseconds(options_.connect_timeout_ms)) != std::future_status::ready) {
        {
            std::lock_guard<std::mutex> lock(mu_);
            ++transport_.connect_failures;
            set_last_error_locked("connect timeout");
        }
        close();
        return false;
    }

    const auto ec = future.get();
    if (ec) {
        {
            std::lock_guard<std::mutex> lock(mu_);
            ++transport_.connect_failures;
            set_last_error_locked(std::string("connect failed: ") + ec.message());
        }
        close();
        return false;
    }

    return true;
}

bool SessionClient::login(const std::string& user, const std::string& token, LoginResult* result) {
    std::vector<std::uint8_t> payload;
    proto::write_lp_utf8(payload, user);
    proto::write_lp_utf8(payload, token);

    const auto version_offset = payload.size();
    payload.resize(version_offset + 4);
    proto::write_be16(proto::kProtocolVersionMajor, payload.data() + version_offset);
    proto::write_be16(proto::kProtocolVersionMinor, payload.data() + version_offset + 2);

    enqueue_packet(game_proto::MSG_LOGIN_REQ, 0, std::move(payload));

    Event login_event;
    if (!wait_for_event(std::chrono::milliseconds(options_.read_timeout_ms),
                        [](const Event& event) { return event.type == EventType::kLoginRes; },
                        "login response",
                        &login_event)) {
        return false;
    }

    if (result != nullptr) {
        *result = login_event.login;
    }
    {
        std::lock_guard<std::mutex> lock(mu_);
        current_user_ = login_event.login.effective_user.empty() ? user : login_event.login.effective_user;
    }
    return true;
}

bool SessionClient::join(const std::string& room, const std::string& password, SnapshotResult* result) {
    std::vector<std::uint8_t> payload;
    proto::write_lp_utf8(payload, room);
    proto::write_lp_utf8(payload, password);
    enqueue_packet(game_proto::MSG_JOIN_ROOM, 0, std::move(payload));

    const std::string expected_room = room.empty() ? std::string("lobby") : room;
    std::string current_user;
    {
        std::lock_guard<std::mutex> lock(mu_);
        current_user = current_user_;
    }

    Event broadcast_event;
    if (!wait_for_event(std::chrono::milliseconds(options_.read_timeout_ms),
                        [&expected_room, &current_user](const Event& event) {
                            return event.type == EventType::kSelfChat &&
                                   event.room == expected_room &&
                                   event.sender == "(system)" &&
                                   event.text.find(current_user) != std::string::npos;
                        },
                        "join confirmation broadcast",
                        &broadcast_event)) {
        return false;
    }

    if (result != nullptr) {
        result->current_room = broadcast_event.room;
    }
    return true;
}

bool SessionClient::send_chat_and_wait_echo(const std::string& room, const std::string& text) {
    std::vector<std::uint8_t> payload;
    proto::write_lp_utf8(payload, room);
    proto::write_lp_utf8(payload, text);
    enqueue_packet(game_proto::MSG_CHAT_SEND, 0, std::move(payload));

    return wait_for_event(
        std::chrono::milliseconds(options_.read_timeout_ms),
        [&room, &text](const Event& event) {
            return event.type == EventType::kSelfChat &&
                   (event.flags & proto::FLAG_SELF) != 0 &&
                   event.room == room &&
                   event.text == text;
        },
        "chat echo");
}

bool SessionClient::send_ping_and_wait_pong() {
    enqueue_packet(proto::MSG_PING, 0);
    return wait_for_event(std::chrono::milliseconds(options_.read_timeout_ms),
                          [](const Event& event) { return event.type == EventType::kPong; },
                          "pong");
}

void SessionClient::close() {
    closing_.store(true);
    connected_.store(false);

    {
        std::lock_guard<std::mutex> lock(mu_);
        cv_.notify_all();
    }

    boost::system::error_code ec;
    resolver_.cancel();
    socket_.cancel(ec);
    socket_.close(ec);

    if (work_guard_) {
        work_guard_->reset();
        work_guard_.reset();
    }

    io_.stop();
    if (io_thread_.joinable()) {
        io_thread_.join();
    }
}

std::string SessionClient::last_error() const {
    std::lock_guard<std::mutex> lock(mu_);
    return last_error_;
}

TransportStats SessionClient::transport_stats() const noexcept {
    std::lock_guard<std::mutex> lock(mu_);
    return transport_;
}

void SessionClient::reset_state() {
    io_.restart();
    closing_.store(false);
    connected_.store(false);
    seq_ = 1;
    read_header_.assign(proto::k_header_bytes, 0);
    read_body_.clear();
    write_queue_.clear();

    std::lock_guard<std::mutex> lock(mu_);
    events_.clear();
    last_error_.clear();
    current_user_.clear();
    transport_ = {};
}

void SessionClient::start_read_header() {
    asio::async_read(
        socket_,
        asio::buffer(read_header_),
        asio::bind_executor(
            strand_,
            [this](const boost::system::error_code& ec, std::size_t) {
                if (ec) {
                    handle_disconnect(ec, "async_read(header)");
                    return;
                }

                proto::PacketHeader header{};
                proto::decode_header(read_header_.data(), header);
                start_read_body(header.length, header.msg_id, header.flags);
            }));
}

void SessionClient::start_read_body(std::uint16_t payload_size, std::uint16_t msg_id, std::uint16_t flags) {
    if (payload_size == 0) {
        handle_frame(msg_id, flags, {});
        start_read_header();
        return;
    }

    read_body_.assign(payload_size, 0);
    asio::async_read(
        socket_,
        asio::buffer(read_body_),
        asio::bind_executor(
            strand_,
            [this, msg_id, flags](const boost::system::error_code& ec, std::size_t) {
                if (ec) {
                    handle_disconnect(ec, "async_read(body)");
                    return;
                }

                handle_frame(msg_id, flags, read_body_);
                start_read_header();
            }));
}

void SessionClient::handle_frame(std::uint16_t msg_id,
                                 std::uint16_t flags,
                                 const std::vector<std::uint8_t>& payload) {
    if (msg_id == proto::MSG_PING) {
        enqueue_packet(proto::MSG_PONG, 0);
        return;
    }

    if (msg_id == proto::MSG_HELLO) {
        Event event;
        event.type = EventType::kHello;
        if (payload.size() >= 12) {
            event.capabilities = proto::read_be16(payload.data() + 4);
        }
        push_event(std::move(event));
        return;
    }

    if (msg_id == proto::MSG_PONG) {
        Event event;
        event.type = EventType::kPong;
        push_event(std::move(event));
        return;
    }

    if (msg_id == proto::MSG_ERR) {
        Event event;
        event.type = EventType::kError;
        std::span<const std::uint8_t> span(payload.data(), payload.size());
        if (span.size() >= 4) {
            event.error_code = proto::read_be16(span.data());
            const auto len = proto::read_be16(span.data() + 2);
            if (span.size() >= 4 + len) {
                event.error_message.assign(reinterpret_cast<const char*>(span.data() + 4), len);
            }
        }
        if (event.error_message.empty()) {
            event.error_message = "server returned MSG_ERR";
        }
        push_event(std::move(event));
        return;
    }

    if (msg_id == game_proto::MSG_LOGIN_RES) {
        server::wire::v1::LoginRes message;
        Event event;
        event.type = EventType::kLoginRes;
        if (server::wire::codec::Decode(payload.data(), payload.size(), message)) {
            event.login.effective_user = message.effective_user();
            event.login.session_id = message.session_id();
            event.login.is_admin = message.is_admin();
            push_event(std::move(event));
            return;
        }

        std::span<const std::uint8_t> span(payload.data(), payload.size());
        if (!span.empty()) {
            span = span.subspan(1);
        }
        std::string ignored_message;
        proto::read_lp_utf8(span, ignored_message);
        if (!span.empty()) {
            auto next = span;
            proto::read_lp_utf8(next, event.login.effective_user);
            span = next;
        }
        if (span.size() >= 4) {
            event.login.session_id = proto::read_be32(span.data());
        }
        push_event(std::move(event));
        return;
    }

    if (msg_id == game_proto::MSG_STATE_SNAPSHOT) {
        server::wire::v1::StateSnapshot snapshot;
        if (!server::wire::codec::Decode(payload.data(), payload.size(), snapshot)) {
            return;
        }
        Event event;
        event.type = EventType::kSnapshot;
        event.snapshot.current_room = snapshot.current_room();
        push_event(std::move(event));
        return;
    }

    if (msg_id == game_proto::MSG_CHAT_BROADCAST) {
        server::wire::v1::ChatBroadcast broadcast;
        if (!server::wire::codec::Decode(payload.data(), payload.size(), broadcast)) {
            return;
        }
        const bool is_self = (flags & proto::FLAG_SELF) != 0;
        const bool is_system = broadcast.sender() == "(system)";
        if (!is_self && !is_system) {
            return;
        }

        Event event;
        event.type = EventType::kSelfChat;
        event.flags = flags;
        event.room = broadcast.room();
        event.sender = broadcast.sender();
        event.text = broadcast.text();
        push_event(std::move(event));
    }
}

void SessionClient::enqueue_packet(std::uint16_t msg_id,
                                   std::uint16_t flags,
                                   std::vector<std::uint8_t> payload) {
    asio::post(
        strand_,
        [this, msg_id, flags, payload = std::move(payload)]() mutable {
            if (!connected_.load()) {
                return;
            }

            proto::PacketHeader header{};
            header.length = static_cast<std::uint16_t>(payload.size());
            header.msg_id = msg_id;
            header.flags = flags;
            header.seq = seq_++;
            const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::system_clock::now().time_since_epoch())
                                    .count();
            header.utc_ts_ms32 = static_cast<std::uint32_t>(now_ms & 0xFFFFFFFFu);

            auto buffer = std::make_shared<std::vector<std::uint8_t>>();
            buffer->resize(proto::k_header_bytes + payload.size());
            proto::encode_header(header, buffer->data());
            if (!payload.empty()) {
                std::memcpy(buffer->data() + proto::k_header_bytes, payload.data(), payload.size());
            }

            const bool writing = !write_queue_.empty();
            write_queue_.push_back(buffer);
            if (!writing) {
                drain_send_queue();
            }
        });
}

void SessionClient::drain_send_queue() {
    if (write_queue_.empty()) {
        return;
    }

    auto buffer = write_queue_.front();
    asio::async_write(
        socket_,
        asio::buffer(*buffer),
        asio::bind_executor(
            strand_,
            [this, buffer](const boost::system::error_code& ec, std::size_t) {
                if (ec) {
                    write_queue_.clear();
                    handle_disconnect(ec, "async_write");
                    return;
                }

                write_queue_.pop_front();
                if (!write_queue_.empty()) {
                    drain_send_queue();
                }
            }));
}

void SessionClient::handle_disconnect(const boost::system::error_code& ec, const char* context) {
    if (closing_.load()) {
        return;
    }

    if (!connected_.exchange(false)) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mu_);
        ++transport_.disconnects;
        set_last_error_locked(std::string(context) + ": " + ec.message());
    }
    cv_.notify_all();
}

void SessionClient::push_event(Event event) {
    {
        std::lock_guard<std::mutex> lock(mu_);
        events_.push_back(std::move(event));
    }
    cv_.notify_all();
}

bool SessionClient::wait_for_event(std::chrono::milliseconds timeout,
                                   const std::function<bool(const Event&)>& predicate,
                                   const char* wait_label,
                                   Event* out) {
    std::unique_lock<std::mutex> lock(mu_);
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    for (;;) {
        auto match_it = std::find_if(events_.begin(), events_.end(), predicate);
        if (match_it != events_.end()) {
            if (out != nullptr) {
                *out = *match_it;
            }
            events_.erase(match_it);
            return true;
        }

        auto error_it = std::find_if(events_.begin(), events_.end(), [](const Event& event) {
            return event.type == EventType::kError;
        });
        if (error_it != events_.end()) {
            set_last_error_locked(std::string("server error: ") + error_it->error_message);
            events_.erase(error_it);
            return false;
        }

        if (!connected_.load() || closing_.load()) {
            if (last_error_.empty()) {
                set_last_error_locked("connection closed");
            }
            return false;
        }

        if (cv_.wait_until(lock, deadline) == std::cv_status::timeout) {
            ++transport_.read_timeouts;
            set_last_error_locked(std::string("timeout waiting for ") + wait_label);
            return false;
        }
    }
}

void SessionClient::set_last_error_locked(std::string message) {
    last_error_ = std::move(message);
}

}  // namespace loadgen
