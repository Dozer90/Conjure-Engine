#!/usr/bin/env python3
"""Mechanical style checks for the Conjure repository (docs/CONVENTIONS.md, docs/design/08_testing.md 5.1
and 7.3, docs/design/10_core.md 3).

Checks every C++ file under engine/, editor/, runtime/, tests/ and samples/ for:

  tabs                  indentation is four spaces, never a tab
  line-length           at most 120 columns
  crlf                  LF line endings only
  trailing-whitespace   no spaces or tabs at the end of a line
  pragma-once           every header starts its directives with #pragma once (no include guards)
  using-namespace       no `using namespace` directive at namespace or global scope in a header
  windows-macro         no enumerator (named or anonymous enum), constant (constexpr, or const with an
                        ALL_CAPS name) or #define named after a Windows macro (ERROR, DELETE, IN, ...)
  enum-class            every enumeration is an `enum class` (plain enums are tolerated only for names
                        ending in _flags, the style guide's optional flag form)
  include-order         unconditional #include directives come in the documented groups, each group
                        separated from the next by a blank line: own module headers (conjure/<module>/
                        for engine code, conjure/testing/ plus the helper headers next to the file for
                        tests), other Conjure headers (private engine/src headers included), third-party
                        headers, standard library and operating-system headers; Conjure headers use
                        quotes, everything else angle brackets
  third-party-include   public headers (engine/include) include only Conjure headers ("conjure/...") and C++
                        standard library headers (a bare name such as <vector>): third-party and operating-
                        system headers are rejected; glm in conjure/core/math/glm_types.h is the single exception
  doctest-include       test code includes doctest only through conjure/testing/doctest.h
  test-case-name        TEST_CASE names (also through TEST_CASE_TEMPLATE, TEST_CASE_FIXTURE and any
                        <PREFIX>_CASE("...") wrapper macro, comments excluded) follow "<type> - <behaviour>"
                        and contain no ';' or unbalanced '[' / ']' (doctest_discover_tests splits them as
                        a CMake list)
  forbidden-in-tests    test code (tests/) creates no std::thread / std::jthread, never sleeps with
                        sleep_for, never reads the wall clock (steady_clock::now, system_clock::now) and
                        never reads environment variables (getenv); files allowed to, with their reason,
                        are listed under "forbidden_in_tests" in tools/style_exceptions.json
  thread-outside-jobs   code that is not test code (engine/, editor/, runtime/, samples/) creates no std::thread
                        / std::jthread outside engine/src/core/jobs/ and engine/include/conjure/core/jobs/
                        (docs/CONVENTIONS.md 3, docs/design/10_core.md C4); it holds a jobs::dedicated_thread
                        instead. std::thread::id, std::thread::hardware_concurrency() and std::this_thread create
                        no thread and are not reported. std::async and the raw OS thread calls (pthread_create,
                        CreateThread, _beginthread, _beginthreadex) are reported in every folder, the two jobs
                        folders included
  file-name             file names are snake_case

With --tests-mirror-headers it also verifies that every engine/include/conjure/<module>/**/<name>.h
has a tests/<module>/**/test_<name>.cpp, every tests/support/include/conjure/testing/<name>.h a
tests/support/tests/test_<name>.cpp, every tests/support/engine/include/conjure/testing/<name>.h a
tests/support/tests/engine/test_<name>.cpp and every tests/benchmarks/include/conjure/bench/<name>.h a
tests/benchmarks/tests/test_<name>.cpp. The mapping is one-to-one (08_testing.md 5.1): when the stem occurs
more than once under the test root, the file in the header's own sub-directory counts (tests/core/math/
test_math.cpp for conjure/core/math/math.h); a test file must be listed in a CMakeLists.txt of its directory
or an ancestor up to the test root (tests/<module>, tests/support/tests, tests/support/tests/engine or
tests/benchmarks/tests; tests/CMakeLists.txt does not count), and may mirror one header only. Known exceptions
live in tools/style_exceptions.json.

Exit code 0 when clean, 1 when a finding was reported, 2 on usage errors (a path argument that does not
exist, lies outside --root, is not a C++ source or names no source at all; a malformed
tools/style_exceptions.json). Output is one `path:line: rule: message` per finding so editors and CI can link
to it.

Usage:
    python tools/check_style.py [--tests-mirror-headers] [--root <repo>] [paths...]
"""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent

SCANNED_DIRECTORIES = ("engine", "editor", "runtime", "tests", "samples")
SOURCE_SUFFIXES = {".h", ".hpp", ".inl", ".cpp", ".c", ".cc"}
HEADER_SUFFIXES = {".h", ".hpp", ".inl"}
SKIPPED_DIRECTORY_NAMES = {"build", "_deps", "third_party", ".git", "__pycache__", "node_modules"}
MAX_LINE_LENGTH = 120

