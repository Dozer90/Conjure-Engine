#!/usr/bin/env python3
"""Check the documentation contract in .claude/skills/write-docs/SKILL.md.

Checks relative documentation links and anchors, wiki boundaries, alert syntax,
unfinished markers, include-path code spans for every public header, and
scope/module handbook coverage. Badges and page-length limits are not required.
Exit 0 when clean, or 1 with one line per problem. No documentation packages are needed.
"""

from __future__ import annotations

import html
import pathlib
import re
import sys
import unicodedata
from urllib.parse import unquote, urlsplit

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import docs_policy
import docs_components
import docs_redirects

ROOT = pathlib.Path(__file__).resolve().parent.parent
DOCS = ROOT / "docs"
API_DOCS = DOCS / "api"
HANDBOOK_DOCS = DOCS / "handbook"
INCLUDE_ROOT = ROOT / "engine" / "include" / "conjure"
MODULE_SRC_ROOT = ROOT / "engine" / "src"

LINK_PATTERN = re.compile(r"\]\((<?[^)\s]+>?)(?:\s+\"[^\"]*\")?\)")
REFERENCE_DEFINITION = re.compile(r"^ {0,3}\[(?!\^)[^\]\n]+\]:[ \t]*<?([^\s>]+)>?", re.MULTILINE)
HTML_LINK = re.compile(r"\b(?:href|src)\s*=\s*(?:\"([^\"]*)\"|'([^']*)')", re.IGNORECASE)
FENCE_OPENING = re.compile(r"^[ \t]*(`{3,}(?!.*`)|~{3,})")
CODE_SPAN = re.compile(r"(?<!`)(`+)(?!`)((?:[^\n]|\n(?![ \t]*\n))+?)(?<!`)\1(?!`)")
CODE_LINK = re.compile(r"\[([^\]\n]+)\]\(([^\s)]+)\)")
ALERT_KINDS = {"NOTE", "TIP", "IMPORTANT", "WARNING", "CAUTION"}


def blank(text: str) -> str:
    return "".join("\n" if character == "\n" else " " for character in text)


def strip_fences(text: str) -> str:
    """Blank fenced examples without losing source positions or line numbers."""
    lines = text.split("\n")
    fence = ""
    for index, line in enumerate(lines):
        opening = FENCE_OPENING.match(line)
        if not fence and opening:
            fence = opening.group(1)
            lines[index] = blank(line)
        elif fence:
            closing = line.strip()
            if closing.startswith(fence) and set(closing) == {fence[0]}:
                fence = ""
            lines[index] = blank(line)
    return "\n".join(lines)


def strip_code(text: str) -> str:
    """Keep linked code signatures in tables; ordinary code examples are not links."""
    code_free = strip_fences(text)

    def replace_span(match: re.Match) -> str:
        line_start = code_free.rfind("\n", 0, match.start()) + 1
        in_table = code_free[line_start:match.start()].lstrip().startswith("|")
        if in_table and CODE_LINK.search(match.group(2)):
            return " " * len(match.group(1)) + match.group(2) + " " * len(match.group(1))
        return blank(match.group(0))

    return CODE_SPAN.sub(replace_span, code_free)


def link_targets(text: str) -> list[str]:
    code_free = strip_code(text)
    targets = [match.group(1).strip("<>") for match in LINK_PATTERN.finditer(code_free)]
    targets += [match.group(1) for match in REFERENCE_DEFINITION.finditer(code_free)]
    targets += [match.group(1) if match.group(1) is not None else match.group(2)
                for match in HTML_LINK.finditer(code_free)]
    return [html.unescape(target) for target in targets]


def iter_pages(directory: pathlib.Path):
    yield from sorted(directory.rglob("*.md"))


def is_within(path: pathlib.Path, directory: pathlib.Path) -> bool:
    return path == directory or directory in path.parents


