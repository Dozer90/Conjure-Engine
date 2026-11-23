"""Generate scope, component/module, header, and structure/class pages while preserving authored regions."""

from __future__ import annotations

import copy
import html
import json
from pathlib import Path
import re
from types import SimpleNamespace

import create_doc
import docs_cpp
import docs_components
import docs_graph
import docs_model
import docs_policy
import docs_render


def cell(value):
    return value.replace("|", "\\|").replace("\n", " ")


def existing_notes(text, legacy):
    if "<!-- doc-content:" in text:
        regions = re.findall(r'<!-- doc-content:([^:]+):start -->\n(.*?)\n<!-- doc-content:\1:end -->',
                             text, re.DOTALL)
        return {name: value for name, value in regions if value.strip()}
    if "<!-- doc-template:" in text:
        return {}
    if not text:
        return {}
    _, _, raw = legacy.document_parts(text)
    purpose, entries = legacy.sections(raw)
    return {"purpose": purpose, "notes": "\n\n".join("## " + name + "\n\n" + value for name, value in entries)}


def authored(name, value, legacy):
    return legacy.content_region(name, value)


def render(kind, values, existing, legacy):
    output = create_doc.render_template(kind, values)
    # These layouts own kind metadata; retain other existing YAML fields once.
    front, _, _ = legacy.document_parts(existing)
    if front:
        front = re.sub(r"(?m)^kind:.*\n?", "", front)
        generated = re.match(r"---\nkind: ([^\n]+)\n---\n", output)
        if generated:
            front = front.replace("\n---", "\nkind: " + generated.group(1) + "\n---", 1)
            output = front + output[generated.end():].lstrip("\n")
    marker = "<!-- doc-template:" + values.get("overview_kind", kind) + " -->"
    output = re.sub(r"(?m)^(# .+)$", lambda match: match.group() + "\n" + marker, output, count=1)
    return output.rstrip() + "\n"


def module_regions(existing, legacy):
    notes = existing_notes(existing, legacy)
    result = {name: notes.get(name, "") for name in ("purpose", "breakdown", "threading", "extensions", "testing")}
    if any(result[name] for name in ("breakdown", "threading", "extensions", "testing")):
        return result
    body = notes.get("body", notes.get("notes", ""))
    if re.search(r"(?mi)^#{2,6}\s+(?:\d+[.)]?\s+)?(?:Contents|Public headers)\s*$", body):
        raise ValueError("Legacy module chapter needs an authored migration into purpose, breakdown, threading, "
                         "extensions, and testing regions; do not copy its contents or header index into Breakdown.")
    intro, entries = legacy.sections(body)
    result["breakdown"] = intro
    for title, value in entries:
        clean_title = re.sub(r"^\d+[.)]?\s+", "", title)
        destination = "breakdown"
        for name, pattern in (("threading", r"thread|ownership"), ("extensions", r"extension|convention"),
                              ("testing", r"test")):
            if re.search(pattern, clean_title, re.IGNORECASE):
                destination = name
                break
        if destination == "breakdown":
            value = "### " + clean_title + "\n\n" + value
        result[destination] += ("\n\n" if result[destination] else "") + value
    return result


def rewrite_reference_links(text, page):
    def replacement(match):
        target = Path(match.group(1)).with_suffix(".md")
        return docs_render.href(page, target)
    return re.sub(r"/reference/([^\s)\"#]+\.html)", replacement, text)


def source_path(model):
    files = sorted((model.root / model.physical / "src" / model.module).rglob(model.header.stem + ".cpp"))
    return ", ".join("`" + path.relative_to(model.root).as_posix() + "`" for path in files) if files else "N/A"


def declaration_code(row):
    """Keep written C++ intact; remove planning citations only from its comments."""
    source = row.definition or row.signature
    for token in reversed(list(docs_cpp.LEXER.finditer(source))):
        if token.lastgroup == "comment" and docs_policy.findings(token.group()):
            cleaned = docs_policy.sanitize(token.group())
            source = source[:token.start()] + cleaned + source[token.end():]
    return source.rstrip(";") + ";"


