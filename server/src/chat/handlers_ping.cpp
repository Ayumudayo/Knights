#include "server/chat/chat_service.hpp"
#include "server/core/protocol/opcodes.hpp"
#include "server/storage/redis/client.hpp"
#include <cstdlib>

using namespace server::core;
namespace proto = server::core::protocol;

namespace server::app::chat {

void ChatService::on_ping(Session& s, std::span<const std::uint8_t> payload) {
    // 요청 payload를 그대로 PONG으로 반사한다.
    std::vector<std::uint8_t> body(payload.begin(), payload.end());
    s.async_send(proto::MSG_PONG, body, 0);

    // 간단한 heartbeat로 로그인 사용자의 presence:user:{uid} TTL을 갱신한다.
    if (!redis_) return;
    try {
        std::string uid;
        {
            std::lock_guard<std::mutex> lk(state_.mu);
            if (!state_.authed.count(&s)) return;
            auto it = state_.user_uuid.find(&s);
            if (it != state_.user_uuid.end()) uid = it->second;
        }
        if (uid.empty()) return;
        touch_user_presence(uid);
    } catch (...) {
        // no-op: heartbeat 갱신 실패는 치명적이지 않으므로 무시한다.
    }
}
} // namespace server::app::chat

