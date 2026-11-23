#!/usr/bin/env python3
"""Create a documentation draft using a write-docs page template.

Examples (run from the repository root):
    python tools/docs.py new scope docs/handbook/engine.md --title "Engine"
    python tools/docs.py new module docs/handbook/engine/core.md --module core --title "Core"
    python tools/docs.py new component docs/handbook/editor/protocol.md --scope editor --component protocol
    python tools/docs.py new header docs/handbook/engine/core/result.md --module core --header result.h
    python tools/docs.py new wiki docs/api/core/result.md --module core --header result.h

No existing file is overwritten. Register the page in nav and the set's index,
write its content with the headers/tests open, then run docs.py check and build.
"""

from __future__ import annotations

import argparse
import os
import pathlib
import re
import string
import sys
from urllib.parse import quote

# docs.py runs this script with python -I, which leaves its own directory off sys.path; the page kinds import their
# sibling tools (check_docs, docs_model, docs_components) when they need them.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

ROOT = pathlib.Path(__file__).resolve().parent.parent
TEMPLATES = pathlib.Path(__file__).resolve().parent / "docs_templates"
KINDS = {"wiki": "wiki.md", "scope": "handbook-scope.md",
         "module": "handbook-module.md", "component": "handbook-module.md",
         "header": "handbook-header.md", "type": "handbook-type.md"}


def module_title(module: str) -> str:
    return {"ecs": "ECS", "rhi": "RHI"}.get(module, module.replace("_", " ").title())


def prune_empty_sections(text: str) -> str:
    """Omit empty engineering sections while retaining authored-region markers."""
    from check_docs import strip_fences

    headings = list(re.finditer(r"(?m)^## [^\n]+\n", strip_fences(text)))
    for index in range(len(headings) - 1, -1, -1):
        heading = headings[index]
        end = headings[index + 1].start() if index + 1 < len(headings) else len(text)
        body = text[heading.end():end]
        visible = re.sub(r"<!--.*?-->", "", body, flags=re.DOTALL)
        visible = re.sub(r"!\[[^\]]*\]\(\s*\)", "", visible)
        # A Markdown table with a header and separator, but no data rows, is empty.
        visible = re.sub(r"(?m)^\|[^\n]*\|\n\|[ :|\-]+\|\s*$(?!\n\|)", "", visible)
        if not visible.strip():
            markers = re.findall(r"<!--.*?-->", body, re.DOTALL)
            text = text[:heading.start()] + "\n\n".join(markers) + "\n\n" + text[end:]
    return text.rstrip() + "\n"


def render_template(kind: str, values: dict[str, str]) -> str:
    """Share the page layouts between draft creation and content regeneration."""
    defaults = {
        "group_heading": "Modules", "group_label": "Module",
        "overview_kind": "component" if kind == "component" else "module",
        "overview_type": "Component" if kind == "component" else "Module", "ownership_row": "",
        "purpose": "<!-- Explain the purpose, audience, and position in the module. -->",
        "syntax": ('```cpp\n#include "' + values.get("include_path", "")
                   + '"\n// Add a complete example using patterns from the real tests.\n```\n'),
        "at_a_glance": "| If you want to... | Then use... |\n|---|---|",
        "variables": "| Name | Access | Purpose |\n|---|---|---|",
        "functions": "| Signature | Purpose |\n|---|---|",
        "modules": "",
        "headers": "",
        "body": "",
        "dependencies": "",
        "constructors": "",
        "types": "",
        "members": "",
        "member_functions": "",
        "internals": "<!-- Explain the internals and algorithms. -->",
        "threading": "<!-- Explain ownership, concurrency, and ordering requirements. -->",
        "extensions": "<!-- Describe supported extension points. -->",
        "tests": "<!-- Describe the tests and how to run them. -->",
        "related": "<!-- Add related links within this documentation set. -->",
        "area": "<!-- Name the area. -->",
        "namespace": "conjure",
        "api": values.get("api_link", "N/A"),
        "dependency_graph": "",
        "breakdown": "<!-- Explain implementation details and algorithms. -->",
        "testing": "<!-- Describe the tests and how to run them. -->",
        "structs": "",
        "classes": "",
        "kind": "class",
        "type": "Class",
        "inheritance": "",
        "destructors": "",
        "operators": "",
    }
    if kind != "wiki":
        defaults.update(variables="", functions="")
    defaults.update(values)
    area = defaults["area"]
    defaults["area_row"] = "| Area | " + area + " |\n" if area and area != "Module" else ""
    text = string.Template((TEMPLATES / KINDS[kind]).read_text(encoding="utf-8")).substitute(defaults)
    return prune_empty_sections(text) if kind != "wiki" else text


