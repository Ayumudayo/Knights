#pragma once

#include <cstdint>

namespace server::core::protocol {

/** @brief Required session state before an opcode is accepted. */
enum class SessionStatus : std::uint8_t {
    kAny = 0,       ///< No session-state restriction.
    kAuthenticated, ///< Session must be authenticated.
    kInRoom,        ///< Session must be joined to a room.
    kAdmin,         ///< Session must have admin privilege.
};

/** @brief Execution placement for opcode handling. */
enum class ProcessingPlace : std::uint8_t {
    kInline = 0, ///< Execute on current path.
    kWorker,     ///< Execute via worker queue.
    kRoomStrand, ///< Execute on room-serialized strand.
};

/** @brief Allowed transports for the opcode. */
enum class TransportMask : std::uint8_t {
    kNone = 0, ///< Reject for all transports.
    kTcp  = 1, ///< Allow TCP transport.
    kUdp  = 2, ///< Allow UDP transport.
    kBoth = 3, ///< Allow both TCP and UDP.
};

/** @brief Delivery guarantee profile for opcode traffic. */
enum class DeliveryClass : std::uint8_t {
    kReliableOrdered = 0, ///< Reliable and ordered.
    kReliable,            ///< Reliable but ordering not guaranteed.
    kUnreliableSequenced, ///< Unreliable and sequence-guarded.
};

/** @brief Runtime transport category for dispatch checks. */
enum class TransportKind : std::uint8_t {
    kTcp = 0,
    kUdp,
};

/** @brief Policy metadata attached to each opcode entry. */
struct OpcodePolicy {
    SessionStatus   required_state  = SessionStatus::kAny;              ///< Required session state.
    ProcessingPlace processing_place = ProcessingPlace::kInline;        ///< Execution placement.
    TransportMask   transport       = TransportMask::kTcp;              ///< Allowed transports.
    DeliveryClass   delivery        = DeliveryClass::kReliableOrdered;  ///< Delivery profile.
    std::uint8_t    channel         = 0;                                ///< Optional channel hint.
};

/**
 * @brief Returns the default policy used for legacy opcode definitions.
 * @return Default-initialized `OpcodePolicy`.
 */
inline constexpr OpcodePolicy default_opcode_policy() noexcept {
    return OpcodePolicy{};
}

/**
 * @brief Checks whether a transport is allowed by a transport mask.
 * @param mask Transport allow-list mask.
 * @param transport Runtime transport kind to validate.
 * @return `true` when transport is permitted by the mask.
 */
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
