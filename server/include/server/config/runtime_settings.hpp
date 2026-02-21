#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace server::config {

/** @brief Supported runtime setting keys in persistent configuration. */
enum class RuntimeSettingKey {
    kPresenceTtlSec,
    kRecentHistoryLimit,
    kRoomRecentMaxlen,
};

/** @brief Validation bounds for a runtime setting key. */
struct RuntimeSettingRule {
    RuntimeSettingKey key_id;     ///< Enum identifier used by code.
    std::string_view key_name;    ///< Persistent key name in storage.
    std::uint32_t min_value;      ///< Inclusive minimum allowed value.
    std::uint32_t max_value;      ///< Inclusive maximum allowed value.
};

/** @brief Compile-time registry of valid runtime setting rules. */
inline constexpr std::array<RuntimeSettingRule, 3> kRuntimeSettingRules{{
    {RuntimeSettingKey::kPresenceTtlSec, "presence_ttl_sec", 5, 3600},
    {RuntimeSettingKey::kRecentHistoryLimit, "recent_history_limit", 5, 2000},
    {RuntimeSettingKey::kRoomRecentMaxlen, "room_recent_maxlen", 5, 5000},
}};

/**
 * @brief Finds a runtime setting rule by its persistent key name.
 * @param key Runtime setting key string.
 * @return Pointer to matching rule, or `nullptr` if not found.
 */
inline constexpr const RuntimeSettingRule* find_runtime_setting_rule(std::string_view key) {
    for (const auto& rule : kRuntimeSettingRules) {
        if (rule.key_name == key) {
            return &rule;
        }
    }
    return nullptr;
}

} // namespace server::config
