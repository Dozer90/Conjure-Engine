#!/usr/bin/env python3
"""Cross-platform build driver for Conjure.

On Windows this imports the MSVC environment (vcvars64.bat, located through vswhere) so the Ninja
presets work from any shell. On Linux and macOS it simply forwards to CMake.

--preset names a build preset or a configure preset of CMakePresets.json or CMakeUserPresets.json. A build
preset brings its configure preset and, for a multi-config generator, its configuration (windows-vs-debug
configures windows-vs and builds and tests Debug); the test preset of the same name runs --test. A configure
preset without a build preset of its name (windows-vs) is accepted with --configure-only only.

Examples:
    python tools/build.py                       # configure + build the default preset
    python tools/build.py --test                # ... and run ctest
    python tools/build.py --preset windows-msvc-dev --target conjure_core_tests
    python tools/build.py --preset windows-vs-debug --test   # the multi-config windows-vs tree, Debug
    python tools/build.py --test -R core        # run only tests matching "core"
    python tools/build.py --configure-only -- -DCONJURE_WITH_PHYSICS=OFF
    python tools/build.py --build-dir build/my_area --target conjure_core_math_tests --test -R core_math
"""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import platform
import shutil
import stat
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
PRESETS_FILE = ROOT / "CMakePresets.json"
USER_PRESETS_FILE = ROOT / "CMakeUserPresets.json"
PRESET_KINDS = ("configurePresets", "buildPresets", "testPresets")


def load_presets(root: pathlib.Path) -> dict[str, list[dict]]:
    """The configure, build and test presets of <root>/CMakePresets.json, followed by those of
    <root>/CMakeUserPresets.json when that file exists (personal presets, docs/handbook/repository-layout.md).
    Raises ValueError naming the file when one is malformed."""
    presets: dict[str, list[dict]] = {kind: [] for kind in PRESET_KINDS}
    for path in (root / PRESETS_FILE.name, root / USER_PRESETS_FILE.name):
        if not path.is_file():
            continue
        try:
            document = json.loads(path.read_text(encoding="utf-8"))
        except ValueError as error:
            raise ValueError(f"{path}: {error}") from error
        if not isinstance(document, dict):
            raise ValueError(f"{path}: expected a JSON object")
        for kind in PRESET_KINDS:
            entries = document.get(kind, [])
            if not isinstance(entries, list) or not all(isinstance(entry, dict) for entry in entries):
                raise ValueError(f"{path}: {kind} must be a list of objects")
            presets[kind].extend(entries)
    return presets


def visible_preset(entries: list[dict], name: str) -> dict | None:
    """The non-hidden preset called 'name', or None."""
    return next((entry for entry in entries if entry.get("name") == name and not entry.get("hidden")), None)


def inherited_field(entries: list[dict], preset: dict, field: str) -> str | None:
    """'field' of a build preset, or of the first preset in its `inherits` chain (a name or a list of names,
    searched in order, depth first) that sets it."""
    stack = [preset]
    visited: set[str] = set()
    while stack:
        current = stack.pop()
        if field in current:
            value = current[field]
            return value if isinstance(value, str) else None
        parents = current.get("inherits", [])
        names = [parents] if isinstance(parents, str) else list(parents)
        # Pushed in reverse so the first parent (and its own parents) is searched first.
        for parent_name in reversed(names):
            if parent_name in visited:
                continue
            visited.add(parent_name)
            parent = next((entry for entry in entries if entry.get("name") == parent_name), None)
            if parent is not None:
                stack.append(parent)
    return None


