#pragma once

#include <cstdint>

namespace server::core::protocol {

enum class SessionStatus : std::uint8_t {
    kAny = 0,
    kAuthenticated,
    kInRoom,
    kAdmin,
};

enum class ProcessingPlace : std::uint8_t {
    kInline = 0,
    kWorker,
    kRoomStrand,
};

enum class TransportMask : std::uint8_t {
    kNone = 0,
    kTcp  = 1,
    kUdp  = 2,
    kBoth = 3,
};

enum class DeliveryClass : std::uint8_t {
    kReliableOrdered = 0,
    kReliable,
    kUnreliableSequenced,
};

enum class TransportKind : std::uint8_t {
    kTcp = 0,
    kUdp,
};

struct OpcodePolicy {
    SessionStatus   required_state  = SessionStatus::kAny;
    ProcessingPlace processing_place = ProcessingPlace::kInline;
    TransportMask   transport       = TransportMask::kTcp;
    DeliveryClass   delivery        = DeliveryClass::kReliableOrdered;
    std::uint8_t    channel         = 0;
};

inline constexpr OpcodePolicy default_opcode_policy() noexcept {
    return OpcodePolicy{};
}

inline constexpr bool transport_allows(TransportMask mask, TransportKind transport) noexcept {
    if (mask == TransportMask::kBoth) {
        return true;
    }
    if (mask == TransportMask::kNone) {
        return false;
    }
    if (transport == TransportKind::kTcp) {
        return mask == TransportMask::kTcp;
    }
    return mask == TransportMask::kUdp;
}

} // namespace server::core::protocol