def resolve_target(page: pathlib.Path, target: str) -> tuple[pathlib.Path | None, str, str]:
    """Return a local file, fragment and error; external web/mail URLs have no local file."""
    try:
        parsed = urlsplit(target)
    except ValueError:
        return None, "", "invalid link target"
    if parsed.scheme in {"http", "https", "mailto"}:
        return None, "", ""
    if parsed.scheme or parsed.netloc or parsed.path.startswith(("/", "\\")):
        return None, "", "link must be relative"
    path_part = unquote(parsed.path)
    if "\\" in path_part or pathlib.PureWindowsPath(path_part).drive or path_part.startswith("/"):
        return None, "", "link must use a relative URL path"
    destination = (page.parent / path_part).resolve() if path_part else page.resolve()
    if destination.suffix == ".html" and destination.with_suffix(".md").is_file():
        destination = destination.with_suffix(".md")
    if destination.is_dir():
        destination = destination / "index.md"
    elif not destination.exists() and not destination.suffix and destination.with_suffix(".md").is_file():
        destination = destination.with_suffix(".md")
    if not destination.is_file():
        return destination, "", "link target does not exist"
    return destination, unquote(parsed.fragment), ""


def heading_anchors(text: str) -> set[str]:
    """Match Markdown's heading slugs, duplicate suffixes, and explicit/HTML anchors."""
    text = strip_fences(text)
    anchors = set(re.findall(r'\b(?:id|name)\s*=\s*[\"\x27]([^\"\x27]+)', strip_code(text)))
    used: set[str] = set()
    lines = text.splitlines()
    for index, line in enumerate(lines):
        heading = re.match(r"^ {0,3}#{1,6}\s+(.+?)(?:\s+#+)?\s*$", line)
        if heading:
            title = heading.group(1)
        elif index + 1 < len(lines) and line.strip() and re.match(r"^ {0,3}(?:=+|-+)\s*$", lines[index + 1]):
            title = line.strip()
        else:
            continue
        explicit = re.search(r"\{[^}]*#([^\s}]+)[^}]*\}\s*$", title)
        if explicit:
            slug = explicit.group(1)
        else:
            title = re.sub(r"!?\[([^\]]*)\]\([^)]*\)", r"\1", title)
            title = CODE_SPAN.sub(lambda match: html.escape(match.group(2)), title)
            title = re.sub(r"<[^>]*>", "", title)
            title = html.unescape(title)
            title = unicodedata.normalize("NFKD", title).encode("ascii", "ignore").decode()
            slug = re.sub(r"[^\w\s-]", "", title).strip().lower()
            slug = re.sub(r"[-\s]+", "-", slug)
        unique = slug
        suffix = 1
        while unique in used:
            unique = f"{slug}_{suffix}"
            suffix += 1
        used.add(unique)
        anchors.add(unique)
    return anchors


