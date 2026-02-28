#include <gtest/gtest.h>

#include <server/core/net/rudp/ack_window.hpp>

TEST(RudpAckWindowImpairmentTest, LargeGapTracksLossAndStalePacketDrops) {
    server::core::net::rudp::AckWindow window;

    const auto first = window.observe(100);
    EXPECT_TRUE(first.accepted);
    EXPECT_EQ(first.ack_largest, 100u);

    const auto gap = window.observe(170);
    EXPECT_TRUE(gap.accepted);
    EXPECT_EQ(gap.estimated_lost_packets, 69u);
    EXPECT_EQ(gap.ack_largest, 170u);
    EXPECT_EQ(gap.ack_mask, 1u);

    const auto stale = window.observe(100);
    EXPECT_FALSE(stale.accepted);
    EXPECT_TRUE(stale.duplicate);
}

TEST(RudpAckWindowImpairmentTest, ReorderPacketUpdatesAckMask) {
    server::core::net::rudp::AckWindow window;

    EXPECT_TRUE(window.observe(10).accepted);
    const auto gap = window.observe(12);
    EXPECT_TRUE(gap.accepted);
    EXPECT_EQ(gap.ack_mask, 5u);

    const auto reorder = window.observe(11);
    EXPECT_TRUE(reorder.accepted);
    EXPECT_TRUE(reorder.reordered);
    EXPECT_EQ(reorder.ack_largest, 12u);
    EXPECT_EQ(reorder.ack_mask, 7u);
}
