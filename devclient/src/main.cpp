#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <cstring>
#include <span>
#include <atomic>

#include <boost/asio.hpp>
#include <clocale>
#if defined(_WIN32)
#  include <windows.h>
#endif

#include "server/core/protocol/frame.hpp"
#include "server/core/protocol.hpp"

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
namespace proto = server::core::protocol;

static void send_frame_simple(asio::ip::tcp::socket& sock, std::uint16_t msg_id, std::uint16_t flags, const std::vector<std::uint8_t>& payload, std::uint32_t& tx_seq) {
    // 헤더 인코딩
    proto::FrameHeader h{};
    h.length = static_cast<std::uint16_t>(payload.size());
    h.msg_id = msg_id;
    h.flags  = flags;
    h.seq    = tx_seq++;
    auto now64 = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    h.utc_ts_ms32 = static_cast<std::uint32_t>(now64 & 0xFFFFFFFFu);

    std::vector<std::uint8_t> buf;
    buf.resize(proto::k_header_bytes + payload.size());
    proto::encode_header(h, buf.data());
    if (!payload.empty()) std::memcpy(buf.data() + proto::k_header_bytes, payload.data(), payload.size());
    asio::write(sock, asio::buffer(buf));
}

static bool recv_one(asio::ip::tcp::socket& sock) {
    std::vector<std::uint8_t> hdr;
    hdr.resize(proto::k_header_bytes);
    asio::read(sock, asio::buffer(hdr));
    proto::FrameHeader h{};
    proto::decode_header(hdr.data(), h);
    std::vector<std::uint8_t> body;
    body.resize(h.length);
    if (h.length) asio::read(sock, asio::buffer(body));

    std::cout << "<recv> msg_id=0x" << std::hex << h.msg_id << std::dec
              << " len=" << h.length << " seq=" << h.seq
              << " ts_ms32=" << h.utc_ts_ms32 << std::endl;

    if (h.msg_id == proto::MSG_CHAT_BROADCAST) {
        std::span<const std::uint8_t> sp(body.data(), body.size());
        std::string room, sender, text;
        if (!proto::read_lp_utf8(sp, room)) room = "";
        if (!proto::read_lp_utf8(sp, sender)) sender = "";
        if (!proto::read_lp_utf8(sp, text)) text = std::string(reinterpret_cast<const char*>(body.data()), body.size());
        std::cout << "[" << room << "] " << sender << ": " << text << std::endl;
    } else if (h.msg_id == proto::MSG_ERR) {
        if (body.size() >= 4) {
            std::uint16_t code = proto::read_be16(body.data());
            std::uint16_t len  = proto::read_be16(body.data() + 2);
            std::string msg;
            if (body.size() >= 4 + len) {
                msg.assign(reinterpret_cast<const char*>(body.data() + 4), len);
            }
            const char* name = "UNKNOWN";
            switch (code) {
                case 0x0002: name = "LENGTH_LIMIT_EXCEEDED"; break;
                case 0x0003: name = "UNKNOWN_MSG_ID"; break;
                case 0x0101: name = "UNAUTHORIZED"; break;
                case 0x0104: name = "NO_ROOM"; break;
                case 0x0105: name = "NOT_MEMBER"; break;
                case 0x0106: name = "ROOM_MISMATCH"; break;
            }
            std::cout << "[ERROR] " << name << "(0x" << std::hex << code << std::dec << ") msg=" << msg << std::endl;
        } else {
            std::cout << "[ERROR] (payload malformed)" << std::endl;
        }
    }
    return true;
}

