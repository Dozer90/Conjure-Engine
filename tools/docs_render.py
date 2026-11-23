"""Render linked declarations and native disclosure controls for engineering documentation."""

from __future__ import annotations

from collections import Counter
import html
import os
from pathlib import Path
import re
from urllib.parse import quote

from docs_model import DocTarget, clean, slug
import docs_cpp

IDENTIFIER = re.compile(r"\b[A-Za-z_]\w*(?:::[A-Za-z_]\w*)*\b")
PROSE_IDENTIFIER = re.compile(r"\b[A-Za-z_]\w*(?:(?:::|->|\.)[A-Za-z_]\w*)*\b")


def href(page, target, rendered=False):
    if isinstance(target, str):
        return target
    fragment = ""
    if isinstance(target, DocTarget):
        fragment = "#" + quote(target.fragment, safe="-") if target.fragment else ""
        target = target.path
    target = target.with_suffix(".html") if rendered else target
    return quote(Path(os.path.relpath(target, page.parent)).as_posix(), safe="/") + fragment


def link(page, target, label):
    return f"[{label}]({href(page, target)})" if target is not None else label


def cpp(text):
    """Colour written declaration tokens without requiring the site toolchain to regenerate pages."""
    keywords = {"alignas", "auto", "bool", "char", "class", "const", "constexpr", "consteval", "constinit",
                "decltype", "default", "delete", "double", "enum", "explicit", "extern", "final", "float",
                "friend", "inline", "int", "long", "mutable", "noexcept", "operator", "override", "private",
                "protected", "public", "requires", "short", "signed", "static", "struct", "template",
                "typename", "union", "unsigned", "using", "virtual", "void", "volatile", "wchar_t"}
    output = []
    attributes = 0
    for token in docs_cpp.LEXER.finditer(text):
        value = token.group()
        style = ""
        if token.lastgroup in {"comment", "directive"}:
            style = "c1" if token.lastgroup == "comment" else "cp"
        elif token.lastgroup in {"string", "raw"}:
            style = "s"
        elif token.lastgroup == "word":
            if attributes or value.startswith("CONJURE_"):
                style = "na"
            elif value in keywords:
                style = "k"
            elif value[0].isdigit():
                style = "mi"
        if value == "[":
            attributes += 1
        elif value == "]":
            attributes = max(0, attributes - 1)
        escaped = html.escape(value, quote=False)
        output.append('<span class="' + style + '">' + escaped + '</span>' if style else escaped)
    return "".join(output)


def parameters(signature, name):
    pattern = re.escape(name).replace(r"\ ", r"\s+") + r"\s*(\()"
    match = re.search(pattern, signature)
    if not match:
        return None
    opening = match.start(1)
    start = opening + 1
    depth = {"(": 0, "[": 0, "{": 0, "<": 0}
    closing = {")": "(", "]": "[", "}": "{", ">": "<"}
    arguments = []
    for token in docs_cpp.LEXER.finditer(signature, start):
        if token.lastgroup in {"space", "comment", "string", "raw"}:
            continue
        value = token.group()
        if value == ")" and depth["("] == 0:
            final = signature[start:token.start()].strip()
            if final:
                arguments.append(final)
            return opening, token.end(), arguments
        if value == "," and not any(depth.values()):
            arguments.append(signature[start:token.start()].strip())
            start = token.end()
        elif value in depth:
            depth[value] += 1
        elif value in closing:
            key = closing[value]
            depth[key] = max(0, depth[key] - 1)
    return None


def format_function(signature, name):
    """Keep each argument on its own line without changing declaration tokens."""
    parsed = parameters(signature, name)
    if parsed is None or not parsed[2]:
        return signature
    opening, end, arguments = parsed
    lines = [signature[:opening + 1]]
    lines += ["    " + value + ("," if index < len(arguments) - 1 else ")" + signature[end:])
              for index, value in enumerate(arguments)]
    return "\n".join(lines)