# Macros defined by the Windows headers that an enumerator or constant must never be named after
# (docs/CONVENTIONS.md 3). Lower-case entries are macros too (rpcndr.h, objbase.h, minwindef.h).
WINDOWS_MACRO_NAMES = {
    "ERROR", "DELETE", "IN", "OUT", "OPTIONAL", "TRUE", "FALSE", "NEAR", "FAR", "OPAQUE", "TRANSPARENT",
    "ABSOLUTE", "RELATIVE", "DOMAIN", "DIFFERENCE", "IGNORE", "INFINITE", "TEXT", "CONST", "VOID", "PASCAL",
    "CDECL", "WINAPI", "APIENTRY", "CALLBACK", "STRICT", "NULL", "MAX_PATH", "CREATE_NEW", "CREATE_ALWAYS",
    "OPEN_EXISTING", "OPEN_ALWAYS", "TRUNCATE_EXISTING", "FILE_BEGIN", "FILE_CURRENT", "FILE_END",
    "GENERIC_READ", "GENERIC_WRITE", "INPUT_KEYBOARD", "INPUT_MOUSE", "MOUSE_MOVED", "ALTERNATE", "WINDING",
    "WIN32", "interface", "small", "hyper", "min", "max", "far", "near", "pascal", "cdecl",
}

# Third-party include prefixes: the include-order rule requires angle brackets for them. The public-header
# rule does not read this list; it accepts Conjure and standard library headers only (is_allowed_public_include).
THIRD_PARTY_INCLUDE_ROOTS = (
    "glm/", "yyjson", "stb_", "nvrhi", "Jolt/", "GLFW/", "imgui", "ImGuizmo", "miniaudio", "cgltf", "doctest",
    "nethost", "hostfxr", "coreclr", "vulkan/", "d3d12", "dxgi", "windows.h", "Windows.h",
    # libjxl's C API and the Highway headers its usage requirements put on the include path (third_party/jxl.cmake).
    "jxl/", "hwy/",
)
# Third-party single-file headers that are included without a directory prefix; everything else
# without a '/' in angle brackets is a standard library or operating-system header.
THIRD_PARTY_FLAT_HEADER_PREFIXES = (
    "yyjson", "stb_", "nvrhi", "imgui", "ImGuizmo", "miniaudio", "cgltf", "nethost", "hostfxr", "coreclr",
)
# Operating-system header directories: angle-bracket includes under these are system headers, not
# third-party ones, even though they carry a directory prefix.
SYSTEM_INCLUDE_DIRECTORIES = (
    "sys/", "netinet/", "arpa/", "net/", "linux/", "mach/", "mach-o/", "CoreFoundation/", "CoreServices/",
    "Foundation/", "AppKit/", "dispatch/", "os/", "objc/", "wrl/", "winrt/",
)
GLM_ALLOWED_HEADER = pathlib.PurePosixPath("engine/include/conjure/core/math/glm_types.h")
DOCTEST_ALLOWED_HEADERS = {
    pathlib.PurePosixPath("tests/support/include/conjure/testing/doctest.h"),
}

# Include groups in the documented order (docs/CONVENTIONS.md 2).
GROUP_OWN = 0
GROUP_CONJURE = 1
GROUP_THIRD_PARTY = 2
GROUP_SYSTEM = 3
GROUP_NAMES = {
    GROUP_OWN: "own module",
    GROUP_CONJURE: "other Conjure",
    GROUP_THIRD_PARTY: "third-party",
    GROUP_SYSTEM: "standard library / system",
}

# A std::thread / std::jthread object or type use. A nested name (std::thread::id,
# std::thread::hardware_concurrency) creates no thread, and std::this_thread is another name.
THREAD_PATTERN = re.compile(r"\bstd::(?:j)?thread\b(?!::)")
# The only directories whose code may create a thread (docs/CONVENTIONS.md 3, docs/design/10_core.md C4):
# the jobs area of core, which owns job_system and dedicated_thread.
THREAD_ALLOWED_DIRECTORIES = (
    pathlib.PurePosixPath("engine/src/core/jobs"),
    pathlib.PurePosixPath("engine/include/conjure/core/jobs"),
)
# Thread creation that does not spell the type: std::async (std::launch::async starts a thread and the default
# policy may) and the operating-system calls. No folder may use them: core::jobs itself creates its threads as
# std::thread / std::jthread objects.
UNOWNED_THREAD_PATTERN = re.compile(
    r"\b(std::async)\b|\b(pthread_create|CreateThread|_beginthread(?:ex)?)\s*\(")

