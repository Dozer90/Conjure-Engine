#!/usr/bin/env python3
"""Self-test for tools/prune_builds.py.

Every case builds its own fake repository in a temporary directory (presets, build trees of chosen ages, folders that
are not build trees, in-source projects, read-only and hard-linked files, paths past 260 characters, links and
junctions pointing out of a tree, trees whose names differ only by a trailing dot, a tree with a file held open or a
program running from it) and asserts what the tool lists, selects, protects and removes, and that nothing outside the
selected trees changes. Paths are built by the test itself, not through the tool's helpers, and on Windows every file
operation the tool makes during a run is audited for a path past 260 characters without the extended-length prefix.
Runs as the CTest entry `build.prune_tool`; no third-party test framework is needed.
"""

from __future__ import annotations

import contextlib
import importlib.util
import io
import json
import os
import pathlib
import platform
import re
import shutil
import stat
import subprocess
import sys
import tempfile
import time

HERE = pathlib.Path(__file__).resolve().parent
TOOL = HERE.parent / "prune_builds.py"
REPOSITORY = HERE.parent.parent
DAY = 86400.0
WINDOWS = os.name == "nt"
AS_ROOT = not WINDOWS and os.geteuid() == 0
ROW = re.compile(r"^(\S+)\s+(\d+\.\d\d GB)\s+(\d+\.\d (?:days|hours) ago)  (.*)$")
MAX_PATH = 259
AUDITED_EVENTS = {"open", "os.remove", "os.rmdir", "os.rename", "os.chmod", "os.scandir", "os.listdir", "os.utime",
                  "os.mkdir", "shutil.rmtree", "ctypes.call_function"}

notes: list[str] = []
unprefixed_long_paths: list[str] | None = None


def load_tool():
    specification = importlib.util.spec_from_file_location("prune_builds", TOOL)
    module = importlib.util.module_from_spec(specification)
    assert specification.loader is not None
    specification.loader.exec_module(module)
    return module


def check(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def note(text: str) -> None:
    if text not in notes:
        notes.append(text)


def audit(event: str, arguments: tuple) -> None:
    """Records a path past MAX_PATH characters without the extended-length prefix while a tool run is watched."""
    if unprefixed_long_paths is None or event not in AUDITED_EVENTS:
        return
    candidates = arguments[1] if event == "ctypes.call_function" and len(arguments) > 1 else arguments[:2]
    for argument in candidates if isinstance(candidates, tuple) else ():
        if isinstance(argument, (str, bytes, os.PathLike)):
            text = os.fsdecode(argument)
            if len(text) > MAX_PATH and not text.startswith("\\\\?\\"):
                unprefixed_long_paths.append(f"{event}: {text}")


# ---------------------------------------------------------------------------------------------------------------
# Fixtures


def long_form(path: pathlib.Path | str) -> str:
    """The absolute path, with the extended-length prefix on Windows (the test's own, not the tool's)."""
    text = os.path.abspath(os.fspath(path))
    return "\\\\?\\" + text if WINDOWS else text


def short_form(path: str) -> str:
    return path[4:] if path.startswith("\\\\?\\") else path


def write_file(path: str, data: bytes = b"x") -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as stream:
        stream.write(data)


def is_link(entry: os.DirEntry) -> bool:
    if entry.is_symlink() or (hasattr(entry, "is_junction") and entry.is_junction()):
        return True
    return WINDOWS and bool(entry.stat(follow_symlinks=False).st_file_attributes & stat.FILE_ATTRIBUTE_REPARSE_POINT)


def walk(top: str):
    """Every entry below top as (path, entry); links are listed, never entered."""
    pending = [top]
    while pending:
        with os.scandir(pending.pop()) as entries:
            for entry in entries:
                yield entry.path, entry
                if not is_link(entry) and entry.is_dir(follow_symlinks=False):
                    pending.append(entry.path)


def snapshot(root: pathlib.Path) -> dict[str, int]:
    """Relative path -> size (-1 for a directory or link) of everything below root."""
    base = long_form(root)
    found: dict[str, int] = {}
    for path, entry in walk(base):
        plain_file = not is_link(entry) and entry.is_file(follow_symlinks=False)
        found[path[len(base):]] = entry.stat(follow_symlinks=False).st_size if plain_file else -1
    return found


def total_size(top: str) -> int:
    return sum(entry.stat(follow_symlinks=False).st_size for _, entry in walk(top)
               if not is_link(entry) and entry.is_file(follow_symlinks=False))


def set_age(top: str, days: float, now: float) -> None:
    """Gives every file and directory below top (not top itself) a modification time `days` days before now."""
    stamp = now - days * DAY
    for path, _ in sorted(walk(top), key=lambda item: -len(item[0])):
        os.utime(path, (stamp, stamp))


def cache_text(home: str) -> bytes:
    return f"# This is the CMakeCache file.\nCMAKE_HOME_DIRECTORY:INTERNAL={home}\n".encode("utf-8")


def make_tree(root: pathlib.Path, name: str, days: float, now: float, payload: int = 1000) -> str:
    """build/<name> as a configured Ninja tree of the checkout at root that last changed `days` days before now;
    returns its long form."""
    tree = long_form(root / "build") + os.sep + name
    write_file(tree + os.sep + "CMakeCache.txt", cache_text(root.as_posix()))
    write_file(tree + os.sep + "build.ninja", b"rule cc\n")
    write_file(os.path.join(tree, "obj", "a.obj"), b"x" * payload)
    set_age(tree, days, now)
    return tree


def write_presets(root: pathlib.Path, name: str, presets: list[dict], include: list[str] | None = None,
                  byte_order_mark: bool = False) -> None:
    document: dict = {"version": 10, "configurePresets": presets}
    if include is not None:
        document["include"] = include
    path = root / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(document), encoding="utf-8-sig" if byte_order_mark else "utf-8")


def make_repository(root: pathlib.Path, now: float) -> None:
    """Presets with one buildable preset (windows-msvc-debug), three build trees, a saved report folder, a log file
    and an in-source scratch project under build/."""
    write_presets(root, "CMakePresets.json", [
        {"name": "base", "hidden": True, "binaryDir": "${sourceDir}/build/${presetName}"},
        {"name": "windows-msvc-debug", "inherits": "base"},
    ])
    make_tree(root, "old_agent", 10.0, now, payload=4096)
    make_tree(root, "fresh_agent", 0.5, now)
    make_tree(root, "windows-msvc-debug", 30.0, now)
    build = long_form(root / "build")
    write_file(os.path.join(build, "saved_report", "findings.json"), b"[]")
    write_file(os.path.join(build, "notes.log"), b"log")
    scratch = os.path.join(build, "icon_lab")
    write_file(os.path.join(scratch, "CMakeLists.txt"), b"project(icon_lab)\n")
    write_file(os.path.join(scratch, "CMakeCache.txt"), cache_text(pathlib.Path(short_form(scratch)).as_posix()))
    set_age(build, 30.0, now)
    set_age(long_form(root / "build" / "fresh_agent"), 0.5, now)
    set_age(long_form(root / "build" / "old_agent"), 10.0, now)


def make_directory_link(link: str, target: str) -> None:
    """A link to a directory: a junction on Windows (no privilege needed), a symbolic link elsewhere."""
    if WINDOWS:
        subprocess.run(["cmd", "/c", "mklink", "/J", short_form(link), short_form(target)], check=True,
                       capture_output=True)
    else:
        os.symlink(target, link, target_is_directory=True)


def try_file_link(link: str, target: str) -> bool:
    """A symbolic link to a file; False on Windows without Developer Mode or the symlink privilege."""
    try:
        os.symlink(target, link)
    except OSError:
        if not WINDOWS:
            raise
        note("file symbolic links need Developer Mode on Windows; the junction checks ran without them")
        return False
    return True


