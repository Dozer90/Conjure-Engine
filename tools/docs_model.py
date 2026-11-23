"""Source inventory and relative documentation targets for the engineering handbook."""

from __future__ import annotations

from collections import defaultdict
from dataclasses import dataclass
import html
from pathlib import Path
import re

import docs_cpp
import docs_components
import docs_policy

ROOT = Path(__file__).resolve().parent.parent


@dataclass(frozen=True)
class DocTarget:
    path: Path
    fragment: str = ""


def clean(text):
    text = re.sub(r"(?m)^\s*///\s?", "", text)
    text = re.sub(r'<see\s+cref="([^"]+)"\s*/>', r"\1", text)
    text = re.sub(r"</?(?:summary|para|c|code)>", "", text)
    return " ".join(docs_policy.sanitize(html.unescape(text)).split())


def slug(name):
    return re.sub(r"[^a-z0-9-]+", "-", name.replace("_", "-").lower()).strip("-")


def entity_kind(row):
    match = re.match(r"(?:template\s*<.*?>\s*)?(class|struct)\b", row.signature)
    return match.group(1) if row.kind == "types" and match else ""


def cmake_dependencies(path):
    text = re.sub(r"#[^\n]*", "", path.read_text(encoding="utf-8"))
    variables = defaultdict(list)
    for match in re.finditer(r"\b(set|list)\s*\((.*?)\)", text, re.DOTALL):
        words = match.group(2).split()
        if match.group(1) == "list":
            if not words or words.pop(0) != "APPEND":
                continue
        if words:
            variables[words[0]].extend(words[1:])
    module = re.search(r"conjure_add_module\s*\((.*?)\n\)", text, re.DOTALL)
    if not module:
        return []
    access = ""
    result = []
    for word in module.group(1).split():
        if word in {"PUBLIC_DEPS", "PRIVATE_DEPS", "SOURCES", "PUBLIC_HEADERS", "DEFINES"}:
            access = word if word.endswith("_DEPS") else ""
            continue
        if not access:
            continue
        expanded = variables.get(word[2:-1], []) if word.startswith("${") else [word]
        for value in expanded:
            if value.startswith("conjure::"):
                item = (value.removeprefix("conjure::"), access.removesuffix("_DEPS").lower())
                if item not in result:
                    result.append(item)
    return result


class SourceIndex:
    def __init__(self, root):
        self.root = Path(root).resolve()
        self.records = {}
        self.symbols = defaultdict(list)
        self.locations = {}
        self.namespaces = {}
        self.type_pages = {}
        self.dependencies = {}
        for physical in ("engine", "editor"):
            include = self.root / physical / "include/conjure"
            for path in sorted(include.rglob("*.h")):
                relative = path.relative_to(include)
                module = relative.parts[0]
                scope = "editor" if module == "editor" else physical
                page = Path("handbook") / scope / module / Path(*relative.parts[1:]).with_suffix(".md")
                page = Path(*(part.replace("_", "-") for part in page.parts))
                if page in {location[2] for location in self.locations.values()}:
                    raise ValueError("Header page paths collide: " + str(page))
                self.locations[path] = (scope, module, page, physical)
                text = path.read_text(encoding="utf-8")
                self.records[path] = docs_cpp.inventory(text)
                namespaces = re.findall(r"\bnamespace\s+(conjure(?:::\w+)*)\s*\{", text)
                namespace = namespaces[0] if namespaces else "conjure::" + module
                self.namespaces[path] = namespace
                destinations = {}
                for row in self.records[path]:
                    if row.kind != "types":
                        continue
                    prefix, suffix = namespace.split("::"), row.name.split("::")
                    overlap = max((size for size in range(1, min(len(prefix), len(suffix)) + 1)
                                   if prefix[-size:] == suffix[:size]), default=0)
                    qualified = "::".join(prefix + suffix[overlap:])
                    names = {row.name, suffix[-1], qualified, qualified.removeprefix("conjure::")}
                    names.update(re.sub(r"<.*", "", name) for name in list(names))
                    for name in names:
                        self.symbols[name].append((path, row))
                    if row.defined and entity_kind(row):
                        destination = page.with_suffix("") / (slug(row.name) + ".md")
                        if destination in destinations and destinations[destination] != row.name:
                            raise ValueError("Type page paths collide: " + str(destination))
                        destinations[destination] = row.name
                        self.type_pages[path, row.name] = destination
            for cmake in sorted((self.root / physical / "src").glob("*/CMakeLists.txt")):
                self.dependencies[cmake.parent.name] = cmake_dependencies(cmake)

    def model(self, header):
        return SourceModel(self, header)


