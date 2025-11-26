#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace server::core::log {

/**
 * @brief 로그 레벨 정의
 * trace: 가장 상세한 로그 (디버깅용)
 * debug: 개발 중 확인용 정보
 * info: 일반적인 정보 (시작/종료, 주요 이벤트)
 * warn: 경고 (잠재적 문제)
 * error: 에러 (기능 실패)
 */
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

