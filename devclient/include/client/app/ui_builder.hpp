#pragma once

class NetClient;

#include <functional>
#include <memory>

#include <ftxui/component/component.hpp>

namespace client::app {

class AppState;
class CommandProcessor;

// FTXUI 라이브러리를 사용하여 터미널 UI 컴포넌트를 조립합니다.
// 좌측 사이드바(방/유저 목록), 우측 채팅창, 하단 입력창 등의 레이아웃을 정의합니다.
class UiBuilder {
public:
    using RefreshRequest = std::function<void()>;

    UiBuilder(AppState& state,
              CommandProcessor& commands,
              ::NetClient& net,
              RefreshRequest request_refresh);

    struct UiComponents {
        ftxui::Component root;
        ftxui::Component input;
    };

    UiComponents Build();

private:
    ftxui::Component BuildLeftPane();
    ftxui::Component BuildRightPane();
    ftxui::Component BuildInputBox();
    ftxui::Element BuildHelpOverlay();

    AppState& state_;
    CommandProcessor& commands_;
    ::NetClient& net_;
    RefreshRequest request_refresh_;

    ftxui::Component rooms_menu_;
    ftxui::Component users_menu_;
    ftxui::Component log_menu_;
    ftxui::Component input_box_;
};

} // namespace client::app

