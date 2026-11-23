"""Reads the nav block of a mkdocs.yml with the standard library, for the documentation tool tests.

The docs tool suites run under CTest and in CI before the documentation toolchain is installed, so they cannot
import PyYAML. The nav that tools/docs_engineering.py writes uses a small part of YAML: a block sequence whose items
are a plain scalar ("- page.md"), a one-key mapping to a scalar ("- Title: page.md") or a one-key mapping to a nested
block sequence ("- Title:" followed by deeper items). Keys and values are plain or JSON-quoted ("- \"widget.h\":"),
and whole-line comments mark the generated regions. nav_tree() returns what yaml.safe_load(text)["nav"] returns for
that subset and raises ValueError on anything else, so a test never compares against a misread tree.
"""

from __future__ import annotations

import json
import re

_ITEM = re.compile(r"( *)- (.*)")


def _scalar(text: str, line: str) -> str:
    text = text.strip()
    if text.startswith('"'):
        value, end = json.JSONDecoder().raw_decode(text)
        if text[end:].strip():
            raise ValueError("unexpected text after a quoted scalar: " + line)
        return value
    if not text or text[0] in "'[{&*!|>%@`" or ": " in text or text.endswith(":") or " #" in text:
        raise ValueError("not a plain nav scalar: " + line)
    return text


def _key_and_rest(text: str, line: str) -> tuple[str | None, str]:
    """The key of a one-key mapping item and the text after its ':', or (None, text) for a scalar item."""
    if text.startswith('"'):
        key, end = json.JSONDecoder().raw_decode(text)
        rest = text[end:]
        if not rest.startswith(":"):
            return None, text
        return key, rest[1:]
    match = re.match(r"([^:]+?):(?: (.*)|$)", text)
    if not match:
        return None, text
    return match.group(1), match.group(2) or ""


def _sequence(lines: list[tuple[int, str, str]], start: int, indent: int) -> tuple[list, int]:
    items: list = []
    position = start
    while position < len(lines):
        depth, text, line = lines[position]
        if depth < indent:
            break
        if depth > indent:
            raise ValueError("unexpected indentation: " + line)
        position += 1
        key, rest = _key_and_rest(text, line)
        if key is None:
            items.append(_scalar(text, line))
        elif rest.strip():
            items.append({key: _scalar(rest, line)})
        else:
            if position >= len(lines) or lines[position][0] <= indent:
                raise ValueError("a nav section without entries: " + line)
            children, position = _sequence(lines, position, lines[position][0])
            items.append({key: children})
    return items, position


def nav_tree(text: str) -> list:
    """The nav of a mkdocs.yml text as nested lists and one-key dictionaries."""
    found = re.search(r"(?m)^nav:[ \t]*\n", text)
    if not found:
        raise ValueError("mkdocs.yml has no nav")
    lines: list[tuple[int, str, str]] = []
    for line in text[found.end():].split("\n"):
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        if not line.startswith(" "):
            break
        match = _ITEM.fullmatch(line.rstrip())
        if not match:
            raise ValueError("not a nav item: " + line)
        lines.append((len(match.group(1)), match.group(2), line))
    if not lines:
        raise ValueError("mkdocs.yml has an empty nav")
    tree, position = _sequence(lines, 0, lines[0][0])
    if position != len(lines):
        raise ValueError("unexpected indentation: " + lines[position][2])
    return tree