def linked_code(text, page, model, current_type="", declaration_name=""):
    literals = [(match.start(), match.end()) for match in docs_cpp.LEXER.finditer(text)
                if match.lastgroup in {"string", "raw", "comment"}]
    pieces = []
    cursor = 0
    for match in IDENTIFIER.finditer(text):
        if any(start <= match.start() < end for start, end in literals):
            continue
        remaining = text[match.end():]
        if match.group() == declaration_name and remaining.startswith("("):
            continue
        if re.match(r"\s*[,)=;]", remaining):
            continue
        target = model.type_reference(match.group(), current_type)
        if target is None:
            continue
        pieces.append(cpp(text[cursor:match.start()]))
        url = html.escape(href(page, target, rendered=True), quote=True)
        pieces.append(f'<a href="{url}">{html.escape(match.group())}</a>')
        cursor = match.end()
    pieces.append(cpp(text[cursor:]))
    return "".join(pieces)


def is_code_name(name, model):
    if name in {"e.g", "i.e"}:
        return False
    return bool("_" in name or "::" in name or "." in name or "->" in name or name in model.type_pages
                or name in model.code_identifiers or re.fullmatch(r"[uif]\d+|usize|isize|byte", name))


def method_reference(name, expression, methods):
    candidates = methods.get(name) if methods else None
    if isinstance(candidates, str):
        return "#" + candidates
    if not candidates:
        return None
    reference = parameters(expression, name)
    if reference and reference[2]:
        normalise = lambda value: "".join(token.group() for token in docs_cpp.LEXER.finditer(value)
                                          if token.lastgroup not in {"space", "comment"})
        wanted = [normalise(value) for value in reference[2]]
        for signature, anchor in candidates:
            parsed = parameters(signature, name)
            if parsed and len(parsed[2]) >= len(wanted):
                if all(normalise(actual).startswith(expected) for actual, expected in zip(parsed[2], wanted)):
                    return "#" + anchor
    return "#" + candidates[0][1]


def inline_code(value, page, model, current_type="", methods=None):
    pieces = []
    cursor = 0
    for match in IDENTIFIER.finditer(value):
        name = match.group()
        target = model.type_reference(name, current_type) if is_code_name(name, model) else None
        if value[match.end():].startswith("("):
            target = method_reference(name, value[match.start():], methods) or target
        if target is None:
            continue
        pieces.append(html.escape(value[cursor:match.start()]))
        pieces.append('<a href="' + html.escape(href(page, target, True), quote=True) + '">'
                      + html.escape(name) + '</a>')
        cursor = match.end()
    pieces.append(html.escape(value[cursor:]))
    return '<code>' + "".join(pieces) + '</code>'


def prose(text, page, model, current_type="", methods=None):
    text = clean(text)
    pieces = []
    cursor, index = 0, 0
    while index < len(text):
        quoted = re.match(r"'([A-Za-z_]\w*(?:(?:::|->|\.)\w+)*)'", text[index:]) if text[index] == "'" else None
        if quoted:
            pieces.append(html.escape(text[cursor:index]))
            value = quoted.group(1) if len(quoted.group(1)) > 1 else quoted.group()
            pieces.append(inline_code(value, page, model, current_type, methods))
            cursor = index = index + quoted.end()
            continue
        if text[index] == "`":
            fence = re.match(r"`+", text[index:]).group()
            closing = text.find(fence, index + len(fence))
            if closing >= 0:
                pieces.append(html.escape(text[cursor:index]))
                pieces.append(inline_code(text[index + len(fence):closing], page, model, current_type, methods))
                cursor = index = closing + len(fence)
                continue
        match = PROSE_IDENTIFIER.match(text, index)
        if not match:
            index += 1
            continue
        name, end = match.group(), match.end()
        template = False
        if end < len(text) and text[end] == "<":
            depth, position = 1, end + 1
            while position < len(text) and depth:
                depth += (text[position] == "<") - (text[position] == ">")
                position += 1
            if not depth:
                end, template = position, True
        call = parameters(text[index:], text[index:end]) if text[end:end + 1] == "(" else None
        if call:
            end = index + call[1]
        literal = name in {"null", "nullptr", "true", "false"}
        if call or template or literal or is_code_name(name, model):
            pieces.append(html.escape(text[cursor:index]))
            pieces.append(inline_code(text[index:end], page, model, current_type, methods))
            cursor = end
        index = end
    pieces.append(html.escape(text[cursor:]))
    return "".join(pieces)


def callout(kind, description, page, model, current_type="", methods=None):
    if kind not in {"note", "tip", "important", "warning", "caution"}:
        raise ValueError("Unsupported callout type: " + kind)
    return ('<div class="admonition ' + kind + '" role="note"><p class="admonition-title">'
            + kind.capitalize() + '</p><p>' + prose(description, page, model, current_type, methods)
            + '</p></div>')


