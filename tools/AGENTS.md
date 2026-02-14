# tools

Aux binaries + code generation.

## Entry Points
- `tools/wb_worker/main.cpp` + `tools/wb_worker/README.md`: Redis Streams -> Postgres write-behind worker.
- `tools/migrations/runner.cpp` + `tools/migrations/*.sql`: schema migrations.
- `tools/gen_opcodes.py`: generates `core/include/server/core/protocol/system_opcodes.hpp` from `core/protocol/system_opcodes.json`.
- `tools/gen_wire_codec.py`: generates `core/include/server/wire/codec.hpp` from `protocol/wire_map.json`.

## Related Tool Docs
- `tools/wb_emit/README.md`: emit test events into Redis Streams.
- `tools/wb_check/README.md`: verify Postgres state after write-behind.
- `tools/wb_dlq_replayer/README.md`: reprocess DLQ stream.
- `tools/migrations/README.md`: migration order + operational notes.

## Build (Windows)
```powershell
pwsh scripts/build.ps1 -Config Debug -Target wb_worker
```

## Notes
- Some migrations use `CREATE INDEX CONCURRENTLY`; run those outside transaction blocks (see `tools/migrations/README.md`).
