# gateway (gateway_app)

Edge TCP gateway: accepts clients, authenticates, selects a backend `server_app`, then bridges the TCP streams 1:1.

## Entry Points
- `gateway/src/main.cpp`: process entry.
- `gateway/src/gateway_app.cpp`: app lifecycle + backend selection + metrics server.
- `gateway/src/gateway_connection.cpp`: per-client session; bridges client <-> backend.
- `gateway/src/session_directory.cpp`: sticky routing store (Redis-backed).
- `gateway/include/gateway/auth/`: pluggable authentication.

## Run (Standard Runtime = Docker)
```powershell
pwsh scripts/deploy_docker.ps1 -Action up -Detached -Build
```

## Config (Env Vars)
- Listen/identity: `GATEWAY_LISTEN`, `GATEWAY_ID`
- Discovery/sticky: `REDIS_URI`, `SERVER_REGISTRY_PREFIX`, `SERVER_REGISTRY_TTL`
- Auth: `ALLOW_ANONYMOUS`, `AUTH_PROVIDER`, `AUTH_ENDPOINT`
- Observability: `METRICS_PORT` (serves `/metrics`)