def extra_declarations(model, page):
    records = model.records[model.header]
    groups = {name: [] for name in ("Enumerations", "Type Aliases", "Concepts", "Unions",
                                   "Forward Declarations", "Constants", "Variables", "Macros", "Declarations")}
    types = {}
    for row in records:
        if row.kind == "types" and (row.name not in types or row.defined):
            types[row.name] = row
    for row in sorted(types.values(), key=lambda item: item.name.casefold()):
        if docs_model.entity_kind(row) and row.defined:
            continue
        signature = row.signature
        if re.search(r"\benum\s+", signature):
            group = "Enumerations"
        elif re.search(r"\b(?:using|typedef)\s+", signature):
            group = "Type Aliases"
        elif re.search(r"\bconcept\s+", signature):
            group = "Concepts"
        elif re.search(r"\bunion\s+", signature):
            group = "Unions"
        elif docs_model.entity_kind(row):
            group = "Forward Declarations"
        elif re.match(r"[A-Z][A-Z_0-9]*\s*\(", signature):
            group = "Macros"
        else:
            group = "Declarations"
        block = ["### `" + row.name + "` {#symbol-" + docs_model.slug(row.name) + "}"]
        code = row.signature if group == "Macros" else declaration_code(row)
        block.append("```cpp\n" + code + "\n```")
        if row.summary:
            block.append(docs_render.prose(docs_model.clean(row.summary), page, model))
        groups[group].append("\n\n".join(block))
    owners = tuple(row.name + "::" for row in types.values() if row.defined)
    for row in sorted(records, key=lambda item: item.name.casefold()):
        if row.kind != "variables" or row.name.startswith(owners):
            continue
        _, type_name, initializer = docs_render.variable_parts(row)
        # A pointer to const data is still a mutable variable; only top-level const counts.
        declarator = type_name.rsplit("*", 1)[-1] if "*" in type_name else type_name
        constant = re.search(r"\bconstexpr\b", type_name) or re.search(r"\bconst\b", declarator)
        group = "Constants" if constant else "Variables"
        code = lambda text: cell(docs_render.table_code(docs_render.linked_code(text, page, model)))
        groups[group].append('| <a id="variable-' + html.escape(row.name, quote=True) + '"></a><code>'
                             + html.escape(row.name) + '</code> | ' + code(type_name) + ' | '
                             + (code(initializer) if initializer else '—') + ' | '
                             + cell(docs_render.prose(docs_model.clean(row.summary), page, model)) + ' |')
    result = []
    for name, entries in groups.items():
        if entries:
            body = "\n\n".join(entries)
            if name in {"Constants", "Variables"}:
                body = "| Name | Type | Value | Description |\n|---|---|---|---|\n" + "\n".join(entries)
            result.append("## " + name + "\n\n" + body)
    return "\n\n".join(result)


def free_functions(model, page, descriptions):
    owners = [row.name + "::" for row in model.types]
    functions = [row for row in model.records[model.header] if row.kind == "functions"
                 and not any(row.name.startswith(prefix) for prefix in owners)]
    if not functions:
        return ""
    view = copy.copy(model)
    view.members = lambda entity, kind: sorted(functions, key=lambda row: row.name.casefold())
    return "## Functions\n\n" + docs_render.function_sections(SimpleNamespace(name=""), page, view, descriptions)


