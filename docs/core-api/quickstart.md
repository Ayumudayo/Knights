# server_core Quickstart

## Goal
Compile a minimal binary that consumes only `[Stable]` `server_core` APIs.

## Minimal Example

```cpp
#include <boost/asio/io_context.hpp>

#include "server/core/app/app_host.hpp"
#include "server/core/concurrent/task_scheduler.hpp"
#include "server/core/net/hive.hpp"

int main() {
    boost::asio::io_context io;
    server::core::net::Hive hive(io);

    server::core::app::AppHost host{"quickstart"};
    host.declare_dependency("sample");
    host.set_dependency_ok("sample", true);
    host.set_ready(true);
    host.set_lifecycle_phase(server::core::app::AppHost::LifecyclePhase::kRunning);

    server::core::concurrent::TaskScheduler scheduler;
    scheduler.post([] {});
    (void)scheduler.poll();

    return 0;
}
```

## Build

```powershell
pwsh scripts/build.ps1 -Config Debug -Target core_public_api_smoke
```

## Notes
- This quickstart intentionally avoids `Transitional` and `Internal` headers.
- Public API boundary is defined in `docs/core-api-boundary.md`.
