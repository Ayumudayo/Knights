#pragma once

#include <cstdint>

namespace server::core { class Dispatcher; }
namespace server::app::chat { class ChatService; }

namespace server::app {

void register_routes(server::core::Dispatcher& d, server::app::chat::ChatService& chat);

} // namespace server::app

