#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <span>
#include <csignal>

#include <boost/asio.hpp>
#include <clocale>
#if defined(_WIN32)
#  include <windows.h>
#endif

#include "server/core/net/acceptor.hpp"
#include "server/core/net/dispatcher.hpp"
#include "server/core/net/session.hpp"
#include "server/core/protocol/opcodes.hpp"
#include "server/core/protocol/protocol_flags.hpp"
#include "server/core/protocol/protocol_errors.hpp"
#include "server/core/util/log.hpp"
#include "server/core/config/options.hpp"
#include "server/core/state/shared_state.hpp"
#include "server/core/concurrent/job_queue.hpp"
#include "server/core/concurrent/thread_manager.hpp"
#include "server/core/memory/memory_pool.hpp"
#include "server/chat/chat_service.hpp"

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
namespace core = server::core;
namespace protocol = server::core::protocol;
namespace corelog = server::core::log;

int main(int argc, char** argv) {
    try {
#if defined(_WIN32)
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
        std::setlocale(LC_ALL, ".UTF-8");
#endif
        unsigned short port = 5000;
        if (argc >= 2) {
            port = static_cast<unsigned short>(std::stoi(argv[1]));
        }

        asio::io_context io;
        core::JobQueue job_queue;
        core::ThreadManager workers(job_queue);
        core::BufferManager buffer_manager(2048, 1024); // 2KB buffers, 1024 count

        core::Dispatcher dispatcher;
        auto options = std::make_shared<core::SessionOptions>();
        options->read_timeout_ms = 60'000;
        options->heartbeat_interval_ms = 10'000;
        auto state = std::make_shared<core::SharedState>();

        server::app::chat::ChatService chat(io, job_queue);

        dispatcher.register_handler(protocol::MSG_PING,
            [](core::Session& s, std::span<const std::uint8_t> payload) {
                std::vector<std::uint8_t> body(payload.begin(), payload.end());
                s.async_send(protocol::MSG_PONG, body, 0);
            });

        dispatcher.register_handler(protocol::MSG_LOGIN_REQ,
            [&chat](core::Session& s, std::span<const std::uint8_t> payload) { chat.on_login(s, payload); });

        dispatcher.register_handler(protocol::MSG_JOIN_ROOM,
            [&chat](core::Session& s, std::span<const std::uint8_t> payload) { chat.on_join(s, payload); });

        dispatcher.register_handler(protocol::MSG_CHAT_SEND,
            [&chat](core::Session& s, std::span<const std::uint8_t> payload) { chat.on_chat_send(s, payload); });

        dispatcher.register_handler(protocol::MSG_LEAVE_ROOM,
            [&chat](core::Session& s, std::span<const std::uint8_t> payload) { chat.on_leave(s, payload); });

        tcp::endpoint ep(tcp::v4(), port);
        auto acceptor = std::make_shared<core::Acceptor>(io, ep, dispatcher, buffer_manager, options, state,
            [&chat](std::shared_ptr<core::Session> sess){
                sess->set_on_close([&chat](std::shared_ptr<core::Session> s){ chat.on_session_close(s); });
            });
        acceptor->start();
        corelog::info("server_app 시작: 0.0.0.0:" + std::to_string(port));

        // 워커 스레드 풀 시작
        unsigned int num_worker_threads = std::max(1u, std::thread::hardware_concurrency());
        workers.Start(num_worker_threads);
        corelog::info(std::to_string(num_worker_threads) + "개의 워커 스레드를 시작합니다.");

        // I/O 스레드 풀 시작
        unsigned int num_io_threads = std::max(1u, std::thread::hardware_concurrency());
        std::vector<std::thread> io_threads;
        io_threads.reserve(num_io_threads);
        for (unsigned int i = 0; i < num_io_threads; ++i) {
            io_threads.emplace_back([&io]() { 
                try {
                    io.run();
                } catch (const std::exception& e) {
                    corelog::error(std::string("I/O 스레드 예외: ") + e.what());
                }
            });
        }
        corelog::info(std::to_string(num_io_threads) + "개의 I/O 스레드를 시작합니다.");

        // 정상 종료(Ctrl+C) 처리
        asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code&, int) {
            corelog::info("서버 종료 신호 수신...");
            acceptor->stop();
            io.stop();
            workers.Stop();
        });

        for (auto& t : io_threads) {
            t.join();
        }

        return 0;
    } catch (const std::exception& ex) {
        corelog::error(std::string("server_app 예외: ") + ex.what());
        return 1;
    }
}