def run(tool, root: pathlib.Path, arguments: list[str]) -> tuple[int, str, str]:
    """Runs the tool's main() with sizes printed in bytes ('<bytes>.00 GB', so the output format stays the same and
    fixtures of a few kilobytes can be checked exactly), and asserts that it never used a long path without the
    extended-length prefix."""
    global unprefixed_long_paths
    out = io.StringIO()
    err = io.StringIO()
    original_format = tool.format_size
    tool.format_size = lambda size: f"{size}.00 GB"
    unprefixed_long_paths = []
    try:
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = tool.main(["--root", str(root)] + arguments)
        violations = unprefixed_long_paths
    finally:
        unprefixed_long_paths = None
        tool.format_size = original_format
    check(not violations, f"long paths used without the \\\\?\\ prefix: {violations[:3]}")
    return code, out.getvalue(), err.getvalue()


def size_column(out: str, name: str) -> int:
    """The size the tool printed for a tree, in bytes (see run())."""
    return int(rows(out)[name][0].split(".")[0])


def tree_bytes(root: pathlib.Path, name: str) -> int:
    return total_size(long_form(root / "build") + os.sep + name)


def rows(out: str) -> dict[str, tuple[str, str, str]]:
    """name -> (size, age, action) of every tree line of the tool's output."""
    found = {}
    for line in out.splitlines():
        match = ROW.match(line)
        if match:
            found[match.group(1)] = (match.group(2), match.group(3), match.group(4))
    return found


def actions(out: str) -> dict[str, str]:
    return {name: row[2] for name, row in rows(out).items()}


def summary(out: str) -> str:
    lines = out.splitlines()
    return lines[-1] if lines else ""


def without(before: dict[str, int], *trees: str) -> dict[str, int]:
    """The snapshot minus everything inside the named build trees."""
    prefixes = tuple(f"{os.sep}build{os.sep}{tree}" for tree in trees)
    return {path: size for path, size in before.items()
            if not any(path == prefix or path.startswith(prefix + os.sep) for prefix in prefixes)}


def identity(path: str) -> tuple[int, int]:
    info = os.lstat(path)
    return info.st_dev, info.st_ino


def is_link_path(path: str) -> bool:
    info = os.lstat(path)
    return stat.S_ISLNK(info.st_mode) or (WINDOWS and bool(info.st_file_attributes & stat.FILE_ATTRIBUTE_REPARSE_POINT))


def remove_fixture(top: str) -> None:
    """Removes a case's directory even when a failed case left read-only or locked-down entries in it: every real
    directory is opened up top down first (links are not followed), then a failed unlink or rmdir is retried once
    after clearing the read-only bit."""
    if not WINDOWS:
        for current, folders, _ in os.walk(top):
            for folder in folders:
                path = os.path.join(current, folder)
                if not os.path.islink(path):
                    os.chmod(path, 0o700)

    def unlock(function, path: str, error) -> None:
        if function not in (os.unlink, os.remove, os.rmdir):
            raise error if isinstance(error, BaseException) else error[1]
        for target in (path, os.path.dirname(path)):
            with contextlib.suppress(OSError):
                os.chmod(target, stat.S_IWRITE | stat.S_IREAD | (stat.S_IEXEC if os.path.isdir(target) else 0))
        function(path)

    if sys.version_info >= (3, 12):
        shutil.rmtree(top, onexc=unlock)
    else:
        shutil.rmtree(top, onerror=unlock)


@contextlib.contextmanager
def temporary_directory():
    """A fresh directory, removed afterwards; a cleanup problem is reported as a note, never in place of the case's
    own result."""
    directory = tempfile.mkdtemp(prefix="prune_builds_")
    try:
        yield pathlib.Path(directory)
    finally:
        try:
            remove_fixture(long_form(directory))
        except Exception as error:
            note(f"could not remove the case directory {directory}: {type(error).__name__}: {error}")


# ---------------------------------------------------------------------------------------------------------------
# Listing and selection


def listing_reports_build_trees_only(tool, root: pathlib.Path) -> None:
    now = time.time()
    make_repository(root, now)
    before = snapshot(root)
    code, out, err = run(tool, root, [])
    check(code == 0 and not err, f"listing exits 0 quietly, got {code}: {err}")
    listed = rows(out)
    check(set(listed) == {"fresh_agent", "old_agent", "windows-msvc-debug"}, f"listed {sorted(listed)}")
    check(listed["old_agent"][1] == "10.0 days ago", f"old_agent age {listed['old_agent'][1]}")
    check(listed["fresh_agent"][1] == "12.0 hours ago", f"fresh_agent age {listed['fresh_agent'][1]}")
    check(actions(out) == {"fresh_agent": "keep", "old_agent": "keep", "windows-msvc-debug": "keep (preset)"},
          f"actions {actions(out)}")
    sizes = {name: tree_bytes(root, name) for name in listed}
    check(all(size_column(out, name) == size for name, size in sizes.items()), f"sizes {listed} != {sizes}")
    check(summary(out) == f"3 build trees, {sum(sizes.values())}.00 GB in all; --delete would free 0.00 GB",
          f"summary {summary(out)!r}")
    check(snapshot(root) == before, "listing changes nothing")


def dry_run_changes_nothing(tool, root: pathlib.Path) -> None:
    now = time.time()
    make_repository(root, now)
    before = snapshot(root)
    code, out, _ = run(tool, root, ["--all"])
    check(code == 0, f"exit {code}")
    check(actions(out) == {"fresh_agent": "would delete", "old_agent": "would delete",
                           "windows-msvc-debug": "keep (preset)"}, f"actions {actions(out)}")
    selected = tree_bytes(root, "fresh_agent") + tree_bytes(root, "old_agent")
    check(summary(out).endswith(f"; --delete would free {selected}.00 GB"), f"summary {summary(out)!r}")
    check(snapshot(root) == before, "a run without --delete changes nothing")


def older_than_removes_stale_trees(tool, root: pathlib.Path) -> None:
    now = time.time()
    make_repository(root, now)
    before = snapshot(root)
    old_size = tree_bytes(root, "old_agent")
    code, out, _ = run(tool, root, ["--older-than", "3", "--delete"])
    check(code == 0, f"exit {code}: {out}")
    check(actions(out) == {"fresh_agent": "keep", "old_agent": "deleted", "windows-msvc-debug": "keep (preset)"},
          f"actions {actions(out)}")
    check(summary(out).endswith(f"; freed {old_size}.00 GB"), f"summary {summary(out)!r}")
    check(snapshot(root) == without(before, "old_agent"), "only old_agent is removed, everything else is untouched")


def keep_and_include_presets(tool, root: pathlib.Path) -> None:
    now = time.time()
    make_repository(root, now)
    before = snapshot(root)
    # On Windows names compare without case, so a --keep in another case must protect the tree too.
    code, out, _ = run(tool, root, ["--all", "--keep", "build/Fresh_Agent" if WINDOWS else "build/fresh_agent",
                                    "--delete"])
    check(code == 0, f"exit {code}: {out}")
    check(actions(out)["fresh_agent"] == "keep (--keep)", f"actions {actions(out)}")
    check(snapshot(root) == without(before, "old_agent"), "--keep and the preset protect their trees from --all")
    code, out, _ = run(tool, root, ["--all", "--include-presets", "--delete"])
    check(code == 0, f"exit {code}: {out}")
    check(snapshot(root) == without(before, "old_agent", "fresh_agent", "windows-msvc-debug"),
          "--all --include-presets removes every build tree and nothing else")


