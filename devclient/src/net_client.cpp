// UTF-8, 한글 주석
#include "client/net_client.hpp"

#include "server/core/protocol/frame.hpp"
#include "server/core/protocol/opcodes.hpp"
#include "server/core/protocol/protocol_flags.hpp"
#include "server/wire/codec.hpp"
#include "wire.pb.h"

#include <boost/system/error_code.hpp>

#include <chrono>
#include <cstring>

using namespace std::chrono;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;
namespace proto = server::core::protocol;

NetClient::NetClient() : socket_(io_), strand_(io_.get_executor()) {
    read_header_.resize(proto::k_header_bytes);
}

NetClient::~NetClient() {
    close();
}

bool NetClient::connect(const std::string& host, unsigned short port) {
    close();
    boost::system::error_code ec;
    io_.restart();

    auto endpoints = resolver_.resolve(host, std::to_string(port), ec);
    if (ec) return false;

    asio::connect(socket_, endpoints, ec);
    if (ec) return false;

    socket_.set_option(tcp::no_delay(true));
    running_.store(true);
    connected_.store(true);
    seq_ = 1;

    work_guard_ = std::make_unique<WorkGuard>(asio::make_work_guard(io_));
    ping_timer_ = std::make_unique<Timer>(io_);

    start_read_header();
    schedule_ping();

    io_thread_ = std::thread([this] { io_.run(); });
    return true;
}

void NetClient::close() {
    running_.store(false);
    connected_.store(false);

    if (ping_timer_) {
        ping_timer_->cancel();
        ping_timer_.reset();
    }

    boost::system::error_code ec;
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
    io_.restart();
    write_queue_.clear();
}

void NetClient::start_read_header() {
    asio::async_read(
        socket_,
        asio::buffer(read_header_),
        boost::asio::bind_executor(
            strand_,
            [this](const boost::system::error_code& ec, std::size_t) {
                if (ec) {
                    handle_disconnect(ec, "async_read(header)");
                    return;
                }
                proto::FrameHeader header{};
                proto::decode_header(read_header_.data(), header);
                read_body_.assign(header.length, 0);
                start_read_body(header);
            }));
}

void NetClient::start_read_body(const proto::FrameHeader& header) {
    if (read_body_.empty()) {
        handle_frame(header, std::span<const std::uint8_t>{});
        start_read_header();
        return;
    }

    asio::async_read(
        socket_,
        asio::buffer(read_body_),
        boost::asio::bind_executor(
            strand_,
            [this, header](const boost::system::error_code& ec, std::size_t) {
                if (ec) {
                    handle_disconnect(ec, "async_read(body)");
                    return;
                }
                handle_frame(header, std::span<const std::uint8_t>(read_body_.data(), read_body_.size()));
                start_read_header();
            }));
}

void NetClient::schedule_ping() {
    if (!ping_timer_) return;
    ping_timer_->expires_after(std::chrono::seconds(8));
    ping_timer_->async_wait(boost::asio::bind_executor(
        strand_,
        [this](const boost::system::error_code& ec) {
            if (ec || !running_.load()) {
                return;
            }
            enqueue_frame(proto::MSG_PING, 0);
            schedule_ping();
        }));
}

