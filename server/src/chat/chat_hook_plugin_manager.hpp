#pragma once

#include "server/chat/chat_hook_plugin_abi.hpp"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace server::app::chat {

class ChatHookPluginManager {
public:
    struct Config {
        std::filesystem::path plugin_path;
        std::filesystem::path cache_dir;
        std::optional<std::filesystem::path> lock_path;
    };

    struct Result {
        ChatHookDecisionV1 decision{ChatHookDecisionV1::kPass};
        std::string notice;
        std::string replacement_text;
    };

    struct MetricsSnapshot {
        std::filesystem::path plugin_path;
        bool loaded{false};
        std::string name;
        std::string version;
        std::uint64_t reload_attempt_total{0};
        std::uint64_t reload_success_total{0};
        std::uint64_t reload_failure_total{0};
    };

    explicit ChatHookPluginManager(Config cfg);

    // Checks plugin_path for changes and hot-reloads if needed.
    void poll_reload();

    // Calls the loaded plugin (if any). Does not throw.
    Result on_chat_send(std::uint32_t session_id,
                        std::string_view room,
                        std::string_view user,
                        std::string_view text) const;

    MetricsSnapshot metrics_snapshot() const;

private:
    struct LoadedPlugin;

    Config cfg_;
    std::atomic<std::shared_ptr<LoadedPlugin>> current_{};
    std::optional<std::filesystem::file_time_type> last_attempt_mtime_;
    mutable std::mutex reload_mu_;
    static std::atomic<std::uint64_t> g_cache_seq_;

    std::atomic<std::uint64_t> reload_attempt_total_{0};
    std::atomic<std::uint64_t> reload_success_total_{0};
    std::atomic<std::uint64_t> reload_failure_total_{0};
};

} // namespace server::app::chat
