#!/usr/bin/env python3
"""Regenerate API/handbook layouts and public-header detail pages without discarding authored prose.

    python tools/docs.py regenerate
    python tools/docs.py regenerate --check

Computed metadata, source summaries, header indexes, and navigation are refreshed.
Text inside doc-content regions is preserved. Design records and skill files are inputs,
not generated pages. --check reports drift without writing files.
"""

from __future__ import annotations

import argparse
import html
import json
import pathlib
import re
import sys

# Drift checks must not create bytecode caches while importing the local helpers.
sys.dont_write_bytecode = True
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import check_docs
import create_doc
import docs_cpp
import docs_policy
import docs_redirects

ROOT = pathlib.Path(__file__).resolve().parent.parent
MARKER = "<!-- doc-template:"


def content_region(name: str, text: str) -> str:
    return f"<!-- doc-content:{name}:start -->\n{text.strip()}\n<!-- doc-content:{name}:end -->"


def existing_region(text: str, name: str, default: str = "") -> str:
    pattern = rf"<!-- doc-content:{re.escape(name)}:start -->\n(.*?)\n<!-- doc-content:{re.escape(name)}:end -->"
    match = re.search(pattern, text, re.DOTALL)
    return match.group(1) if match else default


def heading_id(title: str) -> str:
    return next(iter(check_docs.heading_anchors("# " + title)))


def document_parts(text: str) -> tuple[str, str, str]:
    front = ""
    if text.startswith("---\n"):
        end = text.find("\n---", 4)
        if end >= 0:
            front, text = text[:end + 4] + "\n\n", text[end + 4:].lstrip("\n")
    title_match = re.search(r"^# (.+)$", text, re.MULTILINE)
    title = title_match.group(1) if title_match else "Documentation"
    if title_match:
        text = text[:title_match.start()] + text[title_match.end():]
    text = re.sub(r"(?m)^(?:📘 \*\*Public API wiki\*\*|🛠 \*\*Engineering handbook\*\*).*\n?", "", text)
    return front, title, text.strip()


def sections(text: str) -> tuple[str, list[tuple[str, str]]]:
    visible = check_docs.strip_fences(text)
    matches = list(re.finditer(r"^## (.+)$", visible, re.MULTILINE))
    intro = text[:matches[0].start()].strip() if matches else text.strip()
    result = []
    for index, match in enumerate(matches):
        end = matches[index + 1].start() if index + 1 < len(matches) else len(text)
        result.append((match.group(1), text[match.end():end].strip()))
    return intro, result


def cell(text: str) -> str:
    return text.replace("|", "\\|").replace("\n", " ")


def source_summary(text: str) -> str:
    text = re.sub(r"(?m)^\s*///\s?", "", text)
    text = re.sub(r'<see\s+cref="([^"]+)"\s*/>', r"\1", text)
    text = re.sub(r"</?(?:para|c|code|summary)>", "", text)
    return html.escape(" ".join(docs_policy.sanitize(html.unescape(text)).split()), quote=False)


def header_page(root: pathlib.Path, header: pathlib.Path) -> pathlib.Path:
    relative = header.relative_to(root / "engine/include/conjure")
    module = relative.parts[0]
    scope = "editor" if module == "editor" else "engine"
    slug = pathlib.Path(*(part.replace("_", "-") for part in relative.parts[1:])).with_suffix(".md")
    return root / "docs/handbook" / scope / module / slug


def module_page(root: pathlib.Path, module: str) -> pathlib.Path:
    return root / "docs/handbook" / ("editor" if module == "editor" else "engine") / (module + ".md")


def link(page: pathlib.Path, target: pathlib.Path, label: str, fragment: str = "") -> str:
    value = create_doc.relative_link(page, target, label)
    return value[:-1] + "#" + fragment + ")" if fragment else value


def api_references(page: pathlib.Path, header: pathlib.Path, root: pathlib.Path,
                   originals: dict[pathlib.Path, str]) -> list[str]:
    include = header.relative_to(root / "engine/include").as_posix()
    module = header.relative_to(root / "engine/include/conjure").parts[0]
    matches = []
    for candidate, text in originals.items():
        if candidate.is_relative_to(root / "docs/api" / module) and include in text:
            _, title, _ = document_parts(text)
            matches.append(link(page, candidate, title))
    return matches


