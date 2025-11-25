#include "server/app/config.hpp"
#include "server/core/util/log.hpp"
#include "server/core/config/dotenv.hpp"
#include "server/core/util/paths.hpp"
#include <filesystem>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace server::app {

namespace corelog = server::core::log;
namespace pathutil = server::core::util::paths;

bool ServerConfig::load(int argc, char** argv) {
    // 1. .env 파일 로드
    bool env_loaded = false;
    std::filesystem::path env_path;
    std::filesystem::path exe_dir;

    try {
        exe_dir = pathutil::executable_dir();
        auto local_env = exe_dir / ".env";
        if (std::filesystem::exists(local_env)) {
            env_loaded = server::core::config::load_dotenv(local_env.string(), true);
            if (env_loaded) env_path = local_env;
        } else {
            auto local_default = exe_dir / ".env.default";
            if (std::filesystem::exists(local_default)) {
                try {
                    std::filesystem::copy_file(local_default, local_env, std::filesystem::copy_options::overwrite_existing);
                    corelog::info("Seeded .env next to executable from .env.default");
                    env_loaded = server::core::config::load_dotenv(local_env.string(), true);
                    if (env_loaded) env_path = local_env;
                } catch (const std::exception& copy_ex) {
                    corelog::warn(std::string("Failed to seed .env next to executable: ") + copy_ex.what());
                }
            }
        }
    } catch (const std::exception& ex) {
        corelog::warn(std::string("Executable dir detection failed: ") + ex.what());
    }

    if (!env_loaded) {
        std::filesystem::path repo_env{".env"};
        if (!std::filesystem::exists(repo_env)) {
            std::filesystem::path repo_default{".env.default"};
            if (std::filesystem::exists(repo_default)) {
                try {
                    std::filesystem::copy_file(repo_default, repo_env, std::filesystem::copy_options::overwrite_existing);
                    corelog::info("Seeded repository .env from .env.default");
                } catch (const std::exception& copy_ex) {
                    corelog::warn(std::string("Failed to seed repository .env: ") + copy_ex.what());
                }
            }
        }
        if (std::filesystem::exists(repo_env)) {
            env_loaded = server::core::config::load_dotenv(repo_env.string(), true);
            if (env_loaded) env_path = std::filesystem::absolute(repo_env);
        }
    }

    if (env_loaded) {
        corelog::info(std::string("Loaded environment file: ") + env_path.string());
    } else {
        corelog::info("No .env file located; using existing environment variables");
    }

    // 2. 기본 설정 로드
    if (argc >= 2) {
        port = static_cast<unsigned short>(std::stoi(argv[1]));
    }

    if (const char* val = std::getenv("LOG_BUFFER_CAPACITY"); val && *val) {
        auto cap = std::strtoull(val, nullptr, 10);
        if (cap > 0) log_buffer_capacity = static_cast<std::size_t>(cap);
    }

    // 3. 인스턴스 레지스트리 설정
    if (const char* val = std::getenv("SERVER_ADVERTISE_HOST"); val && *val) {
        advertise_host = val;
    }
    advertise_port = port; // 기본값은 리스닝 포트
    if (const char* val = std::getenv("SERVER_ADVERTISE_PORT"); val && *val) {
        try {
            advertise_port = static_cast<unsigned short>(std::stoul(val));
        } catch (...) {
            corelog::warn("Invalid SERVER_ADVERTISE_PORT value; using listen port");
        }
    }
    if (const char* val = std::getenv("SERVER_HEARTBEAT_INTERVAL"); val && *val) {
        try {
            auto parsed = std::stoul(val);
            if (parsed > 0) registry_heartbeat_interval = std::chrono::seconds{static_cast<long long>(parsed)};
        } catch (...) {}
    }
    if (const char* val = std::getenv("SERVER_REGISTRY_PREFIX"); val && *val) {
        registry_prefix = val;
    }
    if (!registry_prefix.empty() && registry_prefix.back() != '/') {
        registry_prefix.push_back('/');
    }
    if (const char* val = std::getenv("SERVER_REGISTRY_TTL"); val && *val) {
        try {
            auto parsed = std::stoul(val);
            if (parsed > 0) registry_ttl = std::chrono::seconds{static_cast<long long>(parsed)};
        } catch (...) {}
    }
    if (const char* val = std::getenv("SERVER_INSTANCE_ID"); val && *val) {
        server_instance_id = val;
    } else {
        server_instance_id = "server-" + std::to_string(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    // 4. DB 설정
    if (const char* val = std::getenv("DB_URI"); val && *val) db_uri = val;
    if (const char* val = std::getenv("DB_POOL_MIN"); val && *val) db_pool_min = static_cast<std::size_t>(std::strtoul(val, nullptr, 10));
    if (const char* val = std::getenv("DB_POOL_MAX"); val && *val) db_pool_max = static_cast<std::size_t>(std::strtoul(val, nullptr, 10));
    if (const char* val = std::getenv("DB_CONN_TIMEOUT_MS"); val && *val) db_conn_timeout_ms = static_cast<std::uint32_t>(std::strtoul(val, nullptr, 10));
    if (const char* val = std::getenv("DB_QUERY_TIMEOUT_MS"); val && *val) db_query_timeout_ms = static_cast<std::uint32_t>(std::strtoul(val, nullptr, 10));
    if (const char* val = std::getenv("DB_PREPARE_STATEMENTS"); val && *val) db_prepare_statements = (std::strcmp(val, "0") != 0);
    if (const char* val = std::getenv("DB_WORKER_THREADS"); val && *val) db_worker_threads = static_cast<std::size_t>(std::strtoul(val, nullptr, 10));

    // 5. Redis 설정
    if (const char* val = std::getenv("REDIS_URI"); val && *val) redis_uri = val;
    if (const char* val = std::getenv("REDIS_POOL_MAX"); val && *val) redis_pool_max = static_cast<std::size_t>(std::strtoul(val, nullptr, 10));
    if (const char* val = std::getenv("REDIS_USE_STREAMS"); val && *val) redis_use_streams = (std::strcmp(val, "0") != 0);
    if (const char* val = std::getenv("PRESENCE_CLEAN_ON_START"); val && *val) presence_clean_on_start = (std::strcmp(val, "0") != 0);
    if (const char* val = std::getenv("REDIS_CHANNEL_PREFIX"); val && *val) redis_channel_prefix = val;
    if (const char* val = std::getenv("USE_REDIS_PUBSUB"); val && *val) use_redis_pubsub = (std::strcmp(val, "0") != 0);
    if (const char* val = std::getenv("GATEWAY_ID"); val && *val) gateway_id = val;

    // 6. 메트릭 설정
    if (const char* val = std::getenv("METRICS_PORT"); val && *val) {
        unsigned long v = std::strtoul(val, nullptr, 10);
        if (v > 0 && v < 65536) metrics_port = static_cast<unsigned short>(v);
    }

    return true;
}

} // namespace server::app
