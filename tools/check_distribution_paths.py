#!/usr/bin/env python3
"""Distribution source-path check (code sweep F613; the options of cmake/ConjureDistributionPaths.cmake, applied by
cmake/ConjureWarnings.cmake).

__FILE__ (CONJURE_LOG, CONJURE_ASSERT, assert) and std::source_location embed the path the compiler was given, and
debug information and a PDB reference embed more. A Distribution build removes the checkout from those paths
(/d1trimfile on MSVC, -fmacro-prefix-map on GCC and Clang) and carries no debug information, so a shipped binary
does not name the machine that built it. This tool proves it on the built binaries: it reads every byte of the
Distribution conjure_runtime and of the conjure_shared library next to it and fails when one of them contains a
root, that is the source directory or the build directory of the tree.

A root matches in UTF-8 (narrow strings, debug information, PDB paths) and in UTF-16LE (wide strings), with '/'
and '\\' interchangeable and repeatable between its components ('A:\\\\src' in an escaped string) and ASCII letters
compared without regard to case (cl may change the case of a drive letter, and Windows and macOS volumes are
usually case-insensitive). A match must end the root: 'C:/src/conjure' is found in 'C:/src/conjure/engine/log.cpp'
and in 'C:/src/conjure' itself, not in 'C:/src/conjure_old/log.cpp'. An ELF section that is compressed
(SHF_COMPRESSED or .zdebug*, gcc -gz) cannot be read this way and is a finding of its own. Not found: a path the
binary stores in pieces (a directory and a file name apart, as a portable PDB does), compressed in another way, or
through a different spelling of the same directory (an 8.3 short name, a link the build did not use). A finding
names its PE or ELF section; in a Mach-O or any other file it is reported as "unknown section".

Modes:
    --build-dir <dir> [--config <name>]   checks bin/conjure_runtime[.exe] and the conjure_shared library of a
                                          configured and built Distribution build directory, against the
                                          CMAKE_HOME_DIRECTORY and the build directory its CMakeCache.txt names.
                                          The directory must hold a Distribution build (CMAKE_BUILD_TYPE, or
                                          --config for a multi-config generator, binaries in bin/<config>/); any
                                          other configuration exits 77 ("skipped") unless --any-config. A --config
                                          that disagrees with a single-config directory's CMAKE_BUILD_TYPE is a
                                          usage error.
    --binary <file> [...]                 checks the named files, against the repository that holds this tool

Options: --root <dir> (repeatable: another root to look for, in either mode), --no-shared (skip the conjure_shared
library next to the runtime), --any-config, --max-findings <n> (printed per binary, default 20).

Exit code 0 when no binary contains a root, 1 when one does (one `<binary>: 0x<offset> [<section>] <encoding>:
<string>` line per distinct string), 2 on usage errors (missing directory, cache, binary or root, disagreeing
--config), 77 when the build directory is not a Distribution one.

Tests: python tools/tests/test_check_distribution_paths.py. CI runs them in the style-and-headers job of ci.yml,
and the check itself after the export in the distribution-export job of nightly.yml:

    python tools/check_distribution_paths.py --build-dir build/windows-msvc-dist

tests/runtime/CMakeLists.txt registers both with CTest: runtime.check_distribution_paths_tool (the tests, in every
tree) and runtime.distribution_paths (the check, label "process", in a Distribution tree or on the tree that
CONJURE_DISTRIBUTION_BUILD_DIR names).
"""

from __future__ import annotations

import argparse
import importlib.util
import os
import pathlib
import re
import struct
import sys
from types import ModuleType

ROOT = pathlib.Path(__file__).resolve().parent.parent
SYMBOLS_TOOL = pathlib.Path(__file__).resolve().parent / "check_distribution_symbols.py"

PROGRAM = "check_distribution_paths"

EXIT_CLEAN = 0
EXIT_FINDINGS = 1
EXIT_USAGE = 2
EXIT_SKIPPED = 77

DEFAULT_MAX_FINDINGS = 20
# How far the printed string around a match may reach on either side.
CONTEXT_LIMIT = 260

ENCODING_UTF8 = "utf-8"
ENCODING_UTF16 = "utf-16le"

# The characters that continue a directory name: a root followed by one of them is a different directory.
NAME_CHARACTERS = b"A-Za-z0-9_.\\-"

ELF_SHT_NOBITS = 8
ELF_SHF_COMPRESSED = 0x800