def names_select_trees(tool, root: pathlib.Path) -> None:
    now = time.time()
    make_repository(root, now)
    make_tree(root, "third", 1.0, now)
    before = snapshot(root)
    code, out, _ = run(tool, root, ["old_agent", "build/fresh_agent/", "build\\third", "--delete"])
    check(code == 0, f"exit {code}: {out}")
    check(snapshot(root) == without(before, "old_agent", "fresh_agent", "third"),
          "NAME, build/NAME/ and build\\NAME each remove that tree only")
    code, out, _ = run(tool, root, ["windows-msvc-debug", "--delete"])
    check(code == 0 and actions(out)["windows-msvc-debug"] == "keep (preset)",
          "naming a preset tree without --include-presets keeps it")


def bad_names_stop_before_anything_is_removed(tool, root: pathlib.Path) -> None:
    now = time.time()
    make_repository(root, now)
    before = snapshot(root)
    for arguments in (["saved_report"], ["icon_lab"], ["notes.log"], ["missing"], ["../old_agent"],
                      ["build/old_agent/obj"], ["."], [".."], [""], ["old_agent", "missing"],
                      ["--all", "--keep", "missing"], ["--older-than", "-1"]):
        code, out, err = run(tool, root, arguments + ["--delete"])
        check(code == 2, f"{arguments}: exit {code}, expected 2")
        check(err.startswith("prune_builds: ") and not out, f"{arguments}: only an error is printed: {out!r} {err!r}")
        check(snapshot(root) == before, f"{arguments}: nothing is removed")


def in_source_projects_are_never_listed(tool, root: pathlib.Path) -> None:
    checkout = root / "checkout"
    now = time.time()
    make_repository(checkout, now)
    build = long_form(checkout / "build")
    alias = long_form(root / "alias")
    make_directory_link(alias, long_form(checkout))
    other_host = "/mnt/q/no/such/checkout" if WINDOWS else "Q:/no/such/checkout"
    projects = {
        "empty_cache": (b"", True),
        # A scratch project configured in place elsewhere and copied here: its cache names an existing outside
        # directory, so only its own CMakeLists.txt shows that it is a source tree.
        "copied_project": (cache_text(checkout.as_posix()), True),
        "no_home": (b"CMAKE_BUILD_TYPE:STRING=Debug\n", False),
        "other_host": (cache_text(other_host), False),
        "source_inside": (cache_text(pathlib.Path(short_form(build), "source_inside", "src").as_posix()), False),
        "through_alias": (cache_text(pathlib.Path(short_form(alias), "build", "through_alias").as_posix()), False),
    }
    for name, (cache, with_lists) in projects.items():
        write_file(os.path.join(build, name, "CMakeCache.txt"), cache)
        write_file(os.path.join(build, name, "src", "CMakeLists.txt"), b"project(scratch)\n")
        write_file(os.path.join(build, name, "src", "main.cpp"), b"int main() { return 0; }\n")
        if with_lists:
            write_file(os.path.join(build, name, "CMakeLists.txt"), b"project(scratch)\n")
    before = snapshot(checkout)
    code, out, _ = run(tool, checkout, ["--all", "--include-presets", "--delete"])
    check(code == 0, f"exit {code}: {out}")
    check(set(rows(out)) == {"fresh_agent", "old_agent", "windows-msvc-debug"}, f"listed {sorted(rows(out))}")
    check(snapshot(checkout) == without(before, "fresh_agent", "old_agent", "windows-msvc-debug"),
          "every in-source project and every tree configured elsewhere survives --all --include-presets")
    for name in list(projects) + ["icon_lab"]:
        code, _, err = run(tool, checkout, [name, "--delete"])
        check(code == 2 and name in err, f"naming {name} is a usage error: {err!r}")


# ---------------------------------------------------------------------------------------------------------------
# Sizes and ages


def last_use_is_the_newest_change_anywhere(tool, root: pathlib.Path) -> None:
    now = time.time()
    tree = long_form(root / "build" / "vs_tree")
    tlog = os.path.join(tree, "x64", "Debug", "engine.tlog", "CL.write.1.tlog")
    write_file(tree + os.sep + "CMakeCache.txt", cache_text(root.as_posix()))
    write_file(os.path.join(tree, "engine.vcxproj"), b"<Project/>")
    write_file(tlog, b"t" * 300)
    set_age(tree, 10.0, now)
    os.utime(tlog, (now - DAY / 2, now - DAY / 2))
    code, out, _ = run(tool, root, ["--older-than", "3"])
    check(code == 0 and rows(out)["vs_tree"][1] == "12.0 hours ago" and actions(out)["vs_tree"] == "keep",
          f"a Visual Studio tree's age comes from its newest file: {rows(out)}")
    set_age(tree, 10.0, now)
    os.utime(os.path.dirname(tlog), (now - DAY / 2, now - DAY / 2))
    code, out, _ = run(tool, root, ["--older-than", "3"])
    check(rows(out)["vs_tree"][1] == "12.0 hours ago" and actions(out)["vs_tree"] == "keep",
          f"a directory changed by a deletion counts as a change: {rows(out)}")
    set_age(tree, 10.0, now)
    code, out, _ = run(tool, root, ["--older-than", "3"])
    check(rows(out)["vs_tree"][1] == "10.0 days ago" and actions(out)["vs_tree"] == "would delete",
          f"once nothing changed for ten days it is selected: {rows(out)}")
    size, newest, foreign = tool.scan(tree)
    check(size == total_size(tree), f"scan counts every file: {size} != {total_size(tree)}")
    check(abs(newest - (now - 10.0 * DAY)) < 1.0 and foreign is None, "scan reports the newest modification time")


def scan_does_not_follow_links(tool, root: pathlib.Path) -> None:
    now = time.time()
    tree = make_tree(root, "linked_out", 1.0, now)
    outside = long_form(root / "outside")
    write_file(os.path.join(outside, "big.bin"), b"b" * 100000)
    expected = total_size(tree)
    make_directory_link(os.path.join(tree, "obj", "to_outside"), outside)
    size, _, _ = tool.scan(tree)
    check(size == expected, f"a linked directory is not counted: {size} != {expected}")


# ---------------------------------------------------------------------------------------------------------------
# Presets


