#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "server/core/util/log.hpp"

namespace load_balancer {

struct Config {
    std::string grpc_listen_address{"127.0.0.1:7001"};
    std::string instance_id;
    std::string backend_registry_prefix{"gateway/instances"};
    std::string redis_uri;
    
    std::vector<std::string> static_backend_endpoints;
    
    std::chrono::seconds session_binding_ttl{45};
    std::chrono::seconds backend_refresh_interval{5};
    std::chrono::seconds backend_idle_timeout{30};
    std::chrono::seconds heartbeat_interval{5};
    std::chrono::seconds backend_state_ttl{30};
    std::chrono::seconds backend_retry_cooldown{5};
    
    std::size_t backend_failure_threshold{3};
    bool dynamic_backends_active{false};

    static Config load() {
        Config config;
        
        // Environment variables
        const char* kEnvGrpcListen = "LB_GRPC_LISTEN";
        const char* kEnvInstanceId = "LB_INSTANCE_ID";
        const char* kEnvBackendEndpoints = "LB_BACKEND_ENDPOINTS";
        const char* kEnvRedisUri = "LB_REDIS_URI";
        const char* kEnvDynamicBackends = "LB_DYNAMIC_BACKENDS";
        const char* kEnvBackendRefreshInterval = "LB_BACKEND_REFRESH_INTERVAL";
        const char* kEnvBackendRegistryPrefix = "LB_BACKEND_REGISTRY_PREFIX";
        const char* kEnvBackendIdleTimeout = "LB_BACKEND_IDLE_TIMEOUT";
        const char* kDefaultBackendEndpoint = "127.0.0.1:5000";

        if (const char* env = std::getenv(kEnvGrpcListen); env && *env) {
            config.grpc_listen_address = env;
        }

        if (const char* env = std::getenv(kEnvInstanceId); env && *env) {
            config.instance_id = env;
        } else {
            config.instance_id = "lb-" + std::to_string(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count());
        }

        if (const char* env = std::getenv(kEnvBackendRegistryPrefix); env && *env) {
            config.backend_registry_prefix = env;
        }
        if (!config.backend_registry_prefix.empty() && config.backend_registry_prefix.back() != '/') {
            config.backend_registry_prefix.push_back('/');
        }

        if (const char* env = std::getenv(kEnvRedisUri); env && *env) {
            config.redis_uri = env;
        } else if (const char* env = std::getenv("REDIS_URI"); env && *env) {
            config.redis_uri = env;
        }

        const char* backend_env = std::getenv(kEnvBackendEndpoints);
        std::string_view endpoints = (backend_env && *backend_env) ? backend_env : kDefaultBackendEndpoint;
        
        std::size_t start = 0;
        while (start < endpoints.size()) {
            auto end = endpoints.find(',', start);
            if (end == std::string_view::npos) end = endpoints.size();
            auto token = endpoints.substr(start, end - start);
            if (!token.empty()) {
                config.static_backend_endpoints.emplace_back(token);
            }
            start = end + 1;
        }
        if (config.static_backend_endpoints.empty()) {
             config.static_backend_endpoints.emplace_back(kDefaultBackendEndpoint);
        }

        auto parse_seconds = [](const char* name, std::chrono::seconds& out) {
            if (const char* env = std::getenv(name); env && *env) {
                try {
                    auto val = std::stoul(env);
                    if (val > 0) out = std::chrono::seconds{static_cast<long long>(val)};
                } catch (...) {
                    server::core::log::warn(std::string("Config invalid ") + name);
                }
            }
        };

        parse_seconds("LB_SESSION_TTL", config.session_binding_ttl);
        parse_seconds(kEnvBackendRefreshInterval, config.backend_refresh_interval);
        parse_seconds(kEnvBackendIdleTimeout, config.backend_idle_timeout);
        parse_seconds("LB_BACKEND_COOLDOWN", config.backend_retry_cooldown);

        if (const char* env = std::getenv("LB_BACKEND_FAILURE_THRESHOLD"); env && *env) {
            try {
                auto val = std::stoul(env);
                if (val > 0) config.backend_failure_threshold = val;
            } catch (...) {
                server::core::log::warn("Config invalid LB_BACKEND_FAILURE_THRESHOLD");
            }
        }

        if (const char* env = std::getenv(kEnvDynamicBackends); env && *env) {
            config.dynamic_backends_active = (std::strcmp(env, "0") != 0);
        }

        return config;
    }
};

} // namespace load_balancer