def table_code(markup, wrap_after=24):
    """Offer line breaks at code boundaries without changing the displayed tokens."""
    if len(html.unescape(re.sub(r"<[^>]*>", "", markup))) <= wrap_after:
        return '<code class="signature">' + markup + '</code>'
    boundary = r"(?<=::)|(?<=_)|(?<=[a-z0-9])(?=[A-Z])|(?<=&lt;)|(?<=,)"
    parts = re.split(r"(<[^>]*>)", markup)
    wrapped = "".join(part if index % 2 else re.sub(boundary, "<wbr>", part)
                      for index, part in enumerate(parts))
    return '<code class="signature">' + wrapped + '</code>'


def table(rows, page, model, current_type, descriptions):
    output = ['<table class="member-table">', '<thead><tr><th>Access</th><th>Signature</th>'
              '<th>Description</th></tr></thead>', '<tbody>']
    for row in rows:
        description = descriptions.get(row.name, clean(row.summary))
        if not description and "= delete" in row.signature:
            description = "Deleted; this operation cannot be called."
        signature = row.signature.rstrip(";") + ";"
        name = row.name.split("::")[-1]
        signature = format_function(signature, name)
        explanation = (callout("warning", description, page, model, current_type) if "= delete" in row.signature
                       else prose(description, page, model, current_type))
        output.append('<tr><td>' + row.access + '</td><td>'
                      + '<code class="signature">' + linked_code(signature, page, model, current_type,
                                                                  row.name.split("::")[-1].lstrip("~"))
                      + '</code></td><td>'
                      + explanation + '</td></tr>')
    output += ['</tbody>', '</table>']
    return "\n".join(output)


def variable_parts(row):
    """Separate the written declarator and initializer without inferring a default."""
    declaration = row.signature.rstrip(";")
    tokens = [match for match in docs_cpp.LEXER.finditer(declaration)
              if match.lastgroup not in {"space", "comment"}]
    depth = {"(": 0, "[": 0, "<": 0}
    closing = {")": "(", "]": "[", ">": "<"}
    boundary = len(declaration)
    initializer = ""
    for token in tokens:
        value = token.group()
        if value in {"=", "{"} and not any(depth.values()):
            boundary = token.start()
            initializer = declaration[token.end() if value == "=" else token.start():].strip()
            break
        if value in depth:
            depth[value] += 1
        elif value in closing:
            key = closing[value]
            depth[key] = max(0, depth[key] - 1)
    name = row.name.split("::")[-1]
    matches = [token for token in tokens if token.group() == name and token.start() < boundary]
    if not matches:
        raise ValueError("Cannot locate variable name: " + row.name)
    identifier = matches[-1]
    type_name = declaration[:identifier.start()].rstrip() + declaration[identifier.end():boundary].rstrip()
    return name, type_name, initializer


def variable_table(rows, page, model, current_type, descriptions):
    output = ['<table class="variable-table">', '<thead><tr><th>Access</th><th>Name</th><th>Type</th>'
              '<th>Default</th><th>Description</th></tr></thead>', '<tbody>']
    for row in rows:
        name, type_name, initializer = variable_parts(row)
        description = descriptions.get(row.name, clean(row.summary))
        code = lambda text, limit=24: table_code(linked_code(text, page, model, current_type), limit)
        output.append('<tr><td>' + row.access + '</td><td>' + table_code(html.escape(name), 12) + '</td><td>'
                      + code(type_name) + '</td><td>' + (code(initializer, 16) if initializer else '—')
                      + '</td><td>' + prose(description, page, model, current_type) + '</td></tr>')
    output += ['</tbody>', '</table>']
    return "\n".join(output)


