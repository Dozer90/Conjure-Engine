#!/usr/bin/env python3
"""Prepare readable documentation assets without replacing existing customisations.

    python tools/docs.py assets           # create missing editable source assets
    python tools/docs.py assets --format  # format existing CSS and JavaScript as well

Edit docs/assets/stylesheets/ and docs/assets/javascripts/, then build or serve.
The output under site/ is disposable; source assets survive every rebuild.

Only the four files the site loads are imported from Material: its stylesheet, its palette, its bundle and its
search worker. Their licences are in docs/assets/licenses/ and registered in third_party/CMakeLists.txt. The lunr
language and segmenter files are not imported, so search stays English (check_search_languages), and the bundle's
focus-visible polyfill is dropped on import (remove_focus_visible_polyfill).
"""

from __future__ import annotations

import argparse
import importlib.metadata
import pathlib
import re
import sys

import cssbeautifier
import jsbeautifier
import material

ROOT = pathlib.Path(__file__).resolve().parent.parent
THEME_ASSETS = pathlib.Path(material.__file__).resolve().parent / "templates/assets"
# The editable copy of each imported Material asset, relative to docs/assets. Nothing else is imported: a new entry
# needs its licence in docs/assets/licenses/ and a notice in third_party/CMakeLists.txt first.
EDITABLE_NAMES = {
    "main_css": "stylesheets/material-theme.css",
    "palette_css": "stylesheets/material-palette.css",
    "main_js": "javascripts/material-theme.js",
    "search_js": "javascripts/workers/search-worker.js",
}
# The Material version the editable copies were imported from. The build stops when another version is installed,
# because the new templates would run against the old bundle and stylesheet.
VERSION_STAMP = "material-version.txt"
# Languages the search worker can index without the lunr language files, which are not imported.
SEARCH_LANGUAGES = frozenset({"en"})
# Material's bundle carries the WICG focus-visible polyfill, whose licence (the W3C Software and Document License)
# would have to ship in full with it. Every browser the site supports matches the :focus-visible pseudo-class, so the
# import drops the polyfill and its stylesheet uses the pseudo-class instead of the class the polyfill set.
FOCUS_VISIBLE_MARKER = "focus-visible-polyfill-ready"


def theme_files(directory: pathlib.Path) -> dict[str, pathlib.Path]:
    patterns = {
        "main_css": "stylesheets/main.*.min.css",
        "palette_css": "stylesheets/palette.*.min.css",
        "main_js": "javascripts/bundle.*.min.js",
        "search_js": "javascripts/workers/search.*.min.js",
    }
    result = {}
    for name, pattern in patterns.items():
        matches = sorted(directory.glob(pattern))
        if len(matches) != 1:
            raise ValueError(f"Expected one Material theme asset for {pattern}, found {len(matches)}")
        result[name] = matches[0]
    return result


def format_asset(text: str, suffix: str) -> str:
    # The distributed source maps describe minified locations and no longer apply after formatting.
    text = re.sub(r"(?m)^\s*//# sourceMappingURL=[^\n]*\n?", "", text)
    text = re.sub(r"/\*# sourceMappingURL=.*?\*/", "", text, flags=re.DOTALL)
    formatter = cssbeautifier if suffix == ".css" else jsbeautifier
    options = formatter.default_options()
    options.indent_size = 4
    options.indent_with_tabs = False
    options.eol = "\n"
    options.end_with_newline = True
    options.max_preserve_newlines = 2
    options.wrap_line_length = 120
    # Nested chained calls in bundled libraries can need more than one indentation pass.
    # Settle the result here so repeated user formatting is deterministic.
    for _ in range(8):
        formatted = formatter.beautify(text, options)
        if formatted == text:
            return formatted
        text = formatted
    raise ValueError("Asset formatting did not settle after eight passes")


def rewrite_worker_languages(text: str) -> str:
    # Only local Lunr imports change. External URLs, such as mermaid.min.js, stay intact.
    return re.sub(r"/min/(lunr\.[^`\"\s]+?)\.min\.js", r"/languages/\1.js", text)


