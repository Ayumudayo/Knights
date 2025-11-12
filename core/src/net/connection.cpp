#include "server/core/net/connection.hpp"

#include <utility>

namespace server::core::net {

Connection::Connection(std::shared_ptr<Hive> hive)
    : hive_(std::move(hive))
    , socket_(hive_->context()) {}

Connection::~Connection() {
    stop();
}

Connection::socket_type& Connection::socket() {
    return socket_;
}

void Connection::start() {
    if (stopped_.load(std::memory_order_relaxed)) {
        return;
    }
    // 템플릿 메서드 패턴을 따르므로 파생 클래스에서 on_connect/on_read 등을 오버라이드한다.
    on_connect();
    read_loop();
}

void Connection::stop() {
    bool expected = false;
    if (!stopped_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;
    }

    boost::system::error_code ec;
    socket_.close(ec);
    write_queue_.clear();
    on_disconnect();
}

bool Connection::is_stopped() const {
    return stopped_.load(std::memory_order_relaxed);
}

void Connection::async_send(const std::vector<std::uint8_t>& data) {
    if (is_stopped()) {
        return;
    }

    boost::asio::post(io(), [self = shared_from_this(), payload = data]() mutable {
        const bool idle = self->write_queue_.empty();
        self->write_queue_.push_back(std::move(payload));
        if (idle) {
            // 기존 write 작업이 없을 때만 do_write를 시작해 순서를 보장한다.
            self->do_write();
        }
    });
}

void Connection::on_connect() {}
void Connection::on_disconnect() {}
void Connection::on_read(const std::uint8_t*, std::size_t) {}
void Connection::on_write(std::size_t) {}
void Connection::on_error(const boost::system::error_code&) {}

boost::asio::io_context& Connection::io() {
    return hive_->context();
}

void Connection::read_loop() {
    auto self = shared_from_this();
    socket_.async_read_some(boost::asio::buffer(read_buffer_),
        [self](const boost::system::error_code& ec, std::size_t bytes_transferred) {
            self->handle_read(ec, bytes_transferred);
        });
}

void Connection::handle_read(const boost::system::error_code& ec, std::size_t bytes_transferred) {
    if (ec) {
        // read/write 루프는 하나라도 실패하면 세션 전체를 닫아 정의되지 않은 상태를 피한다.
        on_error(ec);
        stop();
        return;
    }

    if (bytes_transferred > 0) {
        on_read(read_buffer_.data(), bytes_transferred);
    }

    if (!is_stopped()) {
        read_loop();
    }
}

void Connection::handle_write(const boost::system::error_code& ec, std::size_t bytes_transferred) {
    if (ec) {
        on_error(ec);
        stop();
        return;
    }

    on_write(bytes_transferred);

    write_queue_.pop_front();
    if (!write_queue_.empty()) {
        // 큐가 비워질 때까지 한 번에 하나의 async_write만 유지해 backpressure를 단순화한다.
        do_write();
    }
}

void Connection::do_write() {
    auto self = shared_from_this();
    boost::asio::async_write(socket_,
        boost::asio::buffer(write_queue_.front()),
        [self](const boost::system::error_code& ec, std::size_t bytes_transferred) {
            self->handle_write(ec, bytes_transferred);
        });
}

} // namespace server::core::net
