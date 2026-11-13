#!/usr/bin/env python3
"""Lists and removes the CMake build trees directly under build/ (docs/handbook/getting-started.md section 6).

Parallel work gives every agent its own build directory (AGENTS.md section 4), and a full Debug tree of the engine
is 11 to 13 GB, so private trees must be removed once no further round of work will use them.

A build tree is a real directory directly under build/ that holds a CMakeCache.txt and no CMakeLists.txt, and
whose recorded source directory (CMAKE_HOME_DIRECTORY) exists on this host and is neither the tree nor inside it,
compared by file identity so that links, junctions and drive aliases cannot disguise an in-source project. A tree
configured on another host that shares the checkout (Windows and WSL) is listed only there. Anything else under
build/ (saved reports, in-source scratch projects, links, logs) is never listed and never touched.

Without --delete the tool only reports: every build tree with its size, the time anything in it last changed (a
configure, a build, a test run or a clean, whatever the generator), and what --delete would do to it.

Selecting trees (a tree is removed when it is selected and not protected):
  NAME ...            the named trees, written as NAME or build/NAME
  --older-than DAYS   trees in which nothing changed for more than DAYS days
  --all               every tree
Protecting trees:
  the binary directories of the configure presets (CMakePresets.json, CMakeUserPresets.json and the files they
  include) are kept unless --include-presets is given; a macro whose value cannot be known here (such as
  $vendor{}) stands for any text in its place
  --keep NAME         keep this tree as well (repeatable)
  a tree that is or holds a mount point (Linux, macOS) is always kept

Removal never follows a link or junction: a link inside a tree is removed and its target is left alone. The tool
first renames the tree to a private name next to it (NAME.prune-<process id>) and checks that what it renamed is
still the directory it listed, so another process that recreates or relinks build/NAME meanwhile cannot redirect
the removal; when anything stops the removal the tree is renamed back. On Windows it works through extended-length
paths, so files past 260 characters are counted and removed, deletes read-only files (git leaves fetched pack files
read-only) without changing the read-only bit their other hard links share, and leaves a tree untouched while a
file in it is held open, a process works in it or a program or library runs from it. On Linux and macOS it makes
read-only, unreadable and unsearchable directories inside the tree accessible, and it refuses to start when
build/ itself is not writable; it cannot tell that a tree is in use there. CMakeCache.txt is removed last, so a
tree that could not be removed completely stays listed and running the tool again retries it.

Usage:
    python tools/prune_builds.py                                   # list the build trees
    python tools/prune_builds.py --older-than 3                    # what --delete would remove
    python tools/prune_builds.py --older-than 3 --delete
    python tools/prune_builds.py core_math build/mod_ecs --delete
    python tools/prune_builds.py --all --keep my_tree --delete

Exit code 0 on success, 1 when a selected tree could not be removed completely, 2 on usage errors.
"""

from __future__ import annotations

import argparse
import errno
import fnmatch
import functools
import json
import os
import pathlib
import platform
import re
import shutil
import stat
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parent.parent

PRESET_FILES = ("CMakePresets.json", "CMakeUserPresets.json")
MAX_EXPANSION_DEPTH = 16
MOVE_ATTEMPTS = 10
RENAME_BACK_ATTEMPTS = 100
REMOVE_ATTEMPTS = 5
RETRY_PAUSE_SECONDS = 0.1
SECONDS_PER_DAY = 86400.0
REMOVERS = (os.unlink, os.remove, os.rmdir)
DIRECTORY_OPENERS = (os.open, os.scandir, os.listdir, os.lstat)
EXTENDED_PREFIX = "\\\\?\\"
EXTENDED_UNC_PREFIX = "\\\\?\\UNC\\"
MACRO = re.compile(r"\$\{(?P<name>\w+)\}|\$(?P<kind>env|penv|vendor)\{(?P<variable>[^}]*)\}")
WILDCARD = ""
FNMATCH_SPECIALS = "*?["

# Files a running process maps as images on Windows; a mapped image blocks deletion but not a directory rename.
IMAGE_SUFFIXES = (".exe", ".dll", ".pyd", ".node", ".ocx", ".cpl", ".sys", ".scr")
DELETE_ACCESS = 0x00010000
FILE_WRITE_DATA = 0x00000002
FILE_SHARE_ALL = 0x00000007
OPEN_EXISTING = 3
FILE_FLAG_BACKUP_SEMANTICS = 0x02000000
FILE_FLAG_OPEN_REPARSE_POINT = 0x00200000
FILE_DISPOSITION_INFO_EX = 21
# FILE_DISPOSITION_FLAG_DELETE | FILE_DISPOSITION_FLAG_POSIX_SEMANTICS | FILE_DISPOSITION_FLAG_IGNORE_READONLY_ATTRIBUTE
FILE_DISPOSITION_FLAGS = 0x00000001 | 0x00000002 | 0x00000010
ERROR_ACCESS_DENIED = 5
ERROR_SHARING_VIOLATION = 32
# ERROR_INVALID_FUNCTION, ERROR_NOT_SUPPORTED, ERROR_INVALID_PARAMETER: the file system lacks FileDispositionInfoEx.
UNSUPPORTED_ERRORS = (1, 50, 87)
IMAGE_RUNNING = "running"
IMAGE_UNCHECKABLE = "uncheckable"