def constructor_section(rows, page, model, current_type, descriptions, deleted_operators=()):
    available = [row for row in rows if "= delete" not in row.signature]
    deleted = [row for row in rows if "= delete" in row.signature]
    output = [table(available, page, model, current_type, descriptions)] if available else []
    groups = set()
    own_name = current_type.split("::")[-1]
    for row in deleted:
        parsed = parameters(row.signature, own_name)
        arguments = parsed[2] if parsed else None
        if arguments == []:
            groups.add("Default")
        elif arguments and len(arguments) == 1:
            argument = arguments[0].split("=", 1)[0].strip()
            pattern = r"(?:const\s+)?" + re.escape(current_type) + r"\s*(&&?)(?:\s+\w+)?"
            match = re.fullmatch(pattern, argument)
            groups.add("Move" if match and match.group(1) == "&&" else "Copy" if match else "Other")
        else:
            groups.add("Other")
    messages = [kind + " constructors have been deleted." for kind in ("Default", "Copy", "Move")
                if kind in groups]
    if "Other" in groups:
        messages.append("Additional constructor overloads have been deleted.")
    for row in deleted_operators:
        if row.name.removeprefix(current_type + "::") != "operator=":
            continue
        parsed = parameters(row.signature, "operator=")
        if not parsed or len(parsed[2]) != 1:
            continue
        argument = parsed[2][0].strip()
        match = re.fullmatch(r"(?:const\s+)?" + re.escape(current_type) + r"\s*(&&?)(?:\s+\w+)?", argument)
        if match:
            kind = "Move" if match.group(1) == "&&" else "Copy"
            if kind == "Copy" and kind in groups:
                messages[messages.index("Copy constructors have been deleted.")] = (
                    "Copy constructor and assignment has been deleted.")
            else:
                messages.append(kind + " assignment has been deleted.")
    if messages:
        output.append(callout("important", " ".join(messages), page, model, current_type))
    return "\n\n".join(output)


def callable_anchor(name):
    if name.startswith("operator"):
        suffix = name.removeprefix("operator").strip()
        encoded = re.sub(r"[^\w\s]", lambda match: "-" + format(ord(match.group()), "x") + "-", suffix)
        return "operator-" + slug(encoded)
    return "function-" + slug(name)


def function_sections(entity, page, model, descriptions, operators=False):
    functions = [row for row in model.members(entity, "functions")
                 if row.name.removeprefix(entity.name + "::").startswith("operator") == operators
                 and not (operators and "= delete" in row.signature)]
    counts = Counter()
    anchors = {}
    methods = {}
    for row in functions:
        name = row.name.removeprefix(entity.name + "::")
        base = callable_anchor(name)
        counts[base] += 1
        anchor = base + "-" + str(counts[base])
        anchors[row] = anchor
        methods.setdefault(name, []).append((row.signature, anchor))
    result = []
    for access in ("public", "protected", "private"):
        members = [row for row in functions if row.access == access]
        if not members:
            continue
        result.append("### " + access.capitalize())
        grouped = {}
        for row in members:
            grouped.setdefault(row.name, []).append(row)
        members = [row for group in grouped.values() for row in group]
        previous_name = None
        for row in members:
            name = row.name.removeprefix(entity.name + "::")
            if name != previous_name:
                result.append("#### `" + name + "` {#" + callable_anchor(name) + "}")
                previous_name = name
            signature = format_function(row.signature.rstrip(";") + ";", name)
            result.append('<div class="function-declaration" id="' + anchors[row]
                          + '"><pre><code>' + linked_code(signature, page, model, entity.name,
                                                         row.name.removeprefix(entity.name + "::"))
                          + '</code></pre></div>')
            description = descriptions.get(row.name, clean(row.summary))
            if not description and "= delete" in row.signature:
                description = "Deleted; this operation cannot be called."
            if description:
                result.append(callout("warning", description, page, model, entity.name, methods)
                              if "= delete" in row.signature else
                              '<p>' + prose(description, page, model, entity.name, methods) + '</p>')
    if result:
        return ("## Operator Overloads\n\n" if operators else "") + "\n\n".join(result)
    return ""


def inheritance(entity, page, model):
    bases = model.bases(entity)
    output = []
    if bases:
        output.append("## Inheritance Hierarchy\n")
        for chain in model.ancestor_chains(entity):
            output.append(" → ".join(link(page, model.type_reference(name), "`" + name + "`")
                                     for name in chain[:-1]) + " → `" + entity.name + "`")
    derived = model.derived(entity)
    if derived:
        output += ["## Derived Classes\n", '<details><summary>Classes that use this as a base</summary>', '<ul>']
        for path, child in sorted(derived, key=lambda item: item[1].name.casefold()):
            target = model.type_reference(child.name)
            if target is None:
                target = model.header_reference(path)
            value = ('<a href="' + html.escape(href(page, target, True), quote=True) + '">'
                     + html.escape(child.name) + '</a>') if target is not None else html.escape(child.name)
            output.append('<li>' + value + '</li>')
        output += ['</ul>', '</details>']
    return "\n\n".join(output)