def metadata(root: pathlib.Path, page: pathlib.Path, title: str, module: str,
             originals: dict[pathlib.Path, str]) -> dict[str, str]:
    scope = "editor" if module == "editor" else "engine"
    physical = root / "engine"
    if module == "runtime":
        physical = root / "runtime"
    include = physical / "include/conjure" / module
    source = physical / "src" / module if module != "runtime" else physical / "src"
    readme = source / "README.md"
    if not readme.exists() and (physical / "README.md").exists():
        readme = physical / "README.md"
    apis = sorted(path for path in originals if path.is_relative_to(root / "docs/api" / module))
    index = root / "docs/api" / module / "index.md"
    api = index if index in originals else next(iter(apis), root / "docs/api/index.md")
    return {
        "title": title, "module": create_doc.module_title(module), "scope_title": scope.capitalize(), "name": title,
        "breadcrumbs": link(page, root / "docs/handbook" / (scope + ".md"), scope.capitalize()),
        "include_path": include.relative_to(root).as_posix() if include.exists() else "No public include directory",
        "source_path": source.relative_to(root).as_posix(),
        "readme_path": readme.relative_to(root).as_posix() if readme.exists() else "See the module overview",
        "api_link": link(page, api, "Relevant API reference"),
        "handbook_link": link(page, module_page(root, module), "Engineering handbook"),
    }


def render_wiki(root: pathlib.Path, page: pathlib.Path, text: str,
                originals: dict[pathlib.Path, str]) -> str:
    front, title, raw = document_parts(text)
    module = page.relative_to(root / "docs/api").parts[0] if page.parent != root / "docs/api" else ""
    breadcrumbs = (link(page, root / "docs/api/index.md", "API") if page.name != "index.md" or module else "API")
    if module and page.name != "index.md" and (root / "docs/api" / module / "index.md") in originals:
        breadcrumbs += " → " + link(page, root / "docs/api" / module / "index.md", create_doc.module_title(module))
    # Navigation and terminology pages retain their specialised reference structure.
    if page.name == "index.md" or not module:
        return render_chapter(root, page, text, breadcrumbs, "wiki-index")
    values = metadata(root, page, title, module, originals)
    values["breadcrumbs"] = breadcrumbs
    if MARKER in text:
        values.update({name: content_region(name, existing_region(text, name))
                       for name in ("purpose", "syntax", "at_a_glance", "variables", "body", "functions", "related")})
        authored = "\n".join(existing_region(text, name) for name in
                             ("purpose", "syntax", "at_a_glance", "variables", "body", "functions", "related"))
    else:
        intro, entries = sections(raw)
        selected = {}
        others = []
        for heading, body in entries:
            name = None
            if re.match(r"(?:Quick example|Quick tour|Syntax)\b", heading, re.IGNORECASE):
                name = "syntax"
            elif heading.lower() == "at a glance":
                name = "at_a_glance"
            elif heading.lower() in {"variables and properties", "variables", "properties"}:
                name = "variables"
            elif heading.lower() in {"api summary", "functions"}:
                name = "functions"
            elif heading.lower() == "see also":
                name = "related"
            if name and name not in selected:
                canonical = {"syntax": "Syntax", "functions": "Functions", "related": "See also"}.get(name, heading)
                alias = f'<a id="{heading_id(heading)}"></a>\n\n' if canonical != heading else ""
                selected[name] = alias + body
            else:
                others.append((heading, body))
        reference_links = ", ".join(f"[{heading}](#{heading_id(heading)})" for heading, _ in others
                                    if heading.lower() not in {"related"})
        selected.setdefault("syntax", "Usage and complete examples are described in the reference sections below.")
        selected.setdefault("at_a_glance", "See the reference sections below for the operations covered by this topic.")
        selected.setdefault("variables", "Types, fields, and state are described in the reference sections below.")
        selected.setdefault("functions", "Operations and their constraints are described in " + reference_links + "."
                            if reference_links else "See the linked topic pages for available operations.")
        selected.setdefault("related", "- " + link(page, root / "docs/api/index.md", "Public API index"))
        body = "\n\n".join("## " + heading + "\n\n" + body for heading, body in others)
        values.update({name: content_region(name, value) for name, value in selected.items()})
        values["purpose"] = content_region("purpose", intro)
        values["body"] = content_region("body", body)
        authored = raw
    includes = sorted(set(re.findall(r"`(conjure/" + re.escape(module) + r"/[^`\n]+\.h)`", authored)))
    # Include paths are derived from the preserved topic text, never guessed from its filename.
    values["include_path"] = "`, `".join(includes) if includes else "See the header references below"
    result = create_doc.render_template("wiki", values)
    return front + result.replace("\n", "\n<!-- doc-template:wiki -->\n", 1)