# ---------------------------------------------------------------------------------------------------------------
# Paths and links


def extended(path: pathlib.Path | str) -> str:
    """The absolute path in the form Windows accepts at any length (\\\\?\\ or \\\\?\\UNC\\); absolute elsewhere.
    Links are not resolved. A path that already has the prefix is returned as it is, because os.path.abspath would
    strip a trailing dot or space from its last component and so name a different directory."""
    text = os.fspath(path)
    if os.name != "nt":
        return os.path.abspath(text)
    if text.startswith(EXTENDED_PREFIX):
        return text
    text = os.path.abspath(text)
    if text.startswith("\\\\"):
        return EXTENDED_UNC_PREFIX + text[2:]
    return EXTENDED_PREFIX + text


def shown(path: pathlib.Path | str) -> str:
    """A path as messages print it: without the extended-length prefix."""
    text = os.fspath(path)
    if text.startswith(EXTENDED_UNC_PREFIX):
        return "\\\\" + text[len(EXTENDED_UNC_PREFIX):]
    if text.startswith(EXTENDED_PREFIX):
        return text[len(EXTENDED_PREFIX):]
    return text


def plain(path: pathlib.Path | str) -> str:
    """A comparable form of a path: absolute, without the extended-length prefix, case-normalised on Windows."""
    text = os.fspath(path)
    if text.startswith(EXTENDED_PREFIX):
        return os.path.normcase(shown(text))
    return os.path.normcase(os.path.abspath(text))


def is_link_entry(entry: os.DirEntry) -> bool:
    """True for a symbolic link, a junction or any other reparse point; such entries are never followed."""
    if entry.is_symlink():
        return True
    checker = getattr(entry, "is_junction", None)
    if checker is not None and checker():
        return True
    if os.name == "nt":
        try:
            return bool(entry.stat(follow_symlinks=False).st_file_attributes & stat.FILE_ATTRIBUTE_REPARSE_POINT)
        except OSError:
            return False
    return False


def is_link_info(info: os.stat_result) -> bool:
    if stat.S_ISLNK(info.st_mode):
        return True
    return os.name == "nt" and bool(getattr(info, "st_file_attributes", 0) & stat.FILE_ATTRIBUTE_REPARSE_POINT)


def is_link_path(path: str) -> bool:
    try:
        return is_link_info(os.lstat(path))
    except OSError:
        return False


def walk(top: str):
    """Every entry below top, depth first; links are listed but never entered, unreadable directories skipped."""
    pending = [top]
    while pending:
        try:
            entries = list(os.scandir(pending.pop()))
        except OSError:
            continue
        for entry in entries:
            yield entry
            try:
                if not is_link_entry(entry) and entry.is_dir(follow_symlinks=False):
                    pending.append(entry.path)
            except OSError:
                continue


# ---------------------------------------------------------------------------------------------------------------
# Windows file API (only loaded on Windows)


@functools.lru_cache(maxsize=None)
def win32():
    """(ctypes, kernel32) with the prototypes this tool uses."""
    import ctypes
    from ctypes import wintypes

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.CreateFileW.argtypes = (wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, wintypes.LPVOID,
                                     wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE)
    kernel32.CreateFileW.restype = wintypes.HANDLE
    kernel32.SetFileInformationByHandle.argtypes = (wintypes.HANDLE, ctypes.c_int, wintypes.LPVOID, wintypes.DWORD)
    kernel32.SetFileInformationByHandle.restype = wintypes.BOOL
    kernel32.CloseHandle.argtypes = (wintypes.HANDLE,)
    kernel32.CloseHandle.restype = wintypes.BOOL
    return ctypes, kernel32


def open_handle(path: str, access: int) -> tuple[int | None, int]:
    """(handle, 0) for the file or link itself (never its target), or (None, Win32 error code)."""
    ctypes, kernel32 = win32()
    handle = kernel32.CreateFileW(path, access, FILE_SHARE_ALL, None, OPEN_EXISTING,
                                  FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, None)
    if handle is None or handle == ctypes.c_void_p(-1).value:
        return None, ctypes.get_last_error()
    return handle, 0


def delete_ignoring_read_only(path: str) -> None:
    """Deletes a file or link without clearing its read-only bit, which every hard link of the file shares. Raises
    OSError (winerror in UNSUPPORTED_ERRORS when the file system cannot do it)."""
    ctypes, kernel32 = win32()
    handle, error = open_handle(path, DELETE_ACCESS)
    if handle is None:
        raise ctypes.WinError(error)
    try:
        flags = ctypes.c_ulong(FILE_DISPOSITION_FLAGS)
        if not kernel32.SetFileInformationByHandle(handle, FILE_DISPOSITION_INFO_EX, ctypes.byref(flags),
                                                   ctypes.sizeof(flags)):
            raise ctypes.WinError(ctypes.get_last_error())
    finally:
        kernel32.CloseHandle(handle)


