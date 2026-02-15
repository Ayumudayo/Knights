#pragma once

#include <cstdint>

#if defined(_WIN32)
#  define CHAT_HOOK_CALL __cdecl
#else
#  define CHAT_HOOK_CALL
#endif

#if defined(_WIN32)
#  if defined(CHAT_HOOK_PLUGIN_BUILD)
#    define CHAT_HOOK_PLUGIN_EXPORT __declspec(dllexport)
#  else
#    define CHAT_HOOK_PLUGIN_EXPORT
#  endif
#else
#  define CHAT_HOOK_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

extern "C" {

static constexpr std::uint32_t CHAT_HOOK_ABI_VERSION_V1 = 1u;

enum class ChatHookDecisionV1 : std::uint32_t {
    kPass = 0,
    kHandled = 1,
    kReplaceText = 2,
    kBlock = 3,
};

struct ChatHookStrBufV1 {
    char* data;
    std::uint32_t capacity;
    std::uint32_t size;
};

struct ChatHookChatSendV1 {
    std::uint32_t session_id;
    const char* room;
    const char* user;
    const char* text;
};

struct ChatHookChatSendOutV1 {
    ChatHookStrBufV1 notice;
    ChatHookStrBufV1 replacement_text;
};

struct ChatHookApiV1 {
    std::uint32_t abi_version;
    const char* name;
    const char* version;

    void* (CHAT_HOOK_CALL* create)();
    void (CHAT_HOOK_CALL* destroy)(void* instance);

    ChatHookDecisionV1 (CHAT_HOOK_CALL* on_chat_send)(
        void* instance,
        const ChatHookChatSendV1* in,
        ChatHookChatSendOutV1* out);
};

CHAT_HOOK_PLUGIN_EXPORT const ChatHookApiV1* CHAT_HOOK_CALL chat_hook_api_v1();

} // extern "C"
