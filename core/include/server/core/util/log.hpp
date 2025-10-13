#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace server::core::log {

enum class level { trace, debug, info, warn, error };

void set_level(level lv);
void set_buffer_capacity(std::size_t capacity);
std::vector<std::string> recent(std::size_t limit = 128);
void trace(const std::string& msg);
void debug(const std::string& msg);
void info(const std::string& msg);
void warn(const std::string& msg);
void error(const std::string& msg);

} // namespace server::core::log

