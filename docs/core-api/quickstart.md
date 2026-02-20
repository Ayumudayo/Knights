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

Standalone engine profile (core-only graph):

```powershell
cmake --preset windows-core-engine
cmake --build --preset windows-core-engine-debug --target server_core
```

## External CMake Consumer (`find_package`)

`server_core` installs CMake package files and exported targets.

```cmake
find_package(server_core CONFIG REQUIRED)

add_executable(app main.cpp)
target_link_libraries(app PRIVATE server_core::server_core)
```

### Local Windows Smoke (repo context)

```powershell
pwsh scripts/build.ps1 -Config Debug -Target server_core
cmake --install build-windows --config Debug --prefix build-windows/install-smoke
```

Consumer configure/build (dependency prefixes included):

```powershell
cmake -S build-windows/package-smoke -B build-windows/package-smoke/build --fresh `
  -DCMAKE_PREFIX_PATH="${PWD}/build-windows/install-smoke;${PWD}/build-windows/vcpkg_installed/x64-windows"
cmake --build build-windows/package-smoke/build --config Debug
```

If dependency discovery fails, ensure `Boost`, `OpenSSL`, and `lz4` package roots are on `CMAKE_PREFIX_PATH`.

## Notes
- This quickstart intentionally avoids `Transitional` and `Internal` headers.
- Public API boundary is defined in `docs/core-api-boundary.md`.
