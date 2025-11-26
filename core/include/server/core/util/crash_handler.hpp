#pragma once

#include <cstddef>

namespace server::core::util::crash {

/**
 * @brief 크래시 핸들러를 설치합니다.
 * 
 * 프로그램이 비정상 종료(Segfault, Abort 등)될 때 스택 트레이스를 출력하도록
 * 시그널 핸들러를 등록합니다. 디버깅에 유용한 정보를 남기기 위함입니다.
 */
void install();

} // namespace server::core::util::crash

