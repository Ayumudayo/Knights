# server (server_app)

Chat logic node. Owns room/user state, storage access, write-behind emission, and Redis instance registry.

## Entry Points
- `server/src/main.cpp`: process entry; calls into bootstrap.
- `server/src/app/bootstrap.cpp`: wiring (io_context/threads/listener/storage/registry).
- `server/src/app/router.cpp`: opcode -> handler routing table.
- `server/src/chat/chat_service_core.cpp`: main chat service state + orchestration.
- `server/src/chat/handlers_*.cpp`: opcode handlers.
- `server/src/state/instance_registry.cpp`: Redis instance registry (discovery for gateways).
- `server/src/app/metrics_server.cpp`: Prometheus `/metrics` endpoint (port via `METRICS_PORT`).

## Run (Standard Runtime = Docker)
```powershell
pwsh scripts/deploy_docker.ps1 -Action up -Detached -Build

# Stack + Prometheus/Grafana
pwsh scripts/run_full_stack_observability.ps1
```

## Config (Env Vars)
- Required: `DB_URI`
- Common: `PORT`, `REDIS_URI`, `WRITE_BEHIND_ENABLED`, `REDIS_STREAM_KEY`, `USE_REDIS_PUBSUB`
- Discovery: `SERVER_INSTANCE_ID`, `SERVER_ADVERTISE_HOST`, `SERVER_ADVERTISE_PORT`, `SERVER_REGISTRY_PREFIX`, `SERVER_REGISTRY_TTL`
- Observability: `METRICS_PORT`

## Notes
- Dispatch latency quantiles can be NaN if there are no samples in the PromQL rate window (normal; send traffic).
- If `WRITE_BEHIND_ENABLED=1`, ensure `wb_worker` is running (included in `docker/stack`).
