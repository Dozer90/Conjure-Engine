#!/usr/bin/env python3
"""Export checks for the umbrella shared library conjure_shared (docs/design/07_modules.md 5,
docs/ARCHITECTURE.md 9, docs/design/09_cross_area_resolutions.md 4 items 13a / 14a).

conjure_shared exports only what carries CONJURE_API (conjure/core/api.h): the static libraries
are compiled with CONJURE_SHARED_EXPORTS and hidden visibility, so a public declaration without the
macro is invisible to plugins and executables that link the umbrella. This tool has three modes.

Annotation check (the default): for every header under engine/include/conjure/<module>/ it reports

  class        a class or struct that is not a template (explicit specialisations count as classes,
               nested classes are checked on their own because MSVC does not export them with the
               enclosing class) and declares at least one member function out of line (a prototype
               ending in ';' that is not defaulted, deleted, pure virtual or a template; operators
               and conversion operators count like any other member) while its head lacks CONJURE_API
  function     a namespace-scope function or operator prototype (not a template, not
               inline/constexpr/consteval, not a friend, not a macro invocation) without CONJURE_API
  variable     an `extern` variable declaration without CONJURE_API

Anything header-only (templates, inline and constexpr functions, enums, aggregates without methods,
macro invocations such as CONJURE_REFLECT_STRUCT) needs no annotation and is not reported.

Export count (--count-exports <binary>): reads the number of exported symbols of a built shared
library from `dumpbin /EXPORTS` (Windows) or `llvm-nm` / `nm` (ELF, Mach-O) and fails when it is
above --limit (default 52428, four fifths of the 65535 a Windows DLL may hold).

Self-test (--self-test): runs the checker over embedded header snippets and the count parser over
embedded tool output, so the CTest entry exports.check_tool proves the tool itself.

Exit code 0 when clean, 1 when a finding was reported or the count is above the limit, 2 on usage
errors (missing directory, missing tool, unparseable tool output). Output is one
`path:line: kind: message` per finding.

Usage:
    python tools/check_exports.py [--root <repo>]                     every module under engine/include
    python tools/check_exports.py [--root <repo>] <module> [...]      only the named modules
    python tools/check_exports.py --all                               the same as no module arguments
    python tools/check_exports.py --count-exports <binary> [--limit <n>] [--tool <dumpbin|nm>]
    python tools/check_exports.py --self-test
"""

from __future__ import annotations

import argparse
import pathlib
import platform
import re
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent

DEFAULT_EXPORT_LIMIT = 52428
WINDOWS_EXPORT_MAXIMUM = 65535

CLASS_HEAD_PATTERN = re.compile(
    r"(?P<template>template\s*<[^;{}]*>\s*)?"
    r"(?<!enum\s)\b(?P<keyword>class|struct)\s+"
    r"(?:\[\[[^\]]*\]\]\s*)?"
    r"(?P<api>CONJURE_API\s+)?"
    r"(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*"
    r"(?P<arguments><[^;{}]*>\s*)?"
    r"(?P<final>final\s*)?"
    r"(?::[^{;]*)?\{"
)
EXPLICIT_SPECIALISATION_PATTERN = re.compile(r"^template\s*<\s*>\s*$")
FUNCTION_PATTERN = re.compile(
    r"^(?P<indent>\s*)(?P<decl>[^\s#/{}][^;{}]*?\))\s*(?:const\s*)?(?:noexcept(?:\([^)]*\))?\s*)?"
    r"(?:->\s*[^;{}]+)?\s*(?:override\s*)?;",
    re.MULTILINE,
)
# String literals are blanked before matching, so `extern "C"` arrives as `extern " "`: any quote
# after the keyword marks a linkage specification, which is a function (checked above), not a variable.
EXTERN_PATTERN = re.compile(r"^[ 	]*(?P<keyword>extern)\s+(?!\")(?P<decl>[^;{}]+);", re.MULTILINE)
IDENTIFIER_CALL = re.compile(r"([A-Za-z_~][A-Za-z0-9_]*)\s*\($")
# An operator prototype's name is not an identifier: `operator&(`, `operator==(`, `operator++(`,
# `operator()(`, `operator[](`, `operator new[](`, `operator bool(`, `operator const char *(`. The
# pattern matches the keyword, the operator token and the opening parenthesis of the parameter list.
OPERATOR_PROTOTYPE = re.compile(
    r"\boperator\s*"
    r"(?:\(\s*\)|\[\s*\]|(?:new|delete)\s*(?:\[\s*\])?|[^\w\s(\[]+|[A-Za-z_][\w:<>,\s*&]*?)"
    r"\s*\("
)
# Symbolic operator tokens hold '=' and '<' / '>', which the top-level assignment check would read as
# an initialiser or a template bracket; they are replaced by a plain word before that check runs.
OPERATOR_TOKEN = re.compile(r"\boperator\s*[^\w\s(\[]+")
MACRO_NAME = re.compile(r"^[A-Z][A-Z0-9_]*$")
KEYWORDS_NOT_FUNCTIONS = {
    "if", "for", "while", "switch", "return", "sizeof", "alignof", "decltype", "static_assert",
    "requires",
}
DUMPBIN_FUNCTION_COUNT = re.compile(r"^\s*(?P<count>\d+)\s+number of functions\s*$", re.MULTILINE)