# Constructs 08_testing.md 5.1 forbids in test code, with the reason shown to the author.
FORBIDDEN_IN_TESTS = (
    (THREAD_PATTERN, "std::thread: use core::jobs (job_system, dedicated_thread)"),
    (re.compile(r"\bsleep_for\s*\("), "sleep_for: wait on a condition or a bounded spin instead"),
    (re.compile(r"\b(?:steady|system|high_resolution)_clock::now\s*\("), "wall-clock read: inject a clock"),
    (re.compile(r"(?<![A-Za-z0-9_])(?:std::)?getenv\s*\("), "environment variable read: only test_main reads them"),
)

INCLUDE_PATTERN = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]')
DIRECTIVE_PATTERN = re.compile(r"^\s*#\s*([a-z]+)")
TEST_CASE_PATTERN = re.compile(
    r'\b(?:TEST_CASE|TEST_CASE_TEMPLATE|TEST_CASE_FIXTURE|[A-Z][A-Z0-9_]*_CASE)\s*\(\s*'
    r'(?:[A-Za-z_][A-Za-z0-9_:]*\s*,\s*)?"((?:[^"\\]|\\.)*)"'
)
USING_NAMESPACE_PATTERN = re.compile(r"^\s*using\s+namespace\s+[A-Za-z_:][A-Za-z0-9_:]*\s*;")
# The enumeration name is optional: the enumerators of an anonymous enum collide with the macros just the same.
ENUM_PATTERN = re.compile(
    r"\benum\b\s*(?:(?:class|struct)\b\s*)?(?:[A-Za-z_][A-Za-z0-9_]*\s*)?(?::[^{;]+)?\{", re.MULTILINE)
PLAIN_ENUM_PATTERN = re.compile(r"\benum\s+(?!class\b|struct\b)([A-Za-z_][A-Za-z0-9_]*)?\s*(?::[^{;]+)?\{")
CONSTANT_PATTERN = re.compile(
    r"\b(?P<keyword>constexpr|const)\s+(?:[A-Za-z_:][A-Za-z0-9_:<>,\s]*?\s+)?\b(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*=")
CONSTANT_NAME_PATTERN = re.compile(r"[A-Z][A-Z0-9_]*")
DEFINE_PATTERN = re.compile(r"^[ \t]*#[ \t]*define[ \t]+([A-Za-z_][A-Za-z0-9_]*)", re.MULTILINE)
IDENTIFIER_PATTERN = re.compile(r"^\s*([A-Za-z_][A-Za-z0-9_]*)")
FILE_NAME_PATTERN = re.compile(r"^[a-z0-9_]+\.[a-z]+$")

# Raw string literals (R"delimiter(...)delimiter"): encoding prefixes longest first, the longest delimiter
# the standard allows and the characters a delimiter may not contain.
RAW_STRING_PREFIXES = ("u8R", "uR", "UR", "LR", "R")
MAX_RAW_DELIMITER_LENGTH = 16
RAW_DELIMITER_EXCLUDED = frozenset(" ()\\\t\v\f\n")


class usage_error(Exception):
    """A bad command line or a malformed tools/style_exceptions.json: reported on stderr with exit code 2."""


class finding:
    """One reported problem."""

    def __init__(self, path: pathlib.Path, line: int, rule: str, message: str) -> None:
        self.path = path
        self.line = line
        self.rule = rule
        self.message = message

    def format(self, root: pathlib.Path) -> str:
        try:
            shown = self.path.resolve().relative_to(root.resolve()).as_posix()
        except ValueError:
            shown = self.path.as_posix()
        return f"{shown}:{self.line}: {self.rule}: {self.message}"


def is_identifier_character(character: str) -> bool:
    return character.isalnum() or character == "_"


def is_digit_separator(text: str, index: int) -> bool:
    """True when the apostrophe at 'index' is a C++14 digit separator (1'000, 0xFFFF'FFFFull) rather than the
    start of a character literal: the run of letters, digits, '_', '.' and apostrophes right before it starts
    with a digit. An encoding prefix (u8'a', L'x', u'x') starts with a letter, so it still opens a literal."""
    start = index
    while start > 0 and (is_identifier_character(text[start - 1]) or text[start - 1] in ".'"):
        start -= 1
    return start < index and text[start].isdigit()


def raw_string_end(text: str, index: int) -> tuple[int, bool] | None:
    """(end offset, terminated) of the raw string literal (R"delimiter(...)delimiter") whose opening quote is
    at 'index', or None when that quote does not open one. An unterminated raw string runs to the end."""
    prefix = next((candidate for candidate in RAW_STRING_PREFIXES if text.endswith(candidate, 0, index)), None)
    if prefix is None:
        return None
    before = index - len(prefix) - 1
    if before >= 0 and is_identifier_character(text[before]):
        return None
    limit = min(len(text), index + 2 + MAX_RAW_DELIMITER_LENGTH)
    open_index = index + 1
    while open_index < limit and text[open_index] not in RAW_DELIMITER_EXCLUDED:
        open_index += 1
    if open_index >= limit or text[open_index] != "(":
        return None
    closing = ")" + text[index + 1:open_index] + '"'
    close_index = text.find(closing, open_index + 1)
    if close_index < 0:
        return len(text), False
    return close_index + len(closing), True


