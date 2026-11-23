"""Keep published documentation about the engine, without internal planning provenance."""

from __future__ import annotations

import html
import re

PLANNING_FILE = re.compile(
    r"(?i)(?:(?<![\w-])(?:docs[/\\])?(?:design|reviews)[/\\][^\s`<>),;]*"
    r"|\b(?:0\d|1\d)_[a-z][a-z0-9_-]*(?:\.md)?\b"
    r"|(?:docs[/\\])?(?-i:ARCHITECTURE|CONVENTIONS|DESIGN_DOC)\.md\b"
    r"|\b(?:AGENTS|CLAUDE|SKILL)\.md\b"
    r"|\.(?:claude|codex|agents)[/\\][^\s`<>),;]+"
    r"|\bdecision-records\.md\b)"
)
DECISION_ID = re.compile(r"(?<![\w:])(?:[AERDPC]\d+(?:\.\d+)*|F\d{3,})(?![\w])")
WAVE = re.compile(r"(?i)\b(?:(?:later|lower|higher|per)[- ]waves?\b|waves?"
                  r"[- ]+\d+[a-z]?(?:\s*(?:to|and|[-–])\s*\d+[a-z]?)?)")
PROVENANCE = re.compile(r"(?i)\b(?:code[- ]sweep(?:\s+F\d+)?|lead architect|area [A-H] judge"
                        r"|binding design|binding record|binding text|design[- ]records?|decision[- ]records?"
                        r"|wave (?:plan|gates)"
                        r"|cross-area (?:decisions?|resolutions?)"
                        r"|AI[- ](?:authored|design decisions?|planning))\b")
BARE_AUTHORITY = re.compile(r"(?i)\b(?:CONVENTIONS|ARCHITECTURE|DESIGN_DOC)\s+\d+(?:\.\d+)*")
SHORTHAND = re.compile(r"(?<![\w.])(?:0\d|1\d)[ \t]+\d+\.\d+(?:[ \t]+steps?[ \t]+[\d, and-]+)?")
AUTHORING = re.compile(r"(?i)\b(?:the design(?:'s)?|design (?:documents?|notes?|rules?|decisions?|gates?)"
                       r"|(?:agent |authoring )?skills?|agent instructions)\b")
LOCATOR = r"(?:[ \t]+(?:sections?[ \t]+)?\d+(?:\.\d+)*(?:[ \t]+(?:and|to|item)[ \t]+\d+(?:\.\d+)*)?)*"
REFERENCE = re.compile(r"`?(?:" + PLANNING_FILE.pattern.replace("(?i)", "") + r")`?" + LOCATOR,
                       re.IGNORECASE)


def findings(text: str) -> list[tuple[int, str]]:
    """Return source lines containing forbidden provenance, including code-span citations."""
    result = []
    for index, line in enumerate(html.unescape(text).splitlines(), 1):
        if any(pattern.search(line) for pattern in (
            PLANNING_FILE, DECISION_ID, WAVE, PROVENANCE, BARE_AUTHORITY, SHORTHAND, AUTHORING
        )):
            result.append((index, line.strip()))
    return result


def _reference_fragments(text: str) -> str:
    text = REFERENCE.sub("", text)
    text = BARE_AUTHORITY.sub("", text)
    text = SHORTHAND.sub("", text)
    text = re.sub(r"(?i)\bcode[- ]sweep\s+F\d+\b", "", text)
    text = DECISION_ID.sub("", text)
    text = WAVE.sub("", text)
    return text


def _parenthesis(match: re.Match) -> str:
    content = match.group(1)
    if not any(pattern.search(content) for pattern in (
        PLANNING_FILE, DECISION_ID, WAVE, PROVENANCE, BARE_AUTHORITY, SHORTHAND
    )):
        return match.group(0)
    cleaned = _reference_fragments(" ".join(content.split()))
    cleaned = re.sub(r"(?i)\b(?:code sweep|design record|cross-area decisions?|binding design)\b", "", cleaned)
    meaningful = re.sub(r"(?i)\b(?:and|or|of|the|see|sections?|items?|to|from|as|in|above|below|rule)\b", "", cleaned)
    meaningful = re.sub(r"[\d\W_]+", "", meaningful)
    if not meaningful:
        return ""
    cleaned = re.sub(r"^[\s,;:–-]+|[\s,;:–-]+$", "", cleaned)
    return "(" + cleaned + ")"


