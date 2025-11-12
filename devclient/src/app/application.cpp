#include "client/app/application.hpp"

#include "client/app/app_state.hpp"
#include "client/app/command_processor.hpp"
#include "client/app/network_router.hpp"
#include "client/app/ui_builder.hpp"
#include "client/net_client.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "server/core/config/dotenv.hpp"
#include "server/core/util/paths.hpp"

namespace client::app {

namespace {
constexpr const char* kDefaultHost = "127.0.0.1";
constexpr unsigned short kDefaultPort = 6000;
constexpr const char* kEnvHost = "DEVCLIENT_HOST";
constexpr const char* kEnvPort = "DEVCLIENT_PORT";
} // namespace

// UI 구성 요소, 명령 처리기, 네트워크 클라이언트를 묶어 운영하는 내부 구현체.
class Application::Impl {
public:
    Impl(std::string host, unsigned short port, bool allow_env_override);
    ~Impl() = default;

    int Run();

private:
    void AppendInitialLogs();
    void Connect();
    void LoadEnvironment();
    static unsigned short ParsePort(const char* value, unsigned short fallback);

    ftxui::ScreenInteractive screen_;
    AppState state_;
    ::NetClient net_;
    std::function<void()> request_refresh_;
    std::function<void(const std::string&)> log_sink_;
    CommandProcessor commands_;
    UiBuilder builder_;
    NetworkRouter router_;
    std::string host_;
    unsigned short port_;
    bool allow_env_override_{true};
};

Application::Impl::Impl(std::string host, unsigned short port, bool allow_env_override)
    : screen_(ftxui::ScreenInteractive::Fullscreen()),
      request_refresh_([this] { screen_.PostEvent(ftxui::Event::Custom); }),
      log_sink_([this](const std::string& message) {
          state_.append_log(message);
          request_refresh_();
      }),
      commands_(state_, net_, log_sink_),
      builder_(state_, commands_, net_, request_refresh_),
      router_(state_, net_, screen_, request_refresh_, log_sink_),
      host_(std::move(host)),
      port_(port),
      allow_env_override_(allow_env_override) {}

int Application::Impl::Run() {
    LoadEnvironment();
    state_.set_preview_room(state_.current_room());
    router_.Initialize();

    auto ui = builder_.Build();
    auto app = CatchEvent(ui.root, [this, input = ui.input](ftxui::Event event) {
        if (event == ftxui::Event::Escape || event == ftxui::Event::CtrlC) {
            if (state_.connected()) {
                net_.send_leave(state_.current_room());
            }
            screen_.ExitLoopClosure()();
            return true;
        }
        if (event == ftxui::Event::F1) {
            state_.toggle_help();
            request_refresh_();
            return true;
        }
        // F5: 현재 방의 스냅샷을 다시 요청한다.
        if (event == ftxui::Event::F5) {
            if (state_.connected()) {
                net_.send_refresh(state_.current_room());
            } else {
                log_sink_("연결되어 있지 않습니다.");
            }
            return true;
        }
        if (event == ftxui::Event::ArrowLeft) {
            state_.set_left_panel_width(state_.left_panel_width() - 1);
            request_refresh_();
            return true;
        }
        if (event == ftxui::Event::ArrowRight) {
            state_.set_left_panel_width(state_.left_panel_width() + 1);
            request_refresh_();
            return true;
        }
        // Enter 키 입력 시 명령어/채팅을 구분해 처리한다.
        if (event == ftxui::Event::Return && input->Focused()) {
            const std::string line = state_.input_buffer();
            state_.input_buffer().clear();
            if (!line.empty()) {
                if (!commands_.Process(line)) {
                    if (state_.username() == AppState::kDefaultUser) {
                        log_sink_("[warn] 게스트는 채팅을 보낼 수 없습니다. /login 으로 로그인하세요.");
                    } else {
                        net_.send_chat(state_.current_room(), line);
                    }
                }
            }
            request_refresh_();
            return true;
        }
        return false;
    });

    AppendInitialLogs();
    Connect();

    ui.input->TakeFocus();
    screen_.Loop(app);

    net_.close();
    return 0;
}

void Application::Impl::AppendInitialLogs() {
    // UI 시작 시 기본 안내 메시지를 출력한다.
    log_sink_("[system] FTXUI 클라이언트를 시작했습니다.");
    log_sink_("[hint] 명령어: /login <name>, /join <room> [password], /whisper <user> <message>, /leave, /refresh");
    log_sink_("[hint] 방이 잠겨 있으면 비밀번호가 필요합니다.");
    log_sink_("[hint] 입력 후 Enter 키를 누르세요.");
}

void Application::Impl::Connect() {
    // 현재 host/port로 TCP 연결을 시도하고 실패 시 로그만 남긴다.
    if (net_.connect(host_, port_)) {
        state_.set_connected(true);
        log_sink_(std::string("연결됨: ") + host_ + ":" + std::to_string(port_));
        net_.send_login(state_.username(), "");
    } else {
        state_.set_connected(false);
        log_sink_("연결에 실패했습니다.");
    }
}

void Application::Impl::LoadEnvironment() {
    // 실행 파일 위치와 repo 루트 순으로 .env를 찾고, DEVCLIENT_HOST/PORT를 덮어쓴다.
    namespace paths = server::core::util::paths;
    bool loaded = false;
    try {
        auto exe_dir = paths::executable_dir();
        auto exe_env = exe_dir / ".env";
        if (std::filesystem::exists(exe_env)) {
            loaded = server::core::config::load_dotenv(exe_env.string(), true);
        }
    } catch (const std::exception& ex) {
        log_sink_(std::string("[warn] .env 로드 실패: ") + ex.what());
    }

    if (!loaded) {
        std::filesystem::path repo_env{".env"};
        if (std::filesystem::exists(repo_env)) {
            loaded = server::core::config::load_dotenv(repo_env.string(), true);
        }
    }

    if (allow_env_override_) {
        if (const char* host_env = std::getenv(kEnvHost); host_env && *host_env) {
            host_ = host_env;
        }
        if (const char* port_env = std::getenv(kEnvPort); port_env && *port_env) {
            port_ = ParsePort(port_env, kDefaultPort);
        }
    }
}

unsigned short Application::Impl::ParsePort(const char* value, unsigned short fallback) {
    if (!value || !*value) {
        return fallback;
    }
    try {
        auto parsed = std::stoul(value);
        if (parsed > 0 && parsed <= 65535) {
            return static_cast<unsigned short>(parsed);
        }
    } catch (...) {
    }
    return fallback;
}

Application::Application(std::string host, unsigned short port, bool allow_env_override)
    : impl_(std::make_unique<Impl>(std::move(host), port, allow_env_override)) {}

Application::Application()
    : Application(kDefaultHost, kDefaultPort, true) {}

Application::~Application() = default;

int Application::Run() {
    return impl_->Run();
}

} // namespace client::app

