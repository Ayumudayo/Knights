#include <gtest/gtest.h>

#include <server/core/net/rudp/rudp_engine.hpp>
#include <server/core/net/rudp/rudp_packet.hpp>
#include <server/core/runtime_metrics.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

std::vector<std::uint8_t> make_hello(std::uint32_t connection_id) {
    server::core::net::rudp::RudpHeader header{};
    header.type = server::core::net::rudp::PacketType::kHello;
    header.connection_id = connection_id;
    return server::core::net::rudp::encode_packet(header, {});
}

} // namespace

TEST(RudpFallbackTest, InvalidVersionPacketRecordsProtocolErrorFallback) {
    const auto before = server::core::runtime_metrics::snapshot();

    server::core::net::rudp::RudpEngine engine;
    auto hello = make_hello(500);
    hello[2] = 9; // invalid version

    const auto result = engine.process_datagram(hello, 1000);
    EXPECT_TRUE(result.fallback_required);
    EXPECT_EQ(result.fallback_reason, "invalid_rudp_packet");

    const auto after = server::core::runtime_metrics::snapshot();
    const auto index = static_cast<std::size_t>(server::core::runtime_metrics::RudpFallbackReason::kProtocolError);
    EXPECT_GE(after.rudp_fallback_total[index], before.rudp_fallback_total[index] + 1);
}

TEST(RudpFallbackTest, InflightLimitRejectionRecordsFallbackMetric) {
    const auto before = server::core::runtime_metrics::snapshot();

    server::core::net::rudp::RudpConfig config{};
    config.max_inflight_packets = 1;
    server::core::net::rudp::RudpEngine engine(config);

    const auto hello = make_hello(777);
    const auto established = engine.process_datagram(hello, 1000);
    ASSERT_TRUE(established.handshake_established);

    std::vector<std::uint8_t> out_a;
    std::vector<std::uint8_t> out_b;
    const std::vector<std::uint8_t> payload{1, 2, 3};

    ASSERT_TRUE(engine.queue_reliable_payload(payload, 0, 1010, out_a));
    EXPECT_FALSE(engine.queue_reliable_payload(payload, 0, 1011, out_b));

    const auto after = server::core::runtime_metrics::snapshot();
    const auto index = static_cast<std::size_t>(server::core::runtime_metrics::RudpFallbackReason::kInflightLimit);
    EXPECT_GE(after.rudp_fallback_total[index], before.rudp_fallback_total[index] + 1);
}
