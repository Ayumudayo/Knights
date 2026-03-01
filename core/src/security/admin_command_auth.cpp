#include "server/core/security/admin_command_auth.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <limits>
#include <openssl/crypto.h>
#include <openssl/hmac.h>
#include <random>
#include <vector>

namespace server::core::security::admin_command_auth {

namespace {

std::string canonical_signing_data(const std::unordered_map<std::string, std::string>& fields) {
    std::vector<std::pair<std::string, std::string>> entries;
    entries.reserve(fields.size());
    for (const auto& [key, value] : fields) {
        if (key == "signature") {
            continue;
        }
        entries.emplace_back(key, value);
    }

    std::sort(entries.begin(), entries.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.first < rhs.first;
    });

    std::string out;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        out += entries[i].first;
        out += '=';
        out += entries[i].second;
        if (i + 1 < entries.size()) {
            out.push_back('\n');
        }
    }
    return out;
}

bool parse_u64(std::string_view text, std::uint64_t& out) {
    if (text.empty()) {
        return false;
    }
    std::uint64_t value = 0;
    for (const char c : text) {
        if (c < '0' || c > '9') {
            return false;
        }
        const std::uint64_t digit = static_cast<std::uint64_t>(c - '0');
        if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / 10u) {
            return false;
        }
        value = (value * 10u) + digit;
    }
    out = value;
    return true;
}

bool signatures_equal(std::string_view lhs, std::string_view rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    if (lhs.empty()) {
        return true;
    }
    return CRYPTO_memcmp(lhs.data(), rhs.data(), lhs.size()) == 0;
}

} // namespace

std::uint64_t now_ms() {
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());
    return static_cast<std::uint64_t>(now.count());
}

std::string make_nonce() {
    static constexpr char kHex[] = "0123456789abcdef";
    std::array<std::uint8_t, 16> bytes{};
    static thread_local std::mt19937_64 rng{std::random_device{}()};

    for (std::size_t i = 0; i < bytes.size(); i += sizeof(std::uint64_t)) {
        const auto value = rng();
        for (std::size_t j = 0; j < sizeof(std::uint64_t) && (i + j) < bytes.size(); ++j) {
            bytes[i + j] = static_cast<std::uint8_t>((value >> (j * 8)) & 0xFFu);
        }
    }

    std::string out;
    out.reserve(bytes.size() * 2);
    for (const auto byte : bytes) {
        out.push_back(kHex[(byte >> 4) & 0x0F]);
        out.push_back(kHex[byte & 0x0F]);
    }
    return out;
}

std::string to_kv_payload(const std::unordered_map<std::string, std::string>& fields) {
    std::vector<std::pair<std::string, std::string>> entries;
    entries.reserve(fields.size());
    for (const auto& [key, value] : fields) {
        entries.emplace_back(key, value);
    }

    std::sort(entries.begin(), entries.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.first < rhs.first;
    });

    std::string out;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        out += entries[i].first;
        out += '=';
        out += entries[i].second;
        if (i + 1 < entries.size()) {
            out.push_back('\n');
        }
    }
    return out;
}

std::string sign_fields(const std::unordered_map<std::string, std::string>& fields, std::string_view secret) {
    if (secret.empty()) {
        return {};
    }

    const std::string signing_data = canonical_signing_data(fields);
    if (signing_data.empty()) {
        return {};
    }

    unsigned int digest_length = 0;
    unsigned char digest[EVP_MAX_MD_SIZE]{};
    if (HMAC(
            EVP_sha256(),
            secret.data(),
            static_cast<int>(secret.size()),
            reinterpret_cast<const unsigned char*>(signing_data.data()),
            signing_data.size(),
            digest,
            &digest_length) == nullptr) {
        return {};
    }

    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.reserve(digest_length * 2);
    for (unsigned int i = 0; i < digest_length; ++i) {
        const auto byte = digest[i];
        out.push_back(kHex[(byte >> 4) & 0x0F]);
        out.push_back(kHex[byte & 0x0F]);
    }
    return out;
}

bool append_signature_fields(
    std::unordered_map<std::string, std::string>& fields,
    std::string_view secret,
    std::uint64_t issued_at_ms,
    std::string nonce
) {
    if (secret.empty()) {
        return false;
    }
    if (nonce.empty()) {
        nonce = make_nonce();
    }

    fields["issued_at"] = std::to_string(issued_at_ms);
    fields["nonce"] = std::move(nonce);
    fields.erase("signature");

    const std::string signature = sign_fields(fields, secret);
    if (signature.empty()) {
        return false;
    }
    fields["signature"] = signature;
    return true;
}

std::string to_string(VerifyResult result) {
    switch (result) {
    case VerifyResult::kOk: return "ok";
    case VerifyResult::kMissingField: return "missing_field";
    case VerifyResult::kInvalidIssuedAt: return "invalid_issued_at";
    case VerifyResult::kExpired: return "expired";
    case VerifyResult::kFuture: return "future";
    case VerifyResult::kSignatureMismatch: return "signature_mismatch";
    case VerifyResult::kReplay: return "replay";
    case VerifyResult::kSecretNotConfigured: return "secret_not_configured";
    }
    return "unknown";
}

Verifier::Verifier(std::string secret, VerifyOptions options)
    : secret_(std::move(secret)), options_(options) {}

bool Verifier::enabled() const noexcept {
    return !secret_.empty();
}

VerifyResult Verifier::verify(const std::unordered_map<std::string, std::string>& fields) {
    return verify(fields, now_ms());
}

VerifyResult Verifier::verify(const std::unordered_map<std::string, std::string>& fields, std::uint64_t now_override_ms) {
    if (secret_.empty()) {
        return VerifyResult::kSecretNotConfigured;
    }

    const auto issued_at_it = fields.find("issued_at");
    const auto nonce_it = fields.find("nonce");
    const auto signature_it = fields.find("signature");
    if (issued_at_it == fields.end() || nonce_it == fields.end() || signature_it == fields.end()) {
        return VerifyResult::kMissingField;
    }
    if (issued_at_it->second.empty() || nonce_it->second.empty() || signature_it->second.empty()) {
        return VerifyResult::kMissingField;
    }

    std::uint64_t issued_at_ms = 0;
    if (!parse_u64(issued_at_it->second, issued_at_ms)) {
        return VerifyResult::kInvalidIssuedAt;
    }

    if (issued_at_ms > now_override_ms + options_.future_skew_ms) {
        return VerifyResult::kFuture;
    }
    if (issued_at_ms + options_.ttl_ms < now_override_ms) {
        return VerifyResult::kExpired;
    }

    const std::string expected = sign_fields(fields, secret_);
    if (expected.empty() || !signatures_equal(expected, signature_it->second)) {
        return VerifyResult::kSignatureMismatch;
    }

    {
        std::lock_guard<std::mutex> lock(nonce_mutex_);

        for (auto it = seen_nonces_.begin(); it != seen_nonces_.end();) {
            if (it->second + options_.ttl_ms < now_override_ms) {
                it = seen_nonces_.erase(it);
            } else {
                ++it;
            }
        }

        if (seen_nonces_.find(nonce_it->second) != seen_nonces_.end()) {
            return VerifyResult::kReplay;
        }
        seen_nonces_.emplace(nonce_it->second, issued_at_ms);
    }

    return VerifyResult::kOk;
}

} // namespace server::core::security::admin_command_auth