def sanitize(text: str) -> str:
    """Remove citations/plan labels while retaining the technical clauses around them."""
    literals = []

    def protect(value: str) -> str:
        literals.append(value)
        return f"DOCS_LITERAL_{len(literals) - 1}_END"

    # Keep examples and their string/regex literals intact. Prose citations in inline
    # code are still cleaned, because they are precisely the unwanted references.
    lines = text.splitlines(keepends=True)
    outside = []
    fenced = []
    fence = ""
    for line in lines:
        # GitHub alerts and nested blockquotes keep their fences behind `>` prefixes.
        content = re.sub(r"^(?:[ \t]*>[ \t]?)+", "", line)
        opening = re.match(r"^[ \t]*(`{3,}(?!.*`)|~{3,})", content)
        if not fence and opening:
            fence = opening.group(1)
            fenced = [line]
        elif fence:
            fenced.append(line)
            if content.strip().startswith(fence) and set(content.strip()) == {fence[0]}:
                outside.append(protect("".join(fenced)) + "\n")
                fence = ""
        else:
            outside.append(line)
    if fenced and fence:
        outside.append(protect("".join(fenced)))
    text = "".join(outside)
    # These source-comment comparisons carry useful contracts; express those
    # contracts directly rather than publishing the history of their wording.
    replacements = {
        "; the design's \"forward declarations only\" is otherwise kept.":
            "; the remaining engine and module types use forward declarations only.",
        '(design rule)': '',
        'Every value marked "tunable" in the design has its default here.':
            'Default values for tunable settings are defined here.',
        "Bake sizes; the defaults are the design's.": "Bake sizes and their defaults.",
        "(the design's signature omitted it, but a non-square viewport cannot be unprojected without it)":
            "(a non-square viewport requires the aspect ratio for unprojection)",
        "asserts too (3.5)": "asserts too",
    }
    def replace_link(match: re.Match) -> str:
        label, target = match.group(1), match.group(2)
        if not PLANNING_FILE.search(target):
            return match.group(0)
        return "" if PLANNING_FILE.search(label) or PROVENANCE.search(label) else label

    text = re.sub(r"\[([^\]\n]+)\]\(([^)\s]+)(?:\s+[^)]*)?\)", replace_link, text)
    code_span = re.compile(r"(?<!`)(`+)(?!`)([^\n]+?)(?<!`)\1(?!`)")
    text = code_span.sub(lambda match: match.group(0) if findings(match.group(2)) else protect(match.group(0)), text)
    # Standalone references and directive tables are navigation to unpublished material.
    lines = []
    dropping_bullet = False
    for line in text.splitlines():
        if re.match(r"^\s*[-*]\s+(?:design records?:|`(?:AGENTS\.md|\.claude/skills))", line, re.I):
            dropping_bullet = True
            continue
        if dropping_bullet:
            if line.startswith(("  ", "\t")) or not line.strip():
                continue
            dropping_bullet = False
        if line.lstrip().startswith("|") and re.search(
            r"(?i)\|\s*(?:Wave|Decision records?|Agent guide|`AGENTS\.md`|`\.claude/skills[^|]*`)\s*\|", line
        ):
            continue
        lines.append(line)
    text = "\n".join(lines)
    previous = None
    while previous != text:
        previous = text
        text = re.sub(r"\(([^()]*)\)", _parenthesis, text)
    text = _reference_fragments(text)
    text = re.sub(r"(?i)\b(?:and |or )?(?:code[- ]sweep|binding design|binding record|design records?)\b", "", text)
    text = re.sub(r"`\s*`", "", text)
    text = re.sub(r"(?<=\S)[ \t]{2,}", " ", text)
    text = re.sub(r"[ \t]+([,;:])", r"\1", text)
    text = re.sub(r"[ \t]+\.(?=\s|$)", ".", text)
    text = re.sub(r"\(\s*[,;:]\s*", "(", text)
    text = re.sub(r"[,;:]\s*\)", ")", text)
    text = re.sub(r"(?m)^[ \t]*[-*][ \t]*$\n?", "", text)
    text = re.sub(r"\n{3,}", "\n\n", text)
    for original, replacement in replacements.items():
        pattern = re.escape(original).replace(r"\ ", r"\s+")
        text = re.sub(pattern, lambda match: replacement, text)
    text = re.sub(r"[ \t]+([,;:])", r"\1", text)
    text = re.sub(r"(?<=\S)[ \t]{2,}", " ", text)
    for index, value in enumerate(literals):
        text = text.replace(f"DOCS_LITERAL_{index}_END", value.rstrip("\n"))
    return text.strip()
