#include "server/chat/chat_service.hpp"
#include "server/core/protocol/opcodes.hpp"
#include "server/storage/redis/client.hpp"
#include <cstdlib>

using namespace server::core;
namespace proto = server::core::protocol;

namespace server::app::chat {

void ChatService::on_ping(Session& s, std::span<const std::uint8_t> payload) {
    // 클라이언트가 보낸 payload 그대로 PONG으로 되돌려 RTT를 측정하게 한다.
    std::vector<std::uint8_t> body(payload.begin(), payload.end());
    s.async_send(proto::MSG_PONG, body, 0);

    // ping은 사실상 heartbeat이므로 Redis presence TTL을 갱신한다.
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
        // 네트워크 hiccup 등으로 실패해도 치명적이지 않으므로 조용히 무시한다.
    }
}
} // namespace server::app::chat

