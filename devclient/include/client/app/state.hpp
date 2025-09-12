#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace client { class NetClient; }

namespace client::app {

struct State {
    // connection
    std::atomic<bool> connected{false};
    std::uint32_t my_sid{0};
    bool cap_sender_sid{false};

    // UI/state
    std::vector<std::string> rooms;
    int rooms_selected{0};
    std::vector<std::string> users{ "(미접속)" };
    int users_selected{0};
    std::vector<std::string> logs;
    std::mutex logs_mu;
    std::string input;
    int left_width{26};
    std::string current_room{"lobby"};
    std::string preview_room;
    std::string username{"guest"};
    bool show_help{false};
    std::string pending_join_room;
};

} // namespace client::app

