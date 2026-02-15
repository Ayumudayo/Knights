#pragma once

#include <csignal>

namespace server::core::app {

namespace detail {

inline volatile std::sig_atomic_t g_termination_signal_received = 0;

inline void termination_signal_handler(int) noexcept {
    g_termination_signal_received = 1;
}

} // namespace detail

// Installs process-wide termination signal handlers (best-effort).
//
// Notes:
// - The handler only flips a `sig_atomic_t` flag (signal-safe).
// - Users should poll `termination_signal_received()` and stop their loops.
inline void install_termination_signal_handlers() noexcept {
#if defined(SIGINT)
    std::signal(SIGINT, detail::termination_signal_handler);
#endif
#if defined(SIGTERM)
    std::signal(SIGTERM, detail::termination_signal_handler);
#endif
}

inline bool termination_signal_received() noexcept {
    return detail::g_termination_signal_received != 0;
}

} // namespace server::core::app