def render_chapter(root: pathlib.Path, page: pathlib.Path, text: str, breadcrumbs: str, kind: str) -> str:
    front, title, raw = document_parts(text)
    if MARKER in text:
        purpose = existing_region(text, "purpose")
        body = existing_region(text, "body")
    else:
        purpose, entries = sections(raw)
        body = "\n\n".join("## " + heading + "\n\n" + content for heading, content in entries)
    return (front + f"# {title}\n<!-- doc-template:{kind} -->\n{breadcrumbs}\n\n"
            + content_region("purpose", purpose) + "\n\n" + content_region("body", body) + "\n")


def plan(root: pathlib.Path) -> tuple[dict[pathlib.Path, str], dict[str, int]]:
    from types import SimpleNamespace
    import docs_engineering
    import docs_model
    originals = {page: docs_policy.sanitize(page.read_text(encoding="utf-8")) for directory in ("api", "handbook")
                 for page in sorted((root / "docs" / directory).rglob("*.md"))}
    index = docs_model.SourceIndex(root)
    helpers = SimpleNamespace(content_region=content_region, existing_region=existing_region,
                              document_parts=document_parts, sections=sections, api_references=api_references,
                              link=link)
    planned = docs_engineering.plan(root, originals, index, helpers)
    for page, text in originals.items():
        if page in planned:
            continue
        if docs_redirects.metadata(text):
            planned[page] = text
            continue
        if page.is_relative_to(root / "docs/api"):
            result = render_wiki(root, page, text, originals)
        else:
            breadcrumbs = link(page, root / "docs/handbook/index.md", "Handbook")
            handbook = root / "docs/handbook"
            if page.is_relative_to(handbook):
                folder = handbook
                for part in page.relative_to(handbook).parts[:-1]:
                    folder /= part
                    overview = folder / "index.md"
                    if overview != page and overview in originals:
                        _, label, _ = document_parts(originals[overview])
                        breadcrumbs += " → " + link(page, overview, label)
            result = render_chapter(root, page, text, breadcrumbs, "chapter")
        planned[page] = result
    # Preserve legacy section URLs after canonical heading names/layouts change.
    for page, text in list(planned.items()):
        old_anchors = check_docs.heading_anchors(originals.get(page, ""))
        missing = old_anchors - check_docs.heading_anchors(text)
        if missing:
            aliases = "\n".join(f'<a id="{anchor}"></a>' for anchor in sorted(missing))
            text += "\n<!-- doc-legacy-anchors:start -->\n" + aliases + "\n<!-- doc-legacy-anchors:end -->\n"
        planned[page] = text.rstrip() + "\n"
    config = root / "mkdocs.yml"
    planned[config] = docs_engineering.navigation(config.read_text(encoding="utf-8"), root, index)
    targets = [root / "docs" / location[2] for location in index.locations.values()]
    targets += [root / "docs" / page for page in index.type_pages.values()]
    return planned, {"existing_pages": len(originals), "header_pages": len(index.records),
                     "type_pages": len(set(index.type_pages.values())),
                     "new_pages": sum(page not in originals for page in targets),
                     "total_pages": sum(path.suffix == ".md" for path in planned)}


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--check", action="store_true", help="report regeneration drift without writing")
    args = parser.parse_args(argv)
    try:
        planned, counts = plan(ROOT)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    changed = [path for path, text in planned.items()
               if not path.is_file() or path.read_text(encoding="utf-8") != text]
    if args.check:
        for path in changed:
            print("Regeneration needed: " + path.relative_to(ROOT).as_posix())
        print(f"docs.regenerate: {len(changed)} file(s) need regeneration")
        return int(bool(changed))
    # Validate every destination before replacing any existing page.
    for path in changed:
        resolved = path.resolve()
        in_repository = resolved.is_relative_to(ROOT.resolve())
        is_document = resolved.is_relative_to((ROOT / "docs").resolve())
        if not in_repository or (not is_document and path != ROOT / "mkdocs.yml"):
            raise ValueError(f"Refusing to write outside documentation: {path}")
    for path in changed:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(planned[path], encoding="utf-8", newline="\n")
    print(f"Regenerated {counts['total_pages']} pages, including {counts['header_pages']} header details "
          f"({counts['new_pages']} new pages); updated {len(changed)} files.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