int main(int argc, char** argv) {
    try {
#if defined(_WIN32)
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
        std::setlocale(LC_ALL, ".UTF-8");
#endif
        if (argc < 3) {
            std::cerr << "사용법: dev_chat_cli <host> <port>" << std::endl;
            return 1;
        }
        std::string host = argv[1];
        unsigned short port = static_cast<unsigned short>(std::stoi(argv[2]));

        asio::io_context io;
        tcp::resolver resolver(io);
        auto endpoints = resolver.resolve(host, std::to_string(port));
        tcp::socket sock(io);
        asio::connect(sock, endpoints);
        sock.set_option(tcp::no_delay(true));
        std::cout << "연결됨: " << host << ":" << port << std::endl;

        // 서버의 HELLO 수신
        recv_one(sock);

        std::string current_room = "lobby";
        std::string username = "guest";

        std::uint32_t seq = 1;
        // 백그라운드 수신 쓰레드 시작
        std::atomic<bool> running{true};
        std::thread rx([&](){
            try {
                while (running.load()) {
                    if (!recv_one(sock)) break;
                }
            } catch (const std::exception& ex) {
                if (running.load()) {
                    std::cerr << "수신 오류: " << ex.what() << std::endl;
                }
            }
        });

        // 주기적 PING 스레드 시작(서버 read 타임아웃 방지)
        std::thread tx_ping([&](){
            try {
                while (running.load()) {
                    std::this_thread::sleep_for(std::chrono::seconds(8));
                    if (!running.load()) break;
                    std::vector<std::uint8_t> payload;
                    send_frame_simple(sock, proto::MSG_PING, 0, payload, seq);
                }
            } catch (...) {
            }
        });

        std::cout << "명령: /login <user>, /join <room>, /leave [room], /rooms, /who [room], /say <text>, 빈 줄 종료" << std::endl;
        std::string line;
        while (true) {
            std::getline(std::cin, line);
            if (!std::cin.good() || line.empty()) break;
            if (!line.empty() && line[0] == '/') {
                // 슬래시 명령 처리
                if (line.rfind("/login ", 0) == 0) {
                    username = line.substr(7);
                    std::vector<std::uint8_t> payload;
                    proto::write_lp_utf8(payload, username);
                    proto::write_lp_utf8(payload, std::string()); // token 비움
                    send_frame_simple(sock, proto::MSG_LOGIN_REQ, 0, payload, seq);
                } else if (line.rfind("/join ", 0) == 0) {
                    current_room = line.substr(6);
                    std::vector<std::uint8_t> payload;
                    proto::write_lp_utf8(payload, current_room);
                    send_frame_simple(sock, proto::MSG_JOIN_ROOM, 0, payload, seq);
                } else if (line.rfind("/leave", 0) == 0) {
                    std::string room = current_room;
                    if (line.size() > 6) {
                        auto pos = line.find_first_not_of(' ', 6);
                        if (pos != std::string::npos) room = line.substr(pos);
                    }
                    std::vector<std::uint8_t> payload;
                    proto::write_lp_utf8(payload, room);
                    send_frame_simple(sock, proto::MSG_LEAVE_ROOM, 0, payload, seq);
                } else if (line == "/rooms") {
                    // 서버가 CHAT_SEND 내 슬래시 명령으로 처리
                    std::vector<std::uint8_t> payload;
                    proto::write_lp_utf8(payload, current_room);
                    proto::write_lp_utf8(payload, std::string("/rooms"));
                    send_frame_simple(sock, proto::MSG_CHAT_SEND, 0, payload, seq);
                } else if (line.rfind("/who", 0) == 0) {
                    std::string what;
                    if (line.size() > 4) {
                        auto pos = line.find_first_not_of(' ', 4);
                        if (pos != std::string::npos) what = line.substr(pos);
                    }
                    std::vector<std::uint8_t> payload;
                    proto::write_lp_utf8(payload, current_room);
                    proto::write_lp_utf8(payload, what.empty()?std::string("/who"):std::string("/who ")+what);
                    send_frame_simple(sock, proto::MSG_CHAT_SEND, 0, payload, seq);
                } else if (line.rfind("/say ", 0) == 0) {
                    std::string text = line.substr(5);
                    std::vector<std::uint8_t> payload;
                    proto::write_lp_utf8(payload, current_room);
                    proto::write_lp_utf8(payload, text);
                    send_frame_simple(sock, proto::MSG_CHAT_SEND, 0, payload, seq);
                } else {
                    std::cout << "알 수 없는 명령" << std::endl;
                }
            } else {
                // 일반 텍스트는 /say 취급
                std::vector<std::uint8_t> payload;
                proto::write_lp_utf8(payload, current_room);
                proto::write_lp_utf8(payload, line);
                send_frame_simple(sock, proto::MSG_CHAT_SEND, 0, payload, seq);
            }
        }
        running.store(false);
        try { sock.close(); } catch (...) {}
        if (rx.joinable()) rx.join();
        if (tx_ping.joinable()) tx_ping.join();
        std::cout << "종료" << std::endl;
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "오류: " << ex.what() << std::endl;
        return 1;
    }
}
