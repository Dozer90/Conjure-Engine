"""Read source declarations for documentation, without expanding macros or copying function bodies.

This is a lexical inventory of the declarations written in a header, including
conditional branches. It does not infer inherited members or macro expansions.
"""

from __future__ import annotations

from collections import namedtuple
import re

Token = namedtuple("Token", "text start end")
Declaration = namedtuple("Declaration", "kind name signature access summary line defined definition",
                         defaults=[False, ""])
LEXER = re.compile(
    r'(?P<directive>^[ \t]*\#(?:\\\n|[^\n])*(?:\n|$))|(?P<space>[^\S\n]+|\n)'
    r'|(?P<comment>//[^\n]*|/\*.*?\*/)'
    r'|(?P<raw>(?:u8|u|U|L)?R"(?P<delimiter>[^ ()\\\t\r\n]{0,16})\(.*?\)(?P=delimiter)")'
    r'|(?P<string>(?:u8|u|U|L)?"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\')'
    r'|(?P<word>[A-Za-z_]\w*|\d+(?:\.\d+)?)|(?P<symbol>::|->|&&|\.\.\.|[^\s])',
    re.MULTILINE | re.DOTALL,
)


def inventory(text: str) -> list[Declaration]:
    """Inventory declarations in namespaces/types; skip statements inside function bodies."""
    tokens = [Token(m.group(), m.start(), m.end()) for m in LEXER.finditer(text)
              if m.lastgroup not in {"space", "directive", "comment"}]
    pairs = {}
    stack = []
    for index, token in enumerate(tokens):
        if token.text in {"{", "(", "["}:
            stack.append(index)
        elif token.text in {"}", ")", "]"}:
            expected = {"}": "{", ")": "(", "]": "["}[token.text]
            if stack and tokens[stack[-1]].text == expected:
                opening = stack.pop()
                pairs[opening] = index
    result = []

    def short(start, end):
        pieces = []
        for position in range(start, end):
            if position > start and tokens[position - 1].end < tokens[position].start:
                pieces.append(" ")
            pieces.append(tokens[position].text)
        return "".join(pieces).rstrip(";")

    def summary(boundary, start):
        comments = text[boundary:tokens[start].start]
        matches = list(re.finditer(r"<summary>(.*?)</summary>", comments, re.DOTALL))
        return matches[-1].group(1) if matches else ""

    def definition(start, closing):
        """Keep enum/union values and conditional branches exactly as written."""
        beginning = tokens[start].start
        indentation = text[text.rfind("\n", 0, beginning) + 1:beginning]
        lines = text[beginning:tokens[closing].end].splitlines()
        if not indentation.strip():
            lines[1:] = [line[len(indentation):] if line.startswith(indentation) else line for line in lines[1:]]
        return "\n".join(lines)

    def add(kind, name, start, end, owner, access, boundary, defined=False, body=""):
        qualified = "::".join((*owner, name))
        result.append(Declaration(kind, qualified, short(start, end), access,
                                  summary(boundary, start), text.count("\n", 0, tokens[start].start) + 1,
                                  defined, body))

    def head(start, end):
        """Find the declared entity after template/attribute prefixes."""
        index = start
        while index < end:
            if tokens[index].text == "[" and index in pairs:
                index = pairs[index] + 1
            elif tokens[index].text == "template" and index + 1 < end and tokens[index + 1].text == "<":
                index += 2
                depth = 1
                while index < end and depth:
                    depth += (tokens[index].text == "<") - (tokens[index].text == ">")
                    index += 1
            else:
                break
        if index < end and tokens[index].text == "requires":
            type_start = next((i for i in range(index + 1, end)
                               if tokens[i].text in {"struct", "class", "union", "enum"}), None)
            if type_start is not None:
                index = type_start
        return index

    def type_name(start, end):
        index = head(start, end)
        if index >= end or tokens[index].text not in {"struct", "class", "union", "enum"}:
            return None
        kind = tokens[index].text
        index += 1
        if index < end and tokens[index].text in {"class", "struct"}:
            index += 1
        while index < end and (tokens[index].text in {"CONJURE_API", "alignas"}
                               or tokens[index].text == "["):
            if tokens[index].text == "[" and index in pairs:
                index = pairs[index] + 1
            elif tokens[index].text == "alignas" and index + 1 in pairs:
                index = pairs[index + 1] + 1
            else:
                index += 1
        if index >= end or not re.fullmatch(r"[A-Za-z_]\w*", tokens[index].text):
            return kind, "(anonymous)"
        name_start = index
        while index < end:
            index += 1
            # Distinguish explicit/partial specialisations and qualified types.
            if index < end and tokens[index].text == "<":
                index += 1
                depth = 1
                while index < end and depth:
                    depth += (tokens[index].text == "<") - (tokens[index].text == ">")
                    index += 1
            if index + 1 < end and tokens[index].text == "::":
                index += 1
                continue
            break
        return kind, short(name_start, index)

    def function(start, end):
        index = head(start, end)
        constrained = index < end and tokens[index].text == "requires"
        operator = next((i for i in range(index, end) if tokens[i].text == "operator"), None)
        if operator is not None:
            opening = operator + 1
            if opening in pairs and tokens[opening].text in {"(", "["}:
                opening = pairs[opening] + 1
            while opening < end and tokens[opening].text != "(":
                opening += 1
            if opening < end:
                return short(operator, opening), pairs.get(opening, opening), opening
        while index < end:
            word = tokens[index].text
            if word == "=":
                return None
            if word == "[" and index in pairs:
                index = pairs[index] + 1
                continue
            if word == "(" and index in pairs:
                name = tokens[index - 1].text if index > start else ""
                if index + 1 < end and tokens[index + 1].text in {"*", "&", "&&"}:
                    return None
                if name in {"decltype", "alignas", "noexcept", "requires", "sizeof", "static_assert"}:
                    index = pairs[index] + 1
                    continue
                if re.fullmatch(r"[A-Za-z_]\w*", name):
                    if index >= 2 and tokens[index - 2].text == "~":
                        name = "~" + name
                    return name, pairs[index], index
                if constrained:
                    index = pairs[index] + 1
                    continue
                return None
            index += 1
        return None

    def variable_name(start, end):
        index = head(start, end)
        if index >= end:
            return None
        if tokens[index].text == "using":
            if index + 1 < end and tokens[index + 1].text != "namespace":
                return "types", tokens[index + 1].text
            return None
        if tokens[index].text == "concept" and index + 1 < end:
            return "types", tokens[index + 1].text
        if (tokens[index].text == "friend" and index + 1 < end
                and tokens[index + 1].text in {"struct", "class"}):
            return None
        if tokens[index].text == "typedef":
            pointer = next((i for i in range(index + 1, end - 1)
                            if tokens[i].text == "*" and tokens[i - 1].text == "("), None)
            if pointer is not None and re.fullmatch(r"[A-Za-z_]\w*", tokens[pointer + 1].text):
                return "types", tokens[pointer + 1].text
            names = [t.text for t in tokens[index + 1:end] if re.fullmatch(r"[A-Za-z_]\w*", t.text)]
            return ("types", names[-1]) if names else None
        if tokens[index].text in {"static_assert", "return", "namespace"}:
            return None
        # Ignore initialisers and array extents when finding the declarator name.
        stop = index
        angle = 0
        while stop < end:
            word = tokens[stop].text
            if not angle and word in {"=", "{", "["}:
                break
            if word == "<":
                angle += 1
            elif word == ">" and angle:
                angle -= 1
            if word == "(" and stop in pairs:
                # A pointer-to-function field: void (*callback)(args).
                inside = tokens[stop + 1:pairs[stop]]
                names = [t.text for t in inside if re.fullmatch(r"[A-Za-z_]\w*", t.text)]
                if any(t.text == "*" for t in inside) and names:
                    return "variables", names[-1]
                stop = pairs[stop]
            stop += 1
        names = [t.text for t in tokens[index:stop] if re.fullmatch(r"[A-Za-z_]\w*", t.text)]
        return ("variables", names[-1]) if len(names) >= 2 else None

    def scope(start, end, owner=(), default_access="public", boundary=0, namespace_depth=0):
        index = start
        access = default_access
        while index < end:
            if tokens[index].text == ";":
                boundary = tokens[index].end
                index += 1
                continue
            if index + 1 < end and tokens[index].text in {"public", "private", "protected"}:
                if tokens[index + 1].text == ":":
                    access = tokens[index].text
                    boundary = tokens[index + 1].end
                    index += 2
                    continue
            # Source macros without a terminating semicolon remain distinct entries;
            # their generated members cannot be inferred from the invocation alone.
            if (tokens[index].text.isupper() and index + 1 in pairs and tokens[index + 1].text == "("
                    and (not owner or owner[-1] != tokens[index].text)):
                closing = pairs[index + 1]
                add("types", tokens[index].text, index, closing + 1, owner, access, boundary)
                boundary = tokens[closing].end
                index = closing + 1
                continue
            start = index
            cursor = index
            declaration_end = None
            while cursor < end:
                word = tokens[cursor].text
                if word in {"(", "["} and cursor in pairs:
                    cursor = pairs[cursor] + 1
                    continue
                if word == ";":
                    declaration_end = cursor
                    break
                if word == "{" and cursor in pairs:
                    prefix = [t.text for t in tokens[head(start, cursor):cursor]]
                    closing = pairs[cursor]
                    if "namespace" in prefix or (prefix and prefix[0] == "extern"):
                        names = []
                        if "namespace" in prefix:
                            names = [word for word in prefix[prefix.index("namespace") + 1:]
                                     if re.fullmatch(r"[A-Za-z_]\w*", word)]
                            if namespace_depth == 0:
                                names = names[2:] if names and names[0] == "conjure" else []
                        scope(cursor + 1, closing, (*owner, *names), access,
                              tokens[cursor].end, namespace_depth + 1)
                    elif (kind := type_name(start, cursor)) is not None:
                        type_kind, name = kind
                        body = definition(start, closing) if type_kind in {"enum", "union"} else ""
                        add("types", name, start, cursor, owner, access, boundary, defined=True, body=body)
                        if type_kind == "enum":
                            enum_start = cursor + 1
                            position = enum_start
                            while position < closing:
                                if tokens[position].text in {"(", "[", "{"} and position in pairs:
                                    position = pairs[position] + 1
                                    continue
                                if tokens[position].text == ",":
                                    if enum_start < position:
                                        add("variables", tokens[enum_start].text, enum_start, position,
                                            (*owner, name), access, tokens[enum_start].start)
                                    enum_start = position + 1
                                position += 1
                            if enum_start < closing:
                                add("variables", tokens[enum_start].text, enum_start, closing,
                                    (*owner, name), access, tokens[enum_start].start)
                        else:
                            scope(cursor + 1, closing, (*owner, name),
                                  "private" if type_kind == "class" else "public",
                                  tokens[cursor].end, namespace_depth)
                    elif (method := function(start, cursor)) is not None:
                        name, parameters_end, _ = method
                        # A constructor's member initialisers are not part of its signature.
                        colon = next((i for i in range(parameters_end + 1, cursor)
                                      if tokens[i].text == ":"), None)
                        signature_end = colon if colon is not None else cursor
                        if colon is not None:
                            # Skip each braced/parenthesised member initialiser to reach the body.
                            position = colon + 1
                            while position < end:
                                if tokens[position].text in {"(", "{"} and position in pairs:
                                    position = pairs[position] + 1
                                    if position < end and tokens[position].text == ",":
                                        position += 1
                                        continue
                                    if position < end and tokens[position].text == "{":
                                        closing = pairs[position]
                                    break
                                position += 1
                        base = re.sub(r"<.*", "", owner[-1]) if owner else ""
                        category = "constructors" if base and name.lstrip("~") == base else "functions"
                        add(category, name, start, signature_end, owner, access, boundary)
                    else:
                        cursor = closing + 1
                        continue
                    index = closing + 1
                    boundary = tokens[closing].end
                    break
                cursor += 1
            else:
                index = end
                continue
            if declaration_end is not None:
                kind = type_name(start, declaration_end)
                method = function(start, declaration_end)
                if kind:
                    add("types", kind[1], start, declaration_end, owner, access, boundary)
                elif method:
                    name = method[0]
                    base = re.sub(r"<.*", "", owner[-1]) if owner else ""
                    category = "constructors" if base and name.lstrip("~") == base else "functions"
                    add(category, name, start, declaration_end, owner, access, boundary)
                elif (variable := variable_name(start, declaration_end)) is not None:
                    add(variable[0], variable[1], start, declaration_end, owner, access, boundary)
                index = declaration_end + 1
                boundary = tokens[declaration_end].end

    scope(0, len(tokens))
    return list(dict.fromkeys(result))