def preset_binary_dirs_are_protected(tool, root: pathlib.Path) -> None:
    os.environ["CONJURE_PRUNE_TEST_VARIABLE"] = "value42"
    for unset in ("CONJURE_NO_SUCH_VARIABLE", "CONJURE_PRUNE_FLAVOR", "CONJURE_PRUNE_NULLED"):
        os.environ.pop(unset, None)
    write_presets(root, "CMakePresets.json", [
        {"name": "base", "hidden": True, "binaryDir": "${sourceDir}/build/${presetName}"},
        {"name": "ci", "inherits": "base"},
        {"name": "custom", "binaryDir": "${sourceDir}/build/custom-dir"},
        {"name": "relative", "binaryDir": "build/relative-dir"},
        {"name": "elsewhere", "binaryDir": "${sourceDir}/out/elsewhere"},
        {"name": "nested", "binaryDir": "${sourceDir}/build/deeper/nested"},
        {"name": "empty-env", "binaryDir": "$env{CONJURE_NO_SUCH_VARIABLE}/build/x"},
        {"name": "nobinary"},
        {"name": "fixed-base", "hidden": True, "binaryDir": "${sourceDir}/build/shared-dir"},
        {"name": "child", "inherits": "fixed-base"},
        # The first parent that sets binaryDir wins, as in CMake.
        {"name": "second", "inherits": ["unrelated", "fixed-base", "base"]},
        {"name": "unrelated", "hidden": True, "generator": "Ninja"},
        # An empty string counts as unset, so the parent's value is inherited (CMake's InheritString).
        {"name": "inh-base", "hidden": True, "binaryDir": "${sourceDir}/build/${presetName}-inh"},
        {"name": "empty-bd", "inherits": "inh-base", "binaryDir": ""},
        # ${fileDir} is the directory of the visible preset's file, even when binaryDir is inherited from a preset
        # in an included file elsewhere.
        {"name": "fd-child", "inherits": "fd-base"},
        {"name": "gen-base", "hidden": True, "generator": "Ninja"},
        {"name": "gen-child", "inherits": "gen-base", "binaryDir": "${sourceDir}/build/g-${generator}"},
        # A null environment entry is skipped, so $env{} falls back to the process environment, as does a name the
        # preset does not set.
        {"name": "null-env", "environment": {"CONJURE_PRUNE_TEST_VARIABLE": None},
         "binaryDir": "${sourceDir}/build/null-$env{CONJURE_PRUNE_TEST_VARIABLE}"},
        {"name": "process-fallback", "binaryDir": "${sourceDir}/build/proc-$env{CONJURE_PRUNE_TEST_VARIABLE}"},
        # The earlier parent's environment wins over a later one's; a child's null entry hides a parent's value.
        {"name": "env-p1", "hidden": True, "environment": {"CONJURE_PRUNE_FLAVOR": "one"}},
        {"name": "env-p2", "hidden": True, "environment": {"CONJURE_PRUNE_FLAVOR": "two"}},
        {"name": "env-two-parents", "inherits": ["env-p1", "env-p2"],
         "binaryDir": "${sourceDir}/build/env2-$env{CONJURE_PRUNE_FLAVOR}"},
        {"name": "nul-base", "hidden": True, "environment": {"CONJURE_PRUNE_NULLED": "fromparent"}},
        {"name": "nul-child", "inherits": "nul-base", "environment": {"CONJURE_PRUNE_NULLED": None},
         "binaryDir": "${sourceDir}/build/nul-$env{CONJURE_PRUNE_NULLED}x"},
        {"name": "host", "binaryDir": "${sourceDir}/build/${hostSystemName}-host"},
        {"name": "money", "binaryDir": "${sourceDir}/build/cost${dollar}dir"},
        {"name": "separated", "binaryDir": "${sourceDir}/build/a${pathListSep}b"},
        {"name": "generated", "generator": "Ninja Multi-Config", "binaryDir": "${sourceDir}/build/${generator}"},
        {"name": "env-base", "hidden": True, "environment": {"TREE_SUFFIX": "from-parent", "OTHER": "unused"}},
        {"name": "env-child", "inherits": "env-base", "environment": {"OTHER": "$env{TREE_SUFFIX}-again"},
         "binaryDir": "${sourceDir}/build/env-$env{OTHER}"},
        {"name": "process-env", "binaryDir": "${sourceDir}/build/penv-$penv{CONJURE_PRUNE_TEST_VARIABLE}"},
        {"name": "missing-env", "binaryDir": "${sourceDir}/build/miss$env{CONJURE_NO_SUCH_VARIABLE}ing"},
        {"name": "vendor", "binaryDir": "${sourceDir}/build/$vendor{ide.folder}-vendor"},
    ], include=["cmake/more-presets.json", "${sourceDir}/cmake/macro-included.json"])
    write_presets(root, "cmake/more-presets.json", [
        {"name": "included", "binaryDir": "${fileDir}/../build/included-dir"},
        {"name": "fd-base", "hidden": True, "binaryDir": "${fileDir}/build/${presetName}"},
    ], include=["nested/extra.json"])
    write_presets(root, "cmake/nested/extra.json", [{"name": "deep", "binaryDir": "${sourceDir}/build/nested-dir"}])
    write_presets(root, "cmake/macro-included.json",
                  [{"name": "macro-included", "binaryDir": "${sourceDir}/build/macro-dir"}])
    write_presets(root, "CMakeUserPresets.json", [
        {"name": "mine", "binaryDir": "${sourceDir}/build/my-tree"},
        {"name": "mine-inherited", "inherits": "base"},
        {"name": "by-name", "binaryDir": "${sourceParentDir}/" + root.name + "/build/${sourceDirName}-tree"},
    ], byte_order_mark=True)
    expected = {"ci", "custom-dir", "relative-dir", "nobinary", "shared-dir", "included-dir", "nested-dir",
                "macro-dir", "my-tree", "mine-inherited", root.name + "-tree", platform.system() + "-host",
                "cost$dir", "a" + os.pathsep + "b", "Ninja Multi-Config", "env-from-parent-again", "penv-value42",
                "missing", "*-vendor", "empty-bd-inh", "fd-child", "g-Ninja", "null-value42", "proc-value42",
                "env2-one", "nul-x"}
    try:
        found = tool.preset_directories(root)
    finally:
        os.environ.pop("CONJURE_PRUNE_TEST_VARIABLE", None)
    check(found == expected, f"protected {sorted(found)}; missing {sorted(expected - found)}, "
                             f"extra {sorted(found - expected)}")

    now = time.time()
    for name in ("base", "custom-dir", "my-tree", "included-dir", "nested-dir", "elsewhere", "loose", "ide-vendor",
                 platform.system().lower() + "-host"):
        make_tree(root, name, 10.0, now)
    code, out, _ = run(tool, root, ["--all", "--delete"])
    check(code == 0, f"exit {code}: {out}")
    check(actions(out) == {"base": "deleted", "custom-dir": "keep (preset)", "elsewhere": "deleted",
                           "ide-vendor": "keep (preset)", "included-dir": "keep (preset)", "loose": "deleted",
                           "my-tree": "keep (preset)", "nested-dir": "keep (preset)",
                           platform.system().lower() + "-host": "keep (preset)" if WINDOWS else "deleted"},
          f"actions {actions(out)}")


def malformed_presets_are_usage_errors(tool, root: pathlib.Path) -> None:
    now = time.time()
    make_repository(root, now)
    before = snapshot(root)
    user = root / "CMakeUserPresets.json"
    for text in ("{ not json", "[]", '{"configurePresets": {}}', '{"include": "other.json"}', '{"include": [3]}',
                 '{"include": ["missing.json"]}', '{"include": ["$env{HOME}/presets.json"]}',
                 '{"include": ["$vendor{x}.json"]}',
                 '{"configurePresets": [{"name": "a", "inherits": "missing"}]}',
                 '{"configurePresets": [{"name": "a", "inherits": ["missing"]}]}',
                 '{"configurePresets": [{"name": "a", "inherits": 5}]}',
                 '{"configurePresets": [{"name": "a", "inherits": ["a", "a"]}]}',
                 '{"configurePresets": [{"name": "a", "inherits": "b"}, {"name": "b", "inherits": ["c", "a"]},'
                 ' {"name": "c"}]}',
                 '{"configurePresets": [{"name": "windows-msvc-debug"}]}'):
        user.write_text(text, encoding="utf-8")
        started = time.monotonic()
        code, _, err = run(tool, root, ["--all", "--delete"])
        check(code == 2 and "CMakeUserPresets.json" in err, f"{text!r}: exit {code}, {err!r}")
        check(time.monotonic() - started < 10.0, f"{text!r}: the error is found at once")
    user.unlink()
    check(snapshot(root) == before, "a malformed preset file stops the tool before anything is removed")