def relative_link(page: pathlib.Path, target: pathlib.Path, label: str) -> str:
    path = pathlib.Path(os.path.relpath(target, page.parent)).as_posix()
    return f"[{label}]({quote(path, safe='/')})"


def create_component_page(page, args):
    import docs_components
    import docs_engineering
    import docs_model
    import docs_policy
    import regenerate_docs

    component = docs_components.BY_SLUG.get(getattr(args, "component", ""))
    if args.scope != "editor" or component is None:
        raise ValueError("component pages require --scope editor and a recognised --component")
    if page != ROOT / "docs" / component.page:
        raise ValueError("use docs/" + component.page.as_posix() + " for this component")
    if page.exists():
        raise FileExistsError("Page already exists: " + str(page))
    originals = {path: docs_policy.sanitize(path.read_text(encoding="utf-8"))
                 for folder in ("api", "handbook") for path in (ROOT / "docs" / folder).rglob("*.md")}
    planned = docs_engineering.plan(ROOT, originals, docs_model.SourceIndex(ROOT), regenerate_docs)
    if page not in planned:
        raise ValueError("the component must own at least one public header")
    page.parent.mkdir(parents=True, exist_ok=True)
    with page.open("x", encoding="utf-8", newline="\n") as output:
        output.write(planned[page])
    return page


