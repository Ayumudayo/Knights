#include "server/core/protocol/packet.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <span>
#include <string>
#include <vector>

int main() {
    std::size_t iterations = 200000;
    if (const char* raw = std::getenv("FUZZ_ITERATIONS"); raw && *raw) {
        try {
            iterations = static_cast<std::size_t>(std::stoull(raw));
        } catch (...) {
            iterations = 200000;
        }
    }

    std::mt19937_64 rng(0xC0FFEEULL);
    std::uniform_int_distribution<std::size_t> size_dist(0, 512);
    std::uniform_int_distribution<int> byte_dist(0, 255);

    std::size_t valid_utf8 = 0;
    std::size_t invalid_utf8 = 0;
    std::size_t parsed_lp_utf8 = 0;

    const auto started_at = std::chrono::steady_clock::now();

    for (std::size_t i = 0; i < iterations; ++i) {
        const std::size_t size = size_dist(rng);
        std::vector<std::uint8_t> bytes(size);
        for (auto& b : bytes) {
            b = static_cast<std::uint8_t>(byte_dist(rng));
        }

        if (!bytes.empty()) {
            const bool ok = server::core::protocol::is_valid_utf8(std::span<const std::uint8_t>(bytes.data(), bytes.size()));
            if (ok) {
                ++valid_utf8;
            } else {
                ++invalid_utf8;
            }
        }

        if (bytes.size() >= server::core::protocol::k_header_bytes) {
            server::core::protocol::PacketHeader header{};
            server::core::protocol::decode_header(bytes.data(), header);

            std::array<std::uint8_t, server::core::protocol::k_header_bytes> encoded{};
            server::core::protocol::encode_header(header, encoded.data());
        }

        std::span<const std::uint8_t> in(bytes.data(), bytes.size());
        std::string out;
        if (server::core::protocol::read_lp_utf8(in, out)) {
            ++parsed_lp_utf8;
        }

        std::vector<std::uint8_t> lp_buf;
        server::core::protocol::write_lp_utf8(lp_buf, "fuzz_payload");
        std::span<const std::uint8_t> lp_in(lp_buf.data(), lp_buf.size());
        std::string parsed;
        if (!server::core::protocol::read_lp_utf8(lp_in, parsed)) {
            std::cerr << "fuzz harness invariant failed: encoded lp_utf8 parse failed\n";
            return 1;
        }
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started_at);
    std::cout << "protocol_fuzz_harness iterations=" << iterations
              << " elapsed_ms=" << elapsed.count()
              << " valid_utf8=" << valid_utf8
              << " invalid_utf8=" << invalid_utf8
              << " parsed_lp_utf8=" << parsed_lp_utf8
              << "\n";
    return 0;
}