def plan(root, originals, index, legacy):
    docs = root / "docs"
    result = {}
    models = {path: index.model(path) for path in index.records}
    modules = {page: text for page, text in originals.items()
               if page.parent in (docs / "handbook/engine", docs / "handbook/editor")
               and not (page.parent.name == "editor" and page.stem in docs_components.BY_SLUG)}
    editor_models = [model for model in models.values() if model.component]
    active_components = [item for item in docs_components.EDITOR_COMPONENTS
                         if any(model.component == item for model in editor_models)]
    overrides_path = root / "tools/docs_content/engine/audio"
    overrides = json.loads((overrides_path / "audio-engine.json").read_text(encoding="utf-8")) \
        if (overrides_path / "audio-engine.json").is_file() else {}
    for scope in ("engine", "editor"):
        page = docs / "handbook" / (scope + ".md")
        if page not in originals:
            continue
        old = originals[page]
        notes = existing_notes(old, legacy)
        rows = []
        for module_page, module_text in sorted(modules.items(), key=lambda item: item[0].stem.casefold()):
            if module_page.parent.name != scope:
                continue
            name = module_page.stem
            label = create_doc.module_title(name)
            pattern = r"(?m)^\|\s*\[" + re.escape(label) + r"\]\([^)]*\)\s*\|\s*(.*?)\s*\|"
            match = re.search(pattern, old)
            purpose = module_regions(module_text, legacy)["purpose"]
            description = match.group(1) if match else purpose.split("\n\n", 1)[0]
            if not description:
                description = label + " module services and implementation."
            rows.append("| " + legacy.link(page, module_page, label) + " | " + cell(description) + " |")
        purpose = notes.get("purpose", "")
        grouping = {}
        if scope == "editor" and active_components:
            rows = ["| " + legacy.link(page, docs / item.page, item.title) + " | " + item.description + " |"
                    for item in active_components]
            grouping = {"group_heading": "Components", "group_label": "Component"}
            architecture = legacy.link(page, docs / "handbook/editor/editor.md", "Editor Architecture")
            if "editor/editor.md" not in purpose:
                purpose += "\n\n" + architecture + " explains the application and its supporting libraries."
        result[page] = render("scope", {"title": scope.capitalize(),
            "purpose": authored("purpose", purpose, legacy), "modules": "\n".join(rows), **grouping}, old, legacy)
    for component in active_components:
        page = component.page
        old = originals.get(docs / page, "")
        notes = existing_notes(old, legacy)
        rows = []
        for model in sorted((item for item in editor_models if item.component == component),
                            key=lambda item: item.header.name.casefold()):
            provides = [docs_render.link(page, model.type_pages[row.name], "`" + row.name + "`")
                        for row in model.types]
            rows.append("| " + docs_render.link(page, model.header_page, model.header.name) + " | "
                        + cell(docs_render.prose(model.purpose(model.header), page, model)) + " | "
                        + "<br>".join(provides) + " |")
        values = {"title": component.title, "breadcrumbs": docs_render.link(page, Path("handbook/editor.md"),
            "Editor"), "namespace": "conjure::editor", "include_path": "engine/include/conjure/editor",
            "source_path": "engine/src/editor", "headers": "\n".join(rows),
            "ownership_row": "| Implementation | " + ", ".join("`" + item + "`" for item in component.libraries)
                + " |\n", "purpose": authored("purpose", notes.get("purpose", component.description), legacy)}
        for name in ("breakdown", "threading", "extensions", "testing"):
            fallback = ("[Editor Architecture](editor.md) describes how this component participates in the application."
                        if name == "breakdown" else "")
            values[name] = authored(name, notes.get(name, fallback), legacy)
        result[docs / page] = render("component", values, old, legacy)
    for page, old in modules.items():
        scope, module = page.parent.name, page.stem
        candidates = [model for model in models.values() if model.scope == scope and model.module == module]
        regions = module_regions(old, legacy)
        if module == "audio" and scope == "engine" and overrides:
            regions["purpose"] = overrides["module_purpose"]
            for name in ("breakdown", "threading", "extensions", "testing"):
                path = overrides_path / ("audio-" + name + ".md")
                regions[name] = rewrite_reference_links(path.read_text(encoding="utf-8").strip(),
                                                       page.relative_to(docs))
        rows = []
        for model in sorted(candidates, key=lambda item: item.header.name.casefold()):
            description = (overrides.get("header_summary", model.purpose(model.header))
                           if module == "audio" and model.header_name == "audio_engine.h"
                           else model.purpose(model.header).split(". ", 1)[0].rstrip(".") + ".")
            provides = [docs_render.link(page.relative_to(docs), model.type_pages[row.name], "`" + row.name + "`")
                        for row in model.types]
            rows.append("| " + docs_render.link(page.relative_to(docs), model.header_page, model.header.name)
                        + " | " + cell(docs_render.prose(description, page.relative_to(docs), model))
                        + " | " + "<br>".join(provides) + " |")
        physical = candidates[0].physical if candidates else ("runtime" if module == "runtime" else "engine")
        include = root / physical / "include/conjure" / module
        source = root / physical / "src" / module if module != "runtime" else root / "runtime/src"
        graph_model = candidates[0] if candidates else SimpleNamespace(
            module=module, scope=scope, dependencies=index.dependencies,
            reference_page=lambda target: target if (docs / target).is_file() else None)
        diagram, _ = docs_graph.graph(graph_model)
        diagram_path = Path("assets/diagrams") / (scope + "-" + module + "-dependencies.svg")
        result[docs / diagram_path] = diagram
        values = {"title": create_doc.module_title(module), "breadcrumbs": legacy.link(
            page, docs / "handbook" / (scope + ".md"), scope.capitalize()), "namespace": "conjure::" + module,
            "include_path": include.relative_to(root).as_posix() if include.is_dir() else "N/A",
            "source_path": source.relative_to(root).as_posix() if source.is_dir() else "N/A",
            "purpose": authored("purpose", regions["purpose"], legacy), "dependency_graph": docs_render.href(
                page.relative_to(docs), diagram_path),
            "headers": "\n".join(rows)}
        values.update({name: authored(name, regions[name], legacy)
                       for name in ("breakdown", "threading", "extensions", "testing")})
        if scope == "editor" and module == "editor":
            values.update(title="Editor Architecture", overview_kind="application", overview_type="Application",
                          headers="", dependency_graph="",
                          ownership_row="| Executable | `conjure_editor` |\n"
                                        "| Libraries | `conjure::editor_core`, `conjure::editor_ui` |\n")
        if not regions["testing"].strip():
            values["testing"] = authored("testing", "Header compilation checks and the module suite:\n\n"
                "```bash\npython tools/build.py --target conjure_header_check\n"
                "python tools/build.py --test -L " + module + "\n```", legacy)
        result[page] = render("module", values, old, legacy)
    for header, model in models.items():
        module_page = docs / model.module_page
        if module_page not in modules:
            raise ValueError("Register the module overview before regenerating: " + str(model.module_page))
        page = model.header_page
        old = originals.get(docs / page, "")
        is_audio = model.scope == "engine" and model.module == "audio" and model.header_name == "audio_engine.h"
        descriptions = overrides.get("descriptions", {}) if is_audio else {}
        purposes = overrides.get("type_purposes", {}) if is_audio else {}
        notes = existing_notes(old, legacy)
        purpose = overrides.get("header_purpose", model.purpose(header)) if is_audio else model.purpose(header)
        purpose = "<p>" + docs_render.prose(purpose, page, model) + "</p>"
        if notes.get("purpose") and notes["purpose"].strip() != purpose.strip():
            purpose += "\n\n" + authored("purpose", notes["purpose"], legacy)
        apis = legacy.api_references(docs / page, header, root, originals)
        api = ", ".join(apis) or legacy.link(docs / page, root / "docs/api/index.md", "Public API")
        indexes = {"struct": [], "class": []}
        for entity in model.types:
            description = purposes.get(entity.name, docs_model.clean(entity.summary))
            indexes[docs_model.entity_kind(entity)].append("| " + docs_render.link(
                page, model.type_pages[entity.name], "`" + entity.name + "`") + " | "
                + cell(docs_render.prose(description, page, model)) + " |")
        values = {"title": header.name, "breadcrumbs": docs_render.link(page, model.scope_page,
            model.scope.capitalize()) + " → " + docs_render.link(page, model.component_page,
            model.component_title), "include_path": header.relative_to(root).as_posix(),
            "api": api, "purpose": purpose, "structs": "\n".join(indexes["struct"]),
            "classes": "\n".join(indexes["class"])}
        body = render("header", values, old, legacy)
        additions = [extra_declarations(model, page), free_functions(model, page, descriptions)]
        preserved = {name: value for name, value in notes.items() if name != "purpose"}
        for name in ("notes", "threading", "extensions", "tests"):
            preserved.setdefault(name, "")
        additions.append(("## Implementation Notes\n\n" if any(preserved.values()) else "") + "\n\n".join(
            authored(name, value, legacy) for name, value in preserved.items()))
        body += "\n" + "\n\n".join(value for value in additions if value) + "\n"
        result[docs / page] = create_doc.prune_empty_sections(body)
        for entity in model.types:
            target = model.type_pages[entity.name]
            old_type = originals.get(docs / target, "")
            constructors = model.members(entity, "constructors")
            regular = [row for row in constructors if "::~" not in row.name]
            destructors = [row for row in constructors if "::~" in row.name]
            variables = model.members(entity, "variables")
            values = {"title": entity.name, "breadcrumbs": docs_render.link(target, model.scope_page,
                model.scope.capitalize()) + " → " + docs_render.link(target, model.component_page,
                model.component_title) + " → " + docs_render.link(target, page, model.header_name),
                "kind": docs_model.entity_kind(entity), "type": docs_model.entity_kind(entity).capitalize(),
                "namespace": "std" if entity.name.startswith("std::") else model.namespace,
                "include_path": header.relative_to(root).as_posix(),
                "source_path": source_path(model), "api": ", ".join(legacy.api_references(
                    docs / target, header, root, originals)) or legacy.link(docs / target,
                    root / "docs/api/index.md", "Public API"), "syntax": entity.signature,
                "inheritance": docs_render.inheritance(entity, target, model),
                "constructors": docs_render.constructor_section(regular, target, model, entity.name, descriptions,
                    [row for row in model.members(entity, "functions") if "= delete" in row.signature]),
                "destructors": docs_render.table(destructors, target, model, entity.name, descriptions)
                    if destructors else "",
                "variables": docs_render.variable_table(variables, target, model, entity.name, descriptions)
                    if variables else "",
                "functions": docs_render.function_sections(entity, target, model, descriptions),
                "operators": docs_render.function_sections(entity, target, model, descriptions, operators=True)}
            body = render("type", values, old_type, legacy)
            type_notes = existing_notes(old_type, legacy)
            type_notes.setdefault("notes", "")
            body += ("\n## Implementation Notes\n\n" if any(type_notes.values()) else "\n") + "\n\n".join(
                authored(name, value, legacy) for name, value in type_notes.items()) + "\n"
            result[docs / target] = create_doc.prune_empty_sections(body)
    return result


