# gateway (gateway_app)

Edge TCP gateway: accepts clients, authenticates, selects a backend `server_app`, then bridges the TCP streams 1:1.

## Entry Points
- `gateway/src/main.cpp`: process entry.
- `gateway/src/gateway_app.cpp`: app lifecycle + backend selection + metrics server.
- `gateway/src/gateway_connection.cpp`: per-client session; bridges client <-> backend.
- `gateway/src/session_directory.cpp`: sticky routing store (Redis-backed).
- `gateway/include/gateway/auth/`: pluggable authentication.

## Flow
- Accept: `server::core::net::Listener` creates `GatewayConnection` per client.
- Auth: `GatewayConnection::on_connect()` runs the authenticator and derives `client_id_` (subject or remote IP).
- Backend select: `GatewayApp::create_backend_session()` -> `GatewayApp::select_best_server()`:
  - Sticky: `SessionDirectory::find_backend(client_id)` -> prefer the previous `InstanceRecord` when still active.
  - Least connections: sort candidates by `active_sessions` and pick the lowest.
  - Binding: `SessionDirectory::refresh_backend(client_id, instance_id)` for next reconnect.
- Bridge:
  - client -> backend: `GatewayConnection::on_read()` -> `BackendSession::send()`.
  - backend -> client: `BackendSession::on_read()` -> `GatewayConnection::handle_backend_payload()`.
- Close: either side closing triggers `GatewayConnection::on_disconnect()` / `handle_backend_close()` and `GatewayApp::close_backend_session()`.

## Run (Standard Runtime = Docker)
```powershell
pwsh scripts/deploy_docker.ps1 -Action up -Detached -Build
```

## Config (Env Vars)
- Listen/identity: `GATEWAY_LISTEN`, `GATEWAY_ID`
- Discovery/sticky: `REDIS_URI`, `SERVER_REGISTRY_PREFIX`, `SERVER_REGISTRY_TTL`
- Auth: `ALLOW_ANONYMOUS`, `AUTH_PROVIDER`, `AUTH_ENDPOINT`
- Observability: `METRICS_PORT` (serves `/metrics`)

## Metrics
- `/metrics` (Prometheus text format) is served by `server::core::metrics::MetricsHttpServer`.
- Current gateway metrics:
  - `gateway_sessions_active` (gauge)
  - `gateway_connections_total` (counter)
