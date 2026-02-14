# tests

GTest unit tests + small Python integration checks.

## C++ (GTest)
- Definitions: `tests/CMakeLists.txt`
- Key targets:
  - `core_general_tests`, `core_concurrency_tests`
  - `server_general_tests`, `server_state_tests`
  - `chat_history_tests`, `storage_basic_tests`

Run:
```powershell
ctest --test-dir build-windows/tests
```

## Python
- `tests/python/verify_chat.py`
- `tests/python/verify_pong.py`
- `tests/python/test_load_balancing.py`

Most Python tests expect the Docker stack (`docker/stack`) to be running.
