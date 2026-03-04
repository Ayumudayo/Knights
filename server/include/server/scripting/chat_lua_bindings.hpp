#pragma once

#include <cstddef>

namespace server::core::scripting {
class LuaRuntime;
}

namespace server::app::scripting {

struct ChatLuaBindingsResult {
    std::size_t attempted{0};
    std::size_t registered{0};
};

std::size_t chat_lua_binding_count();

ChatLuaBindingsResult register_chat_lua_bindings(server::core::scripting::LuaRuntime& runtime);

} // namespace server::app::scripting