def preset_files_are_read_once_by_identity(tool, root: pathlib.Path) -> None:
    """An include reached twice (a diamond, a cycle, or the same file through a link to the checkout) is read once,
    as CMake does, so its presets are not duplicates."""
    checkout = root / "checkout"
    os.makedirs(long_form(checkout / "build"))
    alias = long_form(root / "alias")
    make_directory_link(alias, long_form(checkout))
    write_presets(checkout, "CMakePresets.json", [], include=["cmake/a.json", "cmake/b.json"])
    write_presets(checkout, "cmake/a.json", [{"name": "a-visible", "binaryDir": "${sourceDir}/build/a-dir"}],
                  include=["common.json"])
    write_presets(checkout, "cmake/b.json", [], include=["common.json"])
    write_presets(checkout, "cmake/common.json", [{"name": "common-visible", "binaryDir": "${sourceDir}/build/c-dir"}],
                  include=["a.json"])
    write_presets(checkout, "CMakeUserPresets.json", [],
                  include=[pathlib.Path(short_form(alias), "cmake", "common.json").as_posix()])
    started = time.monotonic()
    found = tool.preset_directories(checkout)
    check(found == {"a-dir", "c-dir"} and time.monotonic() - started < 10.0, f"protected {sorted(found)}")


def preset_binary_dir_through_a_link_is_protected(tool, root: pathlib.Path) -> None:
    checkout = root / "checkout"
    now = time.time()
    make_tree(checkout, "aliased-dir", 10.0, now)
    make_tree(checkout, "loose", 10.0, now)
    alias = long_form(root / "alias")
    make_directory_link(alias, long_form(checkout))
    write_presets(checkout, "CMakeUserPresets.json",
                  [{"name": "aliased", "binaryDir": pathlib.Path(short_form(alias), "build", "aliased-dir").as_posix()}])
    code, out, _ = run(tool, checkout, ["--all"])
    check(code == 0 and actions(out) == {"aliased-dir": "keep (preset)", "loose": "would delete"},
          f"a binaryDir spelled through a link to the checkout protects its tree: {actions(out)}")


def repository_presets_are_protected(tool, root: pathlib.Path) -> None:
    shutil.copyfile(REPOSITORY / "CMakePresets.json", root / "CMakePresets.json")
    document = json.loads((REPOSITORY / "CMakePresets.json").read_text(encoding="utf-8"))
    visible = {preset["name"] for preset in document["configurePresets"] if not preset.get("hidden")}
    check({"windows-msvc-debug", "linux-gcc-debug", "macos-debug"} <= visible, f"presets {sorted(visible)}")
    found = tool.preset_directories(root)
    check(found == visible, f"every configure preset's build/<preset> is protected: {sorted(found ^ visible)}")


# ---------------------------------------------------------------------------------------------------------------
# Links, identity and mounts


def links_under_build_are_ignored(tool, root: pathlib.Path) -> None:
    now = time.time()
    make_repository(root, now)
    elsewhere = long_form(root / "elsewhere_tree")
    write_file(elsewhere + os.sep + "CMakeCache.txt", cache_text(root.as_posix()))
    write_file(os.path.join(elsewhere, "obj", "precious.obj"), b"p" * 2000)
    make_directory_link(long_form(root / "build" / "linked_tree"), elsewhere)
    before = snapshot(root)
    code, out, _ = run(tool, root, [])
    check(code == 0 and "linked_tree" not in rows(out), f"a link directly under build/ is not listed: {rows(out)}")
    code, _, _ = run(tool, root, ["linked_tree", "--delete"])
    check(code == 2, "naming a link directly under build/ is a usage error")
    code, out, _ = run(tool, root, ["--all", "--include-presets", "--delete"])
    check(code == 0, f"exit {code}: {out}")
    check(snapshot(root) == without(before, "old_agent", "fresh_agent", "windows-msvc-debug"),
          "the link and the tree it points at survive --all --include-presets")


def links_inside_a_tree_are_removed_not_followed(tool, root: pathlib.Path) -> None:
    now = time.time()
    tree = make_tree(root, "hard", 10.0, now)
    outside = long_form(root / "outside")
    write_file(os.path.join(outside, "precious.txt"), b"keep me")
    write_file(os.path.join(outside, "inner", "deep.txt"), b"deep")
    make_directory_link(os.path.join(tree, "linked"), outside)
    make_directory_link(os.path.join(tree, "obj", "nested_link"), outside)
    if try_file_link(os.path.join(tree, "file_link"), os.path.join(outside, "precious.txt")):
        try_file_link(os.path.join(tree, "obj", "nested_file_link"), os.path.join(outside, "precious.txt"))
    before = snapshot(root)
    code, out, _ = run(tool, root, ["hard", "--delete"])
    check(code == 0, f"exit {code}: {out}")
    check(snapshot(root) == without(before, "hard"), "the tree is gone and the link targets are untouched")


def replace_tree(tree: str, outside: str | None, home: pathlib.Path) -> None:
    """What another process might do to build/NAME: move the listed tree aside (its inode stays in use, so the
    identity cannot be reused) and put a link to `outside`, or a freshly configured real directory, in its place."""
    os.rename(tree, tree + "_moved")
    if outside is not None:
        make_directory_link(tree, outside)
        return
    write_file(tree + os.sep + "CMakeCache.txt", cache_text(home.as_posix()))
    write_file(os.path.join(tree, "obj", "new_work.obj"), b"another agent's new build")


def replaced_tree_is_left_alone(tool, root: pathlib.Path) -> None:
    """A swap after listing is caught before the move; a swap between the first check and the move is caught on the
    moved-aside directory, which is then put back."""
    for moment in ("after listing", "before the move"):
        for kind in ("link", "directory"):
            repository = root / f"{moment.replace(' ', '_')}_{kind}"
            now = time.time()
            make_repository(repository, now)
            outside = long_form(repository / "outside_tree")
            write_file(outside + os.sep + "CMakeCache.txt", cache_text(repository.as_posix()))
            write_file(os.path.join(outside, "work", "thesis.txt"), b"years of work")
            tree = long_form(repository / "build" / "old_agent")
            hooks = {"after listing": "scan", "before the move": "before_moving"}
            original = getattr(tool, hooks[moment])
            after_swap: list[dict[str, int]] = []

            def swapping(path: str, original=original, outside=outside, tree=tree, repository=repository,
                         kind=kind, after_swap=after_swap):
                result = original(path) if hooks[moment] == "scan" else None
                if path == tree and not after_swap:
                    replace_tree(tree, outside if kind == "link" else None, repository)
                    after_swap.append(snapshot(repository))
                return result if hooks[moment] == "scan" else original(path)

            setattr(tool, hooks[moment], swapping)
            try:
                code, out, _ = run(tool, repository, ["old_agent", "--delete"])
            finally:
                setattr(tool, hooks[moment], original)
            label = f"{kind} swapped in {moment}"
            check(after_swap and code == 1, f"{label}: exit {code}: {out}")
            action = actions(out)["old_agent"]
            check(action.startswith("not removed: ") and "replaced since it was listed; nothing was removed" in action,
                  f"{label}: {action!r}")
            check(not action.endswith("run again to retry"),
                  f"{label}: running again would remove the replacement, so it is not suggested: {action!r}")
            check(summary(out).endswith("; freed 0.00 GB"), f"{label}: {summary(out)!r}")
            check(snapshot(repository) == after_swap[0], f"{label}: the replacement, the link target and the moved "
                                                         f"tree are untouched, the replacement back at its name")