def image_state(path: str) -> str | None:
    """IMAGE_RUNNING when a running process has mapped the file (an executable or a library): such a file refuses
    write access with a sharing violation. A read-only file refuses it in any case, so its bit is cleared for the
    check and set again; when other hard links share that bit it must not change, and the answer is
    IMAGE_UNCHECKABLE. None when the file is not in use."""
    ctypes, kernel32 = win32()
    handle, error = open_handle(path, FILE_WRITE_DATA)
    if handle is not None:
        kernel32.CloseHandle(handle)
        return None
    if error == ERROR_SHARING_VIOLATION:
        return IMAGE_RUNNING
    if error != ERROR_ACCESS_DENIED:
        return None
    info = os.lstat(path)
    if not info.st_file_attributes & stat.FILE_ATTRIBUTE_READONLY:
        return None
    if info.st_nlink != 1:
        return IMAGE_UNCHECKABLE
    os.chmod(path, stat.S_IWRITE)
    try:
        handle, error = open_handle(path, FILE_WRITE_DATA)
        if handle is not None:
            kernel32.CloseHandle(handle)
            return None
        return IMAGE_RUNNING if error == ERROR_SHARING_VIOLATION else None
    finally:
        os.chmod(path, stat.S_IREAD)


def image_refusal(tree: str) -> str | None:
    """Why the tree must be left alone because a running program may have loaded an executable or library in it,
    or None."""
    for entry in walk(tree):
        if not entry.name.lower().endswith(IMAGE_SUFFIXES) or is_link_entry(entry) or not entry.is_file():
            continue
        try:
            state = image_state(entry.path)
        except OSError:
            continue
        if state == IMAGE_RUNNING:
            return f"{shown(entry.path)}: loaded by a running program (in use; nothing was removed)"
        if state == IMAGE_UNCHECKABLE:
            return (f"{shown(entry.path)}: read-only with other hard links, so it cannot be checked for a running "
                    f"program; nothing was removed (clear its read-only bit to let the check run)")
    return None


# ---------------------------------------------------------------------------------------------------------------
# Presets


def macro_values(root: pathlib.Path, file_directory: pathlib.Path) -> dict[str, str]:
    """The ${...} macros of CMake presets that do not depend on the preset."""
    return {
        "sourceDir": str(root),
        "sourceParentDir": str(root.parent),
        "sourceDirName": root.name,
        "fileDir": str(file_directory),
        "hostSystemName": platform.system(),
        "pathListSep": os.pathsep,
        "dollar": "$",
    }


def expand(text: str, values: dict[str, str], environment: dict[str, str | None] | None, depth: int = 0) -> str:
    """CMake preset macro expansion: ${name} from `values`, $env{X} from `environment` and then this process's
    environment (`environment` None where $env{} is not allowed; a null entry is skipped, as CMake does), $penv{X}
    from this process's environment. Anything else (an unknown macro, $vendor{}, $env{} where it is not allowed, a
    reference cycle) becomes WILDCARD."""

    def replace(match: re.Match) -> str:
        name = match.group("name")
        if name is not None:
            return values.get(name, WILDCARD)
        kind = match.group("kind")
        variable = match.group("variable")
        if kind == "penv":
            return os.environ.get(variable, "")
        if kind == "env" and environment is not None:
            value = environment.get(variable)
            if value is None:
                return os.environ.get(variable, "")
            if depth >= MAX_EXPANSION_DEPTH:
                return WILDCARD
            return expand(value, values, environment, depth + 1)
        return WILDCARD

    return MACRO.sub(replace, text)


def as_pattern(text: str) -> str:
    """An fnmatch pattern that matches `text` literally, with WILDCARD matching anything."""
    return "".join("*" if character == WILDCARD else f"[{character}]" if character in FNMATCH_SPECIALS else character
                   for character in text)


def load_preset_documents(root: pathlib.Path) -> list[tuple[dict, pathlib.Path]]:
    """(document, its file) for CMakePresets.json, CMakeUserPresets.json and every file they include, each parsed
    once: a file reached twice, by any spelling (a link or junction in the path included), is recognised by its
    identity, as CMake does. Raises ValueError naming the file when one is malformed or names an include that cannot
    be found, as CMake does."""
    documents: list[tuple[dict, pathlib.Path]] = []
    seen: set[object] = set()
    pending = [root / name for name in PRESET_FILES if (root / name).is_file()]
    while pending:
        path = pending.pop(0)
        try:
            info = os.stat(path)
            key: object = (info.st_dev, info.st_ino)
        except OSError:
            key = plain(path)
        if key in seen:
            continue
        seen.add(key)
        try:
            document = json.loads(path.read_text(encoding="utf-8-sig"))
        except (OSError, ValueError) as error:
            raise ValueError(f"{path}: {error}") from error
        if not isinstance(document, dict):
            raise ValueError(f"{path}: expected a JSON object")
        if not isinstance(document.get("configurePresets", []), list):
            raise ValueError(f"{path}: configurePresets must be a list")
        includes = document.get("include", [])
        if not isinstance(includes, list) or not all(isinstance(entry, str) for entry in includes):
            raise ValueError(f"{path}: include must be a list of file names")
        documents.append((document, path))
        for entry in includes:
            expanded = expand(entry, macro_values(root, path.parent), None)
            if WILDCARD in expanded:
                raise ValueError(f"{path}: include {entry!r} uses a macro that cannot be expanded here")
            target = path.parent / expanded
            if not target.is_file():
                raise ValueError(f"{path}: included file {entry!r} does not exist")
            pending.append(target)
    return documents


