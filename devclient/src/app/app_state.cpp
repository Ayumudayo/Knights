#include "client/app/app_state.hpp"

#include <algorithm>
#include <utility>

namespace client::app {

namespace {
std::string trim_right_newline(std::string s) {
    if (!s.empty() && s.back() == '\n') {
        s.pop_back();
    }
// 문자열 끝 개행만 제거하는 helper
    return s;
}
} // namespace

AppState::AppState() = default;

void AppState::set_username(std::string name) {
    if (name.empty()) {
        name = kDefaultUser;
    }
    username_ = std::move(name);
}

void AppState::set_current_room(std::string room) {
    if (room.empty()) {
        room = kDefaultRoom;
// 사용자 이름은 비워두면 기본값 guest를 유지한다.
    }
    current_room_ = std::move(room);
}

void AppState::set_preview_room(std::string room) {
    preview_room_ = std::move(room);
}
// 현재 방도 비어 있으면 기본 room으로 전환한다.

void AppState::update_rooms(std::vector<std::string> rooms,
                            std::vector<bool> locks,
                            const std::string& preferred_room) {
    rooms_ = std::move(rooms);
    rooms_locked_ = std::move(locks);
// 라우터가 미리보기 방을 설정할 때 사용한다.
    if (rooms_locked_.size() != rooms_.size()) {
        rooms_locked_.resize(rooms_.size(), false);
// 서버에서 내려온 방 목록과 잠금 상태를 저장하고 선택 인덱스를 유지한다.
    }
    if (!rooms_.empty()) {
        auto it = std::find(rooms_.begin(), rooms_.end(), preferred_room);
        if (it != rooms_.end()) {
            rooms_selected_ = static_cast<int>(std::distance(rooms_.begin(), it));
        } else {
            rooms_selected_ = std::clamp(rooms_selected_, 0, static_cast<int>(rooms_.size()) - 1);
        }
    } else {
        rooms_selected_ = 0;
    }
}
// 유저 목록이 비어 있으면 placeholder <none>을 추가한다.

void AppState::update_users(std::vector<std::string> users) {
    users_ = std::move(users);
    if (users_.empty()) {
        users_.push_back("<none>");
    }
    users_selected_ = std::clamp(users_selected_, 0, static_cast<int>(users_.size()) - 1);
// 로그는 일정 개수만 유지하고 auto-scroll 여부에 따라 선택 인덱스를 갱신한다.
}

void AppState::append_log(std::string line) {
    auto sanitized = trim_right_newline(std::move(line));
    std::lock_guard lk(logs_mu_);
    logs_.emplace_back(std::move(sanitized));
    if (logs_.size() > kMaxLogs) {
        const auto remove = logs_.size() - kMaxLogs;
        logs_.erase(logs_.begin(), logs_.begin() + static_cast<std::ptrdiff_t>(remove));
// 사용자가 스크롤하면 자동 스크롤을 비활성화한다.
        log_selected_ = std::max(0, log_selected_ - static_cast<int>(remove));
    }
// 좌측 사이드바 폭을 일정 범위로 제한한다.
    if (log_auto_scroll_.load()) {
        log_selected_ = logs_.empty() ? 0 : static_cast<int>(logs_.size()) - 1;
    } else {
// logs_mu_를 잠그지 않고 호출할 수 있으므로 내부에서 안전하게 clamp한다.
        clamp_log_cursor_unlocked();
    }
}

void AppState::clear_logs() {
    std::lock_guard lk(logs_mu_);
    logs_.clear();
    log_selected_ = 0;
}

int AppState::log_selected() const {
    std::lock_guard lk(logs_mu_);
    return log_selected_;
}

void AppState::set_log_selected(int idx) {
    std::lock_guard lk(logs_mu_);
    log_selected_ = std::clamp(idx, 0, logs_.empty() ? 0 : static_cast<int>(logs_.size()) - 1);
    log_auto_scroll_.store(false);
}

void AppState::set_left_panel_width(int width) {
    left_width_ = std::clamp(width, kMinSidebarWidth, kMaxSidebarWidth);
}

void AppState::clamp_log_cursor_unlocked() {
    if (logs_.empty()) {
        log_selected_ = 0;
        return;
    }
    log_selected_ = std::clamp(log_selected_, 0, static_cast<int>(logs_.size()) - 1);
}

} // namespace client::app