def literal_end(text: str, index: int) -> tuple[int, bool]:
    """(end offset, terminated) of the ordinary string or character literal opening at 'index'. It ends at its
    closing quote, at an unescaped newline (so a stray quote cannot swallow the lines after it) or at the end
    of the text; a backslash-newline continues it."""
    quote = text[index]
    end = index + 1
    length = len(text)
    while end < length and text[end] != quote and text[end] != "\n":
        end += 2 if text[end] == "\\" else 1
    if end < length and text[end] == quote:
        return end + 1, True
    return min(end, length), False


def blank_literal(span: str, terminated: bool) -> str:
    """The literal 'span' with its opening quote, its closing quote (when terminated) and every newline kept
    and everything else replaced by spaces, so offsets and line numbers survive."""
    closing = span[-1] if terminated and len(span) >= 2 else ""
    body = span[1:len(span) - len(closing)]
    return span[:1] + "".join("\n" if character == "\n" else " " for character in body) + closing


def strip_comments(text: str, keep_strings: bool = False) -> str:
    """Replaces comments (and, unless keep_strings is set, string and character literals) with spaces of the
    same length, keeping every newline, so that line numbers and offsets are preserved while regexes no longer
    see commented-out code or text inside literals. Raw strings are one literal and digit separators are
    code."""
    result: list[str] = []
    index = 0
    length = len(text)
    while index < length:
        char = text[index]
        if text.startswith("//", index):
            end = text.find("\n", index)
            end = length if end < 0 else end
            result.append(" " * (end - index))
            index = end
        elif text.startswith("/*", index):
            end = text.find("*/", index + 2)
            end = length if end < 0 else end + 2
            result.append("".join("\n" if c == "\n" else " " for c in text[index:end]))
            index = end
        elif char == "'" and is_digit_separator(text, index):
            result.append(char)
            index += 1
        elif char in "\"'":
            raw = raw_string_end(text, index) if char == '"' else None
            end, terminated = raw if raw is not None else literal_end(text, index)
            span = text[index:end]
            result.append(span if keep_strings else blank_literal(span, terminated))
            index = end
        else:
            result.append(char)
            index += 1
    return "".join(result)


