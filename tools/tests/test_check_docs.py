#!/usr/bin/env python3
"""Regression tests for the write-docs checker; uses only the standard library."""

from __future__ import annotations

import importlib.util
import pathlib
import tempfile
import unittest

CHECKER = pathlib.Path(__file__).resolve().parent.parent / "check_docs.py"


class CheckerTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = pathlib.Path(self.temporary.name)
        spec = importlib.util.spec_from_file_location("audit_checker", CHECKER)
        self.checker = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.checker)
        self.checker.ROOT = self.root
        self.checker.DOCS = self.root / "docs"
        self.checker.API_DOCS = self.root / "docs/api"
        self.checker.HANDBOOK_DOCS = self.root / "docs/handbook"
        self.checker.INCLUDE_ROOT = self.root / "engine/include/conjure"
        self.checker.MODULE_SRC_ROOT = self.root / "engine/src"
        self.write("docs/api/index.md", "# API\n")
        self.write("docs/handbook/index.md", "# Handbook\n")
        self.write("docs/api/widgets/index.md", "# Widgets\n\n`conjure/widgets/widget.h`\n")
        self.write("engine/include/conjure/widgets/widget.h", "#pragma once\n")
        self.write("engine/src/widgets/CMakeLists.txt", "")
        self.write("docs/handbook/engine/widgets.md", "# Widget module\n")

    def write(self, name, text):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
        return path

    def page_problems(self, text, name="docs/api/widgets/probe.md"):
        page = self.write(name, text)
        problems = []
        self.checker.check_page(page, problems)
        return "\n".join(problems)

    def test_removed_badge_and_length_requirements(self):
        self.write("docs/api/widgets/long.md", "# Long\n\n" + "line\n" * 700)
        self.assertEqual(self.checker.collect_problems(), [])

    def test_landing_page_obeys_the_publication_rule(self):
        self.write("docs/index.md", "# Docs\n\nSee AGENTS.md and docs/design/12_audio.md.\n")
        problems = "\n".join(self.checker.collect_problems())
        self.assertIn("docs/index.md:3: internal planning metadata", problems)

    def test_markers_are_words_in_any_context(self):
        for marker in ("TODO", "FIXME", "XXX"):
            self.assertIn("contains " + marker, self.page_problems(f"# Page\n\n```\n{marker}\n```\n"))
        self.assertEqual(self.page_problems("# Page\n\ntodo TODOS XXXs\n"), "")

    def test_missing_links_include_reference_html_and_images(self):
        text = ('# Page\n\n[x](gone.md)\n![x](gone.png)\n[ref]: gone-ref.md\n'
                '<a href="gone-html.md">x</a>\n<img src="gone-src.png">\n')
        problems = self.page_problems(text)
        for target in ("gone.md", "gone.png", "gone-ref.md", "gone-html.md", "gone-src.png"):
            self.assertIn("link target does not exist: " + target, problems)

    def test_code_examples_are_literal_but_table_signatures_are_links(self):
        text = ("# Page\n\n`[x](inline.md)`\n\n```markdown\n[x](fenced.md)\n```\n\n"
                "~~~~\n[x](tilde.md)\n~~~~\n\n"
                "| Function | Purpose |\n|---|---|\n| `[f()](signature.md)` | x |\n")
        problems = self.page_problems(text)
        self.assertIn("signature.md", problems)
        for target in ("inline.md", "fenced.md", "tilde.md"):
            self.assertNotIn(target, problems)

    def test_queries_encoded_paths_directories_and_anchors(self):
        self.write("docs/api/widgets/a file.md", "# First heading\n\n## Second heading\n\n## Second heading\n")
        self.write("docs/api/widgets/math.md", "# Math\n\n## matrix_id\n")
        good = ("# Page\n\n[encoded](a%20file.md?q=yes#first-heading)\n"
                "[duplicate](a%20file.md#second-heading_1)\n[index](.)\n[self](#page)\n"
                "[type](math#matrix_id)\n")
        self.assertEqual(self.page_problems(good), "")
        self.assertIn("link anchor does not exist", self.page_problems("# Page\n\n[x](a%20file.md#missing)\n"))
        self.assertIn("link anchor does not exist", self.page_problems("# Page\n\n[x](#missing)\n"))

    def test_absolute_paths_and_other_protocols_are_rejected(self):
        for target in ("/index.md", "//example.com/page", "file:///tmp/page", "C:/page.md", "%2Findex.md"):
            self.assertIn("relative", self.page_problems(f"# Page\n\n[x]({target})\n"))
        self.assertEqual(self.page_problems("# Page\n\n[x](https://example.com)\n[x](mailto:a@example.com)\n"), "")

    def test_heading_code_generics_and_literal_html_anchors(self):
        text = ('# `result<T>`\n\n[x](#resultt)\n\n'
                '`<a id="not-an-anchor">`\n\n[x](#not-an-anchor)\n')
        problems = self.page_problems(text)
        self.assertNotIn("#resultt", problems)
        self.assertIn("#not-an-anchor", problems)

    def test_wiki_boundaries_apply_to_resolved_targets(self):
        self.write("docs/design/design.md", "# Design\n")
        self.write("AGENTS.md", "# Guide\n")
        for target in ("../../design/design.md", "../../../AGENTS.md",
                       "../../../engine/include/conjure/widgets/widget.h"):
            self.assertIn("wiki link must use the handbook", self.page_problems(f"# Page\n\n[x]({target})\n"))
        text = "# Page\n\n[x](../../design/design.md)\n"
        self.assertIn("internal planning metadata", self.page_problems(text, "docs/handbook/engine/widgets.md"))

    def test_callout_types_and_syntax(self):
        for kind in ("NOTE", "TIP", "IMPORTANT", "WARNING", "CAUTION"):
            self.assertEqual(self.page_problems(f"# Page\n\n> [!{kind}]\n> Context.\n"), "")
        self.assertIn("unsupported callout", self.page_problems("# Page\n\n> [!DANGER]\n> x\n"))
        self.assertIn("GitHub alert syntax", self.page_problems("# Page\n\n!!! note\n    Context\n"))
        self.assertEqual(self.page_problems("# Page\n\n`> [!DANGER]`\n\n```\n!!! note\n```\n"), "")

    def test_every_header_requires_an_exact_code_span(self):
        names = ("widgets.h", "widgets_error.h", "log_category.h", "api.h", "platform_detect.h",
                 "build_info.h", "detail/impl.h", "internal/helper.h", "generated/table.h", "math/math.h")
        for name in names:
            self.write("engine/include/conjure/widgets/" + name, "#pragma once\n")
        problems = []
        self.checker.check_header_coverage(problems)
        self.assertEqual(len(problems), len(names))
        for name in names:
            self.assertTrue(any("conjure/widgets/" + name in problem for problem in problems))
        page = "# Widgets\n\n" + "\n".join(f"`conjure/widgets/{name}`" for name in (*names, "widget.h"))
        self.write("docs/api/widgets/index.md", page)
        problems = []
        self.checker.check_header_coverage(problems)
        self.assertEqual(problems, [])

    def test_editor_headers_require_their_component_overview(self):
        self.write("engine/include/conjure/editor/protocol/access_token.h", "enum class access_level {};\n")
        problems = []
        self.checker.check_module_handbook_pages(problems)
        self.assertTrue(any("handbook/editor/protocol.md" in item for item in problems))
        self.write("docs/handbook/editor/protocol.md", "# Protocol\n")
        problems = []
        self.checker.check_module_handbook_pages(problems)
        self.assertEqual(problems, [])
        for mention in ("conjure/widgets/widget.h", "```cpp\nconjure/widgets/widget.h\n```",
                        "`conjure/widgets/widget.h.extra`"):
            self.write("docs/api/widgets/index.md", "# Widgets\n\n" + mention)
            problems = []
            self.checker.check_header_coverage(problems)
            self.assertTrue(any("`conjure/widgets/widget.h`" in problem for problem in problems))

    def test_scope_module_paths_replace_legacy_paths(self):
        (self.root / "docs/handbook/engine/widgets.md").unlink()
        self.write("docs/handbook/modules/widgets.md", "# Old layout\n")
        self.write("editor/src/tools/CMakeLists.txt", "")
        problems = []
        self.checker.check_module_handbook_pages(problems)
        self.assertEqual(len(problems), 2)
        self.assertTrue(any("docs/handbook/engine/widgets.md" in problem for problem in problems))
        self.assertTrue(any("docs/handbook/editor/tools.md" in problem for problem in problems))
        self.write("docs/handbook/engine/widgets.md", "# Widgets\n")
        self.write("docs/handbook/editor/tools.md", "# Tools\n")
        problems = []
        self.checker.check_module_handbook_pages(problems)
        self.assertEqual(problems, [])

    def test_editor_libraries_belong_to_editor_scope(self):
        self.write("engine/src/editor/CMakeLists.txt", "")
        problems = []
        self.checker.check_module_handbook_pages(problems)
        self.assertTrue(any("docs/handbook/editor/editor.md" in problem for problem in problems))
        self.write("docs/handbook/editor/editor.md", "# Editor\n")
        problems = []
        self.checker.check_module_handbook_pages(problems)
        self.assertEqual(problems, [])


if __name__ == "__main__":
    unittest.main()
