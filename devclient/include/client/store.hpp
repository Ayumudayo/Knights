// UTF-8, 한국어 주석
#pragma once
#include <string>
#include <vector>
#include <deque>
#include <mutex>

struct ClientStore {
    mutable std::mutex mu;
    bool connected{false};
    std::string username{"guest"};
    std::string current_room{"lobby"};
    std::string preview_room; // 좌측 선택 미리보기
    std::vector<std::string> rooms;
    std::vector<std::string> users;
    std::deque<std::string> logs;
    std::uint32_t my_sid{0};
    bool cap_sender_sid{false};

    void append_log(std::string s) {
        if (!s.empty() && s.back() == '\n') s.pop_back();
        std::lock_guard<std::mutex> lk(mu);
        logs.emplace_back(std::move(s));
        const size_t cap = 1000; if (logs.size() > cap) logs.pop_front();
    }
};

