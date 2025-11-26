#pragma once

#include <ftxui/dom/elements.hpp>

#include "client/app/app_state.hpp"

namespace client::ui {

// 화면 최하단에 현재 연결 상태, 사용자 이름, 세션 ID 등을 표시하는 상태바를 렌더링합니다.
ftxui::Element RenderStatusBar(const client::app::AppState& state);

} // namespace client::ui

