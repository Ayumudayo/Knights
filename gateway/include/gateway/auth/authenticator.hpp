#pragma once

#include <string>

namespace gateway::auth {

struct AuthRequest {
    std::string token;
    std::string client_id;
    std::string remote_address;
};

struct AuthResult {
    bool success{false};
    std::string subject;
    std::string failure_reason;
};

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