class finding:
    def __init__(self, path: pathlib.Path, line: int, kind: str, message: str) -> None:
        self.path = path
        self.line = line
        self.kind = kind
        self.message = message

    def format(self, root: pathlib.Path) -> str:
        try:
            shown = self.path.resolve().relative_to(root.resolve()).as_posix()
        except ValueError:
            shown = self.path.as_posix()
        return f"{shown}:{self.line}: {self.kind}: {self.message}"


#------------------------------------------------------------------------------
# Source preparation
#------------------------------------------------------------------------------

def strip_comments_and_strings(text: str) -> str:
    """Blanks comments and string literals, keeping newlines so line numbers survive."""
    result: list[str] = []
    index = 0
    length = len(text)
    while index < length:
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
        elif text[index] in "\"'":
            quote = text[index]
            end = index + 1
            while end < length and text[end] != quote:
                end += 2 if text[end] == "\\" else 1
            end = min(end + 1, length)
            result.append(quote + " " * max(end - index - 2, 0) + (quote if end - index >= 2 else ""))
            index = end
        else:
            result.append(text[index])
            index += 1
    return "".join(result)


def strip_macro_lines(text: str) -> str:
    """Blanks preprocessor directives (including continuation lines) so macro bodies are not parsed."""
    lines = text.split("\n")
    in_macro = False
    for number, line in enumerate(lines):
        stripped = line.strip()
        if in_macro or stripped.startswith("#"):
            in_macro = stripped.endswith("\\")
            lines[number] = ""
    return "\n".join(lines)


