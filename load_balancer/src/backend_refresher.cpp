#include "load_balancer/backend_refresher.hpp"
#include "server/core/util/log.hpp"

namespace load_balancer {

BackendRefresher::BackendRefresher(boost::asio::io_context& io,
                                   const Config& config,
                                   BackendRegistry& registry,
                                   server::state::IInstanceStateBackend* state_backend)
    : timer_(io)
    , config_(config)
    , registry_(registry)
    , state_backend_(state_backend) {
}

void BackendRefresher::start() {
    if (!config_.dynamic_backends_active) {
        return;
    }
    running_ = true;
    refresh();
}

void BackendRefresher::stop() {
    running_ = false;
    timer_.cancel();
}

void BackendRefresher::schedule_refresh() {
    if (!running_) return;

    timer_.expires_after(config_.backend_refresh_interval);
    timer_.async_wait([this](const boost::system::error_code& ec) {
        if (ec == boost::asio::error::operation_aborted) return;
        if (ec) {
            server::core::log::warn("BackendRefresher timer error: " + ec.message());
            schedule_refresh();
            return;
        }
        refresh();
    });
}

void BackendRefresher::refresh() {
    if (!running_) return;

    std::vector<BackendEndpoint> dynamic_backends;
    if (state_backend_) {
        try {
            auto records = state_backend_->list_instances();
            dynamic_backends = registry_.make_backends_from_records(records, config_.instance_id);
        } catch (const std::exception& ex) {
            server::core::log::warn(std::string("BackendRefresher failed to fetch registry: ") + ex.what());
        }
    }

    bool applied = false;
    if (!dynamic_backends.empty()) {
        applied = registry_.apply_snapshot(std::move(dynamic_backends), "registry");
    } else {
        // Fallback to static endpoints if dynamic list is empty
        // Note: In a real scenario, we might want to merge them or have a policy.
        // Here we follow the original logic: if dynamic is empty, try static.
        // But wait, the original logic was: if dynamic requested, use dynamic.
        // If dynamic yields nothing, check if we have static backends configured and use them if registry is empty.
        
        // Re-parsing static endpoints here might be redundant if we stored them parsed in Config.
        // Let's assume Config has raw strings, we need to parse them.
        // Actually Config has vector<string>.
        
        std::vector<BackendEndpoint> static_backends;
        for (size_t i = 0; i < config_.static_backend_endpoints.size(); ++i) {
             // We need a parse helper. For now, let's duplicate simple parsing or move parse_endpoint to a util.
             // Since we don't have a shared util yet, let's do a simple parse.
             // Or better, let's assume Config parses them into BackendEndpoint?
             // The Config struct I wrote stores strings.
             // Let's do simple parsing here.
             std::string_view val = config_.static_backend_endpoints[i];
             std::string host = "127.0.0.1";
             std::uint16_t port = 5000;
             
             auto delim = val.find(':');
             if (delim != std::string_view::npos) {
                 host = std::string(val.substr(0, delim));
                 auto p = val.substr(delim + 1);
                 if (!p.empty()) port = static_cast<std::uint16_t>(std::stoul(std::string(p)));
             } else if (!val.empty()) {
                 host = std::string(val);
             }
             
             BackendEndpoint ep;
             ep.id = "backend-" + std::to_string(i);
             ep.host = host;
             ep.port = port;
             static_backends.push_back(std::move(ep));
        }

        if (!static_backends.empty()) {
             // Only apply static if registry is empty or we want to enforce fallback
             if (registry_.empty()) {
                 applied = registry_.apply_snapshot(std::move(static_backends), "static_fallback");
             }
        }
    }

    if (!applied && registry_.empty()) {
        server::core::log::warn("BackendRefresher has no backend endpoints available");
    }

    schedule_refresh();
}

} // namespace load_balancer
