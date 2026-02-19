#pragma once

#include <cstdint>
#include <string_view>

namespace server::core::api {

inline constexpr std::uint32_t k_version_major = 1;
inline constexpr std::uint32_t k_version_minor = 0;
inline constexpr std::uint32_t k_version_patch = 0;

inline constexpr std::string_view version_string() noexcept { return "1.0.0"; }

} // namespace server::core::api
