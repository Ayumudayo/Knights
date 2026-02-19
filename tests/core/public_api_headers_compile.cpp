#include <array>
#include <cstdint>
#include <span>
#include <sstream>
#include <string>
#include <type_traits>

#include <boost/asio/io_context.hpp>

#include "server/core/api/version.hpp"
#include "server/core/app/app_host.hpp"
#include "server/core/build_info.hpp"
#include "server/core/concurrent/task_scheduler.hpp"
#include "server/core/config/options.hpp"
#include "server/core/metrics/build_info.hpp"
#include "server/core/metrics/http_server.hpp"
#include "server/core/net/dispatcher.hpp"
#include "server/core/net/hive.hpp"
#include "server/core/protocol/packet.hpp"
#include "server/core/protocol/protocol_errors.hpp"
#include "server/core/protocol/protocol_flags.hpp"
#include "server/core/protocol/system_opcodes.hpp"
#include "server/core/util/log.hpp"
#include "server/core/util/paths.hpp"

int main() {
    static_assert(std::is_default_constructible_v<server::core::SessionOptions>);

    boost::asio::io_context io;
    server::core::net::Hive hive(io);

    server::core::concurrent::TaskScheduler scheduler;
    scheduler.post([] {});
    (void)scheduler.poll();

    server::core::metrics::MetricsHttpServer metrics_server(0, [] { return std::string{}; });

    server::core::Dispatcher dispatcher;
    dispatcher.register_handler(server::core::protocol::MSG_PING,
                                [](server::core::Session&, std::span<const std::uint8_t>) {});

    server::core::protocol::PacketHeader header{};
    std::array<std::uint8_t, server::core::protocol::k_header_bytes> encoded{};
    server::core::protocol::encode_header(header, encoded.data());
    server::core::protocol::decode_header(encoded.data(), header);

    (void)server::core::api::version_string();
    (void)server::core::build_info::git_hash();
    (void)server::core::protocol::FLAG_COMPRESSED;
    (void)server::core::protocol::CAP_COMPRESS_SUPP;
    (void)server::core::protocol::errc::UNKNOWN_MSG_ID;

    server::core::log::set_level(server::core::log::level::info);
    (void)server::core::util::paths::executable_dir();

    std::ostringstream metrics;
    server::core::metrics::append_build_info(metrics);

    return 0;
}
