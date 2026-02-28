#include <gtest/gtest.h>

#include <server/core/net/rudp/retransmission_queue.hpp>

TEST(RudpRetransmissionImpairmentTest, AckMaskSelectivelyRemovesPackets) {
    server::core::net::rudp::RetransmissionQueue queue;

    queue.push(10, 1000, {0x10});
    queue.push(11, 1000, {0x11});
    queue.push(12, 1000, {0x12});

    // ack_largest=12, ack_mask bit2=packet10 acked, packet11 remains pending.
    queue.mark_acked(12, 0b101u);
    EXPECT_EQ(queue.inflight_packets(), 1u);

    const auto due = queue.collect_timeouts(1205, 200, 8);
    ASSERT_EQ(due.size(), 1u);
    EXPECT_EQ(due.front().packet_number, 11u);
}

TEST(RudpRetransmissionImpairmentTest, TimeoutBatchRespectsMaxBatchAndBackoffClock) {
    server::core::net::rudp::RetransmissionQueue queue;

    queue.push(1, 100, {0x01});
    queue.push(2, 100, {0x02});
    queue.push(3, 100, {0x03});

    const auto first = queue.collect_timeouts(350, 200, 2);
    ASSERT_EQ(first.size(), 2u);
    EXPECT_EQ(first[0].retransmit_count, 1u);
    EXPECT_EQ(first[1].retransmit_count, 1u);

    const auto remaining = queue.collect_timeouts(350, 200, 4);
    ASSERT_EQ(remaining.size(), 1u);
    EXPECT_EQ(remaining[0].packet_number, 3u);

    const auto second = queue.collect_timeouts(560, 200, 4);
    ASSERT_EQ(second.size(), 3u);
    EXPECT_EQ(second[0].retransmit_count, 2u);
}

TEST(RudpRetransmissionImpairmentTest, ZeroRtoOrBatchSkipsTimeoutCollection) {
    server::core::net::rudp::RetransmissionQueue queue;
    queue.push(1, 100, {0x01});

    const auto zero_rto = queue.collect_timeouts(1000, 0, 8);
    EXPECT_TRUE(zero_rto.empty());

    const auto zero_batch = queue.collect_timeouts(1000, 200, 0);
    EXPECT_TRUE(zero_batch.empty());
    EXPECT_EQ(queue.inflight_packets(), 1u);
}