def resolve_presets(
  name: str,
  presets: dict[str, list[dict]],
  need_build: bool,
) -> tuple[str, str | None, str | None, str | None]:
    """(configure preset, build preset, test preset, configuration) for --preset 'name'. A build preset names
    its configure preset and, for a multi-config generator, its configuration (windows-vs-debug: windows-vs,
    Debug); its test preset is the test preset of the same name, if any. A configure preset alone is enough
    for --configure-only; building needs a build preset, so a configure preset without one of its name is
    rejected with the build presets that drive it. Raises ValueError for an unusable name."""
    build_entries = presets.get("buildPresets", [])
    build = visible_preset(build_entries, name)
    if build is not None:
        configure = inherited_field(build_entries, build, "configurePreset")
        if not configure:
            raise ValueError(f"build preset '{name}' names no configurePreset")
        configuration = inherited_field(build_entries, build, "configuration")
        test = name if visible_preset(presets.get("testPresets", []), name) is not None else None
        return configure, name, test, configuration
    if visible_preset(presets.get("configurePresets", []), name) is None:
        raise ValueError(f"unknown preset '{name}'")
    if not need_build:
        return name, None, None, None
    choices = sorted(
        entry["name"] for entry in build_entries
        if "name" in entry and not entry.get("hidden")
        and inherited_field(build_entries, entry, "configurePreset") == name
    )
    if not choices:
        raise ValueError(f"'{name}' is a configure preset without a build preset; add one to "
                         f"{USER_PRESETS_FILE.name} or pass --configure-only")
    raise ValueError(f"'{name}' is a configure preset without a build preset of its name; use one of: "
                     f"{', '.join(choices)}")


def default_preset() -> str:
    system = platform.system()
    if system == "Windows":
        return "windows-msvc-debug"
    if system == "Darwin":
        return "macos-debug"
    return "linux-gcc-debug"


def find_vswhere() -> pathlib.Path | None:
    program_files = os.environ.get("ProgramFiles(x86)") or os.environ.get("ProgramFiles")
    if not program_files:
        return None
    candidate = pathlib.Path(program_files) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    return candidate if candidate.exists() else None


def msvc_environment() -> dict[str, str]:
    """Returns the environment produced by vcvars64.bat for the newest Visual Studio."""
    if os.environ.get("VSCMD_ARG_TGT_ARCH") == "x64" and shutil.which("cl"):
        return dict(os.environ)

    vswhere = find_vswhere()
    if vswhere is None:
        raise SystemExit("vswhere.exe not found; install Visual Studio with the C++ workload")

    query = subprocess.run(
        [str(vswhere), "-latest", "-prerelease", "-products", "*", "-requires",
         "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"],
        capture_output=True, text=True, check=True,
    )
    install_path = query.stdout.strip()
    if not install_path:
        raise SystemExit("No Visual Studio installation with the C++ toolset was found")

    vcvars = pathlib.Path(install_path) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"
    if not vcvars.exists():
        raise SystemExit(f"vcvars64.bat not found at {vcvars}")

    marker = "__CONJURE_ENV_START__"
    # cmd /s strips the first and last quote of the whole command string, so wrap it once more.
    command = f'""{vcvars}" >nul 2>&1 && echo {marker} && set"'
    # Run from the toolset directory: vcvars pushes/pops directories and fails on some mapped drives.
    # Pass a single string: a list would make Python backslash-escape the quotes, which cmd cannot parse.
    completed = subprocess.run(
        f"cmd.exe /s /c {command}", capture_output=True, text=True, encoding="oem", cwd=str(vcvars.parent)
    )
    if completed.returncode != 0:
        raise SystemExit(f"vcvars64.bat failed ({completed.returncode}): {completed.stdout} {completed.stderr}")
    output = completed.stdout

    environment: dict[str, str] = {}
    seen_marker = False
    for line in output.splitlines():
        if line.strip() == marker:
            seen_marker = True
            continue
        if not seen_marker or "=" not in line:
            continue
        key, _, value = line.partition("=")
        environment[key] = value

    if not environment:
        raise SystemExit("Failed to import the MSVC environment")

    # Visual Studio bundles Ninja; make it available when the shell has none.
    ninja_dir = pathlib.Path(install_path) / "Common7" / "IDE" / "CommonExtensions" / "Microsoft" / "CMake" / "Ninja"
    cmake_dir = pathlib.Path(install_path) / "Common7" / "IDE" / "CommonExtensions" / "Microsoft" / "CMake" / "CMake" / "bin"
    extra = [str(p) for p in (ninja_dir, cmake_dir) if p.exists()]
    if extra:
        # cmd prints the variable as "Path"; look it up case-insensitively so we extend rather
        # than shadow it.
        path_key = next((key for key in environment if key.upper() == "PATH"), "PATH")
        environment[path_key] = os.pathsep.join(extra + [environment.get(path_key, "")])
    return environment


