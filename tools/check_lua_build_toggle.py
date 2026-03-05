#!/usr/bin/env python3
"""Validate BUILD_LUA_SCRIPTING cache state and Lua runtime source selection."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


def read_cache_option(cache_path: Path, key: str) -> str | None:
    try:
        text = cache_path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return None

    prefix = f"{key}:"
    for line in text.splitlines():
        if not line.startswith(prefix):
            continue
        parts = line.split("=", 1)
        if len(parts) != 2:
            return None
        return parts[1].strip()
    return None


def load_compile_sources(compile_commands_path: Path) -> set[str] | None:
    if not compile_commands_path.exists():
        return None

    try:
        payload = json.loads(
            compile_commands_path.read_text(encoding="utf-8", errors="replace")
        )
    except (OSError, json.JSONDecodeError):
        return None

    sources: set[str] = set()
    for entry in payload:
        if not isinstance(entry, dict):
            continue
        file_path = entry.get("file")
        if not isinstance(file_path, str) or not file_path:
            continue
        sources.add(file_path.replace("\\", "/").lower())
    return sources


def contains_source(sources: set[str], suffix: str) -> bool:
    normalized = suffix.replace("\\", "/").lower()
    return any(source.endswith(normalized) for source in sources)


def expected_toggle_value(expect: str) -> str:
    lowered = expect.strip().lower()
    if lowered == "on":
        return "ON"
    if lowered == "off":
        return "OFF"
    raise ValueError(f"unsupported expect value: {expect}")


def run_check(build_dir: Path, expect: str, require_source_check: bool) -> int:
    expected = expected_toggle_value(expect)
    cache_path = build_dir / "CMakeCache.txt"
    compile_commands_path = build_dir / "compile_commands.json"

    errors: list[str] = []

    actual = read_cache_option(cache_path, "BUILD_LUA_SCRIPTING")
    if actual is None:
        errors.append(f"missing BUILD_LUA_SCRIPTING in cache: {cache_path}")
    elif actual != expected:
        errors.append(
            f"BUILD_LUA_SCRIPTING mismatch: expected {expected}, got {actual} ({cache_path})"
        )

    sources = load_compile_sources(compile_commands_path)
    if sources is None:
        if require_source_check:
            errors.append(
                f"compile_commands.json required but missing/unreadable: {compile_commands_path}"
            )
        else:
            print(
                f"[lua-toggle] INFO: compile_commands.json not available, source selection check skipped ({compile_commands_path})"
            )
    else:
        has_enabled = contains_source(sources, "core/src/scripting/lua_runtime.cpp")
        has_disabled = contains_source(
            sources, "core/src/scripting/lua_runtime_disabled.cpp"
        )

        if expected == "ON":
            if not has_enabled:
                errors.append(
                    "expected lua_runtime.cpp to be present in compile_commands"
                )
            if has_disabled:
                errors.append(
                    "expected lua_runtime_disabled.cpp to be absent in compile_commands"
                )
        else:
            if has_enabled:
                errors.append(
                    "expected lua_runtime.cpp to be absent in compile_commands"
                )
            if not has_disabled:
                errors.append(
                    "expected lua_runtime_disabled.cpp to be present in compile_commands"
                )

    if errors:
        print(f"[lua-toggle] FAIL ({len(errors)} issue(s))")
        for item in errors:
            print(f"- {item}")
        return 1

    print(f"[lua-toggle] OK: BUILD_LUA_SCRIPTING={expected} ({build_dir})")
    return 0


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--build-dir", required=True, help="configured build directory path"
    )
    parser.add_argument(
        "--expect", required=True, choices=("on", "off"), help="expected build toggle"
    )
    parser.add_argument(
        "--require-source-check",
        action="store_true",
        help="fail when compile_commands.json is missing or unreadable",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    return run_check(Path(args.build_dir), args.expect, args.require_source_check)


if __name__ == "__main__":
    sys.exit(main())
