#include "server/core/net/dispatcher.hpp"
#include "server/core/net/session.hpp"
#include "server/core/util/log.hpp"
#include "server/core/protocol/protocol_errors.hpp"
#include "server/core/runtime_metrics.hpp"

namespace server::core {

void Dispatcher::register_handler(std::uint16_t msg_id, handler_t handler) {
    // msg_id는 protocol/opcodes.json에서 보장하므로 간단히 map에 저장한다.
    table_[msg_id] = std::move(handler);
}

bool Dispatcher::dispatch(std::uint16_t msg_id, Session& s, std::span<const std::uint8_t> payload) const {
    auto it = table_.find(msg_id);
    if (it == table_.end()) return false;
    try {
        // 핸들러 예외는 세션 단위 오류로 보고, metrics + 에러 패킷을 함께 발생시킨다.
        it->second(s, payload);
    } catch (const std::exception& ex) {
        runtime_metrics::record_dispatch_exception();
        server::core::log::error(std::string("handler exception for msg=") + std::to_string(msg_id) + ": " + ex.what());
        try { s.send_error(server::core::protocol::errc::INTERNAL_ERROR, "internal error"); } catch (...) {}
    } catch (...) {
        runtime_metrics::record_dispatch_exception();
        server::core::log::error(std::string("handler unknown exception for msg=") + std::to_string(msg_id));
        try { s.send_error(server::core::protocol::errc::INTERNAL_ERROR, "internal error"); } catch (...) {}
    }
    return true;
}

} // namespace server::core


