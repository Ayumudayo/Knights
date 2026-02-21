#pragma once

#include <cstdint>
#include <string_view>

namespace server::core::api {

/** @brief Public API semantic version major component. */
inline constexpr std::uint32_t k_version_major = 1;
/** @brief Public API semantic version minor component. */
inline constexpr std::uint32_t k_version_minor = 3;
/** @brief Public API semantic version patch component. */
inline constexpr std::uint32_t k_version_patch = 0;

/**
 * @brief Returns semantic version text for the public API surface.
 * @return SemVer literal in `major.minor.patch` format.
 */
inline constexpr std::string_view version_string() noexcept { return "1.3.0"; }

} // namespace server::core::api