def same_directory(first: str, second: str) -> bool:
    """Whether two paths name the same directory: by file identity when both exist (so a link, junction or drive
    alias in either spelling does not matter), by their normalised spelling otherwise."""
    try:
        return os.path.samestat(os.stat(first), os.stat(second))
    except (OSError, ValueError):
        return plain(first) == plain(second)


def preset_directories(root: pathlib.Path) -> set[str]:
    """fnmatch patterns (compared without case on Windows) of the names directly under build/ that a non-hidden
    configure preset uses as its binary directory. Inheritance and the macros follow CMake: the first parent that
    sets a non-empty value wins, and every macro is expanded in the context of the visible preset (its name, its
    file's directory for ${fileDir}, its merged environment). A macro whose value cannot be known here stands for
    any text in its place. A preset without any binaryDir protects a tree of its own name. Raises ValueError, as
    CMake refuses the files, for a duplicate preset name, an unknown parent or an inheritance cycle."""
    root = pathlib.Path(os.path.abspath(root))
    presets: dict[str, tuple[dict, pathlib.Path]] = {}
    for document, path in load_preset_documents(root):
        for preset in document.get("configurePresets", []):
            if not isinstance(preset, dict) or not isinstance(preset.get("name"), str):
                continue
            if preset["name"] in presets:
                raise ValueError(f"{path}: duplicate configure preset {preset['name']!r}")
            presets[preset["name"]] = (preset, path)

    def parents_of(name: str) -> list[str]:
        preset, path = presets[name]
        parents = preset.get("inherits", [])
        parents = [parents] if isinstance(parents, str) else parents
        if not isinstance(parents, list) or not all(isinstance(parent, str) for parent in parents):
            raise ValueError(f"{path}: preset {name!r}: inherits must be a preset name or a list of them")
        for parent in parents:
            if parent not in presets:
                raise ValueError(f"{path}: preset {name!r} inherits from unknown preset {parent!r}")
        return parents

    finished: set[str] = set()

    def check_cycles(name: str, chain: tuple[str, ...]) -> None:
        if name in chain:
            raise ValueError(f"{presets[name][1]}: preset {name!r} inherits from itself "
                             f"({' -> '.join(chain[chain.index(name):] + (name,))})")
        if name in finished:
            return
        for parent in parents_of(name):
            check_cycles(parent, chain + (name,))
        finished.add(name)

    for name in presets:
        check_cycles(name, ())

    @functools.lru_cache(maxsize=None)
    def inherited(name: str, key: str) -> str | None:
        """The non-empty string value of `key` from the preset or the first parent that sets one (CMake's rule)."""
        value = presets[name][0].get(key)
        if isinstance(value, str) and value:
            return value
        for parent in parents_of(name):
            found = inherited(parent, key)
            if found is not None:
                return found
        return None

    @functools.lru_cache(maxsize=None)
    def environment_of(name: str) -> tuple[tuple[str, str | None], ...]:
        """The preset's environment map merged over its parents' (an earlier parent wins over a later one)."""
        merged: dict[str, str | None] = {}
        for parent in reversed(parents_of(name)):
            merged.update(environment_of(parent))
        own = presets[name][0].get("environment", {})
        if isinstance(own, dict):
            merged.update({key: value for key, value in own.items()
                           if isinstance(key, str) and (value is None or isinstance(value, str))})
        return tuple(merged.items())

    build = str(root / "build")
    patterns: set[str] = set()
    for name, (preset, path) in presets.items():
        if preset.get("hidden"):
            continue
        template = inherited(name, "binaryDir")
        if template is None:
            patterns.add(as_pattern(name))
            continue
        values = macro_values(root, path.parent)
        values["presetName"] = name
        generator = inherited(name, "generator")
        if generator is not None:
            values["generator"] = generator
        expanded = expand(template, values, dict(environment_of(name)))
        location = os.path.abspath(os.path.join(str(root), expanded))
        parent, base = os.path.split(location)
        if WILDCARD in parent or same_directory(parent, build):
            patterns.add(as_pattern(base))
    return patterns


# ---------------------------------------------------------------------------------------------------------------
# Build trees