def build_directory_linked_to_another_disk_works(tool, root: pathlib.Path) -> None:
    """build/ as a link (junction on Windows) to a directory on another disk, a common way to make room for the
    trees. On Linux and macOS the other disk is simulated: the store reports its own device."""
    checkout = root / "checkout"
    store = long_form(root / "store")
    os.makedirs(long_form(checkout))
    os.makedirs(store)
    make_directory_link(long_form(checkout / "build"), store)
    make_tree(checkout, "agent", 10.0, time.time())
    original = tool.device
    real_store = os.path.realpath(store)

    def device(path: str) -> int:
        located = os.path.join(os.path.realpath(os.path.dirname(path)), os.path.basename(path))
        return 4242 if located == real_store or located.startswith(real_store + os.sep) else original(path)

    if not WINDOWS:
        tool.device = device
    try:
        code, out, _ = run(tool, checkout, ["agent", "--delete"])
    finally:
        tool.device = original
    check(code == 0 and actions(out) == {"agent": "deleted"}, f"exit {code}: {out}")
    check(os.listdir(store) == [] and is_link_path(long_form(checkout / "build")),
          "the tree in the store is removed and build/ stays a link")


def gone_tree_is_reported_without_freeing_anything(tool, root: pathlib.Path) -> None:
    now = time.time()
    make_repository(root, now)
    original = tool.scan

    def vanishing_scan(path: str):
        result = original(path)
        if path.endswith(os.sep + "old_agent"):
            remove_fixture(path)
        return result

    tool.scan = vanishing_scan
    try:
        code, out, _ = run(tool, root, ["old_agent", "--delete"])
    finally:
        tool.scan = original
    action = actions(out)["old_agent"]
    check(code == 1 and action.startswith("not removed: ") and "gone since it was listed" in action, f"{out}")
    check(not action.endswith("run again to retry") and summary(out).endswith("; freed 0.00 GB"), f"{out}")


def mounted_file_systems_are_never_entered(tool, root: pathlib.Path) -> None:
    text = "36 35 98:0 /mnt1 /mnt/with\\040space rw,noatime master:1 - ext3 /dev/root rw\n25 1 0:1 / / rw - x y z\n"
    check(tool.parse_mountinfo(text) == ["/mnt/with space", "/"], f"{tool.parse_mountinfo(text)}")
    if WINDOWS:
        check(tool.mount_inside("C:\\x", ["C:\\x"]) is None and not tool.is_mount_point("C:\\Windows"),
              "Windows mounts are reparse points, never entered")
        return
    now = time.time()
    make_repository(root, now)
    tree = long_form(root / "build" / "old_agent")
    original_points = tool.mount_points
    original_device = tool.device
    # Linux lists mounts in /proc/self/mountinfo; macOS has only st_dev, so each source is checked alone.
    sources = {
        "mountinfo, inside": (lambda: ["/", "/proc", os.path.realpath(os.path.join(tree, "obj"))], original_device),
        "st_dev, inside": (lambda: [], lambda path: 4242 if path.endswith(os.sep + "obj") else original_device(path)),
        # The tree and everything in it lie on the mounted file system; only build/ differs.
        "st_dev, the tree itself": (lambda: [], lambda path: 4242 if os.sep + "old_agent" in path
                                    else original_device(path)),
    }
    try:
        for label, (points, device) in sources.items():
            tool.mount_points = points
            tool.device = device
            before = snapshot(root)
            code, out, _ = run(tool, root, ["--all"])
            check(code == 0 and actions(out)["old_agent"] == "keep (a file system is mounted inside)",
                  f"{label}: a tree with a mount is kept: {actions(out)}")
            failures, location, started, retryable = tool.remove_tree(tree, identity(tree))
            check(len(failures) == 1 and "a file system is mounted here; nothing was removed" in failures[0]
                  and location == tree and not started and not retryable,
                  f"{label}: a mount found just before removal stops it: {failures}")
            check(snapshot(root) == before, f"{label}: nothing is removed and the tree keeps its name")
    finally:
        tool.mount_points = original_points
        tool.device = original_device


# ---------------------------------------------------------------------------------------------------------------
# Removal


def read_only_and_long_paths_are_removed(tool, root: pathlib.Path) -> None:
    # A long tree name puts even the files directly in the tree (and in its moved-aside name) past the limit.
    name = "stubborn_" + "n" * 210
    now = time.time()
    tree = make_tree(root, name, 10.0, now)
    check(len(short_form(tree + os.sep + "CMakeCache.txt")) > 260, "files directly in the tree exceed the limit")
    write_file(os.path.join(tree, "obj", "pack.idx"), b"git pack index")
    os.chmod(os.path.join(tree, "obj", "pack.idx"), stat.S_IREAD)
    deep = tree
    for index in range(12):
        deep = os.path.join(deep, f"nested_directory_level_{index:02d}_with_a_long_name")
    write_file(os.path.join(deep, "object_file_past_the_limit.obj"), b"d" * 777)
    os.chmod(os.path.join(deep, "object_file_past_the_limit.obj"), stat.S_IREAD)
    check(len(short_form(deep)) > 300, "the deep path really exceeds the 260-character limit")
    if WINDOWS:
        read_only_directory = os.path.join(tree, "obj", "read_only_directory")
        write_file(os.path.join(read_only_directory, "inside.obj"), b"r")
        os.chmod(read_only_directory, stat.S_IREAD)
        os.chmod(tree, stat.S_IREAD)
    check(tool.scan(tree)[0] == total_size(tree), "scan counts the files past 260 characters")
    before = snapshot(root)
    code, out, _ = run(tool, root, [name, "--delete"])
    check(code == 0, f"exit {code}: {out}")
    check(snapshot(root) == without(before, name),
          "read-only files, read-only directories (the tree itself too) and paths past the limit are removed")


def hard_linked_read_only_files_keep_their_bit(tool, root: pathlib.Path) -> None:
    now = time.time()
    tree = make_tree(root, "linked_pack", 10.0, now)
    outside = long_form(root / "mirror" / "pack.pack")
    write_file(outside, b"shared pack")
    os.link(outside, os.path.join(tree, "obj", "pack.pack"))
    os.link(outside, os.path.join(tree, "top.pack"))
    os.chmod(outside, stat.S_IREAD)
    try:
        code, out, _ = run(tool, root, ["linked_pack", "--delete"])
        check(code == 0 and not os.path.lexists(tree), f"exit {code}: {out}")
        with open(outside, "rb") as stream:
            check(stream.read() == b"shared pack", "the other name keeps the content")
        check(not os.stat(outside).st_mode & stat.S_IWRITE, "the other name keeps its read-only bit")
    finally:
        os.chmod(outside, stat.S_IWRITE | stat.S_IREAD)


def locked_link_at_the_top_of_a_tree_is_removed(tool, root: pathlib.Path) -> None:
    """A link directly in the tree that refuses a plain unlink: read-only on Windows (attrib +r /L), in a tree whose
    directory is not writable elsewhere. The tree holds nothing else, so no other repair can open the way first."""
    tree = long_form(root / "build" / "locked_link")
    write_file(tree + os.sep + "CMakeCache.txt", cache_text(root.as_posix()))
    outside = long_form(root / "outside")
    write_file(os.path.join(outside, "precious.txt"), b"keep me")
    link = os.path.join(tree, "to_outside")
    make_directory_link(link, outside)
    if WINDOWS:
        subprocess.run(["attrib", "+r", "/L", short_form(link)], check=True, capture_output=True)
    else:
        os.chmod(tree, 0o555)
    before = snapshot(root)
    code, out, _ = run(tool, root, ["locked_link", "--delete"])
    check(code == 0 and not os.path.lexists(tree), f"exit {code}: {out}")
    check(snapshot(root) == without(before, "locked_link"), "only the link is removed, never its target")