def remove_tree(path: pathlib.Path) -> None:
    """Deletes a build tree, clearing the read-only bit git leaves on fetched pack files on Windows."""

    def on_error(function, failed_path, _exc_info):
        os.chmod(failed_path, stat.S_IWRITE)
        function(failed_path)

    shutil.rmtree(path, onerror=on_error)


def run(command: list[str], env: dict[str, str]) -> None:
    print("+", " ".join(command), flush=True)
    result = subprocess.run(command, cwd=ROOT, env=env)
    if result.returncode != 0:
        raise SystemExit(result.returncode)


def preset_binary_dir(preset: str) -> pathlib.Path:
    return ROOT / "build" / preset


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--preset", default=default_preset(),
                        help="configure or build preset; a multi-config configure preset such as windows-vs is "
                             "driven through its build presets windows-vs-debug / windows-vs-dev "
                             "(default: %(default)s)")
    parser.add_argument("--build-dir", default=None,
                        help="private build directory (default: build/<configure preset>, e.g. build/windows-vs for "
                             "windows-vs-debug); use when other builds may run concurrently")
    parser.add_argument("--configure-only", action="store_true", help="only run the configure step")
    parser.add_argument("--no-configure", action="store_true", help="skip configure (build dir must exist)")
    parser.add_argument("--target", action="append", default=[], help="build only these targets (repeatable)")
    parser.add_argument("--test", action="store_true", help="run ctest after building")
    parser.add_argument("-R", dest="test_regex", default=None, help="ctest regex filter")
    parser.add_argument("-L", dest="test_label", default=None, help="ctest label filter")
    parser.add_argument("--clean", action="store_true", help="delete the preset's build directory first")
    parser.add_argument("--jobs", "-j", type=int, default=0, help="parallel build jobs (0 = auto)")
    parser.add_argument("cmake_args", nargs="*", help="extra arguments passed to the configure step (after --)")
    args = parser.parse_args()

    try:
        presets = load_presets(ROOT)
        configure_preset, build_preset, test_preset, configuration = resolve_presets(
            args.preset, presets, need_build=not args.configure_only)
    except ValueError as error:
        parser.error(str(error))
    if args.test and not args.build_dir and test_preset is None:
        parser.error(f"preset '{args.preset}' has no test preset")

    env = msvc_environment() if platform.system() == "Windows" else dict(os.environ)

    private_dir = pathlib.Path(args.build_dir).resolve() if args.build_dir else None
    binary_dir = private_dir or preset_binary_dir(configure_preset)
    if args.clean and binary_dir.exists():
        print(f"+ removing {binary_dir}", flush=True)
        remove_tree(binary_dir)

    if not args.no_configure:
        configure = ["cmake", "--preset", configure_preset]
        if private_dir:
            configure += ["-B", str(private_dir)]
        run(configure + list(args.cmake_args), env)

    if args.configure_only:
        return

    if private_dir:
        build = ["cmake", "--build", str(private_dir)]
        if configuration:
            build += ["--config", configuration]
    else:
        build = ["cmake", "--build", "--preset", build_preset]
    if args.jobs > 0:
        build += ["--parallel", str(args.jobs)]
    for target in args.target:
        build += ["--target", target]
    run(build, env)

    if args.test:
        if private_dir:
            test = ["ctest", "--test-dir", str(private_dir), "--output-on-failure"]
            if configuration:
                test += ["-C", configuration]
        else:
            test = ["ctest", "--preset", test_preset]
        if args.test_regex:
            test += ["-R", args.test_regex]
        if args.test_label:
            test += ["-L", args.test_label]
        run(test, env)


if __name__ == "__main__":
    main()