def remove_focus_visible_polyfill(text: str) -> str:
    """Drops the focus-visible polyfill module and its one use from the formatted bundle."""
    if FOCUS_VISIBLE_MARKER not in text:
        return text
    # A module of the bundle is a top-level statement, four spaces in, whose body is indented further.
    modules = [match for match in re.finditer(r"(?ms)^    var (\w+) = \w+\(\(\w+, \w+\) => \{\n"
                                              r"(?:(?!^    \S).)*?^    \}\);\n", text)
               if FOCUS_VISIBLE_MARKER in match.group(0)]
    unreadable = ValueError("Material's bundle holds the focus-visible polyfill in a form tools/docs_assets.py "
                            "cannot remove; remove it from docs/assets/javascripts/material-theme.js by hand "
                            "(docs/assets/README.txt)")
    if len(modules) != 1:
        raise unreadable
    module = modules[0]
    text = text[:module.start()] + text[module.end():]
    name = re.escape(module.group(1))
    text, uses = re.subn(r"(?m)^    var \w+ = \w+\(" + name + r"\(\)\);\n", "", text)
    if uses != 1 or FOCUS_VISIBLE_MARKER in text or re.search(r"\b" + name + r"\b", text):
        raise unreadable
    return text


def use_focus_visible_pseudo_class(text: str) -> str:
    """Matches the focus-visible pseudo-class where the stylesheet matched the polyfill's class."""
    return re.sub(r"\.focus-visible(?![\w-])", ":focus-visible", text)


def check_search_languages(languages: list[str] | None) -> None:
    unsupported = sorted(set(languages or ()) - SEARCH_LANGUAGES)
    if unsupported:
        raise ValueError(f"Search language(s) {', '.join(unsupported)} need lunr language files, which "
                         "docs/assets does not vendor; add them with their licences first (docs/assets/README.txt)")


def check_version_stamp(assets: pathlib.Path, version: str) -> None:
    stamp = assets / VERSION_STAMP
    if not stamp.is_file():
        stamp.parent.mkdir(parents=True, exist_ok=True)
        stamp.write_text(version + "\n", encoding="utf-8", newline="\n")
        return
    imported = stamp.read_text(encoding="utf-8").strip()
    if imported != version:
        raise ValueError(f"docs/assets holds the theme files of mkdocs-material {imported}, but {version} is "
                         f"installed. Move your edits of {', '.join(EDITABLE_NAMES.values())} aside, delete those "
                         f"files and docs/assets/{VERSION_STAMP}, run python tools/docs.py assets, then merge the "
                         "edits back (docs/assets/README.txt)")


def prepare_assets(root: pathlib.Path, theme_root: pathlib.Path = THEME_ASSETS,
                   version: str | None = None) -> tuple[dict[str, str], int]:
    primary = theme_files(theme_root)
    assets = root / "docs/assets"
    distribution = importlib.metadata.distribution("mkdocs-material")
    check_version_stamp(assets, version if version is not None else distribution.version)
    created = 0
    for name, source in primary.items():
        destination = assets / EDITABLE_NAMES[name]
        if not destination.resolve().is_relative_to(root.resolve()):
            raise ValueError(f"Asset destination is outside the repository: {destination}")
        if destination.is_file():
            continue
        text = source.read_text(encoding="utf-8")
        if name == "search_js":
            text = rewrite_worker_languages(text)
        text = format_asset(text, source.suffix)
        if name == "main_js":
            text = remove_focus_visible_polyfill(text)
        elif name == "main_css":
            text = use_focus_visible_pseudo_class(text)
        destination.parent.mkdir(parents=True, exist_ok=True)
        # Exclusive creation protects a concurrently edited source file too.
        with destination.open("x", encoding="utf-8", newline="\n") as output:
            output.write(text)
        created += 1
    license_source = next(path for path in distribution.files if str(path).endswith("/licenses/LICENSE"))
    license_destination = assets / "licenses/mkdocs-material.txt"
    if not license_destination.exists():
        license_destination.parent.mkdir(parents=True, exist_ok=True)
        license_destination.write_text(distribution.locate_file(license_source).read_text(encoding="utf-8"),
                                       encoding="utf-8", newline="\n")
    mapping = {name: "assets/" + path.relative_to(theme_root).as_posix() for name, path in primary.items()}
    return mapping, created


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--format", action="store_true", help="also reformat existing source CSS and JavaScript")
    args = parser.parse_args(argv)
    try:
        _, created = prepare_assets(ROOT)
        formatted = 0
        if args.format:
            for directory in (ROOT / "docs/assets/stylesheets", ROOT / "docs/assets/javascripts"):
                for path in sorted(directory.rglob("*")):
                    if path.suffix not in {".css", ".js"} or not path.is_file():
                        continue
                    original = path.read_text(encoding="utf-8")
                    text = format_asset(original, path.suffix)
                    if original != text:
                        path.write_text(text, encoding="utf-8", newline="\n")
                        formatted += 1
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print(f"docs.assets: {created} source asset(s) created, {formatted} existing file(s) formatted")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
