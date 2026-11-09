#!/usr/bin/env python3
"""Checks that the public headers of an engine module carry CONJURE_API where the umbrella shared
library needs it (docs/design/07_modules.md 5 and 9, "check_exports.py flags public classes
without CONJURE_API").

For every header under engine/include/conjure/<module>/ it reports:

  class        a class or struct (not a template, not a forward declaration) that declares at least one
               member function out of line (a prototype ending in ';' that is not defaulted, deleted,
               pure virtual or a template) and whose head lacks CONJURE_API
  function     a namespace-scope function prototype (not a template, not inline/constexpr/consteval,
               not a friend) without CONJURE_API
  variable     an `extern` variable declaration without CONJURE_API

Anything header-only (templates, inline and constexpr functions, enums, aggregates without methods)
needs no annotation and is not reported. Exit code 0 when clean, 1 when a finding was reported, 2 on
usage errors. Output is one `path:line: kind: message` per finding.

Usage:
    python tools/check_exports.py [--root <repo>] [module ...]      (default module: modules)
    python tools/check_exports.py --all                              every module under engine/include
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent

CLASS_HEAD_PATTERN = re.compile(
    r"(?P<template>template\s*<[^;{}]*>\s*)?"
    r"\b(?P<keyword>class|struct)\s+"
    r"(?P<api>CONJURE_API\s+)?"
    r"(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*"
    r"(?P<final>final\s*)?"
    r"(?::[^{;]*)?\{"
)
FUNCTION_PATTERN = re.compile(
    r"^(?P<indent>\s*)(?P<decl>[^\s#/{}][^;{}]*?\))\s*(?:const\s*)?(?:noexcept(?:\([^)]*\))?\s*)?"
    r"(?:->\s*[^;{}]+)?\s*(?:override\s*)?;",
    re.MULTILINE,
)
EXTERN_PATTERN = re.compile(r"^\s*extern\s+(?!\"C\")(?P<decl>[^;{}]+);", re.MULTILINE)
IDENTIFIER_CALL = re.compile(r"([A-Za-z_~][A-Za-z0-9_]*)\s*\($")
KEYWORDS_NOT_FUNCTIONS = {
    "if", "for", "while", "switch", "return", "sizeof", "alignof", "decltype", "static_assert",
    "CONJURE_ASSERT", "CONJURE_ENUM_FLAGS", "CONJURE_DECLARE_ERROR_CATEGORY", "CONJURE_DECLARE_LOG_CATEGORY",
    "CONJURE_MODULE_SERVICE", "requires", "operator",
}


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
    """True for a member prototype that needs a definition in a .cpp (so an export)."""
    if "= default" in declaration or "= delete" in declaration or "= 0" in declaration:
        return False
    if has_top_level_assignment(declaration):
        return False
    if declaration.lstrip().startswith("template") or declaration.lstrip().startswith("friend"):
        return False
    if re.search(r"\b(inline|constexpr|consteval)\b", declaration):
        return False
    if "(" not in declaration:
        return False
    call = IDENTIFIER_CALL.search(declaration.split("(")[0] + "(")
    if call is None:
        return False
    return call.group(1) not in KEYWORDS_NOT_FUNCTIONS


def outer_class_bodies(code: str) -> list[tuple[re.Match, str, int, int]]:
    """Every class/struct head with its body text, body start offset and end offset, outermost only
    (nested classes are checked as part of their owner's export)."""
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


def check_header(path: pathlib.Path) -> list[finding]:
    text = path.read_text(encoding="utf-8", errors="replace")
    code = blank_function_bodies(strip_macro_lines(strip_comments_and_strings(text)))
    findings: list[finding] = []

    class_ranges: list[tuple[int, int]] = []
    for match, body, body_start, body_end in outer_class_bodies(code):
        class_ranges.append((match.start(), body_end))
        if match.group("template"):
            continue
        # Member prototypes directly inside this class (nested class bodies are blanked first).
        flat_body = body
        for nested_match, nested_body, nested_start, nested_end in outer_class_bodies(body):
            flat_body = flat_body[:nested_start] + " " * (nested_end - nested_start) + flat_body[nested_end:]
        has_out_of_line = False
        for method in FUNCTION_PATTERN.finditer(flat_body):
            if is_out_of_line_method(method.group("decl")):
                has_out_of_line = True
                break
        if has_out_of_line and not match.group("api"):
            findings.append(finding(
                path, line_of(code, match.start("name")), "class",
                f"'{match.group('name')}' declares out-of-line members but is not marked CONJURE_API"))

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
            path, line_of(code, match.start()), "variable",
            f"extern '{declaration.strip()}' is not marked CONJURE_API"))
    return findings


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--root", default=str(ROOT), help="repository root (default: the parent of tools/)")
    parser.add_argument("--all", action="store_true", help="check every module under engine/include/conjure")
    parser.add_argument("modules", nargs="*", help="module names to check (default: modules)")
    args = parser.parse_args(argv)

    root = pathlib.Path(args.root).resolve()
    include_root = root / "engine" / "include" / "conjure"
    if not include_root.is_dir():
        print(f"check_exports: {include_root} is not a directory", file=sys.stderr)
        return 2

    if args.all:
        modules = sorted(entry.name for entry in include_root.iterdir() if entry.is_dir())
    else:
        modules = args.modules or ["modules"]

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


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
