#!/usr/bin/env python3
"""Self-test for the preset resolution of tools/build.py.

Resolves the presets of the real CMakePresets.json and of a fake CMakeUserPresets.json in a temporary
directory, and asserts what tools/build.py would hand to `cmake --preset`, `cmake --build --preset` and
`ctest --preset`. Runs as the CTest entry `build.driver_tool`; no third-party test framework is needed.
"""

from __future__ import annotations

import importlib.util
import json
import pathlib
import shutil
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
DRIVER = HERE.parent / "build.py"
REPOSITORY = HERE.parent.parent


def load_driver():
    specification = importlib.util.spec_from_file_location("conjure_build_driver", DRIVER)
    module = importlib.util.module_from_spec(specification)
    assert specification.loader is not None
    specification.loader.exec_module(module)
    return module


def repository_presets(driver, temporary: pathlib.Path) -> dict:
    """The presets of the committed CMakePresets.json alone (a developer's CMakeUserPresets.json is left out)."""
    root = temporary / "repository"
    root.mkdir()
    shutil.copyfile(REPOSITORY / "CMakePresets.json", root / "CMakePresets.json")
    return driver.load_presets(root)


def expect_value_error(action, needles: list[str]) -> None:
    try:
        action()
    except ValueError as error:
        message = str(error)
        missing = [needle for needle in needles if needle not in message]
        assert not missing, f"message {message!r} lacks {missing}"
        return
    raise AssertionError("no ValueError was raised")


def build_preset_resolves(driver, presets: dict) -> None:
    resolved = driver.resolve_presets("windows-vs-debug", presets, need_build=True)
    assert resolved == ("windows-vs", "windows-vs-debug", "windows-vs-debug", "Debug"), resolved


def build_preset_without_test_preset(driver, presets: dict) -> None:
    resolved = driver.resolve_presets("windows-vs-dev", presets, need_build=True)
    assert resolved == ("windows-vs", "windows-vs-dev", None, "Development"), resolved


def configure_preset_rejected(driver, presets: dict) -> None:
    expect_value_error(lambda: driver.resolve_presets("windows-vs", presets, need_build=True),
                       ["windows-vs", "windows-vs-debug", "windows-vs-dev"])
    resolved = driver.resolve_presets("windows-vs", presets, need_build=False)
    assert resolved == ("windows-vs", None, None, None), resolved


def single_config_resolves(driver, presets: dict) -> None:
    resolved = driver.resolve_presets("linux-gcc-debug", presets, need_build=True)
    assert resolved == ("linux-gcc-debug", "linux-gcc-debug", "linux-gcc-debug", None), resolved


def unknown_preset_rejected(driver, presets: dict) -> None:
    expect_value_error(lambda: driver.resolve_presets("no-such-preset", presets, need_build=True),
                       ["unknown preset 'no-such-preset'"])
    # Hidden presets are building blocks, not something to pass to --preset.
    expect_value_error(lambda: driver.resolve_presets("base", presets, need_build=False), ["unknown preset 'base'"])


def write_user_presets(root: pathlib.Path, document: object) -> None:
    shutil.copyfile(REPOSITORY / "CMakePresets.json", root / "CMakePresets.json")
    (root / "CMakeUserPresets.json").write_text(json.dumps(document), encoding="utf-8")


def user_presets_resolve(driver, temporary: pathlib.Path) -> None:
    root = temporary / "user"
    root.mkdir()
    write_user_presets(root, {
        "version": 8,
        "buildPresets": [{"name": "mine", "configurePreset": "linux-gcc-debug"}],
    })
    presets = driver.load_presets(root)
    resolved = driver.resolve_presets("mine", presets, need_build=True)
    assert resolved == ("linux-gcc-debug", "mine", None, None), resolved


def inherited_fields_resolve(driver, temporary: pathlib.Path) -> None:
    root = temporary / "inherited"
    root.mkdir()
    write_user_presets(root, {
        "version": 8,
        "buildPresets": [
            {"name": "vs-base", "hidden": True, "configurePreset": "windows-vs"},
            {"name": "vs-release-ish", "hidden": True, "inherits": "vs-base", "configuration": "Development"},
            {"name": "mine-vs", "inherits": ["unrelated", "vs-release-ish"]},
            {"name": "unrelated", "hidden": True, "jobs": 4},
        ],
    })
    presets = driver.load_presets(root)
    resolved = driver.resolve_presets("mine-vs", presets, need_build=True)
    assert resolved == ("windows-vs", "mine-vs", None, "Development"), resolved
    expect_value_error(lambda: driver.resolve_presets("windows-vs", presets, need_build=True),
                       ["mine-vs", "windows-vs-debug", "windows-vs-dev"])


def malformed_user_presets_rejected(driver, temporary: pathlib.Path) -> None:
    root = temporary / "malformed"
    root.mkdir()
    shutil.copyfile(REPOSITORY / "CMakePresets.json", root / "CMakePresets.json")
    (root / "CMakeUserPresets.json").write_text("{ not json", encoding="utf-8")
    expect_value_error(lambda: driver.load_presets(root), ["CMakeUserPresets.json"])


def main() -> int:
    driver = load_driver()
    failures: list[str] = []
    with tempfile.TemporaryDirectory(prefix="build_driver_") as directory:
        temporary = pathlib.Path(directory)
        presets = repository_presets(driver, temporary)
        cases = [
            ("build.py - a build preset resolves to its configure preset and configuration",
             lambda: build_preset_resolves(driver, presets)),
            ("build.py - a multi-config build preset without a test preset has none",
             lambda: build_preset_without_test_preset(driver, presets)),
            ("build.py - a configure preset without a build preset of its name is rejected with the choices",
             lambda: configure_preset_rejected(driver, presets)),
            ("build.py - single-config presets resolve to themselves",
             lambda: single_config_resolves(driver, presets)),
            ("build.py - an unknown preset is rejected",
             lambda: unknown_preset_rejected(driver, presets)),
            ("build.py - presets of CMakeUserPresets.json are resolved",
             lambda: user_presets_resolve(driver, temporary)),
            ("build.py - a build preset inheriting its configurePreset and configuration resolves them",
             lambda: inherited_fields_resolve(driver, temporary)),
            ("build.py - a malformed CMakeUserPresets.json is reported by name",
             lambda: malformed_user_presets_rejected(driver, temporary)),
        ]
        for title, case in cases:
            try:
                case()
            except Exception as failure:
                failures.append(f"{title}: {type(failure).__name__}: {failure}")
                continue
            print(f"ok: {title}")

    if failures:
        print("\n".join(f"FAIL: {failure}" for failure in failures))
        return 1
    print(f"build.driver_tool: {len(cases)} case(s) passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
