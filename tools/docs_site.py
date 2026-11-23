"""MkDocs rendering for the page templates in the write-docs skill.

Keep the custom syntax small: headerless metadata tables and linked code
signatures in table cells. Ordinary inline and fenced examples stay literal.
"""

from __future__ import annotations

import logging
import html
import json
import os
import pathlib
import re
import sys
import xml.etree.ElementTree as etree
from urllib.parse import quote, unquote, urlsplit, urlunsplit

from markdown.extensions import Extension
from markdown.preprocessors import Preprocessor
from markdown.treeprocessors import Treeprocessor
from markdown.util import AtomicString
from mkdocs.exceptions import PluginError
from mkdocs.structure.files import InclusionLevel

# MkDocs loads hooks by filename, without adding their directory to the import path.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import check_docs
import docs_assets
import docs_components
import docs_redirects


class MetadataTables(Preprocessor):
    def run(self, lines):
        visible = check_docs.strip_fences("\n".join(lines)).split("\n")
        result = []
        for index, line in enumerate(lines):
            cells = line.strip().strip("|").split("|")
            separator = (visible[index].lstrip().startswith("|") and len(cells) > 1
                         and all(re.fullmatch(r"\s*:?-{3,}:?\s*", cell) for cell in cells))
            previous_row = index > 0 and visible[index - 1].lstrip().startswith("|")
            if separator and not previous_row:
                indent = line[:len(line) - len(line.lstrip())]
                result.append(indent + "| " + " | ".join("" for _ in cells) + " |")
            result.append(line)
        return result


class TemplateTables(Treeprocessor):
    def run(self, root):
        for table in root.iter("table"):
            header = table.find("thead")
            if header is not None and not "".join(header.itertext()).strip():
                table.remove(header)
                table.set("class", "doc-metadata")
            for code in table.iter("code"):
                text = code.text or ""
                matches = list(check_docs.CODE_LINK.finditer(text))
                if not matches:
                    continue
                code.text = AtomicString(text[:matches[0].start()])
                for index, match in enumerate(matches):
                    link = etree.SubElement(code, "a", {"href": match.group(2)})
                    link.text = AtomicString(match.group(1))
                    end = matches[index + 1].start() if index + 1 < len(matches) else len(text)
                    link.tail = AtomicString(text[match.end():end])
        return root


class DocumentationExtension(Extension):
    def extendMarkdown(self, md):
        md.preprocessors.register(MetadataTables(md), "doc_metadata", 27)
        # After inline parsing (20), before MkDocs rewrites links (0).
        md.treeprocessors.register(TemplateTables(md), "doc_tables", 15)
        md.treeprocessors.register(CalloutTypes(md), "doc_callouts", 14)
        md.treeprocessors.register(FunctionGroups(md), "doc_function_groups", 6)


class FunctionGroups(Treeprocessor):
    def run(self, root):
        children = list(root)
        index = 0
        while index < len(children):
            heading = children[index]
            if heading.tag != "h4" or not heading.get("id", "").startswith(("function-", "operator-")):
                index += 1
                continue
            group = etree.Element("details", {"class": "function-group"})
            summary = etree.SubElement(group, "summary")
            root.insert(list(root).index(heading), group)
            root.remove(heading)
            summary.append(heading)
            index += 1
            while index < len(children) and not re.fullmatch(r"h[1-4]", children[index].tag):
                child = children[index]
                root.remove(child)
                group.append(child)
                index += 1
        return root


class CalloutTypes(Treeprocessor):
    def run(self, root):
        kinds = {"note", "tip", "important", "warning", "caution"}
        for element in root.iter("div"):
            if "admonition" not in element.get("class", "").split():
                continue
            title = element.find("p[@class='admonition-title']")
            kind = "".join(title.itertext()).strip().lower() if title is not None else ""
            if kind in kinds:
                element.set("class", "admonition " + kind)
                element.set("role", "note")
        return root


def on_config(config):
    root = pathlib.Path(config.config_file_path).resolve().parent
    try:
        # Hooks run after the plugins, so the search plugin has already filled in its default language.
        for name, plugin in config.plugins.items():
            if name.rsplit("/", 1)[-1] == "search":
                docs_assets.check_search_languages(plugin.config.lang)
        mapping, _ = docs_assets.prepare_assets(root)
    except ValueError as error:
        raise PluginError(str(error)) from error
    config.extra["editable_assets"] = mapping
    config.markdown_extensions.append(DocumentationExtension())
    config.validation.links.anchors = logging.WARNING
    config.validation.links.unrecognized_links = logging.WARNING
    config.validation.nav.omitted_files = logging.WARNING
    return config