def line_of(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def matching_brace(text: str, open_index: int) -> int:
    depth = 0
    for index in range(open_index, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return index
    return len(text)


FUNCTION_BODY_PREFIX = re.compile(
    r"\)\s*(?:const\s*)?(?:volatile\s*)?(?:&&?\s*)?(?:noexcept(?:\([^()]*\))?\s*)?"
    r"(?:->\s*[A-Za-z_:<>,\s*&\[\]]+?\s*)?(?:override\s*|final\s*)*(?:requires[^{;]*)?$"
)


def blank_function_bodies(code: str) -> str:
    """Blanks every `{...}` that follows a parameter list (inline member and free function bodies,
    lambdas, constructor initialiser lists), keeping newlines, so statements inside bodies are never
    mistaken for prototypes."""
    result = list(code)
    index = 0
    length = len(code)
    while index < length:
        if code[index] != "{":
            index += 1
            continue
        before = code[max(0, index - 400):index]
        if FUNCTION_BODY_PREFIX.search(before) is None:
            index += 1
            continue
        close_index = matching_brace(code, index)
        for position in range(index + 1, min(close_index, length)):
            if result[position] != "\n":
                result[position] = " "
        index = close_index + 1
    return "".join(result)


def prepare(text: str) -> str:
    """The analysable form of a header: no comments, strings, directives or function bodies."""
    return blank_function_bodies(strip_macro_lines(strip_comments_and_strings(text)))


#------------------------------------------------------------------------------
# Declaration classification
#------------------------------------------------------------------------------

def has_top_level_assignment(declaration: str) -> bool:
    """True when '=' appears outside every parenthesis: a member or variable initialiser."""
    depth = 0
    for character in declaration:
        if character in "(<[":
            depth += 1
        elif character in ")>]":
            depth -= 1
        elif character == "=" and depth == 0:
            return True
    return False


def is_out_of_line_method(declaration: str) -> bool:
    """True for a prototype that needs a definition in a .cpp (so an export)."""
    if "= default" in declaration or "= delete" in declaration or "= 0" in declaration:
        return False
    if has_top_level_assignment(OPERATOR_TOKEN.sub("operator_token", declaration)):
        return False
    if declaration.lstrip().startswith("template") or declaration.lstrip().startswith("friend"):
        return False
    if re.search(r"\b(inline|constexpr|consteval)\b", declaration):
        return False
    if "(" not in declaration:
        return False
    if OPERATOR_PROTOTYPE.search(declaration) is not None:
        return True
    call = IDENTIFIER_CALL.search(declaration.split("(")[0] + "(")
    if call is None:
        return False
    name = call.group(1)
    if name in KEYWORDS_NOT_FUNCTIONS:
        return False
    # CONJURE_REFLECT_STRUCT(...), CONJURE_COMPONENT(...) and friends expand to templates or
    # constants; a macro invocation is never a prototype.
    return MACRO_NAME.match(name) is None


def is_template_head(match: re.Match) -> bool:
    """True for a class template or partial specialisation; an explicit specialisation
    (`template <>`) is an ordinary class whose members live in a .cpp."""
    template = match.group("template")
    if not template:
        return False
    return EXPLICIT_SPECIALISATION_PATTERN.match(template.strip()) is None


def class_bodies(code: str) -> list[tuple[re.Match, str, int, int]]:
    """Every class/struct head at the top level of 'code' with its body text, body start offset and
    body end offset. Bodies are skipped over, so nested classes are not returned; callers recurse."""
    bodies: list[tuple[re.Match, str, int, int]] = []
    position = 0
    while True:
        match = CLASS_HEAD_PATTERN.search(code, position)
        if match is None:
            break
        open_index = match.end() - 1
        close_index = matching_brace(code, open_index)
        bodies.append((match, code[open_index + 1:close_index], open_index + 1, close_index))
        position = close_index + 1
    return bodies


def blank_ranges(text: str, ranges: list[tuple[int, int]]) -> str:
    result = list(text)
    for start, end in ranges:
        for position in range(start, min(end, len(result))):
            if result[position] != "\n":
                result[position] = " "
    return "".join(result)


def declares_out_of_line_members(body: str) -> bool:
    """True when the class body (nested class bodies already blanked) holds a prototype that needs
    a definition in a .cpp."""
    for method in FUNCTION_PATTERN.finditer(body):
        if is_out_of_line_method(method.group("decl")):
            return True
    return False


def check_classes(
  path: pathlib.Path,
  code: str,
  body: str,
  body_offset: int,
  nested: bool,
  findings: list[finding],
) -> None:
    """Reports every non-template class in 'body' (a header or an enclosing class body) that needs
    CONJURE_API, recursing into nested classes. 'body_offset' maps body positions to 'code'."""
    for match, inner, inner_start, inner_end in class_bodies(body):
        if is_template_head(match):
            continue
        nested_ranges = [(start, end) for _, _, start, end in class_bodies(inner)]
        own_body = blank_ranges(inner, nested_ranges)
        if declares_out_of_line_members(own_body) and not match.group("api"):
            where = "nested class" if nested else "class"
            detail = (
                " (MSVC does not export nested classes with the enclosing one)" if nested else ""
            )
            findings.append(finding(
                path, line_of(code, body_offset + match.start("name")), "class",
                f"{where} '{match.group('name')}' declares out-of-line members but is not marked CONJURE_API{detail}"))
        check_classes(path, code, inner, body_offset + inner_start, True, findings)


def class_ranges_of(body: str, body_offset: int) -> list[tuple[int, int]]:
    """Offsets (in the enclosing text) covered by every class head and body in 'body', so
    namespace-scope checks skip members."""
    ranges: list[tuple[int, int]] = []
    for match, _, _, inner_end in class_bodies(body):
        ranges.append((body_offset + match.start(), body_offset + inner_end))
    return ranges


def check_header(path: pathlib.Path) -> list[finding]:
    text = path.read_text(encoding="utf-8", errors="replace")
    return check_source(path, text)


def check_source(path: pathlib.Path, text: str) -> list[finding]:
    code = prepare(text)
    findings: list[finding] = []

    check_classes(path, code, code, 0, False, findings)
    class_ranges = class_ranges_of(code, 0)

    def inside_class(offset: int) -> bool:
        return any(start <= offset < end for start, end in class_ranges)

    for match in FUNCTION_PATTERN.finditer(code):
        if inside_class(match.start()):
            continue
        declaration = match.group("decl")
        if not is_out_of_line_method(declaration):
            continue
        if "CONJURE_API" in declaration or "typedef" in declaration or "using " in declaration:
            continue
        findings.append(finding(
            path, line_of(code, match.start("decl")), "function",
            f"'{declaration.strip()}' is declared out of line without CONJURE_API"))

    for match in EXTERN_PATTERN.finditer(code):
        if inside_class(match.start()):
            continue
        declaration = match.group("decl")
        if "CONJURE_API" in declaration or "template" in declaration:
            continue
        findings.append(finding(
            path, line_of(code, match.start("keyword")), "variable",
            f"extern '{declaration.strip()}' is not marked CONJURE_API"))
    return findings


#------------------------------------------------------------------------------
# Export count of a built shared library
#------------------------------------------------------------------------------

def parse_dumpbin_exports(output: str) -> int | None:
    """The export address table size from `dumpbin /EXPORTS`, or None when the output has none
    (a binary without an export directory prints no such line)."""
    match = DUMPBIN_FUNCTION_COUNT.search(output)
    if match is None:
        return None
    return int(match.group("count"))


def parse_nm_exports(output: str) -> int:
    """Defined dynamic symbols in `nm -D --defined-only` / `llvm-nm --dynamic --defined-only` output:
    one symbol per line, `[address] <type> <name>`; undefined (U) and empty lines are skipped."""
    count = 0
    for line in output.splitlines():
        parts = line.split()
        if len(parts) < 2:
            continue
        symbol_type = parts[-2] if len(parts) >= 3 else parts[0]
        if symbol_type == "U" or symbol_type == "w":
            continue
        count += 1
    return count


def find_tool(explicit: str | None, binary: pathlib.Path) -> tuple[pathlib.Path | None, str]:
    """The export-listing tool for the binary: ('path', 'dumpbin' | 'nm'); the path is None when
    nothing was found."""
    if explicit:
        candidate = pathlib.Path(explicit)
        kind = "dumpbin" if candidate.stem.lower() == "dumpbin" else "nm"
        return (candidate if candidate.exists() else None), kind
    if binary.suffix.lower() == ".dll" or platform.system() == "Windows":
        found = shutil.which("dumpbin")
        return (pathlib.Path(found) if found else None), "dumpbin"
    for name in ("llvm-nm", "nm"):
        found = shutil.which(name)
        if found:
            return pathlib.Path(found), "nm"
    return None, "nm"


def count_exports(binary: pathlib.Path, tool: pathlib.Path, kind: str) -> int | None:
    """Runs the tool and returns the export count, or None when its output could not be parsed."""
    if kind == "dumpbin":
        command = [str(tool), "/EXPORTS", str(binary)]
    elif tool.stem.lower().startswith("llvm"):
        command = [str(tool), "--dynamic", "--defined-only", str(binary)]
    elif platform.system() == "Darwin":
        command = [str(tool), "-gU", str(binary)]
    else:
        command = [str(tool), "-D", "--defined-only", str(binary)]
    completed = subprocess.run(command, capture_output=True, text=True, errors="replace")
    if completed.returncode != 0:
        print(f"check_exports: {' '.join(command)} failed ({completed.returncode}): {completed.stderr.strip()}",
              file=sys.stderr)
        return None
    if kind == "dumpbin":
        count = parse_dumpbin_exports(completed.stdout)
        if count is None:
            print(f"check_exports: no export table in {binary} (dumpbin printed no 'number of functions')",
                  file=sys.stderr)
        return count
    return parse_nm_exports(completed.stdout)


def run_count(binary_argument: str, limit: int, tool_argument: str | None) -> int:
    binary = pathlib.Path(binary_argument)
    if not binary.is_file():
        print(f"check_exports: {binary} does not exist", file=sys.stderr)
        return 2
    tool, kind = find_tool(tool_argument, binary)
    if tool is None:
        wanted = tool_argument or ("dumpbin" if kind == "dumpbin" else "llvm-nm or nm")
        print(f"check_exports: export-listing tool not found ({wanted}); pass --tool", file=sys.stderr)
        return 2
    count = count_exports(binary, tool, kind)
    if count is None:
        return 2
    verdict = "over" if count > limit else "within"
    print(f"check_exports: {binary.name} exports {count} symbols, {verdict} the limit of {limit} "
          f"(a Windows DLL may hold {WINDOWS_EXPORT_MAXIMUM})")
    if count > limit:
        print(
            "check_exports: conjure_shared exports more than the budget allows; only CONJURE_API-marked "
            "declarations are exported (docs/design/07_modules.md 5), so trim the public surface or move "
            "helpers into detail namespaces without the macro.",
            file=sys.stderr,
        )
        return 1
    return 0


#------------------------------------------------------------------------------
# Self-test
#------------------------------------------------------------------------------

SELF_TEST_CASES: list[tuple[str, str, list[str]]] = [
    ("class without the macro is reported", """
        namespace conjure::demo {
            class widget {
                public:
                    widget();
                    ~widget();
            };
        }
    """, ["class"]),
    ("class with the macro is clean", """
        namespace conjure::demo {
            class CONJURE_API widget {
                public:
                    widget();
                    void run() const noexcept;
            };
        }
    """, []),
    ("header-only class is clean", """
        namespace conjure::demo {
            struct point {
                int x = 0;
                [[nodiscard]] int doubled() const noexcept {
                    return x * 2;
                }
                point(const point &) = default;
                virtual void pure() = 0;
            };
        }
    """, []),
    ("class template is clean", """
        namespace conjure::demo {
            template <typename T>
            class holder {
                public:
                    holder();
                    T value() const;
            };
            template <typename T>
            struct holder<T *> {
                void reset();
            };
        }
    """, []),
    ("explicit specialisation is a class", """
        namespace conjure::demo {
            template <typename T>
            struct converter;
            template <>
            struct converter<bool> {
                [[nodiscard]] static bool from_text(const char *text);
            };
            template <>
            struct CONJURE_API converter<int> {
                [[nodiscard]] static int from_text(const char *text);
            };
        }
    """, ["class"]),
    ("nested class needs its own macro", """
        namespace conjure::demo {
            class CONJURE_API outer {
                public:
                    class inner {
                        public:
                            inner();
                    };
                    class CONJURE_API marked_inner {
                        public:
                            marked_inner();
                    };
                    outer();
            };
        }
    """, ["class"]),
    ("free function without the macro is reported", """
        namespace conjure::demo {
            [[nodiscard]] int parse(const char *text) noexcept;
            [[nodiscard]] CONJURE_API int parse_marked(const char *text) noexcept;
            [[noreturn]] void fail(
              const char *message,
              int code
            );
        }
    """, ["function", "function"]),
    ("inline, constexpr and template functions are clean", """
        namespace conjure::demo {
            inline int one() {
                return 1;
            }
            [[nodiscard]] constexpr int two() noexcept {
                return 2;
            }
            template <typename T>
            T identity(T value);
            using callback = void (*)(int value);
            inline constexpr int LIMIT = static_cast<int>(sizeof(int));
        }
    """, []),
    ("macro invocations are clean", """
        namespace conjure::demo {
            struct velocity {
                float x = 0.0f;
            };
        }
        CONJURE_REFLECT_STRUCT(conjure::demo::velocity, "demo.velocity", CONJURE_FIELD(x));
        CONJURE_COMPONENT(conjure::demo::velocity, "demo.velocity");
        CONJURE_DECLARE_ERROR_CATEGORY(conjure::demo::demo_error, "demo", conjure::demo::demo_error_name);
    """, []),
    ("extern variable without the macro is reported", """
        namespace conjure::demo {
            extern int counter;
            extern CONJURE_API int marked_counter;
        }
        extern "C" void c_entry(void);
    """, ["function", "variable"]),
    ("enum class is not a class", """
        namespace conjure::demo {
            enum class mode : int {
                FIRST = 0,
                SECOND
            };
            [[nodiscard]] CONJURE_API const char *to_string(mode value) noexcept;
        }
    """, []),
    ("statements inside bodies are not prototypes", """
        namespace conjure::demo {
            class CONJURE_API runner {
                public:
                    void run() const {
                        helper(1);
                        other_helper(2, 3);
                    }
            };
            inline void loop() {
                for (int index = 0; index < 3; ++index) {
                    step(index);
                }
            }
        }
    """, []),
    ("comments and strings are ignored", """
        namespace conjure::demo {
            // void commented_out(int value);
            /* void block_commented(int value); */
            inline const char *TEXT = "void in_string(int value);";
        }
    """, []),
    ("free operators without the macro are reported as functions", """
        namespace conjure::demo {
            class CONJURE_API bits {
                public:
                    bits();
            };
            [[nodiscard]] bits operator&(const bits &left, const bits &right);
            [[nodiscard]] bits operator|(const bits &left, const bits &right);
            [[nodiscard]] bool operator==(const bits &left, const bits &right) noexcept;
            [[nodiscard]] bool operator<=>(const bits &left, const bits &right) noexcept;
            [[nodiscard]] CONJURE_API bits operator~(const bits &value);
            inline bool operator!=(const bits &left, const bits &right) noexcept {
                return !(left == right);
            }
        }
    """, ["function", "function", "function", "function"]),
    ("a class whose only out-of-line members are operators is reported", """
        namespace conjure::demo {
            struct desc {
                int value = 0;
                [[nodiscard]] bool operator==(const desc &other) const noexcept;
            };
            struct marked_desc {
                int value = 0;
                [[nodiscard]] bool operator==(const marked_desc &other) const noexcept = default;
                friend bool operator<(const marked_desc &left, const marked_desc &right) noexcept;
            };
            class handle {
                public:
                    handle &operator=(const handle &other) noexcept;
                    [[nodiscard]] int operator()(int argument) const noexcept;
                    [[nodiscard]] int operator[](int index) const noexcept;
            };
        }
    """, ["class", "class"]),
    ("a nested iterator with out-of-line operators is reported as a nested class", """
        namespace conjure::demo {
            class CONJURE_API value {
                public:
                    class array_iterator {
                        public:
                            [[nodiscard]] value operator*() const noexcept;
                            array_iterator &operator++() noexcept;
                            array_iterator operator++(int) noexcept;
                            [[nodiscard]] bool operator==(const array_iterator &other) const noexcept {
                                return this == &other;
                            }
                    };
                    [[nodiscard]] array_iterator begin() const noexcept;
            };
        }
    """, ["class"]),
    ("conversion operators are out-of-line members", """
        namespace conjure::demo {
            class handle {
                public:
                    explicit operator bool() const noexcept;
                    [[nodiscard]] operator const char *() const noexcept;
            };
            class CONJURE_API marked_handle {
                public:
                    explicit operator bool() const noexcept;
                    [[nodiscard]] operator std::string_view() const noexcept;
            };
            class inline_handle {
                public:
                    explicit operator bool() const noexcept {
                        return true;
                    }
            };
        }
    """, ["class"]),
]

SELF_TEST_DUMPBIN_OUTPUT = """
Microsoft (R) COFF/PE Dumper Version 14.51.36231.0
Copyright (C) Microsoft Corporation.  All rights reserved.


Dump of file conjure_shared.dll

File Type: DLL

  Section contains the following exports for conjure_shared.dll

    00000000 characteristics
    FFFFFFFF time date stamp
        0.00 version
           1 ordinal base
        9321 number of functions
        9321 number of names

    ordinal hint RVA      name

          1    0 0001A2B0 ??0error@core@conjure@@QEAA@XZ
"""

SELF_TEST_NM_OUTPUT = """
0000000000012340 T _ZN7conjure4core5errorC1Ev
0000000000012380 T _ZN7conjure4core5errorD1Ev
0000000000045000 D _ZN7conjure4core8LOG_COREE
0000000000046000 B _ZN7conjure4core3log5sinksE
                 U _ZNSt6thread4joinEv
                 w __cxa_finalize
0000000000013000 W _ZNK7conjure4core5error7messageEv
"""


def run_self_test() -> int:
    failures = 0
    for title, snippet, expected_kinds in SELF_TEST_CASES:
        found = [item.kind for item in check_source(pathlib.Path("self_test.h"), snippet)]
        if found != expected_kinds:
            failures += 1
            print(f"self-test FAILED: {title}: expected {expected_kinds}, found {found}")
            for item in check_source(pathlib.Path("self_test.h"), snippet):
                print(f"    {item.format(ROOT)}")
        else:
            print(f"self-test ok: {title}")

    dumpbin_count = parse_dumpbin_exports(SELF_TEST_DUMPBIN_OUTPUT)
    if dumpbin_count != 9321:
        failures += 1
        print(f"self-test FAILED: dumpbin parser: expected 9321, found {dumpbin_count}")
    else:
        print("self-test ok: dumpbin parser")
    if parse_dumpbin_exports("Dump of file plain.exe\n\n  Summary\n") is not None:
        failures += 1
        print("self-test FAILED: dumpbin parser: a binary without exports must yield None")
    else:
        print("self-test ok: dumpbin parser without export table")

    nm_count = parse_nm_exports(SELF_TEST_NM_OUTPUT)
    if nm_count != 5:
        failures += 1
        print(f"self-test FAILED: nm parser: expected 5, found {nm_count}")
    else:
        print("self-test ok: nm parser")

    print(f"check_exports: self-test {len(SELF_TEST_CASES) + 3} case(s), {failures} failure(s)")
    return 1 if failures else 0


#------------------------------------------------------------------------------
# Entry point
#------------------------------------------------------------------------------

def run_check(root: pathlib.Path, modules: list[str]) -> int:
    include_root = root / "engine" / "include" / "conjure"
    if not include_root.is_dir():
        print(f"check_exports: {include_root} is not a directory", file=sys.stderr)
        return 2

    if not modules:
        modules = sorted(entry.name for entry in include_root.iterdir() if entry.is_dir())

    findings: list[finding] = []
    checked = 0
    for module in modules:
        directory = include_root / module
        if not directory.is_dir():
            print(f"check_exports: no public headers at {directory}", file=sys.stderr)
            return 2
        for header in sorted(directory.rglob("*.h")):
            checked += 1
            findings.extend(check_header(header))

    for item in sorted(findings, key=lambda f: (f.path.as_posix(), f.line)):
        print(item.format(root))
    print(f"check_exports: {checked} header(s), {len(findings)} finding(s)")
    return 1 if findings else 0


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--root", default=str(ROOT), help="repository root (default: the parent of tools/)")
    parser.add_argument("--all", action="store_true",
                        help="check every module under engine/include/conjure (the default when no module is named)")
    parser.add_argument("--count-exports", metavar="BINARY", default=None,
                        help="count the exported symbols of a built shared library instead of checking headers")
    parser.add_argument("--limit", type=int, default=DEFAULT_EXPORT_LIMIT,
                        help=f"largest export count --count-exports accepts (default: {DEFAULT_EXPORT_LIMIT})")
    parser.add_argument("--tool", default=None,
                        help="dumpbin, llvm-nm or nm executable for --count-exports (default: searched on PATH)")
    parser.add_argument("--self-test", action="store_true", help="run the checker over embedded cases")
    parser.add_argument("modules", nargs="*", help="module names to check (default: every module)")
    args = parser.parse_args(argv)

    if args.self_test:
        return run_self_test()
    if args.count_exports is not None:
        if args.limit < 1:
            print("check_exports: --limit must be positive", file=sys.stderr)
            return 2
        return run_count(args.count_exports, args.limit, args.tool)

    root = pathlib.Path(args.root).resolve()
    if args.all and args.modules:
        print("check_exports: --all and module names are exclusive", file=sys.stderr)
        return 2
    return run_check(root, args.modules)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
