#pragma once

#include <string>

namespace gateway::auth {

// 인증 요청 정보를 담는 구조체입니다.
struct AuthRequest {
    std::string token;          // 클라이언트가 보낸 인증 토큰 (예: JWT, 세션키)
    std::string client_id;      // 클라이언트가 주장하는 식별자 (예: username)
    std::string remote_address; // 클라이언트 IP 주소
};

struct AuthResult {
    bool success{false};
    std::string subject;
    std::string failure_reason;
};

// 인증 로직을 추상화한 인터페이스입니다.
// 실제 구현체는 JWT 검증, DB 조회 등을 수행할 수 있습니다.
class IAuthenticator {
public:
    virtual ~IAuthenticator() = default;
    virtual AuthResult authenticate(const AuthRequest& request) = 0;
};

class NoopAuthenticator final : public IAuthenticator {
public:
    AuthResult authenticate(const AuthRequest& request) override {
        AuthResult result;
        result.success = true;
        result.subject = request.client_id.empty() ? "anonymous" : request.client_id;
        return result;
    }
};

} // namespace gateway::auth
