#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace server::core::security::admin_command_auth {

enum class VerifyResult {
    kOk,
    kMissingField,
    kInvalidIssuedAt,
    kExpired,
    kFuture,
    kSignatureMismatch,
    kReplay,
    kSecretNotConfigured,
};

struct VerifyOptions {
    std::uint64_t ttl_ms{60'000};
    std::uint64_t future_skew_ms{5'000};
};

std::uint64_t now_ms();
std::string make_nonce();

std::string to_kv_payload(const std::unordered_map<std::string, std::string>& fields);
std::string sign_fields(const std::unordered_map<std::string, std::string>& fields, std::string_view secret);

bool append_signature_fields(
    std::unordered_map<std::string, std::string>& fields,
    std::string_view secret,
    std::uint64_t issued_at_ms,
    std::string nonce = {}
);

std::string to_string(VerifyResult result);

class Verifier {
public:
    explicit Verifier(std::string secret, VerifyOptions options = {});

    bool enabled() const noexcept;
    VerifyResult verify(const std::unordered_map<std::string, std::string>& fields);
    VerifyResult verify(const std::unordered_map<std::string, std::string>& fields, std::uint64_t now_ms_override);

private:
    std::string secret_;
    VerifyOptions options_;
    std::mutex nonce_mutex_;
    std::unordered_map<std::string, std::uint64_t> seen_nonces_;
};

} // namespace server::core::security::admin_command_auth