def cmake_home(cache: str) -> str | None:
    """CMAKE_HOME_DIRECTORY (the source directory) recorded in a CMakeCache.txt."""
    try:
        with open(cache, encoding="utf-8", errors="replace") as stream:
            for line in stream:
                if line.startswith("CMAKE_HOME_DIRECTORY:"):
                    return line.split("=", 1)[1].strip() if "=" in line else None
    except OSError:
        return None
    return None


def source_outside(tree: str, home: str | None) -> bool:
    """True only when `home` names a directory that exists on this host and is neither the tree nor inside it. The
    comparison is by file identity, so a link, junction or drive alias in either spelling cannot hide an in-source
    project; anything that cannot be confirmed counts as not outside."""
    if not home:
        return False
    try:
        candidate = os.path.realpath(home)
        if not os.path.isdir(candidate):
            return False
        tree_info = os.stat(tree)
        while True:
            if os.path.samestat(os.stat(candidate), tree_info):
                return False
            parent = os.path.dirname(candidate)
            if parent == candidate:
                return True
            candidate = parent
    except (OSError, ValueError):
        return False


def build_trees(root: pathlib.Path) -> list[tuple[str, str, tuple[int, int]]]:
    """(name, path, identity) of every build tree directly under <root>/build, sorted by name. The path keeps the
    entry's raw name, so on Windows build/agent. and build/agent stay distinct; identity is (st_dev, st_ino), checked
    again just before removal."""
    build = extended(root / "build")
    if not os.path.isdir(build):
        return []
    trees = []
    for entry in os.scandir(build):
        if is_link_entry(entry) or not entry.is_dir(follow_symlinks=False):
            continue
        path = build + os.sep + entry.name
        cache = path + os.sep + "CMakeCache.txt"
        if not os.path.isfile(cache) or is_link_path(cache) or os.path.lexists(path + os.sep + "CMakeLists.txt"):
            continue
        if not source_outside(path, cmake_home(cache)):
            continue
        info = os.lstat(path)
        trees.append((entry.name, path, (info.st_dev, info.st_ino)))
    return sorted(trees)


def device(path: str) -> int:
    """The file system (st_dev) a path itself lies on; links are not followed."""
    return os.lstat(path).st_dev


def is_mount_point(path: str) -> bool:
    """Whether a directory is the root of a file system other than its parent's (Linux, macOS; False on Windows,
    where mounted folders are reparse points and never entered). The parent is the directory that really holds it:
    when build/ is a link to another disk, the link itself lies on the checkout's file system, the trees do not."""
    if os.name == "nt":
        return False
    try:
        return device(path) != device(os.path.realpath(os.path.dirname(path)))
    except OSError:
        return False


def scan(tree: str) -> tuple[int, float, str | None]:
    """Bytes of every file in the tree, the newest modification time of any file or directory in it, and on Linux
    and macOS the first directory that lies on another file system (a mount point, not entered) or None. Links are
    not followed."""
    size = 0
    newest = 0.0
    foreign = None
    tree_device = None
    if os.name != "nt":
        try:
            tree_device = device(tree)
        except OSError:
            tree_device = None
    pending = [tree]
    while pending:
        directory = pending.pop()
        try:
            entries = list(os.scandir(directory))
        except OSError:
            continue
        for entry in entries:
            try:
                if is_link_entry(entry):
                    continue
                info = entry.stat(follow_symlinks=False)
                if entry.is_dir(follow_symlinks=False):
                    if tree_device is not None and device(entry.path) != tree_device:
                        foreign = foreign or entry.path
                        continue
                    pending.append(entry.path)
                else:
                    size += info.st_size
            except OSError:
                continue
            newest = max(newest, info.st_mtime)
    return size, newest, foreign


def parse_mountinfo(text: str) -> list[str]:
    """Mount points listed in the text of /proc/self/mountinfo (field 5, with octal escapes such as \\040)."""
    points = []
    for line in text.splitlines():
        fields = line.split()
        if len(fields) > 4:
            points.append(re.sub(r"\\([0-7]{3})", lambda match: chr(int(match.group(1), 8)), fields[4]))
    return points


def mount_points() -> list[str]:
    """Every mount point of this process on Linux (bind mounts included); empty elsewhere."""
    try:
        with open("/proc/self/mountinfo", encoding="utf-8", errors="surrogateescape") as stream:
            return parse_mountinfo(stream.read())
    except OSError:
        return []


def mount_inside(tree: str, points: list[str]) -> str | None:
    """The first of `points` that is the tree or lies inside it, or None (always None on Windows, where mounted
    folders are reparse points and never entered)."""
    if os.name == "nt":
        return None
    real = os.path.realpath(tree)
    for point in points:
        if point == real or point.startswith(real.rstrip(os.sep) + os.sep):
            return point
    return None


# ---------------------------------------------------------------------------------------------------------------
# Removal


def describe(path: str, error: BaseException) -> str:
    return f"{shown(path)}: {getattr(error, 'strerror', None) or error}"


