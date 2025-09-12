// UTF-8, 한국어 주석
#include "server/chat/chat_service.hpp"
#include "server/core/protocol/opcodes.hpp"
#include "server/core/protocol/frame.hpp"
#include "server/core/protocol/protocol_errors.hpp"
#include "server/core/protocol/protocol_flags.hpp"
#include "server/core/util/log.hpp"
#include "server/core/concurrent/job_queue.hpp"
#include "wire.pb.h"

namespace {
    template <typename ProtoMsg>
    const std::vector<std::uint8_t>& EncodeToScratch(const ProtoMsg& pb) {
        thread_local std::vector<std::uint8_t> scratch;
        const int sz = pb.ByteSizeLong();
        if (sz <= 0) { scratch.clear(); return scratch; }
        scratch.resize(static_cast<std::size_t>(sz));
        pb.SerializeToArray(scratch.data(), sz);
        return scratch;
    }
}

using namespace server::core;
namespace proto = server::core::protocol;
namespace corelog = server::core::log;

namespace server::app::chat {

ChatService::ChatService(boost::asio::io_context& io, server::core::JobQueue& job_queue)
    : io_(&io), job_queue_(job_queue) {}

ChatService::Strand& ChatService::strand_for(const std::string& room) {
    auto it = room_strands_.find(room);
    if (it == room_strands_.end()) {
        it = room_strands_.emplace(room, std::make_shared<Strand>(io_->get_executor())).first;
    }
    return *it->second;
}

std::string ChatService::gen_hex_name(Session& s) {
    auto now64 = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::uint64_t v = (static_cast<std::uint64_t>(s.session_id()) << 32) ^ static_cast<std::uint64_t>(now64);
    v &= 0xFFFFFFFFull; std::ostringstream oss; oss << std::hex; oss.width(8); oss.fill('0'); oss << v; return oss.str();
}

std::string ChatService::ensure_unique_or_error(Session& s, const std::string& desired) {
    std::lock_guard<std::mutex> lk(state_.mu);
    if (!desired.empty() && desired != "guest") {
        auto itset = state_.by_user.find(desired);
        if (itset != state_.by_user.end()) {
            bool taken = false;
            for (auto wit = itset->second.begin(); wit != itset->second.end(); ) {
                if (auto p = wit->lock()) { taken = true; break; }
                else { wit = itset->second.erase(wit); }
            }
            if (taken) {
                s.send_error(proto::errc::NAME_TAKEN, "name taken");
                return {};
            }
        }
        return desired;
    }
    // 임시 닉 생성
    for (int i=0;i<4;++i) {
        std::string cand = gen_hex_name(s);
        if (!state_.by_user.count(cand) || state_.by_user[cand].empty()) return cand;
    }
    return gen_hex_name(s);
}

// on_login은 handlers_login.cpp로 이동

void ChatService::on_join(Session& s, std::span<const std::uint8_t> payload) {
    auto sp = std::span<const std::uint8_t>(payload.data(), payload.size());
    std::string room;
    if (!proto::read_lp_utf8(sp, room)) { 
        s.send_error(proto::errc::INVALID_PAYLOAD, "bad join payload"); 
        return; 
    }

    auto session_sp = s.shared_from_this();
    job_queue_.Push([this, session_sp, room]() {
        std::string room_to_join = room;
        if (room_to_join.empty()) room_to_join = "lobby";
        corelog::info(std::string("JOIN_ROOM: ") + room_to_join);

        std::vector<std::shared_ptr<Session>> targets;
        std::vector<std::uint8_t> body;
        {
            std::lock_guard<std::mutex> lk(state_.mu);
            if (!state_.authed.count(session_sp.get())) { 
                session_sp->send_error(proto::errc::UNAUTHORIZED, "unauthorized"); 
                return; 
            }
            // 기존 방에서 제거
            auto itold = state_.cur_room.find(session_sp.get());
            if (itold != state_.cur_room.end() && itold->second != room_to_join) {
                auto itroom = state_.rooms.find(itold->second);
                if (itroom != state_.rooms.end()) {
                    itroom->second.erase(session_sp);
                    // 기존 방이 비면(lobby 제외) 제거
                    bool is_empty = true;
                    for (auto wit = itroom->second.begin(); wit != itroom->second.end(); ) {
                        if (wit->expired()) wit = itroom->second.erase(wit); 
                        else { is_empty = false; ++wit; }
                    }
                    if (is_empty && itold->second != "lobby") {
                        state_.rooms.erase(itroom);
                    }
                }
            }
            state_.cur_room[session_sp.get()] = room_to_join;
            state_.rooms[room_to_join].insert(session_sp);
            // 입장 브로드캐스트(Protobuf)
            std::string sender; 
            auto it2 = state_.user.find(session_sp.get()); 
            sender = (it2 != state_.user.end()) ? it2->second : std::string("guest");
            server::wire::v1::ChatBroadcast pb; 
            pb.set_room(room_to_join); 
            pb.set_sender("(system)"); 
            pb.set_text(sender + " 님이 입장했습니다"); 
            pb.set_sender_sid(0);
            {
                auto now64 = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                pb.set_ts_ms(static_cast<std::uint64_t>(now64));
            }
            {
                const auto& scratch = EncodeToScratch(pb);
                body.assign(scratch.begin(), scratch.end());
            }
            // 타겟 수집
            auto it = state_.rooms.find(room_to_join);
            if (it != state_.rooms.end()) {
                auto& set = it->second;
                for (auto wit = set.begin(); wit != set.end(); ) { 
                    if (auto p = wit->lock()) { 
                        targets.emplace_back(std::move(p)); 
                        ++wit; 
                    } else { 
                        wit = set.erase(wit); 
                    } 
                }
            }
        }
        for (auto& t : targets) t->async_send(proto::MSG_CHAT_BROADCAST, body, 0);
    });
}

void ChatService::on_leave(Session& s, std::span<const std::uint8_t> payload) {
    auto sp = std::span<const std::uint8_t>(payload.data(), payload.size());
    std::string room;
    proto::read_lp_utf8(sp, room); // 방 이름 파싱은 실패해도 괜찮음

    auto session_sp = s.shared_from_this();
    job_queue_.Push([this, session_sp, room]() {
        std::vector<std::shared_ptr<Session>> targets;
        std::vector<std::uint8_t> body;
        std::string room_to_leave;
        {
            std::lock_guard<std::mutex> lk(state_.mu);
            if (!state_.authed.count(session_sp.get())) { 
                session_sp->send_error(proto::errc::UNAUTHORIZED, "unauthorized"); 
                return; 
            }
            auto itcr = state_.cur_room.find(session_sp.get());
            if (itcr == state_.cur_room.end()) { 
                session_sp->send_error(proto::errc::NO_ROOM, "no current room"); 
                return; 
            }
            if (!room.empty() && itcr->second != room) { 
                session_sp->send_error(proto::errc::ROOM_MISMATCH, "room mismatch"); 
                return; 
            }
            room_to_leave = itcr->second;
            // 퇴장 방송
            auto itroom = state_.rooms.find(room_to_leave);
            if (itroom != state_.rooms.end()) {
                itroom->second.erase(session_sp);
                std::string sender; 
                auto it2 = state_.user.find(session_sp.get()); 
                sender = (it2 != state_.user.end()) ? it2->second : std::string("guest");
                proto::write_lp_utf8(body, room_to_leave); 
                proto::write_lp_utf8(body, std::string("(system)")); 
                proto::write_lp_utf8(body, sender + " 님이 퇴장했습니다");
                { std::size_t off_sid = body.size(); body.resize(off_sid + 4); proto::write_be32(0, body.data() + off_sid); }
                auto itb = state_.rooms.find(room_to_leave);
                if (itb != state_.rooms.end()) {
                    auto& set = itb->second;
                    for (auto wit = set.begin(); wit != set.end(); ) { 
                        if (auto p = wit->lock()) { 
                            targets.emplace_back(std::move(p)); 
                            ++wit; 
                        } else { 
                            wit = set.erase(wit); 
                        } 
                    }
                    if (set.empty() && room_to_leave != "lobby") state_.rooms.erase(itb);
                }
            }
            state_.cur_room[session_sp.get()] = std::string("lobby");
            state_.rooms["lobby"].insert(session_sp);
        }
        for (auto& t : targets) { 
            auto f = (t.get() == session_sp.get()) ? proto::FLAG_SELF : 0; 
            t->async_send(proto::MSG_CHAT_BROADCAST, body, f); 
        }
        // 로비 입장 알림
        std::vector<std::shared_ptr<Session>> t2; 
        std::vector<std::uint8_t> body2;
        {
            std::lock_guard<std::mutex> lk(state_.mu);
            std::string sender; 
            auto it2 = state_.user.find(session_sp.get()); 
            sender = (it2 != state_.user.end()) ? it2->second : std::string("guest");
            proto::write_lp_utf8(body2, std::string("lobby")); 
            proto::write_lp_utf8(body2, std::string("(system)")); 
            proto::write_lp_utf8(body2, sender + " 님이 입장했습니다");
            { std::size_t off_sid = body2.size(); body2.resize(off_sid + 4); proto::write_be32(0, body2.data() + off_sid); }
            auto itb = state_.rooms.find("lobby"); 
            if (itb != state_.rooms.end()) { 
                auto& set = itb->second; 
                for (auto wit = set.begin(); wit != set.end(); ) { 
                    if (auto p = wit->lock()) { 
                        t2.emplace_back(std::move(p)); 
                        ++wit; 
                    } else { 
                        wit = set.erase(wit); 
                    } 
                } 
            }
        }
        for (auto& t : t2) t->async_send(proto::MSG_CHAT_BROADCAST, body2, 0);
    });
}

void ChatService::send_rooms_list(Session& s) {
    std::vector<std::uint8_t> body;
    std::string msg = "rooms:";
    {
        std::lock_guard<std::mutex> lk(state_.mu);
        std::vector<std::string> to_remove;
        for (auto it = state_.rooms.begin(); it != state_.rooms.end(); ++it) {
            std::size_t alive = 0;
            for (auto wit = it->second.begin(); wit != it->second.end(); ) { if (auto p = wit->lock()) { ++alive; ++wit; } else { wit = it->second.erase(wit); } }
            if (alive == 0 && it->first != std::string("lobby")) { to_remove.push_back(it->first); continue; }
            msg += " " + it->first + "(" + std::to_string(alive) + ")";
        }
        for (auto& name : to_remove) state_.rooms.erase(name);
    }
    server::wire::v1::ChatBroadcast pb; pb.set_room("(system)"); pb.set_sender("server"); pb.set_text(msg); pb.set_sender_sid(0);
    {
        auto now64 = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        pb.set_ts_ms(static_cast<std::uint64_t>(now64));
    }
    {
        const auto& scratch = EncodeToScratch(pb);
        body.assign(scratch.begin(), scratch.end());
    }
    s.async_send(proto::MSG_CHAT_BROADCAST, body, 0);
}

void ChatService::send_room_users(Session& s, const std::string& target) {
    std::vector<std::uint8_t> body;
    server::wire::v1::RoomUsers pb; pb.set_room(target);
    std::uint16_t n = 0;
    {
        std::lock_guard<std::mutex> lk(state_.mu);
        auto itroom = state_.rooms.find(target);
        if (itroom != state_.rooms.end()) {
            for (auto wit = itroom->second.begin(); wit != itroom->second.end(); ) {
                if (auto p = wit->lock()) {
                    auto itu = state_.user.find(p.get());
                    std::string name = (itu != state_.user.end()) ? itu->second : std::string("guest");
                    pb.add_users(name); ++n; ++wit;
                } else { wit = itroom->second.erase(wit); }
            }
        }
    }
    {
        const auto& scratch = EncodeToScratch(pb);
        body.assign(scratch.begin(), scratch.end());
    }
    s.async_send(proto::MSG_ROOM_USERS, body, 0);
}

void ChatService::send_snapshot(Session& s, const std::string& current) {
    std::vector<std::uint8_t> body;
    server::wire::v1::StateSnapshot pb; pb.set_current_room(current);
    {
        std::lock_guard<std::mutex> lk(state_.mu);
        std::vector<std::string> to_remove;
        for (auto it = state_.rooms.begin(); it != state_.rooms.end(); ++it) {
            std::uint32_t alive = 0; for (auto wit = it->second.begin(); wit != it->second.end(); ) { if (auto p = wit->lock()) { ++alive; ++wit; } else { wit = it->second.erase(wit); } }
            if (alive == 0 && it->first != std::string("lobby")) { to_remove.push_back(it->first); continue; }
            auto* ri = pb.add_rooms(); ri->set_name(it->first); ri->set_members(alive);
        }
        for (auto& name : to_remove) state_.rooms.erase(name);
    }
    {
        std::lock_guard<std::mutex> lk(state_.mu);
        auto itroom = state_.rooms.find(current);
        if (itroom != state_.rooms.end()) {
            for (auto wit = itroom->second.begin(); wit != itroom->second.end(); ) { if (auto p = wit->lock()) { auto itu = state_.user.find(p.get()); std::string name = (itu != state_.user.end()) ? itu->second : std::string("guest"); pb.add_users(name); ++wit; } else { wit = itroom->second.erase(wit); } }
        }
    }
    {
        const auto& scratch2 = EncodeToScratch(pb);
        body.assign(scratch2.begin(), scratch2.end());
    }
    s.async_send(proto::MSG_STATE_SNAPSHOT, body, 0);
}

void ChatService::on_chat_send(Session& s, std::span<const std::uint8_t> payload) {
    std::string room, text;
    auto sp = std::span<const std::uint8_t>(payload.data(), payload.size());
    if (!proto::read_lp_utf8(sp, room) || !proto::read_lp_utf8(sp, text)) { 
        s.send_error(proto::errc::INVALID_PAYLOAD, "bad chat payload"); 
        return; 
    }

    auto session_sp = s.shared_from_this();
    job_queue_.Push([this, session_sp, room, text]() {
        corelog::info(std::string("CHAT_SEND: room=") + (room.empty()?"(empty)":room) + ", text=" + text);
        // /refresh는 인증 없이도 허용(스냅샷 반환)
        if (text == "/refresh") {
            std::string current;
            {
                std::lock_guard<std::mutex> lk(state_.mu); 
                auto itcr = state_.cur_room.find(session_sp.get()); 
                current = (itcr != state_.cur_room.end()) ? itcr->second : std::string("lobby");
            }
            send_snapshot(*session_sp, current); 
            return;
        }
        // 인증 체크
        std::string current_room = room;
        {
            std::lock_guard<std::mutex> lk(state_.mu);
            if (!state_.authed.count(session_sp.get())) { 
                session_sp->send_error(proto::errc::UNAUTHORIZED, "unauthorized"); 
                return; 
            }
            if (current_room.empty()) {
                auto it = state_.cur_room.find(session_sp.get()); 
                if (it == state_.cur_room.end()) { 
                    session_sp->send_error(proto::errc::NO_ROOM, "no current room"); 
                    return; 
                } 
                current_room = it->second;
            }
        }
        // 슬래시 명령
        if (!text.empty() && text[0] == '/') {
            if (text == "/rooms") { send_rooms_list(*session_sp); return; }
            if (text.rfind("/who", 0) == 0) {
                std::string target = current_room; 
                if (text.size() > 4) { 
                    auto pos = text.find_first_not_of(' ', 4); 
                    if (pos != std::string::npos) target = text.substr(pos); 
                }
                send_room_users(*session_sp, target); 
                return;
            }
            if (text.rfind("/whisper ", 0) == 0) {
                std::string rest = text.substr(9); auto spc = rest.find(' ');
                if (spc == std::string::npos) {
                    std::vector<std::uint8_t> body; 
                    proto::write_lp_utf8(body, std::string("(system)")); 
                    proto::write_lp_utf8(body, std::string("server")); 
                    proto::write_lp_utf8(body, std::string("usage: /whisper <user> <message>"));
                    auto now64 = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(); 
                    std::size_t off = body.size(); 
                    body.resize(off + 8); 
                    std::uint64_t ts = static_cast<std::uint64_t>(now64); 
                    for (int i = 7; i >= 0; --i) { body[off + i] = static_cast<std::uint8_t>(ts & 0xFF); ts >>= 8; } 
                    session_sp->async_send(proto::MSG_CHAT_BROADCAST, body, 0); 
                    return;
                }
                std::string target_user = rest.substr(0, spc); 
                std::string wtext = rest.substr(spc + 1);
                std::vector<std::shared_ptr<Session>> targets;
                {
                    std::lock_guard<std::mutex> lk(state_.mu); 
                    auto itset = state_.by_user.find(target_user); 
                    if (itset != state_.by_user.end()) { 
                        for (auto wit = itset->second.begin(); wit != itset->second.end(); ) { 
                            if (auto p = wit->lock()) { 
                                targets.emplace_back(std::move(p)); 
                                ++wit; 
                            } else { 
                                wit = itset->second.erase(wit); 
                            } 
                        } 
                    }
                }
                std::string sender; 
                { 
                    std::lock_guard<std::mutex> lk(state_.mu); 
                    auto it2 = state_.user.find(session_sp.get()); 
                    sender = (it2 != state_.user.end()) ? it2->second : std::string("guest"); 
                }
                server::wire::v1::ChatBroadcast pb; 
                pb.set_room("(direct)"); 
                pb.set_sender(sender); 
                pb.set_text(std::string("(to ") + target_user + ") " + wtext); 
                pb.set_sender_sid(session_sp->session_id());
                auto now64 = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(); 
                pb.set_ts_ms(static_cast<std::uint64_t>(now64));
                const auto& scratch = EncodeToScratch(pb); 
                std::vector<std::uint8_t> body(scratch.begin(), scratch.end());
                targets.emplace_back(session_sp); 
                for (auto& t : targets) t->async_send(proto::MSG_CHAT_BROADCAST, body, 0); 
                return;
            }
        }
        // 일반 채팅
        std::vector<std::shared_ptr<Session>> targets;
        std::string sender;
        {
            std::lock_guard<std::mutex> lk(state_.mu);
            auto it2 = state_.user.find(session_sp.get()); 
            sender = (it2 != state_.user.end()) ? it2->second : std::string("guest");
            if (sender == "guest") { 
                session_sp->send_error(proto::errc::UNAUTHORIZED, "guest cannot chat"); 
                return; 
            }
            auto it = state_.rooms.find(current_room); 
            if (it != state_.rooms.end()) { 
                auto& set = it->second; 
                for (auto wit = set.begin(); wit != set.end(); ) { 
                    if (auto p = wit->lock()) { 
                        targets.emplace_back(std::move(p)); 
                        ++wit; 
                    } else { 
                        wit = set.erase(wit); 
                    } 
                } 
            }
        }
        server::wire::v1::ChatBroadcast pb; 
        pb.set_room(current_room); 
        pb.set_sender(sender); 
        pb.set_text(text); 
        pb.set_sender_sid(session_sp->session_id());
        auto now64 = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(); 
        pb.set_ts_ms(static_cast<std::uint64_t>(now64));
        const auto& body = EncodeToScratch(pb);
        if (targets.empty()) { 
            session_sp->async_send(proto::MSG_CHAT_BROADCAST, body, proto::FLAG_SELF); 
        } else { 
            for (auto& t : targets) { 
                auto f = (t.get() == session_sp.get()) ? proto::FLAG_SELF : 0; 
                t->async_send(proto::MSG_CHAT_BROADCAST, body, f); 
            } 
        }
    });
}

// on_session_close는 session_events.cpp로 이동

} // namespace server::app::chat
