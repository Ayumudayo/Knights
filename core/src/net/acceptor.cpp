#include "server/core/net/acceptor.hpp"
#include "server/core/net/session.hpp"
#include "server/core/net/dispatcher.hpp"
#include "server/core/util/log.hpp"
#include "server/core/config/options.hpp"
#include "server/core/state/shared_state.hpp"
#include "server/core/memory/memory_pool.hpp"
#include "server/core/runtime_metrics.hpp"

using boost::system::error_code;

namespace server::core {

Acceptor::Acceptor(asio::io_context& io,
                   const asio::ip::tcp::endpoint& ep,
                   Dispatcher& dispatcher,
                   BufferManager& buffer_manager,
                   std::shared_ptr<const SessionOptions> options,
                   std::shared_ptr<SharedState> state,
                   new_session_cb_t on_new_session)
    : io_(io), 
      acceptor_(io), 
      dispatcher_(dispatcher), 
      buffer_manager_(buffer_manager),
      options_(std::move(options)), 
      state_(std::move(state)), 
      on_new_session_(std::move(on_new_session)) {
    error_code ec;
    auto fail = [&](const char* stage) {
        if (!ec) return false;
        log::error(std::string("acceptor ") + stage + " failed: " + ec.message());
        return true;
    };

    // 초기 바인드 단계는 실패 시 다시 복구하기 어렵기 때문에 각 단계를 명시적으로 검증합니다.
    // bind()나 listen() 실패는 포트 충돌 등 치명적인 문제일 가능성이 높습니다.
    acceptor_.open(ep.protocol(), ec);
    if (fail("open")) return;
    acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true), ec);
    if (fail("set_option")) return;
    acceptor_.bind(ep, ec);
    if (fail("bind")) return;
    acceptor_.listen(asio::socket_base::max_listen_connections, ec);
    if (fail("listen")) return;
}

void Acceptor::start() {
    if (running_) return;
    running_ = true;
    // 외부에서 start/stop을 여러 번 호출해도 accept loop가 중복되지 않도록 플래그로 보호합니다.
    // Acceptor는 서버의 문지기 역할을 하며, 클라이언트의 연결 요청을 기다립니다.
    log::info("Acceptor started");
    do_accept();
}

void Acceptor::stop() {
    if (!running_) return;
    running_ = false;
    error_code ec;
    // acceptor를 닫으면 대기 중인 async_accept 작업이 취소(operation_aborted)됩니다.
    acceptor_.close(ec);
}

void Acceptor::do_accept() {
    if (!running_) return;
    
    // 비동기 accept 호출입니다.
    // 클라이언트가 접속하면 람다 함수가 호출됩니다.
    // 'self'를 캡처하여 비동기 작업 도중 Acceptor 객체가 파괴되지 않도록 수명을 연장합니다.
    acceptor_.async_accept(
        [self = shared_from_this()](const error_code& ec, asio::ip::tcp::socket socket) {
            if (ec) {
                if (self->running_) {
                    log::warn(std::string("accept failed: ") + ec.message());
                    // 일시적인 오류일 수 있으므로 다시 accept를 시도합니다.
                    self->do_accept();
                }
                return;
            }

            if (self->state_ && self->state_->max_connections > 0) {
                auto cur = self->state_->connection_count.load();
                if (cur >= self->state_->max_connections) {
                    // 상태 공유 객체가 제공하는 상한을 넘으면 TCP 레벨에서 바로 끊어
                    // 리소스(버퍼, 세션 객체) 할당을 최소화합니다.
                    // 이는 DDoS 공격이나 과부하 상황에서 서버를 보호하는 기본적인 방어책입니다.
                    log::warn("max concurrent connections reached; closing new connection");
                    error_code ignored;
                    socket.shutdown(asio::ip::tcp::socket::shutdown_both, ignored);
                    socket.close(ignored);
                    self->do_accept();
                    return;
                }
            }

            runtime_metrics::record_accept();
            try {
                // Session은 공유 상태 및 Dispatcher 의존성이 많으므로 예외가 발생하면
                // accept loop만 유지하고 연결을 폐기합니다.
                // Session 객체는 클라이언트와의 1:1 통신을 담당합니다.
                auto session = std::make_shared<Session>(std::move(socket), self->dispatcher_, self->buffer_manager_, self->options_, self->state_);
                if (self->on_new_session_) self->on_new_session_(session);
                session->start();
            } catch (const std::exception& ex) {
                log::error(std::string("session creation threw: ") + ex.what());
            }

            // 다음 연결을 계속 기다립니다.
            self->do_accept();
        });
}

} // namespace server::core



