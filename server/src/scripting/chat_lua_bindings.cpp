#include "server/scripting/chat_lua_bindings.hpp"

#include "server/core/scripting/lua_runtime.hpp"

#include <array>

namespace server::app::scripting {

namespace {

struct BindingEntry {
    const char* table;
    const char* name;
};

constexpr std::array<BindingEntry, 20> kBindings{{
    {"server", "get_user_name"},
    {"server", "get_user_room"},
    {"server", "get_room_users"},
    {"server", "get_room_list"},
    {"server", "get_room_owner"},
    {"server", "is_user_muted"},
    {"server", "is_user_banned"},
    {"server", "get_online_count"},
    {"server", "get_room_count"},
    {"server", "send_notice"},
    {"server", "broadcast_room"},
    {"server", "broadcast_all"},
    {"server", "kick_user"},
    {"server", "mute_user"},
    {"server", "ban_user"},
    {"server", "log_info"},
    {"server", "log_warn"},
    {"server", "log_debug"},
    {"server", "hook_name"},
    {"server", "script_name"},
}};

} // namespace

std::size_t chat_lua_binding_count() {
    return kBindings.size();
}

ChatLuaBindingsResult register_chat_lua_bindings(server::core::scripting::LuaRuntime& runtime) {
    ChatLuaBindingsResult result{};

    for (const auto& binding : kBindings) {
        ++result.attempted;
        if (runtime.register_host_api(binding.table, binding.name, []() {})) {
            ++result.registered;
        }
    }

    return result;
}

} // namespace server::app::scripting
