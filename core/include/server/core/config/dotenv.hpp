#pragma once

#include <string>

namespace server::core::config {

// 간단한 .env 로더
// - 라인별 KEY=VALUE 파싱, '#' 주석/공백 허용
// - 양쪽 공백 트리밍, 값의 양끝 큰따옴표/작은따옴표 제거
// - override=false면 기존 환경변수 보존
// - 성공적으로 파일을 열면 true 반환(없어도 false지만 오류는 아님)
bool load_dotenv(const std::string& path = ".env", bool override_existing = false);

} // namespace server::core::config

