// UTF-8 인코딩, 한국어 주석
// Knights: FTXUI 기반 최소 클라이언트(처음부터 다시 시작)
// - 레이아웃: 상단 상태바 / 좌측(방/유저) / 우측(로그) / 하단 입력
// - Enter로 입력 전송(로그에 추가), q/ESC/Ctrl+C로 종료

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <algorithm>
#include <string>
#include <vector>

using namespace ftxui;

int main() {
    auto screen = ScreenInteractive::Fullscreen();

    // 상태(임시: 네트워크 미연결, UI만 구성)
    bool connected = false;
    std::vector<std::string> rooms = {"lobby", "games", "random"};
    int rooms_selected = 0;
    std::vector<std::string> users = {"(미접속)"};
    int users_selected = 0;
    std::vector<std::string> logs;
    std::string input;
    int left_width = 26; // 좌측 패널 너비(←/→ 키로 조절)

    auto append_log = [&](std::string s) {
        if (!s.empty() && s.back() == '\n') s.pop_back();
        logs.emplace_back(std::move(s));
        const size_t kMaxLogs = 1000;
        if (logs.size() > kMaxLogs) {
            logs.erase(logs.begin(), logs.begin() + (logs.size() - kMaxLogs));
        }
    };

    // 메뉴 옵션: 방 이동 시 유저 목록 더미 갱신
    MenuOption rooms_opt;
    rooms_opt.on_change = [&] {
        users.clear();
        if (rooms_selected == 0) users = {"system", "guest", "you"};
        if (rooms_selected == 1) users = {"knight", "archer"};
        if (rooms_selected == 2) users = {"foo", "bar", "baz"};
    };

    // 컴포넌트 구성
    auto rooms_menu = Menu(&rooms, &rooms_selected, rooms_opt);
    auto users_menu = Menu(&users, &users_selected);

    auto left_container = Container::Vertical({
        rooms_menu,
        users_menu,
    });

    auto input_comp = Input(&input, "메시지 입력...");
    auto send_btn = Button("보내기", [&] {
        if (!input.empty()) {
            append_log("[me] " + input);
            input.clear();
        }
    });
    auto input_row = Container::Horizontal({ input_comp, send_btn });

    // 우측(로그) 렌더러: DOM만, 포커스 없음
    auto right_renderer = Renderer([&] {
        Elements lines;
        lines.reserve(logs.size());
        for (auto& l : logs) {
            lines.push_back(paragraph(l));
        }
        auto header = hbox({
            text(" 방: ") | dim, text(rooms[rooms_selected]) | bold,
            text("  "),
            text("유저: ") | dim, text(std::to_string((int)users.size())),
            filler(),
            text(connected ? "연결됨" : "미연결") | color(connected ? Color::Green : Color::Red),
        });

        return vbox({ header, separator(), vbox(std::move(lines)) | vscroll_indicator | yframe }) | flex;
    });

    // 좌측 렌더러
    auto left_renderer = Renderer(left_container, [&] {
        auto r = window(text("방"), rooms_menu->Render() | vscroll_indicator | yframe);
        auto u = window(text("유저"), users_menu->Render() | vscroll_indicator | yframe);
        return vbox({ r | flex, separator(), u | flex }) | size(WIDTH, EQUAL, left_width);
    });

    // 입력 행 렌더러
    auto input_renderer = Renderer(input_row, [&] {
        return hbox({ text("> "), input_comp->Render() | flex, text(" "), send_btn->Render() });
    });

    // 상단 상태바
    auto status_bar = [&] {
        return hbox({
            text(" Knights Client ") | bold,
            separator(),
            text("상태: "), text(connected ? "연결됨" : "미연결") | color(connected ? Color::Green : Color::Red),
            filler(),
            text(" q/ESC: 종료  ←/→: 폭 조절  Enter: 보내기 ") | dim,
        }) | bgcolor(Color::GrayDark) | color(Color::White);
    };

    // 레이아웃 전체 렌더러(포커스 이동을 위해 빈 우측 더미 포함)
    auto right_dummy = Renderer([] { return text(""); });
    auto top_container = Container::Horizontal({ left_container, right_dummy });
    auto main_container = Container::Vertical({ top_container, input_row });

    auto app = Renderer(main_container, [&] {
        auto content = hbox({ left_renderer->Render(), separator(), right_renderer->Render() | flex });
        return vbox({ status_bar(), separator(), content | flex, separator(), input_renderer->Render() });
    });

    // 키 이벤트: 종료/Enter/좌우 폭 조절
    auto app_with_events = CatchEvent(app, [&](Event e) {
        if (e == Event::Character('q') || e == Event::Escape || e == Event::CtrlC) {
            screen.ExitLoopClosure()();
            return true;
        }
        if (e == Event::Return) {
            if (input_comp->Focused()) {
                if (!input.empty()) {
                    append_log("[me] " + input);
                    input.clear();
                }
                return true;
            }
        }
        if (e == Event::ArrowLeft) {
            left_width = std::max(16, left_width - 1);
            return true;
        }
        if (e == Event::ArrowRight) {
            left_width = std::min(60, left_width + 1);
            return true;
        }
        return false;
    });

    // 초기 로그 몇 줄
    append_log("[system] FTXUI 클라이언트가 시작되었습니다.");
    append_log("[system] 아직 서버 연결 로직은 없습니다.");
    append_log("[hint] Enter로 입력을 로그에 추가합니다.");

    screen.Loop(app_with_events);
    return 0;
}