void NetClient::enqueue_frame(std::uint16_t msg_id, std::uint16_t flags, std::vector<std::uint8_t> payload) {
    asio::post(strand_, [this, msg_id, flags, payload = std::move(payload)]() mutable {
        if (!connected_.load()) {
            return;
        }

        proto::FrameHeader header{};
        header.length = static_cast<std::uint16_t>(payload.size());
        header.msg_id = msg_id;
        header.flags = flags;
        header.seq = seq_++;
        auto now64 = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        header.utc_ts_ms32 = static_cast<std::uint32_t>(now64 & 0xFFFFFFFFu);

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

void NetClient::drain_send_queue() {
    if (write_queue_.empty()) return;
    auto buffer = write_queue_.front();
    asio::async_write(
        socket_,
        asio::buffer(*buffer),
        boost::asio::bind_executor(
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

void NetClient::handle_frame(const proto::FrameHeader& hh, std::span<const std::uint8_t> in) {
    if (hh.msg_id == proto::MSG_PING) {
        enqueue_frame(proto::MSG_PONG, 0);
        return;
    }

    if (hh.msg_id == proto::MSG_PONG) {
        return;
    }

    if (hh.msg_id == proto::MSG_ERR) {
        std::uint16_t code = 0, len = 0;
        std::string msg;
        if (in.size() >= 4) {
            code = proto::read_be16(in.data());
            len = proto::read_be16(in.data() + 2);
            in = in.subspan(4);
            if (in.size() >= len) {
                msg.assign(reinterpret_cast<const char*>(in.data()), len);
            }
        }
        if (on_err_) on_err_(code, msg);
        return;
    }

    if (hh.msg_id == proto::MSG_LOGIN_RES) {
        server::wire::v1::LoginRes pb;
        if (server::wire::codec::Decode(in.data(), in.size(), pb)) {
            if (on_login_) on_login_(pb.effective_user(), pb.session_id());
        } else {
            auto cur = in;
            if (!cur.empty()) cur = cur.subspan(1);
            std::string m;
            proto::read_lp_utf8(cur, m);
            std::string effective;
            if (!cur.empty()) {
                auto tmp = cur;
                std::string t;
                if (proto::read_lp_utf8(tmp, t)) {
                    effective = std::move(t);
                    cur = tmp;
                }
            }
            std::uint32_t sid = 0;
            if (cur.size() >= 4) sid = proto::read_be32(cur.data());
            if (on_login_) on_login_(effective, sid);
        }
        return;
    }

    if (hh.msg_id == proto::MSG_CHAT_BROADCAST) {
        std::string room; std::string sender; std::string text;
        std::uint32_t sender_sid = 0;
        auto cur = in;
        proto::read_lp_utf8(cur, room);
        proto::read_lp_utf8(cur, sender);
        proto::read_lp_utf8(cur, text);
        if (cur.size() >= 4) sender_sid = proto::read_be32(cur.data());
        if (on_bcast_) on_bcast_(std::move(room), std::move(sender), std::move(text), hh.flags, sender_sid);
        return;
    }

    if (hh.msg_id == proto::MSG_ROOM_USERS) {
        std::string room;
        proto::read_lp_utf8(in, room);
        std::vector<std::string> list;
        while (!in.empty()) {
            std::string entry;
            if (!proto::read_lp_utf8(in, entry)) break;
            list.emplace_back(std::move(entry));
        }
        if (on_room_users_) on_room_users_(std::move(room), std::move(list));
        return;
    }

    if (hh.msg_id == proto::MSG_STATE_SNAPSHOT) {
        server::wire::v1::StateSnapshot pb;
        if (server::wire::codec::Decode(in.data(), in.size(), pb)) {
            std::vector<std::string> rooms;
            std::vector<bool> locked;
            rooms.reserve(pb.rooms_size());
            locked.reserve(pb.rooms_size());
            for (const auto& r : pb.rooms()) {
                rooms.push_back(r.name());
                locked.push_back(r.locked());
            }
            std::vector<std::string> users;
            users.reserve(pb.users_size());
            for (const auto& u : pb.users()) {
                users.push_back(u);
            }
            if (on_snapshot_) {
                on_snapshot_(pb.current_room(), std::move(rooms), std::move(users), std::move(locked));
            }
        }
        return;
    }

    if (hh.msg_id == proto::MSG_WHISPER_BROADCAST) {
        std::string sender; std::string recipient; std::string text;
        bool outgoing = false;
        proto::read_lp_utf8(in, sender);
        proto::read_lp_utf8(in, recipient);
        proto::read_lp_utf8(in, text);
        if (!in.empty()) outgoing = in.front() != 0;
        if (on_whisper_) on_whisper_(std::move(sender), std::move(recipient), std::move(text), outgoing);
        return;
    }

    if (hh.msg_id == proto::MSG_WHISPER_RES) {
        bool ok = false;
        std::string reason;
        if (!in.empty()) {
            ok = in.front() != 0;
            in = in.subspan(1);
        }
        proto::read_lp_utf8(in, reason);
        if (on_whisper_result_) on_whisper_result_(ok, std::move(reason));
        return;
    }

    if (hh.msg_id == proto::MSG_HELLO) {
        std::uint16_t caps = 0;
        if (in.size() >= 12) {
            caps = proto::read_be16(in.data() + 4);
        }
        if (on_hello_) on_hello_(caps);
        return;
    }
}

void NetClient::handle_disconnect(const boost::system::error_code& ec, const char* context) {
    if (!running_.exchange(false)) {
        return;
    }
    connected_.store(false);

    boost::system::error_code ignore;
    socket_.cancel(ignore);
    socket_.close(ignore);
    if (ping_timer_) {
        ping_timer_->cancel();
    }
    write_queue_.clear();

    if (on_disconnected_) {
        std::string reason = ec ? ec.message() : "connection closed";
        if (context) {
            reason = std::string(context) + ": " + reason;
        }
        on_disconnected_(std::move(reason));
    }
}

void NetClient::send_login(const std::string& user, const std::string& token) {
    std::vector<std::uint8_t> p;
    proto::write_lp_utf8(p, user);
    proto::write_lp_utf8(p, token);
    enqueue_frame(proto::MSG_LOGIN_REQ, 0, std::move(p));
}

void NetClient::send_join(const std::string& room, const std::string& password) {
    std::vector<std::uint8_t> p;
    proto::write_lp_utf8(p, room);
    if (!password.empty()) proto::write_lp_utf8(p, password);
    enqueue_frame(proto::MSG_JOIN_ROOM, 0, std::move(p));
}

void NetClient::send_leave(const std::string& room) {
    std::vector<std::uint8_t> p;
    proto::write_lp_utf8(p, room);
    enqueue_frame(proto::MSG_LEAVE_ROOM, 0, std::move(p));
}

void NetClient::send_chat(const std::string& room, const std::string& text) {
    std::vector<std::uint8_t> p;
    proto::write_lp_utf8(p, room);
    proto::write_lp_utf8(p, text);
    enqueue_frame(proto::MSG_CHAT_SEND, 0, std::move(p));
}

void NetClient::send_refresh(const std::string& current_room) {
    std::vector<std::uint8_t> p;
    proto::write_lp_utf8(p, current_room);
    enqueue_frame(proto::MSG_REFRESH_REQ, 0, std::move(p));
}

void NetClient::send_who(const std::string& room) {
    std::vector<std::uint8_t> p;
    proto::write_lp_utf8(p, room);
    enqueue_frame(proto::MSG_ROOM_USERS_REQ, 0, std::move(p));
}

void NetClient::send_rooms(const std::string& current_room) {
    std::vector<std::uint8_t> p;
    proto::write_lp_utf8(p, current_room);
    enqueue_frame(proto::MSG_ROOMS_REQ, 0, std::move(p));
}

void NetClient::send_whisper(const std::string& user, const std::string& text) {
    std::vector<std::uint8_t> p;
    proto::write_lp_utf8(p, user);
    proto::write_lp_utf8(p, text);
    enqueue_frame(proto::MSG_WHISPER_REQ, 0, std::move(p));
}
