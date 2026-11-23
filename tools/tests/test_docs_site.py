#!/usr/bin/env python3
"""Rendering and real MkDocs build regressions; run with the docs toolchain."""

from __future__ import annotations

import html
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

import markdown
import yaml
from admonitions.admonition import AdmonitionConverter

TOOLS = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(TOOLS))
sys.path.append(str(TOOLS / "tests"))
from docs_site import DocumentationExtension
import docs_assets
import docs_policy
import nav_outline


class AssetTests(unittest.TestCase):
    def test_css_and_javascript_are_formatted_without_changing_literals(self):
        css = 'a:hover{content:";{}";background:url("data:image/svg+xml;utf8,<svg>{x}</svg>");color:red}'
        formatted = docs_assets.format_asset(css, ".css")
        self.assertIn("\n    color: red", formatted)
        self.assertIn('content: ";{}"', formatted)
        self.assertIn('url("data:image/svg+xml;utf8,<svg>{x}</svg>")', formatted)
        self.assertEqual(docs_assets.format_asset(formatted, ".css"), formatted)
        javascript = ('/*! Keep this licence. */\nfunction f(){return /a{2}/.test("aa")}\n'
                      '//# sourceMappingURL=x.js.map\n')
        formatted = docs_assets.format_asset(javascript, ".js")
        self.assertIn("Keep this licence.", formatted)
        self.assertIn("\n    return /a{2}/", formatted)
        self.assertNotIn("sourceMappingURL", formatted)
        self.assertEqual(docs_assets.format_asset(formatted, ".js"), formatted)

    def test_language_imports_change_without_changing_external_urls(self):
        original = ('importScripts(`${e}/min/lunr.${language}.min.js`,`${e}/min/lunr.multi.min.js`);'
                    'const remote="https://unpkg.com/mermaid@11/dist/mermaid.min.js";')
        rewritten = docs_assets.rewrite_worker_languages(original)
        self.assertIn("/languages/lunr.${language}.js", rewritten)
        self.assertIn("/languages/lunr.multi.js", rewritten)
        self.assertIn("https://unpkg.com/mermaid@11/dist/mermaid.min.js", rewritten)

    def test_nested_language_library_formatting_is_repeatable(self):
        source = docs_assets.THEME_ASSETS / "javascripts/lunr/min/lunr.fr.min.js"
        formatted = docs_assets.format_asset(source.read_text(encoding="utf-8"), ".js")
        self.assertEqual(docs_assets.format_asset(formatted, ".js"), formatted)

    def test_preparation_keeps_existing_authored_assets(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            theme = root / "theme"
            for name, text in {
                "stylesheets/main.12345678.min.css": "a{color:red}",
                "stylesheets/palette.12345678.min.css": "a{color:blue}",
                "javascripts/bundle.12345678.min.js": "function f(){return 1;}",
                "javascripts/workers/search.12345678.min.js": 'importScripts(`${e}/min/lunr.fr.min.js`);',
                "javascripts/lunr/min/lunr.fr.min.js": "var locale='fr';",
            }.items():
                path = theme / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(text, encoding="utf-8")
            editable = root / "docs/assets/stylesheets/material-theme.css"
            editable.parent.mkdir(parents=True)
            editable.write_text("/* My theme edit. */\na { color: green; }\n", encoding="utf-8")
            mapping, created = docs_assets.prepare_assets(root, theme, version="9.9.9")
            self.assertEqual(created, 3)
            self.assertIn("My theme edit.", editable.read_text(encoding="utf-8"))
            self.assertEqual(mapping["main_css"], "assets/stylesheets/main.12345678.min.css")
            self.assertTrue((root / "docs/assets/javascripts/workers/search-worker.js").is_file())
            # Only the four theme files are vendored, never the lunr language files.
            self.assertFalse((root / "docs/assets/javascripts/lunr").exists())
            self.assertEqual((root / "docs/assets/material-version.txt").read_text(encoding="utf-8"), "9.9.9\n")
            _, created = docs_assets.prepare_assets(root, theme, version="9.9.9")
            self.assertEqual(created, 0)
            self.assertIn("My theme edit.", editable.read_text(encoding="utf-8"))
            # Another installed Material would render its templates against the old bundle.
            with self.assertRaisesRegex(ValueError, "mkdocs-material 9.9.9, but 10.0.0 is installed"):
                docs_assets.prepare_assets(root, theme, version="10.0.0")

    def test_search_languages_need_vendored_language_files(self):
        docs_assets.check_search_languages(["en"])
        docs_assets.check_search_languages(None)
        with self.assertRaisesRegex(ValueError, "de, fr need lunr language files"):
            docs_assets.check_search_languages(["en", "fr", "de"])

    def test_vendored_assets_are_the_licensed_ones(self):
        root = TOOLS.parent
        assets = root / "docs/assets"
        # Every file under docs/assets is Conjure's own (brand/, diagrams/ and the names below) or a vendored file
        # with its notice in third_party/CMakeLists.txt.
        own = {"README.txt", docs_assets.VERSION_STAMP, "stylesheets/conjure-docs.css", "javascripts/conjure-docs.js",
               "javascripts/handbook-redirects.js"}
        icons = {"callout-caution.svg", "callout-note.svg", "callout-tip.svg", "callout-warning.svg"}
        licences = {"licenses/" + path.name for path in (assets / "licenses").iterdir()}
        present = {path.relative_to(assets).as_posix() for path in assets.rglob("*") if path.is_file()}
        present = {name for name in present if not name.startswith(("brand/", "diagrams/"))}
        self.assertEqual(present, own | licences | {"icons/" + name for name in icons}
                         | set(docs_assets.EDITABLE_NAMES.values()))
        version = (assets / docs_assets.VERSION_STAMP).read_text(encoding="utf-8").strip()
        requirements = (root / "tools/docs_requirements.txt").read_text(encoding="utf-8")
        self.assertRegex(requirements, r"(?m)^mkdocs-material==" + re.escape(version) + r"$")
        notices = (root / "third_party/CMakeLists.txt").read_text(encoding="utf-8")
        self.assertRegex(notices, r'conjure_third_party_notice\(mkdocs_material\s+NAME "Material for MkDocs" '
                         r'VERSION ' + re.escape(version) + r'\s')
        for licence in licences:
            self.assertIn("../docs/assets/" + licence, notices)
        icon_notice = re.search(r"(?s)conjure_third_party_notice\(material_design_icons\b.*?\n\)", notices)
        self.assertIsNotNone(icon_notice)
        for name in icons:
            self.assertIn(name, icon_notice.group(0))
        # The polyfill's licence text is not shipped, so neither is the polyfill (remove_focus_visible_polyfill).
        bundle = (assets / docs_assets.EDITABLE_NAMES["main_js"]).read_text(encoding="utf-8")
        stylesheet = (assets / docs_assets.EDITABLE_NAMES["main_css"]).read_text(encoding="utf-8")
        self.assertNotIn(docs_assets.FOCUS_VISIBLE_MARKER, bundle)
        self.assertNotRegex(stylesheet, r"\.focus-visible(?![\w-])")

    def test_focus_visible_polyfill_is_left_out_of_an_import(self):
        polyfill = ('var lo=xr((Er,po)=>{(function(e,t){t()})(Er,function(){"use strict";function e(r){'
                    'r.addEventListener("focus",function(k){k.target.classList.add("focus-visible")},!0)}'
                    'window.dispatchEvent(new CustomEvent("focus-visible-polyfill-ready"));e(document)})});')
        bundle = ('"use strict";(()=>{var xr=(e,t)=>()=>(t||e((t={exports:{}}).exports,t),t.exports);'
                  + polyfill + 'var qr=xr((dy,On)=>{On.exports=function(){return 1}});var pL=Lt(lo());'
                  'function f(){return qr()}})();')
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            theme = root / "theme"
            for name, text in {
                "stylesheets/main.12345678.min.css": ".md-nav__link.focus-visible,.md-clipboard:not(.focus-visible)"
                                                     ",.js-focus-visible a{color:red}",
                "stylesheets/palette.12345678.min.css": "a{color:blue}",
                "javascripts/bundle.12345678.min.js": bundle,
                "javascripts/workers/search.12345678.min.js": "function g(){return 2;}",
            }.items():
                path = theme / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(text, encoding="utf-8")
            docs_assets.prepare_assets(root, theme, version="9.9.9")
            imported = (root / "docs/assets/javascripts/material-theme.js").read_text(encoding="utf-8")
            self.assertNotIn("focus-visible", imported)
            self.assertNotIn("lo()", imported)
            self.assertIn("var qr = xr((dy, On) => {", imported)
            self.assertIn("return qr()", imported)
            stylesheet = (root / "docs/assets/stylesheets/material-theme.css").read_text(encoding="utf-8")
            self.assertIn(".md-nav__link:focus-visible,", stylesheet)
            self.assertIn(".md-clipboard:not(:focus-visible),", stylesheet)
            self.assertIn(".js-focus-visible a", stylesheet)
        # A bundle whose polyfill is still used elsewhere cannot be cut cleanly, so the import stops.
        formatted = docs_assets.format_asset(bundle.replace("function f(){", "function f(){lo();"), ".js")
        with self.assertRaisesRegex(ValueError, "remove it from docs/assets/javascripts/material-theme.js by hand"):
            docs_assets.remove_focus_visible_polyfill(formatted)
        self.assertEqual(docs_assets.remove_focus_visible_polyfill("function f(){}\n"), "function f(){}\n")

    def test_nav_outline_reads_the_nav_as_yaml_does(self):
        # The standard-library suites read the generated nav with nav_outline instead of PyYAML.
        loader = type("NavLoader", (yaml.SafeLoader,), {})
        loader.add_multi_constructor("tag:yaml.org,2002:python/", lambda *_: None)
        text = (TOOLS.parent / "mkdocs.yml").read_text(encoding="utf-8")
        self.assertEqual(nav_outline.nav_tree(text), yaml.load(text, Loader=loader)["nav"])
        with self.assertRaises(ValueError):
            nav_outline.nav_tree("nav:\n  - [flow, sequence]\n")


def copy_site_tools(root):
    """Copies the MkDocs hook, the modules it imports and the theme overrides into a fixture repository."""
    (root / "tools").mkdir(parents=True, exist_ok=True)
    for filename in ("check_docs.py", "docs_site.py", "docs_assets.py", "docs_policy.py", "docs_components.py",
                     "docs_redirects.py"):
        shutil.copyfile(TOOLS / filename, root / "tools" / filename)
    shutil.copytree(TOOLS / "docs_theme", root / "tools/docs_theme")


def site_settings():
    """The settings of mkdocs.yml without its nav, which is last."""
    return (TOOLS.parent / "mkdocs.yml").read_text(encoding="utf-8").split("\nnav:\n", 1)[0]


def build_site(root):
    return subprocess.run([sys.executable, "-m", "mkdocs", "build", "--strict"], cwd=root, capture_output=True,
                          text=True, encoding="utf-8")


class RenderingTests(unittest.TestCase):
    def build_one_page_site(self, root, settings):
        copy_site_tools(root)
        (root / "mkdocs.yml").write_text(settings + "\nnav:\n  - Home: index.md\n", encoding="utf-8")
        (root / "docs").mkdir(exist_ok=True)
        (root / "docs/index.md").write_text("# Home\n", encoding="utf-8")
        return build_site(root)

    def test_strict_build_stops_for_a_search_language_without_files(self):
        settings = site_settings()
        self.assertIn("\nplugins:\n  - search\n", settings)
        settings = settings.replace("\nplugins:\n  - search\n", "\nplugins:\n  - search:\n      lang: [en, fr]\n")
        with tempfile.TemporaryDirectory() as temporary:
            result = self.build_one_page_site(pathlib.Path(temporary), settings)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Search language(s) fr need lunr language files", result.stdout + result.stderr)
        # The hook reports the problem as a plugin error, not as a crash.
        self.assertNotIn("Traceback", result.stdout + result.stderr)

    def test_strict_build_stops_for_theme_files_of_another_material(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            stamp = root / "docs/assets" / docs_assets.VERSION_STAMP
            stamp.parent.mkdir(parents=True)
            stamp.write_text("0.0.1\n", encoding="utf-8")
            result = self.build_one_page_site(root, site_settings())
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("mkdocs-material 0.0.1, but", result.stdout + result.stderr)
        self.assertNotIn("Traceback", result.stdout + result.stderr)

    def test_callout_commands_keep_their_code_block_after_cleanup(self):
        source = ("> [!TIP]\n> Useful variants of the one command:\n>\n> ```bash\n"
                  "> python tools/build.py --test\n> python tools/build.py --no-configure\n> ```")
        cleaned = docs_policy.sanitize(source)
        converted = AdmonitionConverter().on_page_markdown(cleaned + "\n", page=None, config=None, files=None)
        output = markdown.markdown(converted, extensions=["admonition", "pymdownx.superfences"])
        self.assertIn('class="admonition tip"', output)
        self.assertIn("<pre", output)
        self.assertIn("<code", output)
        # Ignore syntax-highlighting spans when checking the literal command lines.
        literal = html.unescape(re.sub(r"<[^>]+>", "", output))
        self.assertIn("python tools/build.py --test\npython tools/build.py --no-configure", literal)
        self.assertNotIn("bash python", literal)

    def test_function_headings_toggle_overloads_and_remain_in_contents(self):
        source = ('### Public\n\n#### `play` {#function-play}\n\n'
                  '<div class="function-declaration" id="function-play-1">'
                  '<pre><code>void play();</code></pre></div>\n\n'
                  '<p>Starts playback.</p>\n\n#### `stop` {#function-stop}\n\n'
                  '<div class="function-declaration" id="function-stop-1"><pre><code>void stop();</code></pre></div>')
        renderer = markdown.Markdown(extensions=["toc", "attr_list", DocumentationExtension()])
        output = renderer.convert(source)
        self.assertRegex(output, r'<summary>\s*<h4 id="function-play">')
        self.assertEqual(output.count('<details class="function-group">'), 2)
        self.assertEqual(renderer.toc.count('href="#function-play"'), 1)
        self.assertNotRegex(output, r'<details\b[^>]*\bopen\b')

    def render(self, source):
        return markdown.markdown(source, extensions=["tables", "fenced_code", DocumentationExtension()])

    def test_metadata_tables_and_clickable_code_signatures(self):
        source = ("|---|---|\n| Name | Widget |\n"
                  "| Function | `[result](result.md)<T> [f()](#f)([args](args.md))` |\n")
        output = self.render(source)
        self.assertIn('<table class="doc-metadata">', output)
        self.assertNotIn("<thead>", output)
        self.assertIn('<code><a href="result.md">result</a>&lt;T&gt;', output)
        self.assertIn('<a href="#f">f()</a>', output)
        self.assertIn('<a href="args.md">args</a>', output)
        self.assertNotIn("&amp;lt;", output)

    def test_regular_tables_and_literal_examples_remain_literal(self):
        source = ("`[x](literal.md)`\n\n```markdown\n|---|---|\n| A | B |\n```\n\n"
                  "| Name | Value |\n|---|---|\n| X | Y |\n")
        output = self.render(source)
        self.assertIn("<thead>", output)
        self.assertIn("<code>[x](literal.md)</code>", output)
        self.assertNotIn('href="literal.md"', output)
        self.assertIn("|---|---|", output)

    def test_all_alert_kinds_render_as_boxes(self):
        plugin = AdmonitionConverter()
        for kind in ("NOTE", "TIP", "IMPORTANT", "WARNING", "CAUTION"):
            source = f"> [!{kind}]\n> Context.\n"
            converted = plugin.on_page_markdown(source, page=None, config=None, files=None)
            output = markdown.markdown(converted, extensions=["admonition"])
            self.assertIn('class="admonition ', output)
            self.assertIn(kind.title(), output)

    def test_strict_site_build_output_links_and_diagrams(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)

            def write(name, text):
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(text, encoding="utf-8")

            copy_site_tools(root)
            config = site_settings()
            config += ("\nnav:\n  - API: api/widgets/widget.md\n"
                       "  - Engineering handbook:\n      - Engineering Handbook:\n"
                       "          - handbook/index.md\n          - Getting Started:\n"
                       "              - handbook/getting-started/index.md\n"
                       "              - Setup: handbook/getting-started/setup.md\n      - Engine:\n"
                       "          - handbook/engine.md\n          - Widgets:\n"
                       "              - handbook/engine/widgets.md\n              - widget.h:\n"
                       "                  - handbook/engine/widgets/widget.md\n"
                       "                  - widget: handbook/engine/widgets/widget/widget.md\n"
                       "      - Editor:\n          - handbook/editor.md\n          - Protocol:\n"
                       "              - handbook/editor/protocol.md\n              - access_token.h:\n"
                       "                  - handbook/editor/editor/protocol/access-token.md\n"
                       "                  - token: handbook/editor/editor/protocol/access-token/token.md\n")
            write("mkdocs.yml", config)
            write("docs/assets/stylesheets/conjure-docs.css", "/* Fixture styles. */\n")
            write("docs/assets/javascripts/handbook-redirects.js", (TOOLS.parent /
                  "docs/assets/javascripts/handbook-redirects.js").read_text(encoding="utf-8"))
            shutil.copytree(TOOLS.parent / "docs/assets/brand", root / "docs/assets/brand")
            write("docs/handbook/index.md", "# Handbook\n")
            write("docs/handbook/getting-started/index.md", "# Getting Started\n")
            write("docs/handbook/getting-started/setup.md", "# Setup\n")
            write("docs/handbook/old-widget.md", '---\nredirect_to: engine/widgets/widget.md\n'
                  'redirect_fragments: {"old-constructor":"engine/widgets/widget.md#widget-internals"}\n'
                  'search: {exclude: true}\n---\n# Moved widget\n\n'
                  '[Widget](engine/widgets/widget.md)\n')
            write("docs/handbook/engine.md", "# Engine\n")
            write("docs/handbook/engine/widgets.md", "# Widgets\n")
            write("docs/handbook/engine/widgets/widget.md", "# Widget internals\n")
            write("docs/handbook/engine/widgets/widget/widget.md", "# widget\n")
            write("docs/handbook/editor.md", "# Editor\n")
            write("docs/handbook/editor/protocol.md", "# Protocol\n")
            write("docs/handbook/editor/editor/protocol/access-token.md", "# Access token\n")
            write("docs/handbook/editor/editor/protocol/access-token/token.md", "# token\n")
            source = ("# Widget\n\n|---|---|\n| Name | Widget |\n"
                      "| Handbook | [Internals](../../handbook/engine/widgets/widget.md) |\n\n"
                      "## Functions\n\n| Signature | Purpose |\n|---|---|\n"
                      "| `[f()](widget#function-f)` | Runs f |\n\n## Function f\n\n"
                      "[Handbook directory](../../handbook)\n\n"
                      "> [!CAUTION]\n> Data loss.\n\n```mermaid\ngraph LR\n  A --> B\n```\n")
            write("docs/api/widgets/widget.md", source)
            result = build_site(root)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            target = root / "site/api/widgets/widget.html"
            self.assertTrue(target.is_file())
            self.assertFalse((root / "site/api/widgets/widget/index.html").exists())
            output = target.read_text(encoding="utf-8")
            editor_output = (root / "site/handbook/editor/editor/protocol/access-token.html").read_text(
                encoding="utf-8").split("<article", 1)[0]
            self.assertNotIn('id="__nav_2_label"', editor_output)
            self.assertIn("doc-scope-navigation", editor_output)
            self.assertEqual(len(re.findall(r'class="[^"]*\bdoc-primary-group\b', editor_output)), 3)
            self.assertNotRegex(editor_output, r'class="[^"]*md-nav__item--section[^\n]*doc-primary-group')
            for position in (1, 2, 3):
                self.assertIn(f'id="__nav_2_{position}"', editor_output)
                self.assertRegex(editor_output, f'for="__nav_2_{position}" id="__nav_2_{position}_label" tabindex="0"')
            redirect_output = (root / "site/handbook/old-widget.html").read_text(encoding="utf-8")
            self.assertIn('name="doc-redirect" content="engine/widgets/widget.html"', redirect_output)
            self.assertIn('engine/widgets/widget.html#widget-internals', redirect_output)
            self.assertIn('assets/javascripts/handbook-redirects.js', redirect_output)
            # Each extra script of mkdocs.yml loads once (the fixture keeps its settings above the nav).
            for page in (redirect_output, (root / "site/api/widgets/widget.html").read_text(encoding="utf-8")):
                for script in ("conjure-docs.js", "handbook-redirects.js"):
                    self.assertEqual(len(re.findall(r'<script src="[^"]*assets/javascripts/' + re.escape(script)
                                                    + '"', page)), 1, script)
            self.assertRegex(editor_output, r'<div class="md-nav__link md-nav__container">\s*'
                             r'<a href="../../protocol.html"[^>]*>\s*<span class="md-ellipsis">\s*Protocol')
            self.assertEqual(editor_output.count('<a href="../../protocol.html"'), 1)
            navigation = output.split("<article", 1)[0]
            for label, url in (("Engine", "../../handbook/engine.html"),
                               ("Widgets", "../../handbook/engine/widgets.html"),
                               ("widget.h", "../../handbook/engine/widgets/widget.html")):
                # The parent label is the link; the landing page is not a duplicate child.
                pattern = (r'<div class="md-nav__link md-nav__container">\s*<a href="'
                           + re.escape(url) + r'"[^>]*>\s*<span class="md-ellipsis">\s*'
                           + re.escape(label) + r'\s*</span>')
                self.assertRegex(navigation, pattern)
                self.assertEqual(navigation.count('href="' + url + '"'), 1)
            self.assertNotRegex(navigation, r'<span class="md-ellipsis">\s*Widget internals\s*</span>')
            self.assertNotRegex(navigation, r'<span class="md-ellipsis">\s*Headers\s*</span>')
            self.assertIn('href="../../handbook/engine/widgets/widget/widget.html"', navigation)
            article = output.split('<article class="md-content__inner md-typeset">', 1)[1].split("</article>", 1)[0]
            self.assertIn('<code><a href="widget.html#function-f">f()</a></code>', article)
            self.assertIn('href="../../handbook/index.html"', output)
            self.assertIn("../../handbook/engine/widgets/widget.html", output)
            self.assertIn('class="mermaid"', output)
            self.assertIn('class="admonition caution"', output)
            self.assertIn('class="doc-metadata"', output)
            for asset in ("stylesheets/material-theme.css", "stylesheets/material-palette.css",
                          "stylesheets/conjure-docs.css", "javascripts/material-theme.js",
                          "javascripts/workers/search-worker.js"):
                self.assertIn("assets/" + asset, output)
                self.assertTrue((root / "site/assets" / asset).is_file())
            self.assertNotIn(".min.css", output)
            self.assertNotIn(".min.js", output)
            self.assertIn('assets/brand/conjure-docs-logo.svg', output)
            self.assertIn('assets/brand/conjure-docs-favicon.svg', output)
            self.assertEqual(list((root / "site/assets").rglob("*.map")), [])
            worker = (root / "site/assets/javascripts/workers/search-worker.js").read_text(encoding="utf-8")
            self.assertIn("/languages/lunr.", worker)
            self.assertNotIn("/min/lunr.", worker)
            # A second build must use an author's edited source, rather than re-importing the vendor file.
            editable = root / "docs/assets/stylesheets/material-theme.css"
            editable.write_text(editable.read_text(encoding="utf-8") + "\n/* Author's custom theme edit. */\n",
                                encoding="utf-8")
            result = build_site(root)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("Author's custom theme edit.",
                          (root / "site/assets/stylesheets/material-theme.css").read_text(encoding="utf-8"))
            # Strict builds enforce the same wiki boundaries as the standalone checker.
            write("docs/design/private.md", "# Private design\n")
            write("docs/api/widgets/widget.md", source + "\n[Private](../../design/private.md)\n")
            result = build_site(root)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("wiki link must use the handbook", result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
