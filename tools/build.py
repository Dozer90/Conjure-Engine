#!/usr/bin/env python3
"""Cross-platform build driver for Conjure.

On Windows this imports the MSVC environment (vcvars64.bat, located through vswhere) so the Ninja
presets work from any shell. On Linux and macOS it simply forwards to CMake.

Examples:
    python tools/build.py                       # configure + build the default preset
    python tools/build.py --test                # ... and run ctest
    python tools/build.py --preset windows-msvc-dev --target conjure_core_tests
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
    parser.add_argument("--preset", default=default_preset(), help="CMake preset name (default: %(default)s)")
    parser.add_argument("--build-dir", default=None,
                        help="private build directory (default: build/<preset>); use when other builds may run concurrently")
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

    env = msvc_environment() if platform.system() == "Windows" else dict(os.environ)

    private_dir = pathlib.Path(args.build_dir).resolve() if args.build_dir else None
    binary_dir = private_dir or preset_binary_dir(args.preset)
    if args.clean and binary_dir.exists():
        print(f"+ removing {binary_dir}", flush=True)
        remove_tree(binary_dir)

    if not args.no_configure:
        configure = ["cmake", "--preset", args.preset]
        if private_dir:
            configure += ["-B", str(private_dir)]
        run(configure + list(args.cmake_args), env)

    if args.configure_only:
        return

    if private_dir:
        build = ["cmake", "--build", str(private_dir)]
    else:
        build = ["cmake", "--build", "--preset", args.preset]
    if args.jobs > 0:
        build += ["--parallel", str(args.jobs)]
    for target in args.target:
        build += ["--target", target]
    run(build, env)

    if args.test:
        if private_dir:
            test = ["ctest", "--test-dir", str(private_dir), "--output-on-failure"]
        else:
            test = ["ctest", "--preset", args.preset]
        if args.test_regex:
            test += ["-R", args.test_regex]
        if args.test_label:
            test += ["-L", args.test_label]
        run(test, env)


if __name__ == "__main__":
    main()
