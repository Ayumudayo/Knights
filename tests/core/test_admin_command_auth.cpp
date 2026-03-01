#include <gtest/gtest.h>

#include "server/core/security/admin_command_auth.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace {

using server::core::security::admin_command_auth::VerifyOptions;
using server::core::security::admin_command_auth::VerifyResult;
using server::core::security::admin_command_auth::Verifier;

VerifyOptions make_options(std::uint64_t ttl_ms = 60'000, std::uint64_t future_skew_ms = 5'000) {
    VerifyOptions options;
    options.ttl_ms = ttl_ms;
    options.future_skew_ms = future_skew_ms;
    return options;
}

std::unordered_map<std::string, std::string> make_fields(std::string op, std::string request_id) {
    std::unordered_map<std::string, std::string> fields;
    fields["op"] = std::move(op);
    fields["actor"] = "admin-user";
    fields["request_id"] = std::move(request_id);
    fields["client_ids"] = "user-a,user-b";
    fields["reason"] = "maintenance";
    return fields;
}

} // namespace

TEST(AdminCommandAuthTest, SignAndVerifyRoundTrip) {
    auto fields = make_fields("disconnect", "admin-101");
    const std::string secret = "unit-test-secret";
    const std::uint64_t issued_at = 1'000;

    ASSERT_TRUE(server::core::security::admin_command_auth::append_signature_fields(
        fields,
        secret,
        issued_at,
        "nonce-1"));

    Verifier verifier(secret, make_options());
    EXPECT_TRUE(verifier.enabled());
    EXPECT_EQ(verifier.verify(fields, issued_at + 100), VerifyResult::kOk);
}

TEST(AdminCommandAuthTest, RejectsTamperedPayload) {
    auto fields = make_fields("disconnect", "admin-102");
    const std::string secret = "unit-test-secret";
    const std::uint64_t issued_at = 2'000;

    ASSERT_TRUE(server::core::security::admin_command_auth::append_signature_fields(
        fields,
        secret,
        issued_at,
        "nonce-2"));

    fields["reason"] = "tampered";

    Verifier verifier(secret, make_options());
    EXPECT_EQ(verifier.verify(fields, issued_at + 10), VerifyResult::kSignatureMismatch);
}

TEST(AdminCommandAuthTest, RejectsReplayNonceWithinTtlWindow) {
    auto fields = make_fields("announce", "admin-103");
    const std::string secret = "unit-test-secret";
    const std::uint64_t issued_at = 3'000;

    ASSERT_TRUE(server::core::security::admin_command_auth::append_signature_fields(
        fields,
        secret,
        issued_at,
        "nonce-replay"));

    Verifier verifier(secret, make_options());
    EXPECT_EQ(verifier.verify(fields, issued_at + 50), VerifyResult::kOk);
    EXPECT_EQ(verifier.verify(fields, issued_at + 60), VerifyResult::kReplay);
}

TEST(AdminCommandAuthTest, RejectsExpiredAndFutureIssuedAt) {
    const std::string secret = "unit-test-secret";
    Verifier verifier(secret, make_options(100, 50));

    auto expired_fields = make_fields("settings", "admin-104");
    ASSERT_TRUE(server::core::security::admin_command_auth::append_signature_fields(
        expired_fields,
        secret,
        1'000,
        "nonce-expired"));
    EXPECT_EQ(verifier.verify(expired_fields, 1'101), VerifyResult::kExpired);

    auto future_fields = make_fields("settings", "admin-105");
    ASSERT_TRUE(server::core::security::admin_command_auth::append_signature_fields(
        future_fields,
        secret,
        2'000,
        "nonce-future"));
    EXPECT_EQ(verifier.verify(future_fields, 1'900), VerifyResult::kFuture);
}

TEST(AdminCommandAuthTest, RejectsMissingAndInvalidIssuedAtFields) {
    const std::string secret = "unit-test-secret";
    Verifier verifier(secret, make_options());

    auto missing_fields = make_fields("moderation", "admin-106");
    ASSERT_TRUE(server::core::security::admin_command_auth::append_signature_fields(
        missing_fields,
        secret,
        4'000,
        "nonce-missing"));
    missing_fields.erase("nonce");
    EXPECT_EQ(verifier.verify(missing_fields, 4'010), VerifyResult::kMissingField);

    auto invalid_fields = make_fields("moderation", "admin-107");
    ASSERT_TRUE(server::core::security::admin_command_auth::append_signature_fields(
        invalid_fields,
        secret,
        4'000,
        "nonce-invalid"));
    invalid_fields["issued_at"] = "not-a-number";
    invalid_fields["signature"] = server::core::security::admin_command_auth::sign_fields(invalid_fields, secret);
    EXPECT_EQ(verifier.verify(invalid_fields, 4'010), VerifyResult::kInvalidIssuedAt);
}

TEST(AdminCommandAuthTest, ReturnsSecretNotConfiguredWhenDisabled) {
    auto fields = make_fields("announce", "admin-108");
    Verifier verifier("", make_options());

    EXPECT_FALSE(verifier.enabled());
    EXPECT_EQ(verifier.verify(fields, 0), VerifyResult::kSecretNotConfigured);
    EXPECT_EQ(server::core::security::admin_command_auth::to_string(VerifyResult::kSecretNotConfigured), "secret_not_configured");
}