def load_symbols_tool() -> ModuleType:
    """tools/check_distribution_symbols.py, whose cache reading and binary lookup this tool shares. Loaded by path
    so the tool also runs under `python -I`, which leaves the script's directory off sys.path."""
    specification = importlib.util.spec_from_file_location("conjure_check_distribution_symbols", SYMBOLS_TOOL)
    if specification is None or specification.loader is None:
        raise ImportError(f"cannot load {SYMBOLS_TOOL}")
    module = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(module)
    return module


symbols = load_symbols_tool()


class finding:
    def __init__(self, binary: pathlib.Path, offset: int, section: str, encoding: str, text: str) -> None:
        self.binary = binary
        self.offset = offset
        self.section = section
        self.encoding = encoding
        self.text = text

    def format(self) -> str:
        return f"{self.binary.name}: 0x{self.offset:x} [{self.section}] {self.encoding}: {self.text}"


#------------------------------------------------------------------------------
# Roots
#------------------------------------------------------------------------------

def root_components(root: str) -> tuple[bool, list[str]]:
    """Whether the root starts with a separator (a POSIX or UNC path), and its non-empty components."""
    leading = root[:1] in ("/", "\\")
    components = [component for component in re.split(r"[\\/]+", root) if component]
    return leading, components


def root_pattern(root: str, encoding: str) -> re.Pattern[bytes] | None:
    """The pattern that finds a root in a binary in one encoding; None for a root without components."""
    leading, components = root_components(root)
    if not components:
        return None
    if encoding == ENCODING_UTF16:
        separator = b"(?:[\\\\/]\x00)+"
        boundary = b"(?![" + NAME_CHARACTERS + b"]\x00)"
    else:
        separator = b"[\\\\/]+"
        boundary = b"(?![" + NAME_CHARACTERS + b"\x80-\xff])"
    parts = [re.escape(component.encode(encoding)) for component in components]
    body = (separator if leading else b"") + separator.join(parts)
    return re.compile(body + boundary, re.IGNORECASE)


def ascii_lower(text: str) -> str:
    """The text with its ASCII letters in lower case, the only letters the patterns compare without case."""
    return "".join(character.lower() if character.isascii() else character for character in text)


def expand_roots(roots: list[str]) -> list[str]:
    """Each root made absolute and, when that differs, with its links resolved. A root that differs from an earlier
    one only in separators or the case of ASCII letters is dropped: its pattern would find the same places."""
    expanded: list[str] = []
    keys: set[tuple[bool, tuple[str, ...]]] = set()
    for root in roots:
        for candidate in (os.path.abspath(root), str(pathlib.Path(root).resolve())):
            leading, components = root_components(candidate)
            key = (leading, tuple(ascii_lower(component) for component in components))
            if key in keys:
                continue
            keys.add(key)
            expanded.append(candidate)
    return expanded


#------------------------------------------------------------------------------
# Sections
#------------------------------------------------------------------------------

class section:
    def __init__(self, name: str, offset: int, size: int, compressed: bool) -> None:
        self.name = name
        self.offset = offset
        self.size = size
        self.compressed = compressed


def pe_sections(data: bytes) -> list[section]:
    """The sections of a PE image (raw data ranges); empty when the headers are not readable."""
    if len(data) < 0x40 or data[:2] != b"MZ":
        return []
    header = struct.unpack_from("<I", data, 0x3C)[0]
    if header + 24 > len(data) or data[header:header + 4] != b"PE\x00\x00":
        return []
    count, optional_size = struct.unpack_from("<H12xH", data, header + 6)
    table = header + 24 + optional_size
    sections: list[section] = []
    for index in range(count):
        entry = table + index * 40
        if entry + 40 > len(data):
            return sections
        raw_name = data[entry:entry + 8].split(b"\x00", 1)[0].decode("ascii", errors="replace")
        raw_size, raw_offset = struct.unpack_from("<II", data, entry + 16)
        sections.append(section(raw_name, raw_offset, raw_size, False))
    return sections


