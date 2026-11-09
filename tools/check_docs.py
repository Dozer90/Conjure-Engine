#!/usr/bin/env python3
"""Structural checks for the Conjure documentation (docs/design/16_documentation.md D9).

Checks every Markdown page under docs/api and docs/handbook for:
  * the document-set badge on the first content line (📘 Public API wiki / 🛠 Engineering handbook),
  * relative links that resolve to an existing file (anchors are not verified),
  * no TODO/FIXME markers, no page longer than MAX_PAGE_LINES,
and the two coverage rules:
  * every public header that counts as public API is referenced from a wiki page of its module,
  * every engine module has a handbook page under docs/handbook/modules/.

Exit code 0 when clean, 1 with one line per problem otherwise. Run through CTest as `docs.check`.
"""

from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
DOCS = ROOT / "docs"
API_DOCS = DOCS / "api"
HANDBOOK_DOCS = DOCS / "handbook"
INCLUDE_ROOT = ROOT / "engine" / "include" / "conjure"
MODULE_SRC_ROOT = ROOT / "engine" / "src"

API_BADGE = "📘 **Public API wiki**"
HANDBOOK_BADGE = "🛠 **Engineering handbook**"
MAX_PAGE_LINES = 400
LINK_PATTERN = re.compile(r"\]\(([^)\s]+)(?:\s+\"[^\"]*\")?\)")


def iter_pages(directory: pathlib.Path):
    yield from sorted(directory.rglob("*.md"))


def first_content_line(text: str) -> str:
    lines = text.split("\n")
    index = 0
    # Skip front matter and the H1 title; the badge must be the first paragraph after the title.
    if lines and lines[0].strip() == "---":
        index = 1
        while index < len(lines) and lines[index].strip() != "---":
            index += 1
        index += 1
    while index < len(lines) and (not lines[index].strip() or lines[index].startswith("# ")):
        index += 1
    return lines[index].strip() if index < len(lines) else ""


def check_page(page: pathlib.Path, badge: str, problems: list[str]) -> None:
    text = page.read_text(encoding="utf-8")
    relative = page.relative_to(ROOT).as_posix()

    if badge not in first_content_line(text):
        problems.append(f"{relative}: first content line after the title must carry the badge '{badge}'")

    line_count = text.count("\n") + 1
    if line_count > MAX_PAGE_LINES:
        problems.append(f"{relative}: {line_count} lines exceeds {MAX_PAGE_LINES}; split the page")

    for marker in ("TODO", "FIXME", "XXX"):
        if re.search(rf"\b{marker}\b", text):
            problems.append(f"{relative}: contains {marker}")

    for match in LINK_PATTERN.finditer(text):
        target = match.group(1)
        if target.startswith(("http://", "https://", "mailto:", "#")):
            continue
        path_part = target.split("#", 1)[0]
        if not path_part:
            continue
        resolved = (page.parent / path_part).resolve()
        if not resolved.exists():
            problems.append(f"{relative}: link target does not exist: {target}")


def public_headers_by_module() -> dict[str, list[pathlib.Path]]:
    """Public headers grouped by module directory name under engine/include/conjure/."""
    result: dict[str, list[pathlib.Path]] = {}
    for module_dir in sorted(INCLUDE_ROOT.iterdir()):
        if not module_dir.is_dir():
            continue
        headers = sorted(path for path in module_dir.rglob("*.h") if needs_wiki_page(module_dir.name, path))
        if headers:
            result[module_dir.name] = headers
    return result


# Exclusion policy for the wiki coverage rule (docs/design/16_documentation.md D5). Everything public is
# covered unless it matches one of these data-driven rules; keep the table short and reviewable.
EXCLUDED_STEMS = {
    "api",              # CONJURE_API export macros: build plumbing
    "platform_detect",  # compiler/platform macros: build plumbing
    "build_info",       # generated version header
    "log_category",     # per-module log category declaration: internal plumbing
}
EXCLUDED_SUFFIXES = ("_error",)          # <module>_error.h: users meet errors through result<T>
EXCLUDED_DIRECTORIES = {"detail", "internal", "generated"}


def needs_wiki_page(module: str, header: pathlib.Path) -> bool:
    """Decides whether a public header is user-facing API that the wiki must cover.

    Umbrella headers (``ecs/ecs.h``, ``core/math/math.h``, the ``core/math.h`` convenience include),
    error enumerations, log categories, build-system headers and anything under a detail/internal/
    generated directory are excluded; every other public header must be referenced by a wiki page.
    """
    stem = header.stem
    if stem in EXCLUDED_STEMS:
        return False
    if stem.endswith(EXCLUDED_SUFFIXES):
        return False
    relative_parts = header.relative_to(INCLUDE_ROOT / module).parts
    if any(part in EXCLUDED_DIRECTORIES for part in relative_parts[:-1]):
        return False
    is_umbrella = stem == module or stem == header.parent.name
    return not is_umbrella


def check_header_coverage(problems: list[str]) -> None:
    wiki_text_by_module: dict[str, str] = {}
    for module, headers in public_headers_by_module().items():
        module_docs = API_DOCS / module
        if module not in wiki_text_by_module:
            wiki_text_by_module[module] = "\n".join(
                page.read_text(encoding="utf-8") for page in iter_pages(module_docs)
            ) if module_docs.exists() else ""
        corpus = wiki_text_by_module[module]
        for header in headers:
            include_path = header.relative_to(INCLUDE_ROOT.parent).as_posix()   # conjure/<module>/<name>.h
            if include_path not in corpus:
                problems.append(f"docs/api/{module}: no wiki page references `{include_path}`")


def check_module_handbook_pages(problems: list[str]) -> None:
    for module_dir in sorted(MODULE_SRC_ROOT.iterdir()):
        if not module_dir.is_dir() or not (module_dir / "CMakeLists.txt").exists():
            continue
        page = HANDBOOK_DOCS / "modules" / f"{module_dir.name}.md"
        if not page.exists():
            problems.append(f"docs/handbook/modules/{module_dir.name}.md: missing handbook page for module '{module_dir.name}'")


def main() -> int:
    problems: list[str] = []

    for page in iter_pages(API_DOCS):
        check_page(page, API_BADGE, problems)
    for page in iter_pages(HANDBOOK_DOCS):
        check_page(page, HANDBOOK_BADGE, problems)

    check_header_coverage(problems)
    check_module_handbook_pages(problems)

    if problems:
        print("\n".join(problems))
        print(f"\n{len(problems)} documentation problem(s)")
        return 1
    print("docs.check: clean")
    return 0


if __name__ == "__main__":
    sys.exit(main())
