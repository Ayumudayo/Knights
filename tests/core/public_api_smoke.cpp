#include <array>
#include <cstdint>
#include <span>
#include <sstream>
#include <string>

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
    (void)server::core::api::version_string();

    boost::asio::io_context io;
    server::core::net::Hive hive(io);

    server::core::app::AppHost host{"core_public_api_smoke"};
    host.declare_dependency("sample");
    host.set_dependency_ok("sample", true);
    host.set_ready(true);
    host.set_lifecycle_phase(server::core::app::AppHost::LifecyclePhase::kRunning);

    server::core::concurrent::TaskScheduler scheduler;
    scheduler.post([] {});
    (void)scheduler.poll();

    server::core::SessionOptions options{};
    options.read_timeout_ms = 1000;

    server::core::metrics::MetricsHttpServer metrics_server(0, [] { return std::string{}; });

    server::core::Dispatcher dispatcher;
    dispatcher.register_handler(server::core::protocol::MSG_PING,
                                [](server::core::Session&, std::span<const std::uint8_t>) {});

    server::core::protocol::PacketHeader header{};
    std::array<std::uint8_t, server::core::protocol::k_header_bytes> encoded{};
    server::core::protocol::encode_header(header, encoded.data());
    server::core::protocol::decode_header(encoded.data(), header);

    (void)server::core::protocol::FLAG_COMPRESSED;
    (void)server::core::protocol::CAP_COMPRESS_SUPP;
    (void)server::core::protocol::errc::UNKNOWN_MSG_ID;
    (void)server::core::build_info::git_hash();

    server::core::log::set_level(server::core::log::level::info);
    (void)server::core::util::paths::executable_dir();

    std::ostringstream metrics;
    server::core::metrics::append_build_info(metrics);

    (void)host.dependency_metrics_text();
    (void)host.lifecycle_metrics_text();

    return 0;
}