def header_navigation(entries, index, indent):
    result = ""
    for header, page in sorted(entries, key=lambda item: item[0].name.casefold()):
        types = sorted(((name, target) for (path, name), target in index.type_pages.items() if path == header),
                       key=lambda item: item[0].casefold())
        result += indent + "- " + json.dumps(header.name) + ":"
        if not types:
            result += " " + page.as_posix() + "\n"
        else:
            result += "\n" + indent + "    - " + page.as_posix() + "\n"
            for name, target in types:
                result += indent + "    - " + json.dumps(name) + ": " + target.as_posix() + "\n"
    return result


def editor_navigation(text, index):
    groups = {}
    for header, (scope, module, page, physical) in index.locations.items():
        if scope == "editor" and module == "editor":
            component = docs_components.editor_component(header.relative_to(
                index.root / physical / "include/conjure/editor"))
            groups.setdefault(component, []).append((header, page))
    if not groups:
        return text
    handbook = text.find("  - Engineering handbook:")
    match = re.compile(r"(?m)^      - Editor:\n").search(text, handbook) if handbook >= 0 else None
    if not match:
        raise ValueError("Register the Editor scope in mkdocs.yml before regenerating")
    following = re.search(r"(?m)^      - |^  - ", text[match.end():])
    end = match.end() + following.start() if following else len(text)
    replacement = ("      - Editor:\n          - handbook/editor.md\n"
                   "          - Architecture: handbook/editor/editor.md\n")
    for component, entries in sorted(groups.items(), key=lambda item: item[0].title.casefold()):
        replacement += "          - " + component.title + ":\n              - " + component.page.as_posix() + "\n"
        replacement += header_navigation(entries, index, "              ")
    return text[:match.start()] + replacement + text[end:]


