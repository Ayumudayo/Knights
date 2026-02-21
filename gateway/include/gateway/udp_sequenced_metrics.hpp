#pragma once

#include <cstdint>

namespace gateway {

/**
 * @brief Tracks sequence-based UDP quality signals for a single bound session.
 *
 * The tracker identifies duplicates/reorders and estimates loss and jitter
 * using packet sequence and receive-time deltas.
 */
class UdpSequencedMetrics {
public:
    /** @brief Classification and quality deltas for one packet update. */
    struct UpdateResult {
        bool accepted{false};                    ///< Packet accepted as forward progress.
        bool duplicate{false};                   ///< Packet sequence duplicated latest accepted seq.
        bool reordered{false};                   ///< Packet sequence older than latest accepted seq.
        std::uint64_t estimated_lost_packets{0}; ///< Estimated missing packets before this packet.
        std::uint64_t jitter_ms{0};              ///< Interarrival jitter delta in milliseconds.
    };

    /** @brief Clears tracker state, typically on rebind. */
    void reset() {
        initialized_ = false;
        last_seq_ = 0;
        last_recv_ms_ = 0;
        last_interarrival_ms_ = 0;
    }

    /**
     * @brief Processes one packet sample and updates quality statistics.
     * @param seq Packet sequence number from transport header.
     * @param recv_unix_ms Packet receive unix timestamp in milliseconds.
     * @return Packet classification and estimated quality deltas.
     */
    UpdateResult on_packet(std::uint32_t seq, std::uint64_t recv_unix_ms) {
        UpdateResult result{};

        if (!initialized_) {
            initialized_ = true;
            last_seq_ = seq;
            last_recv_ms_ = recv_unix_ms;
            last_interarrival_ms_ = 0;
            result.accepted = true;
            return result;
        }

        if (seq == last_seq_) {
            result.duplicate = true;
            return result;
        }

        if (seq < last_seq_) {
            result.reordered = true;
            return result;
        }

        result.accepted = true;
        if (seq > (last_seq_ + 1u)) {
            result.estimated_lost_packets = static_cast<std::uint64_t>(seq - last_seq_ - 1u);
        }

        if (last_recv_ms_ != 0) {
            if (recv_unix_ms >= last_recv_ms_) {
                const auto interarrival_ms = recv_unix_ms - last_recv_ms_;
                if (last_interarrival_ms_ != 0) {
                    result.jitter_ms = (interarrival_ms >= last_interarrival_ms_)
                        ? (interarrival_ms - last_interarrival_ms_)
                        : (last_interarrival_ms_ - interarrival_ms);
                }
                last_interarrival_ms_ = interarrival_ms;
            } else {
                last_interarrival_ms_ = 0;
            }
        }

        last_seq_ = seq;
        last_recv_ms_ = recv_unix_ms;
        return result;
    }

private:
    bool initialized_{false};
    std::uint32_t last_seq_{0};
    std::uint64_t last_recv_ms_{0};
    std::uint64_t last_interarrival_ms_{0};
};

} // namespace gateway
