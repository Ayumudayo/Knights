#include "client/ui/status_bar.hpp"

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>

using namespace ftxui;

namespace client::ui {

Element RenderStatusBar(const client::app::State& st) {
    auto sz = Terminal::Size();
    int w = sz.dimx;
    bool wide = w >= 110;
    bool mid  = w >= 90;
    bool sml  = w >= 70;

    Elements parts;
    parts.push_back(text(wide ? " Knights Client " : (mid ? " Knights " : " KC ")) | bold);
    parts.push_back(separator());

    auto led = text(st.connected.load() ? "●" : "○") | color(st.connected.load() ? Color::Green : Color::Red);
    if (sml) {
        parts.push_back(text(" 상태:"));
        parts.push_back(text(st.connected.load() ? "연결됨" : "미연결") | color(st.connected.load() ? Color::Green : Color::Red));
    } else {
        parts.push_back(led);
    }

    if (mid) {
        parts.push_back(text("  사용자:"));
        auto user_elem = text(st.username);
        if (st.username == "guest") user_elem = user_elem | color(Color::Yellow);
        else user_elem = user_elem | bold;
        parts.push_back(user_elem);
        parts.push_back(text("  방:"));
        parts.push_back(text(st.current_room) | bold);
    } else if (sml) {
        parts.push_back(text("  "));
        auto combo = st.username + "@" + st.current_room;
        auto combo_elem = text(combo);
        if (st.username == "guest") combo_elem = combo_elem | color(Color::Yellow);
        else combo_elem = combo_elem | bold;
        parts.push_back(combo_elem);
    }

    parts.push_back(filler());
    if (wide) {
        parts.push_back(text(" F1:Help  F2:Rooms  F3:Who  F4:Login  F5:Join  F6:Refresh  ESC: 종료  ←/→: 폭 조절  Enter: 보내기 ") | dim);
    } else if (mid) {
        parts.push_back(text(" F1 Help  F2 Rooms  F3 Who  F4 Login  F5 Join  F6 Refresh ") | dim);
    } else if (sml) {
        parts.push_back(text(" F1 Help  F6 Refresh ") | dim);
    }

    return hbox(std::move(parts)) | bgcolor(Color::GrayDark) | color(Color::White);
}

} // namespace client::ui