def line_of(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def iter_source_files(root: pathlib.Path, explicit: list[str]) -> list[pathlib.Path]:
    """Every C++ source to check: the files and directories named on the command line (relative to the root),
    or the scanned directories of the whole repository. Raises usage_error for a path that does not exist,
    lies outside the root or is not a C++ source, and when the named paths hold no source at all."""
    if explicit:
        files: list[pathlib.Path] = []
        resolved_root = root.resolve()
        for entry in explicit:
            path = pathlib.Path(entry)
            path = path if path.is_absolute() else root / path
            if not path.exists():
                raise usage_error(f"{entry} does not exist")
            if not path.resolve().is_relative_to(resolved_root):
                raise usage_error(f"{entry} is outside the repository root {root}")
            if path.is_dir():
                files.extend(collect(path))
            elif path.suffix in SOURCE_SUFFIXES:
                files.append(path)
            else:
                raise usage_error(
                    f"{entry} is not a C++ source file (accepted: {', '.join(sorted(SOURCE_SUFFIXES))})")
        if not files:
            raise usage_error("no C++ source file under the given paths")
        return sorted(set(files))

    files = []
    for name in SCANNED_DIRECTORIES:
        directory = root / name
        if directory.is_dir():
            files.extend(collect(directory))
    return sorted(set(files))


def collect(directory: pathlib.Path) -> list[pathlib.Path]:
    files: list[pathlib.Path] = []
    for path in directory.rglob("*"):
        if any(part in SKIPPED_DIRECTORY_NAMES for part in path.relative_to(directory).parts):
            continue
        if path.is_file() and path.suffix in SOURCE_SUFFIXES:
            files.append(path)
    return files


def relative_posix(root: pathlib.Path, path: pathlib.Path) -> pathlib.PurePosixPath:
    return pathlib.PurePosixPath(path.resolve().relative_to(root.resolve()).as_posix())


def check_file(
    root: pathlib.Path,
    path: pathlib.Path,
    forbidden_exceptions: dict[str, str],
    used_forbidden_exceptions: set[str],
) -> list[finding]:
    findings: list[finding] = []
    raw = path.read_bytes()
    if b"\r" in raw:
        first = raw.split(b"\r", 1)[0].count(b"\n") + 1
        findings.append(finding(path, first, "crlf", "CRLF line ending; the repository uses LF"))
    text = raw.decode("utf-8", errors="replace").replace("\r\n", "\n").replace("\r", "\n")
    relative = relative_posix(root, path)
    is_header = path.suffix in HEADER_SUFFIXES
    is_test_code = relative.parts[:1] == ("tests",)

    if not FILE_NAME_PATTERN.match(path.name):
        findings.append(finding(path, 1, "file-name", f"'{path.name}' is not snake_case"))

    lines = text.split("\n")
    for number, line in enumerate(lines, start=1):
        if "\t" in line:
            findings.append(finding(path, number, "tabs", "tab character; indent with four spaces"))
        if len(line) > MAX_LINE_LENGTH:
            findings.append(finding(path, number, "line-length", f"{len(line)} columns, maximum is {MAX_LINE_LENGTH}"))
        if line != line.rstrip(" \t"):
            findings.append(finding(path, number, "trailing-whitespace", "trailing whitespace"))

    code = strip_comments(text)
    code_lines = code.split("\n")
    # Include targets and TEST_CASE names are string literals: those rules read a copy that keeps them.
    literal_code = strip_comments(text, keep_strings=True)
    literal_lines = literal_code.split("\n")

    if is_header:
        check_pragma_once(path, code_lines, findings)
        for number, line in enumerate(code_lines, start=1):
            if USING_NAMESPACE_PATTERN.match(line):
                findings.append(finding(path, number, "using-namespace", "using-namespace directive in a header"))

    check_windows_macro_names(path, code, findings)
    check_enum_class(path, code, findings)
    check_includes(path, relative, literal_lines, findings)
    check_include_order(path, relative, literal_lines, findings)
    if is_test_code and not is_header:
        check_test_case_names(path, literal_code, findings)
    if is_test_code:
        key = relative.as_posix()
        if key in forbidden_exceptions:
            used_forbidden_exceptions.add(key)
        else:
            check_forbidden_in_tests(path, code, findings)
    else:
        in_jobs_area = any(relative.is_relative_to(directory) for directory in THREAD_ALLOWED_DIRECTORIES)
        check_thread_outside_jobs(path, code, in_jobs_area, findings)
    return findings


def check_test_case_names(path: pathlib.Path, code: str, findings: list[finding]) -> None:
    for match in TEST_CASE_PATTERN.finditer(code):
        name = match.group(1)
        line = line_of(code, match.start(1))
        if " - " not in name:
            findings.append(finding(
                path, line, "test-case-name", f"'{name}' does not follow \"<type> - <behaviour>\""))
        if ";" in name or name.count("[") != name.count("]"):
            findings.append(finding(
                path, line, "test-case-name",
                f"'{name}' contains ';' or unbalanced brackets, which break CTest discovery"))


def check_forbidden_in_tests(path: pathlib.Path, code: str, findings: list[finding]) -> None:
    for pattern, message in FORBIDDEN_IN_TESTS:
        for match in pattern.finditer(code):
            findings.append(finding(
                path, line_of(code, match.start()), "forbidden-in-tests",
                f"{message} (08_testing.md 5.1; list the file in tools/style_exceptions.json with a reason "
                f"when the design allows it)"))


def check_thread_outside_jobs(path: pathlib.Path, code: str, in_jobs_area: bool, findings: list[finding]) -> None:
    """Test code has its own rule (forbidden-in-tests) with an exception table; this one has no exceptions.

    The two jobs folders of core may name std::thread / std::jthread (in_jobs_area); std::async and the raw
    operating-system thread calls are reported there as well.
    """
    if not in_jobs_area:
        for match in THREAD_PATTERN.finditer(code):
            findings.append(finding(
                path, line_of(code, match.start()), "thread-outside-jobs",
                f"{match.group(0)} outside core::jobs: hold a core::jobs::dedicated_thread or use the job_system "
                f"(docs/CONVENTIONS.md 3, 10_core.md C4)"))
    for match in UNOWNED_THREAD_PATTERN.finditer(code):
        name = match.group(1) or match.group(2)
        findings.append(finding(
            path, line_of(code, match.start()), "thread-outside-jobs",
            f"{name} creates a thread that core::jobs does not own: hold a core::jobs::dedicated_thread or use "
            f"the job_system (docs/CONVENTIONS.md 3, 10_core.md C4)"))


def check_pragma_once(path: pathlib.Path, code_lines: list[str], findings: list[finding]) -> None:
    for number, line in enumerate(code_lines, start=1):
        stripped = line.strip()
        if not stripped:
            continue
        if stripped.startswith("#pragma once"):
            return
        findings.append(finding(path, number, "pragma-once", "header does not start with #pragma once"))
        return
    findings.append(finding(path, 1, "pragma-once", "header has no #pragma once"))


def check_enum_class(path: pathlib.Path, code: str, findings: list[finding]) -> None:
    for match in PLAIN_ENUM_PATTERN.finditer(code):
        name = match.group(1) or "<anonymous>"
        if name.endswith("_flags"):
            continue
        findings.append(finding(
            path, line_of(code, match.start()), "enum-class",
            f"enum '{name}' is not an enum class (docs/CONVENTIONS.md 3)"))


def check_windows_macro_names(path: pathlib.Path, code: str, findings: list[finding]) -> None:
    for match in ENUM_PATTERN.finditer(code):
        body_start = match.end()
        depth = 1
        index = body_start
        while index < len(code) and depth > 0:
            if code[index] == "{":
                depth += 1
            elif code[index] == "}":
                depth -= 1
            index += 1
        body = code[body_start:index - 1]
        offset = body_start
        for enumerator in body.split(","):
            identifier = IDENTIFIER_PATTERN.match(enumerator)
            if identifier and identifier.group(1) in WINDOWS_MACRO_NAMES:
                findings.append(finding(
                    path, line_of(code, offset + identifier.start(1)), "windows-macro",
                    f"enumerator '{identifier.group(1)}' is a Windows macro name"))
            offset += len(enumerator) + 1

    for match in CONSTANT_PATTERN.finditer(code):
        name = match.group("name")
        if name not in WINDOWS_MACRO_NAMES:
            continue
        # docs/CONVENTIONS.md 1: a constexpr name is a constant, but `const float near = 1.0f;` is a local
        # variable (camelCase); only an ALL_CAPS const name is a constant.
        if match.group("keyword") == "const" and not CONSTANT_NAME_PATTERN.fullmatch(name):
            continue
        findings.append(finding(
            path, line_of(code, match.start("name")), "windows-macro",
            f"constant '{name}' is a Windows macro name"))

    for match in DEFINE_PATTERN.finditer(code):
        name = match.group(1)
        if name in WINDOWS_MACRO_NAMES:
            findings.append(finding(
                path, line_of(code, match.start(1)), "windows-macro",
                f"macro '{name}' redefines a Windows macro name"))


def check_includes(
    path: pathlib.Path,
    relative: pathlib.PurePosixPath,
    code_lines: list[str],
    findings: list[finding],
) -> None:
    is_public_header = relative.parts[:2] == ("engine", "include")
    is_test_code = relative.parts[:1] == ("tests",)
    for number, line in enumerate(code_lines, start=1):
        match = INCLUDE_PATTERN.match(line)
        if not match:
            continue
        target = match.group(2)
        if is_public_header and not is_allowed_public_include(target, match.group(1) == '"', relative):
            findings.append(finding(
                path, number, "third-party-include",
                f"public header includes <{target}>; public headers include only Conjure and C++ standard "
                f"library headers (glm only through conjure/core/math/glm_types.h)"))
        if is_test_code and target.startswith("doctest/") and relative not in DOCTEST_ALLOWED_HEADERS:
            findings.append(finding(
                path, number, "doctest-include",
                "include \"conjure/testing/doctest.h\" instead of <doctest/doctest.h>"))


def is_allowed_public_include(target: str, quoted: bool, relative: pathlib.PurePosixPath) -> bool:
    """True for an include a public header may carry (docs/CONVENTIONS.md 2): a Conjure header, a bare C++
    standard library name (<vector>, <cstdint>), or glm inside conjure/core/math/glm_types.h. Everything else
    is rejected, so a third-party or operating-system header needs no list entry to be caught. A Conjure header
    in angle brackets is left to the include-order rule."""
    if target.startswith("conjure/"):
        return True
    if quoted:
        return False
    if target.startswith("glm/"):
        return relative == GLM_ALLOWED_HEADER
    return "/" not in target and "." not in target


def own_module(relative: pathlib.PurePosixPath) -> str | None:
    """The module whose headers count as "own" for a file: conjure/<module>/ for engine code, the
    executable's name for editor/ and runtime/, the test support headers (conjure/testing/) for tests."""
    parts = relative.parts
    if parts[:3] == ("engine", "include", "conjure") and len(parts) > 4:
        return parts[3]
    if parts[:2] == ("engine", "src") and len(parts) > 3:
        return parts[2]
    if parts[:1] in (("editor",), ("runtime",)):
        return parts[0]
    if parts[:1] == ("tests",):
        return "testing"
    return None


def include_group(target: str, quoted: bool, own: str | None) -> int:
    if target.startswith("conjure/"):
        module = target.split("/")[1] if "/" in target[len("conjure/"):] else ""
        return GROUP_OWN if module == own else GROUP_CONJURE
    if quoted:
        # "core/jobs/thread_name.h" is a private Conjure header reached through engine/src: its first
        # component is the module. A bare "math_test.h" is a helper next to the file, so the file's own.
        if "/" not in target:
            return GROUP_OWN
        return GROUP_OWN if target.split("/")[0] == own else GROUP_CONJURE
    if target.startswith(SYSTEM_INCLUDE_DIRECTORIES):
        return GROUP_SYSTEM
    if "/" in target or target.startswith(THIRD_PARTY_FLAT_HEADER_PREFIXES):
        return GROUP_THIRD_PARTY
    return GROUP_SYSTEM


def check_include_order(
    path: pathlib.Path,
    relative: pathlib.PurePosixPath,
    code_lines: list[str],
    findings: list[finding],
) -> None:
    """Only unconditional includes (preprocessor depth 0) take part: platform switches legitimately
    pick different system headers per branch."""
    own = own_module(relative)
    depth = 0
    highest_group = -1
    highest_target = ""
    previous: tuple[int, int, str] | None = None   # (line, group, target) of the previous include
    for number, line in enumerate(code_lines, start=1):
        directive = DIRECTIVE_PATTERN.match(line)
        if directive and directive.group(1) in ("if", "ifdef", "ifndef"):
            depth += 1
            continue
        if directive and directive.group(1) == "endif":
            depth = max(depth - 1, 0)
            continue
        match = INCLUDE_PATTERN.match(line)
        if not match or depth > 0:
            continue

        quoted = match.group(1) == '"'
        target = match.group(2)
        if target.startswith("conjure/") and not quoted:
            findings.append(finding(
                path, number, "include-order", f"Conjure header <{target}> must be included with quotes"))
        if quoted and target.startswith(THIRD_PARTY_INCLUDE_ROOTS):
            findings.append(finding(
                path, number, "include-order", f"third-party header \"{target}\" must use angle brackets"))

        group = include_group(target, quoted, own)
        if group < highest_group:
            findings.append(finding(
                path, number, "include-order",
                f"{GROUP_NAMES[group]} header {target} comes after the {GROUP_NAMES[highest_group]} header "
                f"{highest_target}; order is own module, other Conjure, third-party, standard library"))
        elif previous is not None and previous[0] == number - 1 and previous[1] != group:
            findings.append(finding(
                path, number, "include-order",
                f"separate the {GROUP_NAMES[group]} includes from the {GROUP_NAMES[previous[1]]} includes "
                f"with a blank line"))

        if group > highest_group:
            highest_group = group
            highest_target = target
        previous = (number, group, target)


def load_exceptions(root: pathlib.Path) -> dict[str, dict[str, str]]:
    """The parsed tools/style_exceptions.json (empty when absent); usage_error when it is malformed."""
    path = root / "tools" / "style_exceptions.json"
    if not path.exists():
        return {}
    try:
        with path.open(encoding="utf-8") as stream:
            data = json.load(stream)
    except ValueError as error:
        raise usage_error(f"{path}: {error}") from error
    if not isinstance(data, dict):
        raise usage_error(f"{path}: expected a JSON object")
    return data


def exception_table(exceptions: dict[str, dict[str, str]], key: str) -> dict[str, str]:
    """One table of style_exceptions.json (path -> reason); usage_error when it is not one."""
    table = exceptions.get(key, {})
    if not isinstance(table, dict) or not all(isinstance(value, str) for value in table.values()):
        raise usage_error(f"tools/style_exceptions.json: {key} must map paths to reasons")
    return table


def mirrored_headers(root: pathlib.Path) -> list[tuple[pathlib.Path, pathlib.Path, str, tuple[str, ...]]]:
    """(header, test root, exception key, the header's sub-directory below its module) of every header that
    needs a test file."""
    pairs: list[tuple[pathlib.Path, pathlib.Path, str, tuple[str, ...]]] = []

    include_root = root / "engine" / "include" / "conjure"
    if include_root.is_dir():
        for header in sorted(include_root.rglob("*.h")):
            parts = header.relative_to(include_root).parts
            pairs.append((header, root / "tests" / parts[0], header.relative_to(root / "engine" / "include").as_posix(),
                          parts[1:-1]))

    support_root = root / "tests" / "support" / "include" / "conjure" / "testing"
    if support_root.is_dir():
        for header in sorted(support_root.glob("*.h")):
            pairs.append((header, root / "tests" / "support" / "tests",
                          header.relative_to(root / "tests" / "support" / "include").as_posix(), ()))

    # The engine-level support library (tests/support/engine) mirrors its headers into its own self-test
    # directory; the key carries the "engine/" prefix so an exception can name either library's header.
    engine_support_root = root / "tests" / "support" / "engine" / "include" / "conjure" / "testing"
    if engine_support_root.is_dir():
        for header in sorted(engine_support_root.glob("*.h")):
            pairs.append((header, root / "tests" / "support" / "tests" / "engine",
                          "engine/" + header.relative_to(root / "tests" / "support" / "engine" / "include").as_posix(),
                          ()))

    # The benchmark harness (tests/benchmarks/include/conjure/bench) has its self-tests in tests/benchmarks/tests.
    bench_root = root / "tests" / "benchmarks" / "include" / "conjure" / "bench"
    if bench_root.is_dir():
        for header in sorted(bench_root.glob("*.h")):
            pairs.append((header, root / "tests" / "benchmarks" / "tests", f"bench/{header.name}", ()))
    return pairs


def is_listed_in_cmake(test_file: pathlib.Path, test_root: pathlib.Path, listings: dict[pathlib.Path, str]) -> bool:
    """True when a CMakeLists.txt in the test file's directory or an ancestor up to 'test_root' names it, so the
    file is built. 'listings' caches the text of every CMakeLists.txt read."""
    name = re.compile(rf"(?<![\w.-]){re.escape(test_file.name)}(?![\w.])")
    directory = test_file.parent
    while True:
        listing = directory / "CMakeLists.txt"
        if listing not in listings:
            listings[listing] = listing.read_text(encoding="utf-8", errors="replace") if listing.is_file() else ""
        if name.search(listings[listing]):
            return True
        if directory == test_root or directory == directory.parent:
            return False
        directory = directory.parent


def check_tests_mirror_headers(root: pathlib.Path, exceptions: dict[str, str]) -> list[finding]:
    """One test file per public header (08_testing.md 5.1): test_<stem>.cpp under the header's test root, in
    the header's sub-directory when the stem occurs more than once, listed in a CMakeLists.txt and mirroring no
    other header."""
    findings: list[finding] = []
    listings: dict[pathlib.Path, str] = {}
    claimed: dict[pathlib.Path, str] = {}
    used_exceptions: set[str] = set()
    for header, test_root, key, sub_directory in mirrored_headers(root):
        expected = f"test_{header.stem}.cpp"
        if key in exceptions:
            used_exceptions.add(key)
            continue
        shown_root = test_root.relative_to(root).as_posix()
        candidates = sorted(test_root.rglob(expected)) if test_root.is_dir() else []
        if not candidates:
            findings.append(finding(
                header, 1, "tests-mirror-headers",
                f"no {shown_root}/**/{expected} for this header "
                f"(add it or list the header in tools/style_exceptions.json with a reason)"))
            continue
        test_file = candidates[0]
        if len(candidates) > 1:
            test_file = test_root.joinpath(*sub_directory, expected)
            if test_file not in candidates:
                findings.append(finding(
                    header, 1, "tests-mirror-headers",
                    f"several {expected} under {shown_root}; keep the test in the header's sub-directory "
                    f"({'/'.join((shown_root, *sub_directory, expected))})"))
                continue
        shown_test = test_file.relative_to(root).as_posix()
        if not is_listed_in_cmake(test_file, test_root, listings):
            findings.append(finding(
                header, 1, "tests-mirror-headers", f"{shown_test} is not listed in any CMakeLists.txt"))
            continue
        first = claimed.setdefault(test_file, key)
        if first != key:
            findings.append(finding(
                header, 1, "tests-mirror-headers",
                f"{shown_test} already mirrors {first}; one test file per public header (08_testing.md 5.1)"))

    for key in sorted(set(exceptions) - used_exceptions):
        findings.append(finding(
            root / "tools" / "style_exceptions.json", 1, "tests-mirror-headers",
            f"exception '{key}' names a header that does not exist; remove it"))
    return findings


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--tests-mirror-headers", action="store_true",
                        help="also check that every public header has a test file")
    parser.add_argument("--root", default=str(ROOT), help="repository root (default: the parent of tools/)")
    parser.add_argument("paths", nargs="*", help="files or directories to check (default: the whole repository)")
    args = parser.parse_args(argv)

    root = pathlib.Path(args.root).resolve()
    if not root.is_dir():
        print(f"check_style: {root} is not a directory", file=sys.stderr)
        return 2

    try:
        exceptions = load_exceptions(root)
        forbidden_exceptions = exception_table(exceptions, "forbidden_in_tests")
        mirror_exceptions = exception_table(exceptions, "tests_mirror_headers")
        files = iter_source_files(root, args.paths)
    except usage_error as error:
        print(f"check_style: {error}", file=sys.stderr)
        return 2
    used_forbidden_exceptions: set[str] = set()

    findings: list[finding] = []
    for path in files:
        findings.extend(check_file(root, path, forbidden_exceptions, used_forbidden_exceptions))

    # Stale entries are only detectable on a whole-repository run.
    if not args.paths:
        for key in sorted(set(forbidden_exceptions) - used_forbidden_exceptions):
            findings.append(finding(
                root / "tools" / "style_exceptions.json", 1, "forbidden-in-tests",
                f"exception '{key}' names a file that does not exist; remove it"))

    if args.tests_mirror_headers:
        findings.extend(check_tests_mirror_headers(root, mirror_exceptions))

    for item in sorted(findings, key=lambda f: (f.path.as_posix(), f.line, f.rule)):
        print(item.format(root))

    summary = f"check_style: {len(files)} files, {len(findings)} finding(s)"
    print(summary)
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