def elf_sections(data: bytes) -> list[section]:
    """The sections of an ELF file (32 or 64 bit, either byte order) that occupy file bytes; empty when the
    headers are not readable."""
    if len(data) < 0x34 or data[:4] != b"\x7fELF" or data[4] not in (1, 2) or data[5] not in (1, 2):
        return []
    wide = data[4] == 2
    order = "<" if data[5] == 1 else ">"
    if wide:
        if len(data) < 0x40:
            return []
        table, entry_size, count, names_index = struct.unpack_from(order + "Q10xHHH", data, 0x28)
        header_format = order + "IIQQQQ"
    else:
        table, entry_size, count, names_index = struct.unpack_from(order + "I10xHHH", data, 0x20)
        header_format = order + "IIIIII"
    if entry_size < struct.calcsize(header_format) or table + count * entry_size > len(data):
        return []
    headers = [struct.unpack_from(header_format, data, table + index * entry_size) for index in range(count)]
    if names_index >= count:
        return []
    names_offset = headers[names_index][4]
    sections: list[section] = []
    for name_index, kind, flags, _, offset, size in headers:
        if kind == ELF_SHT_NOBITS or size == 0:
            continue
        start = names_offset + name_index
        end = data.find(b"\x00", start)
        name = data[start:end].decode("ascii", errors="replace") if 0 <= start < end else "?"
        compressed = bool(flags & ELF_SHF_COMPRESSED) or name.startswith(".zdebug")
        sections.append(section(name, offset, size, compressed))
    return sections


def binary_sections(data: bytes) -> list[section]:
    return pe_sections(data) or elf_sections(data)


def section_name(sections: list[section], offset: int) -> str:
    for item in sections:
        if item.offset <= offset < item.offset + item.size:
            return item.name
    return "headers or unknown section" if sections else "unknown section"


#------------------------------------------------------------------------------
# Scanning
#------------------------------------------------------------------------------

def surrounding_text(data: bytes, start: int, end: int, encoding: str) -> str:
    """The printable string a match sits in, at most CONTEXT_LIMIT bytes beyond the match on either side."""
    step = 2 if encoding == ENCODING_UTF16 else 1

    def printable(position: int) -> bool:
        if position < 0 or position + step > len(data):
            return False
        if step == 2:
            return data[position + 1] == 0 and 0x20 <= data[position] < 0x7F
        return data[position] >= 0x20 and data[position] != 0x7F

    left = start
    while start - left < CONTEXT_LIMIT and printable(left - step):
        left -= step
    right = end
    while right - end < CONTEXT_LIMIT and printable(right):
        right += step
    return data[left:right].decode(encoding, errors="replace")


def scan_bytes(binary: pathlib.Path, data: bytes, roots: list[str]) -> tuple[list[finding], int]:
    """The findings of a binary: each distinct string that contains a root (once per section) and each compressed
    ELF section; with the number of places a root starts (a place two roots share counts once)."""
    sections = binary_sections(data)
    findings: list[finding] = []
    for item in sections:
        if item.compressed:
            findings.append(finding(binary, item.offset, item.name, "compressed",
                                    "a compressed section cannot be scanned; build Distribution without -gz "
                                    "and without debug information, or strip the binary"))
    seen: set[tuple[str, str]] = set()
    places: set[tuple[int, str]] = set()
    for root in roots:
        for encoding in (ENCODING_UTF8, ENCODING_UTF16):
            pattern = root_pattern(root, encoding)
            if pattern is None:
                continue
            for match in pattern.finditer(data):
                places.add((match.start(), encoding))
                text = surrounding_text(data, match.start(), match.end(), encoding)
                name = section_name(sections, match.start())
                if (text, name) in seen:
                    continue
                seen.add((text, name))
                findings.append(finding(binary, match.start(), name, encoding, text))
    findings.sort(key=lambda item: item.offset)
    return findings, len(places)


def check_binaries(binaries: list[pathlib.Path], roots: list[str], max_findings: int) -> int:
    print("check_distribution_paths: roots " + ", ".join(roots))
    total = 0
    for binary in binaries:
        try:
            data = binary.read_bytes()
        except OSError as error:
            print(f"check_distribution_paths: cannot read {binary}: {error}", file=sys.stderr)
            return EXIT_USAGE
        findings, matches = scan_bytes(binary, data, roots)
        verdict = "clean" if not findings else f"{len(findings)} finding(s), a root at {matches} place(s)"
        print(f"check_distribution_paths: {binary.name}: {len(data)} byte(s) scanned, {verdict}")
        for item in findings[:max_findings]:
            print(f"  {item.format()}")
        if len(findings) > max_findings:
            print(f"  ... {len(findings) - max_findings} more")
        total += len(findings)

    if total:
        sys.stdout.flush()
        print(
            "check_distribution_paths: a Distribution binary must not name the checkout that built it (code sweep "
            "F613). Compile it with the Distribution path options of cmake/ConjureDistributionPaths.cmake (a target "
            "that replaces its directory's compile options loses them) and without debug information (-g, /DEBUG and "
            "a PDB path bring the roots back).",
            file=sys.stderr,
        )
        return EXIT_FINDINGS
    return EXIT_CLEAN


