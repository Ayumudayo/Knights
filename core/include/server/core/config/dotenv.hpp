#pragma once

#include <string>

namespace server::core::config {

// 지정한 .env 파일을 읽어 환경 변수로 적용한다.
// - 각 줄은 KEY=VALUE 형태로 해석하며 '#' 으로 시작하는 주석과 공백 줄은 건너뛴다.
// - 값이 따옴표로 감싸져 있으면 양끝 따옴표를 제거한다.
// - override_existing 이 false 이면 이미 설정된 환경 변수는 덮어쓰지 않는다.
// - 파일을 성공적으로 처리하면 true, 그렇지 않으면 false 를 반환한다.
bool load_dotenv(const std::string& path = ".env", bool override_existing = false);

} // namespace server::core::config

