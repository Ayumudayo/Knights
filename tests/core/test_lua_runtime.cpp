#include "server/core/scripting/lua_runtime.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace server::core::scripting {
namespace {

class LuaRuntimeTest : public ::testing::Test {
protected:
    void SetUp() override {
        const auto nonce = std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
        temp_dir_ = std::filesystem::temp_directory_path()
            / ("knights_lua_runtime_test_" + nonce);
        ASSERT_TRUE(std::filesystem::create_directories(temp_dir_));
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(temp_dir_, ec);
    }

    static void write_text(const std::filesystem::path& path, const std::string& text) {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.good());
        out << text;
        out.flush();
        ASSERT_TRUE(out.good());
    }

    std::filesystem::path temp_dir_;
};

TEST_F(LuaRuntimeTest, LoadScriptReflectsBuildToggle) {
    LuaRuntime runtime;

    const auto script_path = temp_dir_ / "sample.lua";
    write_text(script_path, "return 1\n");

    const auto result = runtime.load_script(script_path, "sample");

#if KNIGHTS_BUILD_LUA_SCRIPTING
    EXPECT_TRUE(result.ok);
    EXPECT_TRUE(result.error.empty());

    const auto metrics = runtime.metrics_snapshot();
    EXPECT_EQ(metrics.loaded_scripts, 1u);
#else
    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.error.empty());
    EXPECT_NE(result.error.find("disabled"), std::string::npos);
#endif
}

TEST_F(LuaRuntimeTest, CallUnknownEnvironmentIsSkippableWhenEnabled) {
    LuaRuntime runtime;

    const auto result = runtime.call("missing_env", "on_login");

#if KNIGHTS_BUILD_LUA_SCRIPTING
    EXPECT_TRUE(result.ok);
    EXPECT_FALSE(result.executed);
    EXPECT_TRUE(result.error.empty());
#else
    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.executed);
    EXPECT_NE(result.error.find("disabled"), std::string::npos);
#endif
}

TEST_F(LuaRuntimeTest, ResetClearsRuntimeState) {
    LuaRuntime runtime;

    const auto script_path = temp_dir_ / "sample.lua";
    write_text(script_path, "return 1\n");

    (void)runtime.register_host_api("server", "log_info", []() {});
    (void)runtime.load_script(script_path, "sample");
    (void)runtime.call("sample", "on_login");

    runtime.reset();
    const auto metrics = runtime.metrics_snapshot();

    EXPECT_EQ(metrics.loaded_scripts, 0u);
    EXPECT_EQ(metrics.registered_host_api, 0u);
    EXPECT_EQ(metrics.calls_total, 0u);
}

} // namespace
} // namespace server::core::scripting