#------------------------------------------------------------------------------
# Modes
#------------------------------------------------------------------------------

def checked_roots(arguments: list[str]) -> list[str] | None:
    """The roots of the --root arguments, each an existing directory; None (after a message) otherwise."""
    roots: list[str] = []
    for argument in arguments:
        if not argument or not pathlib.Path(argument).is_dir():
            print(f"check_distribution_paths: --root {argument!r} is not a directory", file=sys.stderr)
            return None
        roots.append(argument)
    return roots


def run_build_dir(
  build_dir_argument: str,
  config_argument: str | None,
  any_config: bool,
  with_shared: bool,
  extra_roots: list[str],
  max_findings: int,
) -> int:
    build_dir = pathlib.Path(build_dir_argument)
    cache = symbols.read_build_cache(build_dir, PROGRAM)
    if cache is None:
        return EXIT_USAGE
    source_dir = cache.get("CMAKE_HOME_DIRECTORY", "")
    if not source_dir:
        print(f"{PROGRAM}: the CMakeCache.txt of {build_dir} names no CMAKE_HOME_DIRECTORY", file=sys.stderr)
        return EXIT_USAGE
    code, binaries = symbols.resolve_distribution_binaries(build_dir, cache, config_argument, any_config, with_shared,
                                                           PROGRAM)
    if code != EXIT_CLEAN:
        return code
    cache_dir = cache.get("CMAKE_CACHEFILE_DIR", "") or str(build_dir)
    roots = expand_roots([source_dir, cache_dir, str(build_dir)] + extra_roots)
    return check_binaries(binaries, roots, max_findings)


def run_binaries(binary_arguments: list[str], extra_roots: list[str], max_findings: int) -> int:
    binaries: list[pathlib.Path] = []
    for argument in binary_arguments:
        binary = pathlib.Path(argument)
        if not binary.is_file():
            print(f"check_distribution_paths: {binary} does not exist", file=sys.stderr)
            return EXIT_USAGE
        binaries.append(binary)
    roots = expand_roots([str(ROOT)] + extra_roots)
    return check_binaries(binaries, roots, max_findings)


#------------------------------------------------------------------------------
# Entry point
#------------------------------------------------------------------------------

def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build-dir", default=None, help="a configured and built Distribution build directory")
    parser.add_argument("--config", default=None,
                        help="the configuration of a multi-config build directory (binaries in bin/<config>/)")
    parser.add_argument("--binary", action="append", default=[], metavar="FILE",
                        help="a binary to check instead of a build directory (repeatable)")
    parser.add_argument("--root", action="append", default=[], metavar="DIR",
                        help="another directory whose path must not appear in the binaries (repeatable)")
    parser.add_argument("--no-shared", action="store_true",
                        help="check the runtime executable only, not the conjure_shared library next to it")
    parser.add_argument("--any-config", action="store_true",
                        help="check a build directory even when it is not a Distribution build")
    parser.add_argument("--max-findings", type=int, default=DEFAULT_MAX_FINDINGS,
                        help=f"distinct strings printed per binary (default: {DEFAULT_MAX_FINDINGS})")
    args = parser.parse_args(argv)

    if args.max_findings < 1:
        print("check_distribution_paths: --max-findings must be positive", file=sys.stderr)
        return EXIT_USAGE
    extra_roots = checked_roots(args.root)
    if extra_roots is None:
        return EXIT_USAGE
    if args.build_dir and args.binary:
        print("check_distribution_paths: --build-dir and --binary are exclusive", file=sys.stderr)
        return EXIT_USAGE
    if args.binary:
        return run_binaries(args.binary, extra_roots, args.max_findings)
    if args.build_dir:
        return run_build_dir(args.build_dir, args.config, args.any_config, not args.no_shared, extra_roots,
                             args.max_findings)
    parser.print_usage(sys.stderr)
    print("check_distribution_paths: pass --build-dir <dir> or --binary <file>", file=sys.stderr)
    return EXIT_USAGE


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
