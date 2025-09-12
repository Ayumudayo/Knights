#pragma once

#include <ftxui/dom/elements.hpp>
#include <string>
#include "client/app/state.hpp"

namespace client::ui {

// 상태바 DOM을 구성하는 함수. 반응형 레이아웃을 적용한다.
ftxui::Element RenderStatusBar(const client::app::State& st);

} // namespace client::ui

