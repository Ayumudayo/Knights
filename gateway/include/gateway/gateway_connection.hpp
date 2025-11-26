#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "gateway/auth/authenticator.hpp"
#include "gateway/gateway_app.hpp"
#include "gateway_lb.grpc.pb.h"
#include "server/core/net/connection.hpp"

namespace gateway {

class GatewayApp;

/**
 * @brief 클라이언트와 로드 밸런서 사이를 중계하는 연결 클래스
 * 
 * GatewayConnection은 클라이언트의 TCP 연결을 관리하며, 수신된 데이터를
 * gRPC를 통해 로드 밸런서(Load Balancer)로 전달합니다.
 * 반대로 로드 밸런서로부터 받은 데이터를 클라이언트에게 TCP로 전송합니다.
 * 
 * 주요 역할:
 * 1. 클라이언트 인증 (Authenticator 위임)
 * 2. 로드 밸런서와의 gRPC 스트림 세션 생성 및 관리
 * 3. 양방향 데이터 포워딩 (TCP <-> gRPC)
 * 
 * 이 클래스는 `server::core::net::Connection`을 상속받아 비동기 I/O를 처리합니다.
 */
class GatewayConnection : public server::core::net::Connection {
public:
    /**
     * @brief 생성자
     * @param hive I/O 컨텍스트
     * @param authenticator 인증 로직 구현체
     * @param app Gateway 애플리케이션 (LB 세션 팩토리 역할)
     */
    GatewayConnection(std::shared_ptr<server::core::net::Hive> hive,
                      std::shared_ptr<auth::IAuthenticator> authenticator,
                      GatewayApp& app);

    /**
     * @brief 백엔드(로드 밸런서/서버)로부터 전달된 데이터를 처리합니다.
     * @param payload 클라이언트에게 전송할 원본 데이터
     */
    void handle_backend_payload(std::vector<std::uint8_t> payload);

    /**
     * @brief 백엔드 연결이 끊어졌을 때 호출됩니다.
     * 클라이언트와의 TCP 연결도 함께 종료합니다.
     */
    void handle_backend_close(const std::string& reason);

protected:
    // --- Connection 이벤트 핸들러 ---

    /**
     * @brief 클라이언트 연결 시 호출
     */
    void on_connect() override;

    /**
     * @brief 클라이언트 연결 종료 시 호출
     * LB 세션도 함께 정리합니다.
     */
    void on_disconnect() override;

    /**
     * @brief 클라이언트로부터 데이터를 수신했을 때 호출
     * 인증 전이면 인증을 시도하고, 인증 후면 LB로 데이터를 포워딩합니다.
     */
    void on_read(const std::uint8_t* data, std::size_t length) override;

    /**
     * @brief 소켓 에러 발생 시 호출
     */
    void on_error(const boost::system::error_code& ec) override;

private:
    /**
     * @brief 로드 밸런서와 gRPC 스트림 세션을 시작합니다.
     * @param handshake_payload 초기 핸드셰이크 데이터 (인증 정보 등)
     */
    void open_lb_session(const std::string& handshake_payload);

    /**
     * @brief 로드 밸런서로 데이터를 전송합니다.
     * @param payload 전송할 데이터
     * @param kind 메시지 종류 (Connect, Data, Disconnect)
     */
    void send_to_lb(const std::vector<std::uint8_t>& payload, gateway::lb::RouteMessageKind kind);

    std::shared_ptr<auth::IAuthenticator> authenticator_;
    GatewayApp& app_;
    
    std::string session_id_; // Gateway에서 생성한 고유 세션 ID
    std::string client_id_;  // 인증된 클라이언트 ID (유저 ID)
    
    GatewayApp::LbSessionPtr lb_session_; // LB와의 gRPC 스트림 핸들
    
    bool authenticated_{false};
    auth::AuthResult last_auth_result_{};
    std::atomic<bool> closing_{false};
};

} // namespace gateway