def patiently(action, attempts: int) -> None:
    """Runs a file system action, retrying over a moment on Windows: virus scanners and the search indexer hold new
    files open briefly. Raises the last OSError."""
    count = attempts if os.name == "nt" else 1
    for attempt in range(count):
        try:
            action()
            return
        except OSError:
            if attempt + 1 >= count:
                raise
            time.sleep(RETRY_PAUSE_SECONDS)


def add_owner_access(path: str) -> None:
    """Gives the owner read, write and search access to a directory inside the tree (POSIX); links are left alone."""
    info = os.lstat(path)
    if stat.S_ISLNK(info.st_mode):
        return
    os.chmod(path, stat.S_IMODE(info.st_mode) | stat.S_IRWXU)


def clear_directory_read_only(path: str) -> None:
    """Clears the read-only attribute of a directory (Windows); a directory has no hard links to share it."""
    info = os.lstat(path)
    if not is_link_info(info) and info.st_file_attributes & stat.FILE_ATTRIBUTE_READONLY:
        os.chmod(path, stat.S_IWRITE)


def remove_windows_file(path: str) -> None:
    """Deletes a file or file link whose plain unlink failed (read-only, briefly held). The read-only bit is ignored
    rather than cleared, because every hard link of the file shares it, names outside the tree included."""
    try:
        patiently(lambda: delete_ignoring_read_only(path), REMOVE_ATTEMPTS)
        return
    except OSError as error:
        if getattr(error, "winerror", None) not in UNSUPPORTED_ERRORS:
            raise
    if os.lstat(path).st_nlink > 1:
        raise OSError(errno.EACCES, "read-only file with other hard links, left so that they keep the read-only bit",
                      shown(path))
    os.chmod(path, stat.S_IWRITE)
    patiently(lambda: os.unlink(path), REMOVE_ATTEMPTS)


def remove_again(function, path: str) -> None:
    """Retries an unlink or rmdir that failed, after making the entry removable. Raises the last OSError."""
    if os.name == "nt":
        if function is os.rmdir:
            clear_directory_read_only(path)
            patiently(lambda: os.rmdir(path), REMOVE_ATTEMPTS)
        else:
            remove_windows_file(path)
        return
    add_owner_access(os.path.dirname(path))
    function(path)


def remove_link(path: str, failures: list[str]) -> None:
    """Removes a link or junction itself, never its target; a read-only link, or one in a directory that is not
    writable, gets the same retry as a file."""
    try:
        os.unlink(path)
        return
    except OSError:
        pass
    try:
        os.rmdir(path)
        return
    except OSError:
        pass
    try:
        remove_again(os.unlink, path)
    except OSError as error:
        failures.append(describe(path, error))


def remove_file(path: str, failures: list[str]) -> None:
    try:
        os.unlink(path)
        return
    except OSError:
        pass
    try:
        remove_again(os.unlink, path)
    except OSError as error:
        failures.append(describe(path, error))


def remove_directory(path: str, failures: list[str], opened: set[str]) -> None:
    """shutil.rmtree, which does not follow links, with retries for read-only, unreadable and unsearchable entries.
    Every directory it repairs is the failed entry or its parent, both inside the tree."""

    def retry(function, failed: str, error: BaseException) -> None:
        if isinstance(error, FileNotFoundError):
            return
        try:
            if function in REMOVERS:
                remove_again(function, failed)
                return
            if os.name != "nt" and function in DIRECTORY_OPENERS and failed not in opened:
                opened.add(failed)
                add_owner_access(os.path.dirname(failed))
                if not stat.S_ISDIR(os.lstat(failed).st_mode):
                    os.unlink(failed)
                    return
                add_owner_access(failed)
                remove_directory(failed, failures, opened)
                return
        except Exception as retry_error:
            error = retry_error
        failures.append(describe(failed, error))

    if sys.version_info >= (3, 12):
        shutil.rmtree(path, onexc=retry)
    else:
        shutil.rmtree(path, onerror=lambda function, failed, info: retry(function, failed, info[1]))


def changed(path: str, identity: tuple[int, int], tree: str) -> str | None:
    """Why `path` (the tree, or where it was moved aside) must not be touched because it is no longer the directory
    listed as `tree`, or None."""
    try:
        info = os.lstat(path)
    except OSError as error:
        return f"{describe(tree, error)} (gone since it was listed; nothing was removed)"
    if is_link_info(info) or not stat.S_ISDIR(info.st_mode) or (info.st_dev, info.st_ino) != identity:
        return f"{shown(tree)}: replaced since it was listed; nothing was removed"
    return None