def cache_goes_last_and_a_partial_removal_can_be_retried(tool, root: pathlib.Path) -> None:
    now = time.time()
    make_repository(root, now)
    original = tool.remove_directory

    def failing(path: str, failures: list[str], opened: set[str]) -> None:
        if os.path.basename(path) == "obj":
            failures.append(f"{path}: simulated failure")
            return
        original(path, failures, opened)

    old_size = tree_bytes(root, "old_agent")
    tool.remove_directory = failing
    try:
        code, out, _ = run(tool, root, ["old_agent", "--delete"])
    finally:
        tool.remove_directory = original
    check(code == 1, f"a partial removal exits 1, got {code}")
    action = actions(out)["old_agent"]
    check(action.startswith("not removed completely: ") and "simulated failure" in action
          and action.endswith("; run again to retry"), f"action {action!r}")
    tree = long_form(root / "build" / "old_agent")
    check(os.path.isfile(tree + os.sep + "CMakeCache.txt"), "CMakeCache.txt stays while anything else is left, and "
                                                            "the tree is back at its own name")
    check(not os.path.exists(tree + os.sep + "build.ninja"), "everything that could be removed is removed")
    check(summary(out).endswith(f"; freed {old_size - total_size(tree)}.00 GB"),
          f"only what was removed counts as freed: {summary(out)!r}")
    code, out, _ = run(tool, root, [])
    check("old_agent" in rows(out), "the half-removed tree is still listed")
    code, out, _ = run(tool, root, ["old_agent", "--delete"])
    check(code == 0 and not os.path.lexists(tree), f"running again removes the rest: {out}")


def trailing_dot_names_are_separate_trees(tool, root: pathlib.Path) -> None:
    now = time.time()
    agent = make_tree(root, "agent", 10.0, now, payload=2222)
    dotted = make_tree(root, "agent.", 0.5, now, payload=3333)
    check(tool.scan(agent)[0] == total_size(agent) and tool.scan(dotted)[0] == total_size(dotted)
          and total_size(agent) != total_size(dotted), "each tree is measured through its own name")
    before = snapshot(root)
    code, out, _ = run(tool, root, ["--older-than", "3"])
    listed = rows(out)
    check(listed.get("agent", ("", "", ""))[1:] == ("10.0 days ago", "would delete")
          and listed.get("agent.", ("", "", ""))[1:] == ("12.0 hours ago", "keep"), f"rows {listed}")
    code, out, _ = run(tool, root, ["--all", "--keep", "agent", "--delete"])
    check(code == 0, f"exit {code}: {out}")
    check(snapshot(root) == without(before, "agent."), "removing agent. leaves agent and its files alone")
    make_tree(root, "agent.", 0.5, now, payload=3333)
    code, out, _ = run(tool, root, ["agent.", "--delete"])
    check(code == 0 and snapshot(root) == without(before, "agent."), "naming agent. removes only agent.")


def tree_in_use_is_left_alone(tool, root: pathlib.Path) -> None:
    now = time.time()
    make_repository(root, now)
    before = snapshot(root)
    held = open(os.path.join(long_form(root / "build" / "old_agent"), "obj", "a.obj"), "rb")
    try:
        code, out, _ = run(tool, root, ["old_agent", "--delete"])
    finally:
        held.close()
    action = actions(out)["old_agent"]
    check(code == 1 and "in use or a process works in it; nothing was removed" in action, f"exit {code}: {out}")
    check(action.startswith("not removed: ") and action.endswith("; run again to retry"), f"{action!r}")
    check(summary(out).endswith("; freed 0.00 GB"), f"{summary(out)!r}")
    check(snapshot(root) == before, "nothing of a tree in use is removed")
    code, out, _ = run(tool, root, ["old_agent", "--delete"])
    check(code == 0 and snapshot(root) == without(before, "old_agent"), f"once released it is removed: {out}")