class SourceModel:
    def __init__(self, index, header):
        self.index = index
        self.root = index.root
        self.header = Path(header)
        self.scope, self.module, self.header_page, self.physical = index.locations[self.header]
        self.header_name = self.header.name
        self.scope_page = Path("handbook") / (self.scope + ".md")
        self.module_page = Path("handbook") / self.scope / (self.module + ".md")
        component = (docs_components.editor_component(self.header.relative_to(
            self.root / self.physical / "include/conjure/editor")) if self.module == "editor" else None)
        self.component = component
        self.component_page = component.page if component else self.module_page
        self.component_title = component.title if component else self.module.replace("_", " ").title()
        if self.module in {"ecs", "rhi"}:
            self.component_title = self.module.upper()
        self.namespace = index.namespaces[self.header]
        self.records, self.symbols, self.dependencies = index.records, index.symbols, index.dependencies
        by_name = {row.name: row for row in self.records[self.header] if row.defined and entity_kind(row)}
        self.types = sorted(by_name.values(), key=lambda row: row.name.casefold())
        self.type_pages = {row.name: index.type_pages[self.header, row.name] for row in self.types}
        self.code_identifiers = {row.name.split("::")[-1] for rows in self.records.values() for row in rows
                                 if row.name.split("::")[-1].isupper()}
        self.code_identifiers.update(name for row in self.records[self.header]
                                     for name in re.findall(r"\b[a-z]\w*[A-Z]\w*\b", row.signature))

    def members(self, entity, kind):
        prefix = entity.name + "::"
        rows = [row for row in self.records[self.header] if row.kind == kind
                and row.name.startswith(prefix) and "::" not in row.name[len(prefix):]]
        return sorted(dict.fromkeys(rows), key=lambda row: row.name.casefold())

    def type_reference(self, name, current_type=""):
        names = (name, "conjure::" + name, self.namespace + "::" + name, current_type + "::" + name)
        candidates = list(dict.fromkeys(item for key in names for item in self.symbols.get(key, [])))
        if not candidates:
            return None
        def rank(item):
            path, row = item
            defined = row.defined or row.signature.startswith(("using ", "typedef "))
            return (not defined, path != self.header, self.index.locations[path][1] != self.module,
                    row.name != current_type + "::" + name, str(path))
        path, row = min(candidates, key=rank)
        if (path, row.name) in self.index.type_pages:
            return self.index.type_pages[path, row.name]
        fragment = "" if entity_kind(row) else "symbol-" + slug(row.name)
        return DocTarget(self.index.locations[path][2], fragment)

    def reference_page(self, page):
        page = Path(page).with_suffix(".md")
        return page if (self.root / "docs" / page).is_file() else None

    def header_reference(self, path):
        return self.index.locations[path][2]

    def purpose(self, path):
        source = path.read_text(encoding="utf-8")
        rows = self.records[path]
        main = next((row for row in rows if row.name == path.stem and row.summary and row.defined), None)
        if main:
            return clean(main.summary)
        before = source.split("namespace ", 1)[0]
        for block in re.findall(r"(?m)(?:^//[^\n]*\n)+", before):
            block = re.split(r"\n[ \t]*//[ \t]*\n", block, maxsplit=1)[0]
            value = re.sub(r"(?m)^//[ \t]?", "", block)
            if not re.search(r"copyright|SPDX", value, re.IGNORECASE):
                return clean(value)
        return "Declarations for " + path.name + "."

    def bases(self, entity):
        split = re.split(r"(?<!:):(?!:)", entity.signature, maxsplit=1)
        if len(split) == 1:
            return []
        values = []
        start, depth = 0, 0
        for position, character in enumerate(split[1]):
            depth += (character == "<") - (character == ">")
            if character == "," and depth == 0:
                values.append(split[1][start:position])
                start = position + 1
        values.append(split[1][start:])
        return [re.sub(r"\b(public|private|protected|virtual)\s+", "", base).strip() for base in values]

    def ancestor_chains(self, entity, visited=frozenset()):
        if entity.name in visited:
            return [[entity.name]]
        result = []
        for name in self.bases(entity):
            parent = next((row for rows in self.records.values() for row in rows
                           if row.defined and entity_kind(row)
                           and (row.name == name or row.name.endswith("::" + name))), None)
            chains = self.ancestor_chains(parent, visited | {entity.name}) if parent else [[name]]
            result.extend([*chain, entity.name] for chain in chains)
        return result or [[entity.name]]

    def derived(self, entity):
        return [(path, row) for path, rows in self.records.items() for row in rows
                if row.defined and entity_kind(row) and entity.name in self.bases(row)]