def before_moving(tree: str) -> tuple[str, bool] | None:
    """(why, whether running again later can succeed) when the tree must be left where it is, found before it is
    moved aside, or None: on Windows a program or library in it that a running process has loaded (which does not
    stop a rename); on Linux and macOS a tree that is or holds a mount point, or a build/ that is not writable."""
    if os.name == "nt":
        reason = image_refusal(tree)
        return None if reason is None else (reason, True)
    if is_mount_point(tree) or mount_inside(tree, mount_points()) is not None:
        return f"{shown(tree)}: a file system is mounted here; nothing was removed", False
    build = os.path.dirname(tree)
    if not os.access(build, os.W_OK | os.X_OK):
        return f"{shown(build)}: not writable, so the tree could not be removed; nothing was removed", False
    return None


def after_moving(staging: str, identity: tuple[int, int], tree: str) -> tuple[str, bool] | None:
    """(why, whether running again later can succeed) when the tree, now moved aside to `staging`, must be put back
    untouched, or None: it is not the directory that was listed (another process replaced build/NAME before the
    move; running again would remove the replacement, so that is never suggested), on Windows a program from it
    started meanwhile, on Linux and macOS a file system is mounted inside it."""
    reason = changed(staging, identity, tree)
    if reason is not None:
        return reason, False
    if os.name == "nt":
        reason = image_refusal(staging)
        return None if reason is None else (reason, True)
    mount = mount_inside(staging, mount_points()) or scan(staging)[2]
    if mount is not None:
        return f"{shown(mount)}: a file system is mounted here; nothing was removed", False
    return None


def put_back(staging: str, tree: str) -> tuple[list[str], str]:
    """Renames a tree that was moved aside back to its name. Returns (failures, where the tree is now)."""
    if os.path.lexists(tree):
        return [f"{shown(tree)} was created again meanwhile, so the tree was left as {shown(staging)}"], staging
    try:
        patiently(lambda: os.rename(staging, tree), RENAME_BACK_ATTEMPTS)
    except OSError as error:
        return [f"{describe(staging, error)} (could not be renamed back to {os.path.basename(tree)}; rename it by "
                f"hand)"], staging
    return [], tree


def remove_contents(staging: str) -> list[str]:
    """Removes everything in a tree that was moved aside, CMakeCache.txt last; returns what could not be removed."""
    failures: list[str] = []
    opened: set[str] = set()
    try:
        if os.name == "nt":
            clear_directory_read_only(staging)
        entries = list(os.scandir(staging))
    except OSError as error:
        return [describe(staging, error)]
    for entry in entries:
        if entry.name == "CMakeCache.txt":
            continue
        try:
            if is_link_entry(entry):
                remove_link(entry.path, failures)
            elif entry.is_dir(follow_symlinks=False):
                remove_directory(entry.path, failures, opened)
            else:
                remove_file(entry.path, failures)
        except OSError as error:
            failures.append(describe(entry.path, error))
    if not failures:
        remove_file(staging + os.sep + "CMakeCache.txt", failures)
    return failures


def remove_tree(tree: str, identity: tuple[int, int]) -> tuple[list[str], str, bool, bool]:
    """Removes a build tree. Returns (what could not be removed, where the tree is now, whether removal started,
    whether running again later can succeed): an empty list on success. The tree is first renamed to a private name
    next to it, so another process that recreates or relinks build/NAME meanwhile cannot redirect the removal; on
    Windows the rename also fails while a file in the tree is held open or a process works in it. Whenever the
    removal stops, the tree is renamed back."""
    reason = changed(tree, identity, tree)
    if reason is not None:
        return [reason], tree, False, False
    refused = before_moving(tree)
    if refused is not None:
        return [refused[0]], tree, False, refused[1]
    staging = f"{tree}.prune-{os.getpid()}"
    try:
        patiently(lambda: os.rename(tree, staging), MOVE_ATTEMPTS)
    except OSError as error:
        why = "a file in it is in use or a process works in it" if os.name == "nt" else "it could not be moved aside"
        return [f"{describe(tree, error)} ({why}; nothing was removed)"], tree, False, os.name == "nt"
    refused = after_moving(staging, identity, tree)
    if refused is not None:
        failures, location = put_back(staging, tree)
        return [refused[0]] + failures, location, False, refused[1]
    failures = remove_contents(staging)
    if failures:
        more, location = put_back(staging, tree)
        return failures + more, location, True, True
    try:
        if os.name == "nt":
            clear_directory_read_only(staging)
        patiently(lambda: os.rmdir(staging), REMOVE_ATTEMPTS)
    except OSError as error:
        return [f"{describe(staging, error)} (everything in it was removed; delete the empty directory by hand)"], \
            staging, True, False
    return [], tree, True, False


# ---------------------------------------------------------------------------------------------------------------
# Command line


def tree_name(argument: str) -> str:
    """NAME or build/NAME (either separator, trailing separators ignored) to NAME."""
    text = argument.replace("\\", "/").rstrip("/")
    if text.startswith("build/"):
        text = text[len("build/"):]
    if not text or "/" in text or text in (".", ".."):
        raise ValueError(f"name a directory directly under build/, e.g. core_math (got {argument!r})")
    return text