def running_program_leaves_its_tree_alone(tool, root: pathlib.Path) -> None:
    now = time.time()
    make_repository(root, now)
    program = os.path.join(long_form(root / "build" / "old_agent"), "bin", "waiter.exe")
    os.makedirs(os.path.dirname(program))
    shutil.copyfile(os.path.join(os.environ["SystemRoot"], "System32", "PING.EXE"), program)
    other_name = long_form(root / "elsewhere.exe")
    expected = {"writable": "loaded by a running program (in use; nothing was removed)",
                "read-only": "loaded by a running program (in use; nothing was removed)",
                "read-only and hard-linked": "read-only with other hard links, so it cannot be checked for a running "
                                             "program; nothing was removed"}
    for variant, message in expected.items():
        if variant == "read-only":
            os.chmod(program, stat.S_IREAD)
        if variant == "read-only and hard-linked":
            os.link(program, other_name)
        before = snapshot(root)
        process = subprocess.Popen([short_form(program), "-n", "60", "127.0.0.1"], cwd=str(root),
                                   stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            code, out, _ = run(tool, root, ["old_agent", "--delete"])
        finally:
            process.kill()
            process.wait()
        check(code == 1 and message in actions(out)["old_agent"]
              and actions(out)["old_agent"].endswith("; run again to retry"), f"{variant}: exit {code}: {out}")
        check(snapshot(root) == before, f"{variant}: nothing of a tree with a running program is removed")
        check((variant != "writable") == (not os.stat(program).st_mode & stat.S_IWRITE),
              f"{variant}: the program keeps its read-only bit")
    os.chmod(other_name, stat.S_IWRITE)
    os.unlink(other_name)
    code, out, _ = run(tool, root, ["old_agent", "--delete"])
    check(code == 0 and not os.path.lexists(os.path.dirname(os.path.dirname(program))),
          f"once the program has exited the tree is removed: {out}")


def permissions_are_repaired_inside_the_tree_only(tool, root: pathlib.Path) -> None:
    now = time.time()
    tree = make_tree(root, "locked", 10.0, now)
    outside = os.path.join(long_form(root), "outside")
    write_file(os.path.join(outside, "inner", "precious.txt"), b"keep me")
    write_file(os.path.join(outside, "alone.txt"), b"alone")
    make_directory_link(os.path.join(tree, "obj", "to_outside"), outside)
    # The last two hold only a subdirectory: a file next to it would be repaired first and make the parent searchable.
    for name, mode, with_file in (("read_only", 0o500, True), ("unreadable", 0o000, True), ("write_only", 0o300, True),
                                  ("unsearchable", 0o600, False), ("list_only", 0o444, False)):
        directory = os.path.join(tree, "obj", name)
        write_file(os.path.join(directory, "deeper", "file.o"), b"o")
        if with_file:
            write_file(os.path.join(directory, "top.o"), b"o")
        # Links in a directory that is not writable fail to unlink at first, so the retry runs on them.
        make_directory_link(os.path.join(directory, "deeper", "to_outside"), outside)
        try_file_link(os.path.join(directory, "deeper", "to_alone"), os.path.join(outside, "alone.txt"))
        os.chmod(os.path.join(directory, "deeper"), 0o500)
        os.chmod(directory, mode)
    os.chmod(tree, 0o555)
    os.chmod(os.path.join(outside, "alone.txt"), 0o400)
    os.chmod(os.path.join(outside, "inner"), 0o500)
    os.chmod(outside, 0o500)
    try:
        before = snapshot(root / "outside")
        code, out, _ = run(tool, root, ["locked", "--delete"])
        check(code == 0, f"exit {code}: {out}")
        check(not os.path.lexists(tree), "read-only, unreadable, write-only and unsearchable directories are removed")
        check(snapshot(root / "outside") == before, "the link targets are untouched")
        modes = [stat.S_IMODE(os.lstat(os.path.join(outside, name)).st_mode) for name in ("", "inner", "alone.txt")]
        check(modes == [0o500, 0o500, 0o400], f"the permissions outside the tree are unchanged: {modes}")
    finally:
        for path in (outside, os.path.join(outside, "inner")):
            os.chmod(path, 0o700)


def unwritable_build_directory_stops_before_removing(tool, root: pathlib.Path) -> None:
    now = time.time()
    make_tree(root, "agent", 10.0, now)
    build = long_form(root / "build")
    before = snapshot(root)
    os.chmod(build, 0o555)
    try:
        code, out, _ = run(tool, root, ["agent", "--delete"])
    finally:
        os.chmod(build, 0o755)
    action = actions(out)["agent"]
    check(code == 1 and "not writable" in action and "nothing was removed" in action, f"exit {code}: {out}")
    check(snapshot(root) == before, "nothing is removed when the tree itself could not be removed")
    code, out, _ = run(tool, root, ["agent", "--delete"])
    check(code == 0 and snapshot(root) == without(before, "agent"), f"once build/ is writable it is removed: {out}")


# ---------------------------------------------------------------------------------------------------------------
# Helpers and formats


def paths_names_and_formats(tool, root: pathlib.Path) -> None:
    if WINDOWS:
        check(tool.extended("C:\\a\\b") == "\\\\?\\C:\\a\\b", tool.extended("C:\\a\\b"))
        check(tool.extended("\\\\server\\share\\x") == "\\\\?\\UNC\\server\\share\\x",
              tool.extended("\\\\server\\share\\x"))
        check(tool.extended("\\\\?\\C:\\build\\agent.") == "\\\\?\\C:\\build\\agent.",
              "an extended path keeps its trailing dot")
        check(tool.shown("\\\\?\\UNC\\server\\share\\x") == "\\\\server\\share\\x", "shown drops the UNC prefix")
        check(tool.shown("\\\\?\\C:\\a") == "C:\\a", "shown drops the prefix")
        check(tool.plain("\\\\?\\C:\\A\\Agent.") == "c:\\a\\agent.", tool.plain("\\\\?\\C:\\A\\Agent."))
        check(tool.same_name("Agent", "agent") and not tool.same_name("agent.", "agent"),
              "names compare without case but with their trailing dots")
        check(tool.matches_preset("Windows-Host", {"windows-host"}), "preset patterns compare without case")
    else:
        check(tool.extended("/a/b/../c") == "/a/c", tool.extended("/a/b/../c"))
        check(tool.extended("relative/x") == os.path.abspath("relative/x"), "a relative path is made absolute")
        check(not tool.same_name("Agent", "agent"), "names compare with case")
    check(tool.as_pattern("a*b?c[d" + tool.WILDCARD + "e") == "a[*]b[?]c[[]d*e", tool.as_pattern("a*b?c[d"))
    check(tool.matches_preset("a*b", {tool.as_pattern("a*b")}) and not tool.matches_preset("axb", {"a[*]b"}),
          "special characters in a binaryDir are matched literally")
    check(tool.format_size(5 * 1024 ** 3) == "5.00 GB", tool.format_size(5 * 1024 ** 3))
    check(tool.format_age(2 * DAY) == "2.0 days ago" and tool.format_age(1800) == "0.5 hours ago",
          f"{tool.format_age(2 * DAY)} / {tool.format_age(1800)}")
    check(tool.tree_name("build\\x\\") == "x" and tool.tree_name("x") == "x", "tree names")


def single_tree_summary(tool, root: pathlib.Path) -> None:
    make_tree(root, "only", 1.0, time.time(), payload=10)
    code, out, _ = run(tool, root, [])
    check(code == 0 and summary(out).startswith("1 build tree, "), f"summary {summary(out)!r}")
    code, out, _ = run(tool, (root / "nowhere"), [])
    check(code == 0 and summary(out) == "0 build trees, 0.00 GB in all; --delete would free 0.00 GB",
          f"a repository without build/ lists nothing: {out!r}")


def main() -> int:
    tool = load_tool()
    if WINDOWS:
        sys.addaudithook(audit)
    cases = [
        ("listing reports the build trees only, with sizes, ages and protections", listing_reports_build_trees_only),
        ("a run without --delete changes nothing", dry_run_changes_nothing),
        ("--older-than removes only stale trees and nothing outside them", older_than_removes_stale_trees),
        ("--keep and the presets protect trees; --include-presets lifts the preset protection",
         keep_and_include_presets),
        ("NAME, build/NAME/ and build\\NAME select trees", names_select_trees),
        ("a bad name, an unknown --keep or a negative age stops before anything is removed",
         bad_names_stop_before_anything_is_removed),
        ("in-source projects and trees configured on another host are never listed, however they are spelled",
         in_source_projects_are_never_listed),
        ("a tree's last use is the newest change of any file or directory in it",
         last_use_is_the_newest_change_anywhere),
        ("sizes do not follow links", scan_does_not_follow_links),
        ("binaryDir of presets, user presets and (nested, macro-named) included files is protected, macros expanded",
         preset_binary_dirs_are_protected),
        ("a malformed preset file, a missing include or parent, a cycle or a duplicate name is a usage error",
         malformed_presets_are_usage_errors),
        ("a binaryDir spelled through a link to the checkout is protected", preset_binary_dir_through_a_link_is_protected),
        ("a preset file included twice (diamond, cycle, link) is read once", preset_files_are_read_once_by_identity),
        ("a build/ linked to another disk works", build_directory_linked_to_another_disk_works),
        ("every configure preset of the repository is protected", repository_presets_are_protected),
        ("links directly under build/ are never listed or followed", links_under_build_are_ignored),
        ("links inside a tree are removed, their targets kept", links_inside_a_tree_are_removed_not_followed),
        ("a tree replaced by a link or another directory after listing or before the move is left alone",
         replaced_tree_is_left_alone),
        ("a tree removed by someone else meanwhile is reported, nothing counted as freed",
         gone_tree_is_reported_without_freeing_anything),
        ("a tree that is or holds a mount point is never removed (mountinfo and st_dev)",
         mounted_file_systems_are_never_entered),
        ("read-only files and directories and paths past 260 characters are removed",
         read_only_and_long_paths_are_removed),
        ("a hard-linked read-only file is removed and its other name keeps the bit",
         hard_linked_read_only_files_keep_their_bit),
        ("CMakeCache.txt goes last, so a partial removal stays listed and can be retried",
         cache_goes_last_and_a_partial_removal_can_be_retried),
        ("names that differ by a trailing dot are separate trees", trailing_dot_names_are_separate_trees),
        ("paths, names, patterns and formats", paths_names_and_formats),
        ("the summary counts trees", single_tree_summary),
    ]
    if WINDOWS:
        cases.append(("a tree with a file held open is left alone until it is released", tree_in_use_is_left_alone))
        cases.append(("a tree a program runs from is left alone until it exits, also when it cannot be checked",
                      running_program_leaves_its_tree_alone))
    elif AS_ROOT:
        note("running as root, which ignores permissions: the permission repair cases did not run")
    else:
        cases.append(("read-only, unreadable and unsearchable directories are repaired inside the tree only",
                      permissions_are_repaired_inside_the_tree_only))
        cases.append(("an unwritable build/ stops the tool before it removes anything",
                      unwritable_build_directory_stops_before_removing))
    if not AS_ROOT:
        cases.append(("a locked link directly in a tree is removed, its target kept",
                      locked_link_at_the_top_of_a_tree_is_removed))

    failures: list[str] = []
    for title, case in cases:
        with temporary_directory() as directory:
            try:
                case(tool, directory)
            except Exception as failure:
                failures.append(f"prune_builds.py - {title}: {type(failure).__name__}: {failure}")
                continue
        print(f"ok: prune_builds.py - {title}")

    for text in notes:
        print(f"note: {text}")
    if failures:
        print("\n".join(f"FAIL: {failure}" for failure in failures))
        return 1
    print(f"build.prune_tool: {len(cases)} case(s) passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
