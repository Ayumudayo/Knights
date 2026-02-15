#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
import json
import sys
from pathlib import Path
from typing import Any, Dict, List, Tuple


def parse_id(v: Any) -> int:
    if isinstance(v, int):
        return v
    s = str(v).strip()
    if s.lower().startswith("0x"):
        return int(s, 16)
    return int(s, 10)


def load_spec(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def parse_groups(spec: dict):
    raw = spec.get("groups")
    if not raw:
        return [], {}

    order = []
    by_name = {}
    for g in raw:
        name = str(g.get("name", "")).strip()
        if not name:
            raise ValueError("group.name is required")
        if name in by_name:
            raise ValueError(f"duplicate group: {name}")

        id_min = parse_id(g.get("id_min"))
        id_max = parse_id(g.get("id_max"))
        if not (0 <= id_min <= 0xFFFF and 0 <= id_max <= 0xFFFF and id_min <= id_max):
            raise ValueError(f"invalid group range: {name} [0x{id_min:04X}..0x{id_max:04X}]")

        desc = str(g.get("desc", "")).strip()
        order.append(name)
        by_name[name] = {
            "id_min": id_min,
            "id_max": id_max,
            "desc": desc,
        }

    ranges = sorted((by_name[n]["id_min"], by_name[n]["id_max"], n) for n in order)
    prev_max = None
    prev_name = None
    for id_min, id_max, name in ranges:
        if prev_max is not None and id_min <= prev_max:
            raise ValueError(
                f"overlapping group ranges: {prev_name} ends at 0x{prev_max:04X}, "
                f"but {name} starts at 0x{id_min:04X}"
            )
        prev_max = id_max
        prev_name = name

    return order, by_name


def parse_opcodes(spec: dict):
    items = spec.get("opcodes", [])
    group_order, groups = parse_groups(spec)
    require_group = bool(group_order)
    group_index = {name: i for i, name in enumerate(group_order)}

    seen_ids = set()
    seen_names = set()
    out = []
    for it in items:
        name = str(it.get("name", "")).strip()
        if not name:
            raise ValueError("opcode.name is required")

        val = parse_id(it.get("id"))
        if not (0 <= val <= 0xFFFF):
            raise ValueError(f"opcode id out of range: {name}={val}")

        desc = str(it.get("desc", ""))
        group = str(it.get("group", "")).strip()
        direction = str(it.get("dir", "")).strip()

        if require_group:
            if not group:
                raise ValueError(f"missing group for opcode: {name}")
            if group not in groups:
                raise ValueError(f"unknown group '{group}' for opcode: {name}")
            g = groups[group]
            if not (g["id_min"] <= val <= g["id_max"]):
                raise ValueError(
                    f"opcode id out of group range: {name}=0x{val:04X} not in {group}"
                    f" [0x{g['id_min']:04X}..0x{g['id_max']:04X}]"
                )

        if name in seen_names:
            raise ValueError(f"duplicate opcode name: {name}")
        if val in seen_ids:
            raise ValueError(f"duplicate opcode id: 0x{val:04X}")
        seen_names.add(name)
        seen_ids.add(val)

        out.append({
            "id": val,
            "name": name,
            "desc": desc,
            "group": group,
            "dir": direction,
        })

    if require_group:
        out.sort(key=lambda x: (group_index[x["group"]], x["id"]))
    else:
        out.sort(key=lambda x: x["id"])

    return group_order, groups, out


def escape_md(s: str) -> str:
    return s.replace("|", "\\|").replace("\n", " ").strip()


def render_spec_md(spec_path: Path, title: str) -> str:
    repo_root = Path(__file__).resolve().parent.parent
    spec = load_spec(spec_path)
    ns = str(spec.get("namespace", "")).strip()
    group_order, groups, opcodes = parse_opcodes(spec)

    lines = []
    lines.append(f"## {title}")
    lines.append("")
    try:
        rel = spec_path.relative_to(repo_root).as_posix()
    except Exception:
        rel = spec_path.as_posix()
    lines.append(f"- Source: `{rel}`")
    if ns:
        lines.append(f"- Namespace: `{ns}`")
    lines.append("")

    if group_order:
        by_group = {g: [] for g in group_order}
        for o in opcodes:
            by_group[o["group"]].append(o)

        for gname in group_order:
            g = groups[gname]
            gdesc = g.get("desc", "")
            lines.append(f"### {gname} (0x{g['id_min']:04X}..0x{g['id_max']:04X})")
            if gdesc:
                lines.append("")
                lines.append(escape_md(gdesc))
            lines.append("")
            lines.append("| ID | Name | Dir | Desc |")
            lines.append("|---:|------|:---:|------|")
            for o in by_group[gname]:
                oid = f"0x{o['id']:04X}"
                name = escape_md(o["name"])
                direction = escape_md(o.get("dir", ""))
                desc = escape_md(o.get("desc", ""))
                lines.append(f"| {oid} | `{name}` | `{direction}` | {desc} |")
            lines.append("")
    else:
        lines.append("| ID | Name | Dir | Desc |")
        lines.append("|---:|------|:---:|------|")
        for o in opcodes:
            oid = f"0x{o['id']:04X}"
            name = escape_md(o["name"])
            direction = escape_md(o.get("dir", ""))
            desc = escape_md(o.get("desc", ""))
            lines.append(f"| {oid} | `{name}` | `{direction}` | {desc} |")
        lines.append("")

    return "\n".join(lines)


def main(argv: List[str]) -> int:
    ap = argparse.ArgumentParser(description="Generate opcode docs (and validate uniqueness).")
    ap.add_argument("--out", default="docs/protocol/opcodes.md", help="Output markdown path")
    ap.add_argument("--check", action="store_true", help="Fail if output differs")
    args = ap.parse_args(argv)

    repo_root = Path(__file__).resolve().parent.parent
    sys_spec = repo_root / "core/protocol/system_opcodes.json"
    game_spec = repo_root / "server/protocol/game_opcodes.json"
    out_path = repo_root / args.out

    # Cross-file uniqueness (shared 16-bit msg_id space).
    all_ids: Dict[int, Tuple[str, str]] = {}
    for spec_path in (sys_spec, game_spec):
        spec = load_spec(spec_path)
        _group_order, _groups, opcodes = parse_opcodes(spec)
        for o in opcodes:
            oid = int(o["id"])
            if oid in all_ids:
                prev_name, prev_file = all_ids[oid]
                raise ValueError(
                    f"duplicate opcode id across specs: 0x{oid:04X} ({prev_name} in {prev_file})"
                    f" and ({o['name']} in {spec_path.as_posix()})"
                )
            all_ids[oid] = (o["name"], spec_path.as_posix())

    text = []
    text.append("# Opcodes")
    text.append("")
    text.append("Source of truth: `core/protocol/system_opcodes.json`, `server/protocol/game_opcodes.json`.")
    text.append("Generated by `tools/gen_opcode_docs.py`. Do not edit by hand.")
    text.append("")
    text.append(render_spec_md(sys_spec, "System (Core)") )
    text.append(render_spec_md(game_spec, "Game (Server)") )
    out = "\n".join(text).rstrip() + "\n"

    if args.check:
        if not out_path.exists():
            print(f"missing generated docs: {out_path}")
            return 1
        current = out_path.read_text(encoding="utf-8")
        if current.replace("\r\n", "\n") != out.replace("\r\n", "\n"):
            print(f"opcode docs out of date: {out_path}")
            print("run: python tools/gen_opcode_docs.py")
            return 1
        return 0

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(out, encoding="utf-8")
    print(f"[gen_opcode_docs] generated {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