def same_name(first: str, second: str) -> bool:
    """Whether two names denote the same directory under build/: case is ignored on Windows, a trailing dot is not
    (the tool reaches build/agent. and build/agent separately through extended-length paths)."""
    return os.path.normcase(first) == os.path.normcase(second)


def matches_preset(name: str, patterns: set[str]) -> bool:
    return any(fnmatch.fnmatchcase(os.path.normcase(name), os.path.normcase(pattern)) for pattern in patterns)


def format_size(size: int) -> str:
    return f"{size / (1024 ** 3):.2f} GB"


def format_age(seconds: float) -> str:
    days = seconds / SECONDS_PER_DAY
    return f"{days:.1f} days ago" if days >= 1.0 else f"{seconds / 3600.0:.1f} hours ago"


def plan(root: pathlib.Path, names: list[str], keep: list[str], arguments: argparse.Namespace,
         now: float) -> list[dict]:
    """One row per build tree: name, path, identity, size, age in seconds and the action --delete takes. Raises
    ValueError, before anything is removed, when a name or a --keep name is not a build tree or a preset file is
    malformed."""
    trees = build_trees(root)
    known = [name for name, _, _ in trees]
    unknown = [name for name in names + keep if not any(same_name(name, tree) for tree in known)]
    if unknown:
        raise ValueError(f"not a build tree directly under build/: {', '.join(unknown)} (a build tree holds the "
                         f"CMakeCache.txt of an out-of-source build whose source directory exists on this host)")
    named: set[str] = set()
    for name in names:
        matches = [name] if name in known else [tree for tree in known if same_name(name, tree)]
        if len(matches) > 1:
            raise ValueError(f"{name} matches several build trees ({', '.join(matches)}); give the exact name")
        named.update(matches)
    presets = preset_directories(root)
    points = mount_points()
    rows = []
    for name, path, identity in trees:
        size, newest, foreign = scan(path)
        age = max(0.0, now - newest)
        selected = arguments.all or name in named or (
            arguments.older_than is not None and age > arguments.older_than * SECONDS_PER_DAY)
        if foreign is not None or mount_inside(path, points) is not None or is_mount_point(path):
            action = "keep (a file system is mounted inside)"
        elif any(same_name(name, kept) for kept in keep):
            action = "keep (--keep)"
        elif matches_preset(name, presets) and not arguments.include_presets:
            action = "keep (preset)"
        elif selected:
            action = "delete"
        else:
            action = "keep"
        rows.append({"name": name, "path": path, "identity": identity, "size": size, "age": age, "action": action})
    return rows


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("names", nargs="*", help="build trees to remove: NAME or build/NAME")
    parser.add_argument("--older-than", type=float, metavar="DAYS", help="select trees unchanged for over DAYS days")
    parser.add_argument("--all", action="store_true", help="select every build tree")
    parser.add_argument("--keep", action="append", default=[], metavar="NAME", help="never remove this tree")
    parser.add_argument("--include-presets", action="store_true", help="allow removing the presets' build trees")
    parser.add_argument("--delete", action="store_true", help="remove the selected trees (default: report only)")
    parser.add_argument("--root", type=pathlib.Path, default=ROOT, help="repository root (default: this checkout)")
    arguments = parser.parse_args(argv)
    if arguments.older_than is not None and arguments.older_than < 0:
        print("prune_builds: --older-than must not be negative", file=sys.stderr)
        return 2
    try:
        names = [tree_name(name) for name in arguments.names]
        keep = [tree_name(name) for name in arguments.keep]
        rows = plan(pathlib.Path(os.path.abspath(arguments.root)), names, keep, arguments, time.time())
    except ValueError as error:
        print(f"prune_builds: {error}", file=sys.stderr)
        return 2

    failed = False
    freed = 0
    for row in rows:
        verb = row["action"]
        if verb == "delete" and not arguments.delete:
            verb = "would delete"
        elif verb == "delete":
            failures, location, started, retryable = remove_tree(row["path"], row["identity"])
            if failures:
                failed = True
                if started:
                    remaining = scan(location)[0] if os.path.lexists(location) else 0
                    freed += max(0, row["size"] - remaining)
                more = f" (and {len(failures) - 1} more)" if len(failures) > 1 else ""
                if location != row["path"]:
                    where = f"; the tree is now {shown(location)}"
                elif retryable and os.path.isfile(location + os.sep + "CMakeCache.txt"):
                    where = "; run again to retry"
                else:
                    where = ""
                state = "not removed completely" if started else "not removed"
                verb = f"{state}: {failures[0]}{more}{where}"
            else:
                freed += row["size"]
                verb = "deleted"
        print(f"{row['name']:32s} {format_size(row['size']):>10s}  {format_age(row['age']):>16s}  {verb}")

    selected = sum(row["size"] for row in rows if row["action"] == "delete")
    total = sum(row["size"] for row in rows)
    summary = f"freed {format_size(freed)}" if arguments.delete else f"--delete would free {format_size(selected)}"
    print(f"{len(rows)} build tree{'' if len(rows) == 1 else 's'}, {format_size(total)} in all; {summary}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
