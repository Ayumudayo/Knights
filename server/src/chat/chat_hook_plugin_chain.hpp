#pragma once

#include "chat_hook_plugin_manager.hpp"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace server::app::chat {

class ChatHookPluginChain {
public:
    struct Config {
        // If set, these plugins are loaded in-order (no sorting).
        std::vector<std::filesystem::path> plugin_paths;

        // If set (and plugin_paths is empty), load all modules in this directory,
        // ordered lexicographically by filename.
        std::optional<std::filesystem::path> plugins_dir;

        // Shared cache directory for all plugins.
        std::filesystem::path cache_dir;

        // Optional lock/sentinel override for single-plugin mode.
        std::optional<std::filesystem::path> single_lock_path;
    };

    struct Outcome {
        bool stop_default{false};
        std::vector<std::string> notices;
    };

    struct MetricsSnapshot {
        bool configured{false};
        std::string mode; // none|dir|paths|single
        std::vector<ChatHookPluginManager::MetricsSnapshot> plugins;
    };

    explicit ChatHookPluginChain(Config cfg);

    // Scans and hot-reloads any changed plugins.
    void poll_reload();

    // Applies all plugins in order.
    // May mutate text via kReplaceText decisions.
    Outcome on_chat_send(std::uint32_t session_id,
                         std::string_view room,
                         std::string_view user,
                         std::string& text) const;

    MetricsSnapshot metrics_snapshot() const;

private:
    using PluginList = std::vector<std::shared_ptr<ChatHookPluginManager>>;

    static std::string normalize_key(const std::filesystem::path& p);
    static std::string module_extension();
    bool get_desired_paths(std::vector<std::filesystem::path>& out) const;

    Config cfg_;
    mutable std::mutex mu_;
    std::unordered_map<std::string, std::shared_ptr<ChatHookPluginManager>> by_key_;
    std::atomic<std::shared_ptr<const PluginList>> ordered_{};
};

} // namespace server::app::chat
