import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path


def _load_module():
    repo_root = Path(__file__).resolve().parents[2]
    module_path = repo_root / "tools" / "check_lua_build_toggle.py"

    spec = importlib.util.spec_from_file_location("check_lua_build_toggle", module_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"failed to load module spec: {module_path}")

    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


class LuaBuildToggleCheckerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.mod = _load_module()

    def _write_cache(self, build_dir: Path, value: str) -> None:
        content = f"# synthetic cache for tests\nBUILD_LUA_SCRIPTING:BOOL={value}\n"
        (build_dir / "CMakeCache.txt").write_text(content, encoding="utf-8")

    def _write_compile_commands(self, build_dir: Path, sources: list[str]) -> None:
        payload = [{"file": item} for item in sources]
        (build_dir / "compile_commands.json").write_text(
            json.dumps(payload),
            encoding="utf-8",
        )

    def test_off_mode_passes_with_disabled_runtime_source(self):
        with tempfile.TemporaryDirectory() as tmp:
            build_dir = Path(tmp)
            self._write_cache(build_dir, "OFF")
            self._write_compile_commands(
                build_dir,
                [
                    str(
                        build_dir
                        / "core"
                        / "src"
                        / "scripting"
                        / "lua_runtime_disabled.cpp"
                    ),
                ],
            )

            rc = self.mod.main(["--build-dir", str(build_dir), "--expect", "off"])
            self.assertEqual(0, rc)

    def test_off_mode_fails_when_enabled_runtime_source_exists(self):
        with tempfile.TemporaryDirectory() as tmp:
            build_dir = Path(tmp)
            self._write_cache(build_dir, "OFF")
            self._write_compile_commands(
                build_dir,
                [
                    str(build_dir / "core" / "src" / "scripting" / "lua_runtime.cpp"),
                ],
            )

            rc = self.mod.main(["--build-dir", str(build_dir), "--expect", "off"])
            self.assertEqual(1, rc)

    def test_on_mode_passes_with_enabled_runtime_source(self):
        with tempfile.TemporaryDirectory() as tmp:
            build_dir = Path(tmp)
            self._write_cache(build_dir, "ON")
            self._write_compile_commands(
                build_dir,
                [
                    str(build_dir / "core" / "src" / "scripting" / "lua_runtime.cpp"),
                ],
            )

            rc = self.mod.main(
                [
                    "--build-dir",
                    str(build_dir),
                    "--expect",
                    "on",
                    "--require-source-check",
                ]
            )
            self.assertEqual(0, rc)

    def test_missing_compile_commands_passes_without_require_flag(self):
        with tempfile.TemporaryDirectory() as tmp:
            build_dir = Path(tmp)
            self._write_cache(build_dir, "OFF")

            rc = self.mod.main(["--build-dir", str(build_dir), "--expect", "off"])
            self.assertEqual(0, rc)

    def test_missing_compile_commands_fails_with_require_flag(self):
        with tempfile.TemporaryDirectory() as tmp:
            build_dir = Path(tmp)
            self._write_cache(build_dir, "OFF")

            rc = self.mod.main(
                [
                    "--build-dir",
                    str(build_dir),
                    "--expect",
                    "off",
                    "--require-source-check",
                ]
            )
            self.assertEqual(1, rc)


if __name__ == "__main__":
    unittest.main()