def on_nav(nav, *, config, files):
    """Use canonical handbook pages as their section's landing page without moving files."""
    def visit(item):
        children = getattr(item, "children", ()) or ()
        pages = [item] if item.is_page else []
        for child in children:
            pages.extend(visit(child))
        if children and children[0].is_page:
            first = children[0]
            source = first.file.src_uri
            prefix = source.removesuffix(".md") + "/"
            component = source in {item.page.as_posix() for item in docs_components.EDITOR_COMPONENTS}
            if source.startswith("handbook/") and (component or any(
                    page.file.src_uri.startswith(prefix) for page in pages)):
                first.doc_section_index = True
        return pages

    for item in nav.items:
        if item.title == "Engineering handbook":
            item.doc_scope_sections = True
            for section in item.children:
                section.doc_collapsible = True
        visit(item)
    return nav


def on_files(files, *, config):
    docs = pathlib.Path(config.docs_dir).resolve()
    for file in list(files):
        if file.is_documentation_page() and pathlib.Path(file.abs_src_path).is_file():
            if docs_redirects.metadata(pathlib.Path(file.abs_src_path).read_text(encoding="utf-8")):
                file.inclusion = InclusionLevel.NOT_IN_NAV
        is_theme_asset = file.src_uri.startswith(("assets/stylesheets/", "assets/javascripts/"))
        if not is_theme_asset:
            continue
        is_source_asset = pathlib.Path(file.abs_src_path).resolve().is_relative_to(docs)
        if is_theme_asset and not is_source_asset:
            files.remove(file)
    return files


def on_post_page(output, *, page, config):
    redirect = docs_redirects.metadata(pathlib.Path(page.file.abs_src_path).read_text(encoding="utf-8"))
    if not redirect:
        return output
    source = pathlib.Path(page.file.abs_src_path)

    def rendered(target):
        destination, fragment, error = check_docs.resolve_target(source, target)
        if error or destination is None:
            raise PluginError("Invalid handbook redirect: " + target)
        relative = pathlib.Path(os.path.relpath(destination.with_suffix(".html"), source.parent)).as_posix()
        return quote(relative, safe="/") + ("#" + quote(fragment) if fragment else "")

    target = rendered(redirect[0])
    fragments = {key: rendered(value) for key, value in redirect[1].items()}
    tags = ('<meta name="doc-redirect" content="' + html.escape(target, quote=True) + '">\n'
            '<meta name="doc-redirect-fragments" content="'
            + html.escape(json.dumps(fragments), quote=True) + '">\n'
            '<noscript><meta http-equiv="refresh" content="0; url='
            + html.escape(target, quote=True) + '"></noscript>\n')
    return output.replace("</head>", tags + "</head>", 1)


def on_page_markdown(markdown, *, page, config, files):
    source = pathlib.Path(page.file.abs_src_path)
    # The same rules apply to `check`, `build`, and the live preview.
    problems = []
    check_docs.check_page(source, problems)
    if problems:
        raise PluginError("\n".join(problems))
    # Directory breadcrumbs and extensionless type links become explicit source-page links.
    visible = check_docs.strip_code(markdown)
    replacements = []
    matches = [(match, 1) for pattern in (check_docs.LINK_PATTERN, check_docs.REFERENCE_DEFINITION)
               for match in pattern.finditer(visible)]
    matches += [(match, 1 if match.group(1) is not None else 2)
                for match in check_docs.HTML_LINK.finditer(visible)]
    for match, group in matches:
        target = match.group(group).strip("<>")
        destination, _, error = check_docs.resolve_target(source, target)
        if error or destination is None:
            continue
        parsed = urlsplit(target)
        if parsed.path.endswith(".html") and destination.suffix == ".md":
            continue
        original = (source.parent / unquote(parsed.path)).resolve()
        if parsed.path and original != destination:
            path = pathlib.Path(os.path.relpath(destination, source.parent)).as_posix()
            replacement = urlunsplit(("", "", quote(path, safe="/"), parsed.query, parsed.fragment))
            replacements.append((match.start(group), match.end(group), replacement))
    for start, end, replacement in sorted(replacements, reverse=True):
        markdown = markdown[:start] + replacement + markdown[end:]
    return markdown
