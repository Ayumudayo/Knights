#include "server/core/util/crash_handler.hpp"

#include "server/core/util/log.hpp"

#include <atomic>
#include <csignal>
#include <cstring>
#include <utility>

#if defined(_WIN32)
#  include <windows.h>
#  include <DbgHelp.h>
#else
#  include <execinfo.h>
#  include <signal.h>
#  include <unistd.h>
#endif

namespace server::core::util::crash {

namespace {
std::atomic<bool> g_installed{false};

void safe_write_literal(const char* text) noexcept {
    if (!text) return;
    std::size_t len = 0;
    while (text[len] != '\0') ++len;
#if defined(_WIN32)
    ::OutputDebugStringA(text);
    HANDLE stderr_handle = ::GetStdHandle(STD_ERROR_HANDLE);
    if (stderr_handle != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        ::WriteFile(stderr_handle, text, static_cast<DWORD>(len), &written, nullptr);
    }
#else
    ::write(STDERR_FILENO, text, len);
#endif
}

void safe_write_newline() noexcept {
    safe_write_literal("\n");
}

void safe_write_number(unsigned long long value) noexcept {
    char buffer[32];
    std::size_t pos = 0;
    do {
        buffer[pos++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    } while (value && pos < sizeof(buffer));
    for (std::size_t i = 0; i < pos / 2; ++i) {
        std::swap(buffer[i], buffer[pos - i - 1]);
    }
    buffer[pos] = '\0';
    safe_write_literal(buffer);
}

#if defined(_WIN32)
void dump_stack() noexcept {
    void* frames[64];
    USHORT captured = ::RtlCaptureStackBackTrace(0, 64, frames, nullptr);
    HANDLE process = ::GetCurrentProcess();

    char symbol_buffer[sizeof(SYMBOL_INFO) + 256];
    PSYMBOL_INFO symbol = reinterpret_cast<PSYMBOL_INFO>(symbol_buffer);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = 255;

    for (USHORT i = 0; i < captured; ++i) {
        DWORD64 displacement = 0;
        if (::SymFromAddr(process, reinterpret_cast<DWORD64>(frames[i]), &displacement, symbol)) {
            safe_write_literal("#");
            safe_write_number(i);
            safe_write_literal(" ");
            safe_write_literal(symbol->Name);
            safe_write_literal(" +0x");

            char hex_buffer[17];
            const char* digits = "0123456789abcdef";
            DWORD64 value = displacement;
            std::size_t idx = 0;
            do {
                hex_buffer[idx++] = digits[value & 0xF];
                value >>= 4;
            } while (value && idx < sizeof(hex_buffer));
            for (std::size_t j = 0; j < idx / 2; ++j) {
                std::swap(hex_buffer[j], hex_buffer[idx - j - 1]);
            }
            hex_buffer[idx] = '\0';
            safe_write_literal(hex_buffer);
            safe_write_newline();
        }
    }
}
#else
void dump_stack() noexcept {
    void* buffer[64];
    int size = ::backtrace(buffer, 64);
    if (size > 0) {
        ::backtrace_symbols_fd(buffer, size, STDERR_FILENO);
    }
}
#endif

void handle_crash_signal(int signo) noexcept {
    safe_write_literal("=== Knights crash handler: signal ");
    safe_write_number(static_cast<unsigned long long>(signo));
    safe_write_literal(" ===");
    safe_write_newline();
    dump_stack();
    safe_write_literal("=== crash handler end ===");
    safe_write_newline();
    std::signal(signo, SIG_DFL);
    std::raise(signo);
}

#if defined(_WIN32)
LONG WINAPI unhandled_exception_handler(EXCEPTION_POINTERS* info) {
    (void)info;
    safe_write_literal("=== Knights crash handler: unhandled exception ===\n");
    dump_stack();
    safe_write_literal("=== crash handler end ===\n");
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

#if !defined(_WIN32)
void install_handlers() {
    struct sigaction sa {};
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = handle_crash_signal;

    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGILL, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
    sigaction(SIGFPE, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);
}
#endif

} // namespace

void install() {
    bool expected = false;
    if (!g_installed.compare_exchange_strong(expected, true)) {
        return;
    }

#if defined(_WIN32)
    ::SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);
    ::SymInitialize(::GetCurrentProcess(), nullptr, TRUE);
    ::SetUnhandledExceptionFilter(unhandled_exception_handler);
#endif

#if defined(_WIN32)
    std::signal(SIGSEGV, handle_crash_signal);
    std::signal(SIGILL, handle_crash_signal);
    std::signal(SIGABRT, handle_crash_signal);
    std::signal(SIGFPE, handle_crash_signal);
#  ifdef SIGBUS
    std::signal(SIGBUS, handle_crash_signal);
#  endif
#else
    install_handlers();
#endif

    server::core::log::info("CrashHandler installed.");
}

} // namespace server::core::util::crash
