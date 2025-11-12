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

    // 초기 바인드 단계는 실패 시 다시 복구하기 어렵기 때문에 각 단계를 명시적으로 검증한다.
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
    // 외부에서 start/stop을 여러 번 호출해도 accept loop가 중복되지 않도록 플래그로 보호한다.
    log::info("Acceptor started");
    do_accept();
}

void Acceptor::stop() {
    if (!running_) return;
    running_ = false;
    error_code ec;
    acceptor_.close(ec);
}

void Acceptor::do_accept() {
    if (!running_) return;
    acceptor_.async_accept(
        [self = shared_from_this()](const error_code& ec, asio::ip::tcp::socket socket) {
            if (ec) {
                if (self->running_) {
                    log::warn(std::string("accept failed: ") + ec.message());
                    self->do_accept();
                }
                return;
            }

            if (self->state_ && self->state_->max_connections > 0) {
                auto cur = self->state_->connection_count.load();
                if (cur >= self->state_->max_connections) {
                    // 상태 공유 객체가 제공하는 상한을 넘으면 TCP 레벨에서 바로 끊어
                    // 리소스(버퍼, 세션 객체) 할당을 최소화한다.
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
                // accept loop만 유지하고 연결을 폐기한다.
                auto session = std::make_shared<Session>(std::move(socket), self->dispatcher_, self->buffer_manager_, self->options_, self->state_);
                if (self->on_new_session_) self->on_new_session_(session);
                session->start();
            } catch (const std::exception& ex) {
                log::error(std::string("session creation threw: ") + ex.what());
            }

            self->do_accept();
        });
}

} // namespace server::core