def create_page(args) -> pathlib.Path:
    supplied = pathlib.Path(args.path)
    if supplied.is_absolute() or pathlib.PureWindowsPath(args.path).drive or "\\" in args.path:
        raise ValueError("use a repository-relative URL path with forward slashes")
    page = (ROOT / supplied).resolve()
    docs = (ROOT / "docs").resolve()
    if docs not in page.parents:
        raise ValueError("the page must be inside docs/")
    if not re.fullmatch(r"[a-z0-9]+(?:-[a-z0-9]+)*\.md", page.name):
        raise ValueError("page filenames must be lower-case with hyphens and end in .md")
    if args.kind == "component":
        return create_component_page(page, args)
    if args.kind == "module" and args.scope == "editor" and args.module == "editor":
        raise ValueError("use an Editor component page; the editor application is not an engine module")
    if args.kind != "scope" and not args.module:
        raise ValueError("--module is required for wiki, module, and header pages")
    if args.module and not re.fullmatch(r"[a-z][a-z0-9_]*", args.module):
        raise ValueError("--module must be a module directory name")
    handbook = docs / "handbook"
    module_page = handbook / args.scope / f"{args.module}.md"
    expected = {"scope": handbook / f"{args.scope}.md", "module": module_page}
    if args.kind in expected and page != expected[args.kind]:
        raise ValueError(f"use {expected[args.kind].relative_to(ROOT).as_posix()} for this page type")
    base = docs / "api" / args.module if args.kind == "wiki" else handbook / args.scope / args.module
    if args.kind in {"wiki", "header", "type"} and base not in page.parents:
        raise ValueError(f"this page must be under {base.relative_to(ROOT).as_posix()}/")
    if args.kind in {"wiki", "header", "type"} and not args.header:
        raise ValueError("--header is required for wiki and header pages")

    source_scope = "engine" if args.scope == "editor" and args.module == "editor" else args.scope
    include_root = ROOT / source_scope / "include" / "conjure" / args.module
    include = include_root
    source_root = ROOT / source_scope / "src" / args.module
    source_text = "<!-- Name the implementation file, if this header has one. -->"
    if args.header:
        include = (include_root / args.header).resolve()
        if include_root.resolve() not in include.parents or include.suffix != ".h" or not include.is_file():
            raise ValueError("--header must name an existing .h file inside the module's include directory")
        sources = sorted(source_root.rglob(include.stem + ".cpp"))
        if len(sources) == 1:
            source_text = f"`{sources[0].relative_to(ROOT).as_posix()}`"

    api_pages = sorted((docs / "api" / args.module).rglob("*.md")) if args.module else []
    preferred = [candidate for candidate in api_pages if candidate.stem == page.stem]
    api_page = next(iter(preferred or api_pages), None)
    api_link = relative_link(page, api_page, "Relevant API reference") if api_page else "<!-- Add the API link. -->"
    siblings = [handbook / args.scope / args.module / page.name, module_page]
    sibling = next((candidate for candidate in siblings if candidate.is_file()), None)
    handbook_link = (relative_link(page, sibling, "Engineering handbook") if sibling
                     else "<!-- Add the corresponding handbook link when that page exists. -->")
    title = args.title or page.stem.replace("-", " ").capitalize()
    scope_title = args.scope.capitalize()
    if args.kind == "wiki":
        breadcrumbs = relative_link(page, docs / "api" / "index.md", "API")
    else:
        breadcrumbs = relative_link(page, handbook / f"{args.scope}.md", scope_title)
        if args.kind == "header":
            if args.scope == "editor" and args.module == "editor":
                import docs_components
                component = docs_components.editor_component(pathlib.Path(args.header))
                breadcrumbs += " → " + relative_link(page, docs / component.page, component.title)
            else:
                breadcrumbs += " → " + relative_link(page, module_page, module_title(args.module))
    if args.kind == "type":
        symbol = getattr(args, "symbol", None)
        if not symbol:
            raise ValueError("--symbol is required for structure/class pages")
        import docs_model
        model = docs_model.SourceIndex(ROOT).model(include)
        target = ROOT / "docs" / model.type_pages.get(symbol, pathlib.Path("invalid"))
        if page != target:
            raise ValueError("--symbol must name a fully defined type and the path must match its header hierarchy")
        if page.exists():
            raise FileExistsError("Page already exists: " + str(page))
        from types import SimpleNamespace
        import docs_engineering
        import docs_policy
        import regenerate_docs
        originals = {path: docs_policy.sanitize(path.read_text(encoding="utf-8"))
                     for directory in ("api", "handbook") for path in (ROOT / "docs" / directory).rglob("*.md")}
        helpers = SimpleNamespace(content_region=regenerate_docs.content_region,
            existing_region=regenerate_docs.existing_region, document_parts=regenerate_docs.document_parts,
            sections=regenerate_docs.sections, api_references=regenerate_docs.api_references, link=regenerate_docs.link)
        planned = docs_engineering.plan(ROOT, originals, model.index, helpers)
        text = planned[page]
        page.parent.mkdir(parents=True, exist_ok=True)
        with page.open("x", encoding="utf-8", newline="\n") as output:
            output.write(text)
        return page
    values = {
        "title": title, "breadcrumbs": breadcrumbs, "module": module_title(args.module),
        "scope_title": scope_title, "name": include.stem,
        "namespace": "conjure::" + args.module if args.module else "conjure",
        "include_path": (include.relative_to(ROOT / source_scope / "include").as_posix()
                         if args.kind == "wiki" else include.relative_to(ROOT).as_posix()),
        "source_path": source_text if args.kind == "header" else source_root.relative_to(ROOT).as_posix(),
        "readme_path": (source_root / "README.md").relative_to(ROOT).as_posix(),
        "api_link": api_link, "handbook_link": handbook_link,
    }
    if args.kind == "header":
        values["api"] = api_link
        values["title"] = args.title or include.name
    if args.kind == "module":
        # A fresh module draft has no graph image until its source dependencies are regenerated.
        values["dependency_graph"] = ""
    text = render_template(args.kind, values)
    if args.kind == "module":
        text = text.replace('![Module dependencies]()', '')
        text = re.sub(r'!\[[^\]]*\]\(\)', '', text)
    page.parent.mkdir(parents=True, exist_ok=True)
    with page.open("x", encoding="utf-8", newline="\n") as output:
        output.write(text)
    return page


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("kind", choices=KINDS)
    parser.add_argument("path", help="repository-relative output path, including docs/ and .md")
    parser.add_argument("--scope", choices=("engine", "editor"), default="engine")
    parser.add_argument("--module", default="")
    parser.add_argument("--component", default="", help="editor component slug, e.g. protocol or applications")
    parser.add_argument("--header", default="", help="include filename relative to its module, e.g. math/vector.h")
    parser.add_argument("--title", default="")
    parser.add_argument("--symbol", default="", help="fully defined class/structure name for a type page")
    args = parser.parse_args(argv)
    try:
        page = create_page(args)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print(f"Created draft: {page.relative_to(ROOT).as_posix()}")
    print("Write the body, register nav/index entries, then run docs.py check and build.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
