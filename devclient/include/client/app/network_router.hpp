#pragma once

#include <functional>
#include <string>
#include <vector>

namespace ftxui {
class ScreenInteractive;
}

class NetClient;

namespace client::app {

class AppState;

// 네트워크로부터 수신된 메시지를 적절한 UI 업데이트 로직으로 라우팅합니다.
// NetClient의 콜백을 등록하여, 서버 메시지(채팅, 방 목록 갱신 등)가 오면 AppState를 변경하고 UI를 갱신합니다.
class NetworkRouter {
public:
    using LogSink = std::function<void(const std::string&)>;
    using RefreshRequest = std::function<void()>;

    NetworkRouter(AppState& state,
                  ::NetClient& net,
                  ftxui::ScreenInteractive& screen,
                  RefreshRequest request_refresh,
                  LogSink log_sink);

    void Initialize();

private:
    void HandleSystemRoomsBroadcast(const std::string& payload);
    void HandleSystemRoomNotice(const std::string& room, const std::string& text);

    AppState& state_;
    ::NetClient& net_;
    ftxui::ScreenInteractive& screen_;
    RefreshRequest request_refresh_;
    LogSink log_sink_;
};

} // namespace client::app