def handbook_sections(text):
    """Place contributor chapters, Engine, and Editor in sibling sidebar sections."""
    match = re.search(r"(?m)^  - Engineering handbook:\n", text)
    if not match:
        return text
    following = re.search(r"(?m)^  - ", text[match.end():])
    end = match.end() + following.start() if following else len(text)
    chunks = [part for part in re.split(r"(?m)(?=^      - )", text[match.end():end]) if part.strip()]
    scopes = {}
    general = ""
    for chunk in chunks:
        label = re.match(r"      - (Engine|Editor|Engineering Handbook):\n", chunk)
        if label and label.group(1) in {"Engine", "Editor"}:
            scopes[label.group(1)] = chunk
        elif label:
            general += chunk.split("\n", 1)[1]
        else:
            general += "".join("    " + line if line.strip() else line for line in chunk.splitlines(keepends=True))
    body = "      - Engineering Handbook:\n" + general + "".join(scopes.get(name, "") for name in ("Engine", "Editor"))
    return text[:match.end()] + body + text[end:]


def navigation(text, root, index):
    text = re.sub(r"(?m)^ *# BEGIN GENERATED HEADER NAV\n.*?^ *# END GENERATED HEADER NAV\n?", "", text,
                  flags=re.DOTALL)
    groups = {}
    for header, (scope, module, page, _) in index.locations.items():
        groups.setdefault((scope, module), []).append((header, page))
    for (scope, module), entries in sorted(groups.items()):
        if scope == "editor" and module == "editor":
            continue
        relative = "handbook/" + scope + "/" + module + ".md"
        match = re.search(r"(?m)^( *)(- (?:(.+): )?)" + re.escape(relative) + r"\s*$", text)
        if not match:
            raise ValueError("Register the module overview in mkdocs.yml before regenerating: " + relative)
        indent, _, label = match.groups()
        if label:
            replacement = indent + "- " + label + ":\n" + indent + "    - " + relative + "\n"
            child = indent + "    "
        else:
            replacement = indent + "- " + relative + "\n"
            child = indent
        replacement += child + "# BEGIN GENERATED HEADER NAV\n"
        replacement += header_navigation(entries, index, child)
        replacement += child + "# END GENERATED HEADER NAV\n"
        text = text[:match.start()] + replacement + text[match.end():].lstrip("\n")
    text = handbook_sections(editor_navigation(text, index))
    for scope in ("Engine", "Editor"):
        handbook = text.find("  - Engineering handbook:")
        if handbook < 0:
            continue
        match = re.compile(r"(?m)^      - " + scope + r":\n").search(text, handbook)
        if not match:
            continue
        following = re.search(r"(?m)^      - |^  - ", text[match.end():])
        end = match.end() + following.start() if following else len(text)
        body = text[match.end():end]
        parts = re.split(r"(?m)(?=^          - )", body)
        overview = [part for part in parts if re.match(r"          - handbook/", part)]
        entries = [part for part in parts if part.startswith("          - ") and part not in overview]
        prefix = "".join(part for part in parts if not part.startswith("          - "))
        ordered = prefix + "".join(overview) + "".join(sorted(entries, key=str.casefold))
        text = text[:match.end()] + ordered + text[end:]
    return text.rstrip() + "\n"