def check_page(page: pathlib.Path, problems: list[str]) -> None:
    text = page.read_text(encoding="utf-8")
    relative = page.relative_to(ROOT).as_posix()
    try:
        redirect = docs_redirects.metadata(text)
    except ValueError as error:
        problems.append(f"{relative}: {error}")
        redirect = None
    if redirect:
        for target in (redirect[0], *redirect[1].values()):
            destination, fragment, error = resolve_target(page, target)
            if error or destination is None or not is_within(destination, DOCS.resolve()):
                problems.append(f"{relative}: redirect must name an existing local documentation page: {target}")
            elif destination.suffix != ".md":
                problems.append(f"{relative}: redirects must point directly to a Markdown page: {target}")
            else:
                try:
                    chained = docs_redirects.metadata(destination.read_text(encoding="utf-8"))
                except ValueError:
                    problems.append(f"{relative}: redirect target contains invalid redirect metadata: {target}")
                    continue
                if chained:
                    problems.append(f"{relative}: redirects must point directly to a Markdown page: {target}")
                elif fragment and fragment not in heading_anchors(destination.read_text(encoding="utf-8")):
                    problems.append(f"{relative}: redirect anchor does not exist: {target}")
    for line, _ in docs_policy.findings(text):
        problems.append(f"{relative}:{line}: internal planning metadata is not allowed in published documentation")
    for marker in ("TODO", "FIXME", "XXX"):
        if re.search(rf"\b{marker}\b", text):
            problems.append(f"{relative}: contains {marker}")
    code_free = strip_code(text)
    for kind in re.findall(r"^\s*>\s*\[!([^]\n]+)\]", code_free, re.MULTILINE):
        if kind not in ALERT_KINDS:
            problems.append(f"{relative}: unsupported callout [!{kind}]")
    if re.search(r"^\s*(?:!!!|\?\?\?)\s+\w", code_free, re.MULTILINE):
        problems.append(f"{relative}: use GitHub alert syntax for callouts")
    for target in link_targets(text):
        destination, fragment, error = resolve_target(page, target)
        if error:
            problems.append(f"{relative}: {error}: {target}")
        if destination is None:
            continue
        if is_within(page.resolve(), API_DOCS.resolve()):
            forbidden = (DOCS / "design", ROOT / "engine", ROOT / "AGENTS.md")
            if any(is_within(destination, path.resolve()) for path in forbidden):
                problems.append(f"{relative}: wiki link must use the handbook: {target}")
        if not error and fragment and destination.suffix == ".md":
            if fragment not in heading_anchors(destination.read_text(encoding="utf-8")):
                problems.append(f"{relative}: link anchor does not exist: {target}")


def public_headers_by_module() -> dict[str, list[pathlib.Path]]:
    result: dict[str, list[pathlib.Path]] = {}
    if INCLUDE_ROOT.exists():
        for module_dir in sorted(INCLUDE_ROOT.iterdir()):
            if module_dir.is_dir():
                result[module_dir.name] = sorted(module_dir.rglob("*.h"))
    return result


def check_header_coverage(problems: list[str]) -> None:
    for module, headers in public_headers_by_module().items():
        spans: set[str] = set()
        for page in iter_pages(API_DOCS / module):
            text = strip_fences(page.read_text(encoding="utf-8"))
            spans.update(match.group(2).strip() for match in CODE_SPAN.finditer(text))
        for header in headers:
            include_path = header.relative_to(INCLUDE_ROOT.parent).as_posix()
            if include_path not in spans:
                problems.append(f"docs/api/{module}: no wiki code span references `{include_path}`")


def check_module_handbook_pages(problems: list[str]) -> None:
    for scope, source in (("engine", MODULE_SRC_ROOT), ("editor", ROOT / "editor" / "src")):
        if not source.exists():
            continue
        for module_dir in sorted(source.iterdir()):
            if not module_dir.is_dir() or module_dir.name.startswith("."):
                continue
            # The editor libraries live in engine/src/editor but belong to the Editor scope.
            page_scope = "editor" if module_dir.name == "editor" else scope
            page = HANDBOOK_DOCS / page_scope / f"{module_dir.name}.md"
            if not page.is_file():
                path = page.relative_to(ROOT).as_posix()
                owner = ("Editor application architecture" if module_dir.name == "editor"
                         else f"{page_scope} module '{module_dir.name}'")
                problems.append(f"{path}: missing handbook page for {owner}")
    try:
        for relative in sorted(docs_components.component_pages(ROOT)):
            if not (DOCS / relative).is_file():
                problems.append("docs/" + relative.as_posix() + ": missing Editor component overview")
    except ValueError as error:
        problems.append(str(error))


def collect_problems() -> list[str]:
    problems: list[str] = []
    if (DOCS / "index.md").is_file():
        check_page(DOCS / "index.md", problems)
    for directory in (API_DOCS, HANDBOOK_DOCS):
        for page in iter_pages(directory):
            check_page(page, problems)
    check_header_coverage(problems)
    check_module_handbook_pages(problems)
    return problems


def main() -> int:
    problems = collect_problems()
    if problems:
        print("\n".join(problems))
        print(f"\n{len(problems)} documentation problem(s)")
        return 1
    print("docs.check: clean")
    return 0


if __name__ == "__main__":
    sys.exit(main())
