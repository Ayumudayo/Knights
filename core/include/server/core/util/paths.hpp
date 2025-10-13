#pragma once

#include <filesystem>

namespace server::core::util::paths {

std::filesystem::path executable_path();
std::filesystem::path executable_dir();

} // namespace server::core::util::paths

