#!/usr/bin/env python3
"""Content preservation, source metadata, navigation, and repeatability of documentation regeneration."""

from __future__ import annotations

import contextlib
import importlib.util
import io
import html
import pathlib
import re
import tempfile
import unittest
from unittest.mock import patch

TOOLS = pathlib.Path(__file__).resolve().parent.parent


def load_generator():
    spec = importlib.util.spec_from_file_location("regenerate_docs", TOOLS / "regenerate_docs.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class RegenerationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = pathlib.Path(self.temporary.name)
        self.generator = load_generator()
        self.write("docs/api/index.md", "# API\n\n[Widgets](widgets/index.md)\n")
        self.write("docs/api/widgets/index.md", "# Widgets\n\nWidget topics.\n\n[Widget](widget.md)\n")
        self.write("docs/api/widgets/widget.md", '''# Widget

Keep this exact purpose and `conjure/widgets/widget.h` include.

## At a glance

| If you want to... | Use |
|---|---|
| Call it | `widget()` |

## Quick example

```cpp
// Keep this exact example.
widget value = {};
```

## Lifetime

Keep this exact lifetime warning.

## API summary

| Signature | Purpose |
|---|---|
| `widget()` | Makes the widget. |

## See also

- [Module](index.md)
''')
        self.write("docs/handbook/index.md", "# Handbook\n\n[Engine](engine.md)\n")
        self.write("docs/handbook/engine.md", "# Engine\n\nEngine purpose.\n\n## Modules\n\n"
                   "| Module | Description |\n|---|---|\n| [Widgets](engine/widgets.md) | Retain this description. |\n")
        self.write("docs/handbook/engine/widgets.md", "# Widgets module\n\nKeep the implementation explanation.\n\n"
                   "## 1. Internals\n\nKeep the algorithm.\n\n## 2. Threading\n\nMain thread only.\n\n"
                   "## 3. Extension points\n\nUse registration.\n")
        self.write("engine/include/conjure/widgets/widget.h", '''#pragma once
namespace conjure::widgets {
    /// <summary>
    /// A widget with a stable identity.
    /// </summary>
    struct widget {
        /// <summary>
        /// Reports whether this widget is usable.
        /// </summary>
        [[nodiscard]] bool valid() const;
    };
}
''')
        self.write("engine/src/widgets/widget.cpp", "// implementation\n")
        self.write("tests/widgets/test_widget.cpp", '#include "conjure/widgets/widget.h"\n')
        self.write("tests/widgets/CMakeLists.txt", "conjure_add_test_module(widgets SOURCES test_widget.cpp)\n")
        self.write("mkdocs.yml", "site_name: Test\nnav:\n  - API: api/index.md\n"
                   "  - Handbook:\n      - Widgets: handbook/engine/widgets.md\n")

    def write(self, name, text):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8", newline="\n")
        return path

    def apply(self, plan):
        for path, text in plan.items():
            self.write(path.relative_to(self.root).as_posix(), text)

    def test_all_page_types_source_metadata_and_authored_content(self):
        planned, counts = self.generator.plan(self.root)
        self.assertEqual(counts["header_pages"], 1)
        wiki = planned[self.root / "docs/api/widgets/widget.md"]
        for sentence in ("Keep this exact purpose", "Keep this exact example", "Keep this exact lifetime warning"):
            self.assertIn(sentence, wiki)
        for section in ("## Syntax", "## At a glance", "## Variables and Properties", "## Functions"):
            self.assertIn(section, wiki)
        self.assertIn('id="quick-example"', wiki)
        self.assertIn('id="api-summary"', wiki)
        header = planned[self.root / "docs/handbook/engine/widgets/widget.md"]
        self.assertIn("A widget with a stable identity.", html.unescape(re.sub(r"<[^>]*>", "", header)))
        detail = planned[self.root / "docs/handbook/engine/widgets/widget/widget.md"]
        self.assertIn("`engine/src/widgets/widget.cpp`", detail)
        self.assertIn("#### `valid` {#function-valid}", detail)
        self.assertIn("nodiscard", detail)
        for section in ("Constructors", "Destructors", "Variables", "Operator Overloads"):
            self.assertNotIn("## " + section, detail)
        self.assertLess(detail.index("## Syntax"), detail.index("## Functions"))
        module = planned[self.root / "docs/handbook/engine/widgets.md"]
        graph_section = module.split("## Dependency Graph", 1)[1].split("## Headers", 1)[0].strip()
        self.assertTrue(graph_section.startswith("![Widgets module dependencies]("))
        self.assertEqual(len(graph_section.splitlines()), 1)
        self.assertIn("Keep the implementation explanation.", module)
        self.assertIn("Keep the algorithm.", module)
        self.assertIn("widgets/widget.md", module)
        self.assertIn("Retain this description.", planned[self.root / "docs/handbook/engine.md"])
        self.assertIn("python tools/build.py --test -L widgets", module)

    def test_regeneration_is_byte_for_byte_repeatable(self):
        first, _ = self.generator.plan(self.root)
        self.apply(first)
        second, _ = self.generator.plan(self.root)
        self.assertEqual(first, second)

    def test_authored_edits_survive_refresh(self):
        first, _ = self.generator.plan(self.root)
        self.apply(first)
        page = self.root / "docs/handbook/engine/widgets/widget.md"
        text = page.read_text(encoding="utf-8")
        text = text.replace("<!-- doc-content:threading:start -->\n",
                            "<!-- doc-content:threading:start -->\nA precise additional ownership rule.\n")
        page.write_text(text, encoding="utf-8")
        second, _ = self.generator.plan(self.root)
        self.assertIn("A precise additional ownership rule.", second[page])

    def test_an_existing_hand_written_header_page_is_adopted(self):
        page = self.write("docs/handbook/engine/widgets/widget.md", "# Widget details\n\nOriginal purpose.\n\n"
                          "## Design choices\n\nOriginal algorithm.\n\n"
                          "## Ownership\n\nOriginal thread rule.\n\n## Tests\n\nOriginal test guidance.\n")
        planned, _ = self.generator.plan(self.root)
        for sentence in ("Original purpose.", "Original algorithm.", "Original thread rule.",
                         "Original test guidance."):
            self.assertIn(sentence, planned[page])
        self.apply(planned)
        second, _ = self.generator.plan(self.root)
        self.assertEqual(planned, second)

    def test_source_changes_refresh_generated_summaries(self):
        first, _ = self.generator.plan(self.root)
        self.apply(first)
        header = self.root / "engine/include/conjure/widgets/widget.h"
        text = header.read_text(encoding="utf-8").replace("A widget with a stable identity.",
                                                        "A revised widget contract.")
        header.write_text(text, encoding="utf-8")
        second, _ = self.generator.plan(self.root)
        page = second[self.root / "docs/handbook/engine/widgets/widget.md"]
        visible = html.unescape(re.sub(r"<[^>]*>", "", page))
        self.assertIn("A revised widget contract.", visible)
        self.assertNotIn("A widget with a stable identity.", visible)

    def test_members_include_non_public_declarations_and_exclude_bodies(self):
        self.write("engine/include/conjure/widgets/widget.h", '''class widget {
            int mValue = 0;
        protected:
            static constexpr int LIMIT = 8;
        public:
            widget() noexcept : mValue(2) { int local = 9; }
            int value() const noexcept { return mValue; }
        };''')
        planned, _ = self.generator.plan(self.root)
        page = planned[self.root / "docs/handbook/engine/widgets/widget/widget.md"]
        import html
        import re
        visible = html.unescape(re.sub(r"<[^>]*>", "", page))
        for fragment in ("widget() noexcept;", "mValue", "private", "protected",
                         "static constexpr int", "LIMIT", "int value() const noexcept;"):
            self.assertIn(fragment, visible)
        self.assertNotIn("int local = 9", page)
        self.assertNotIn("return mValue", page)

    def test_published_pages_omit_planning_citations_from_source_and_prose(self):
        header = self.root / "engine/include/conjure/widgets/widget.h"
        header.write_text('#pragma once\n// Backend interface (docs/design/12_audio.md 4.3).\n'
                          '// Every method is main-thread only except state().\n'
                          'namespace conjure::widgets { struct widget {}; }\n', encoding="utf-8")
        topic = self.root / "docs/api/widgets/widget.md"
        topic.write_text(topic.read_text(encoding="utf-8")
                         + "\nThe subsystem (wave 4) owns components (09_cross_area_resolutions.md R7).\n",
                         encoding="utf-8")
        planned, _ = self.generator.plan(self.root)
        for path, text in planned.items():
            if path.suffix == ".md":
                self.assertFalse(self.generator.docs_policy.findings(text), path)
        detail = planned[self.root / "docs/handbook/engine/widgets/widget.md"]
        self.assertIn("Every method is main-thread only except state().", html.unescape(re.sub(r"<[^>]*>", "", detail)))

    def test_check_mode_does_not_write_and_becomes_clean(self):
        with patch.object(self.generator, "ROOT", self.root), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(self.generator.main(["--check"]), 1)
            self.assertFalse((self.root / "docs/handbook/engine/widgets/widget.md").exists())
            self.assertEqual(self.generator.main([]), 0)
            self.assertEqual(self.generator.main(["--check"]), 0)

    def test_frontmatter_and_literal_headings_are_preserved(self):
        path = self.root / "docs/api/widgets/widget.md"
        text = path.read_text(encoding="utf-8")
        text = "---\ntitle: Custom\n---\n" + text + "\n```markdown\n## Literal heading\n```\n"
        path.write_text(text, encoding="utf-8")
        planned, _ = self.generator.plan(self.root)
        self.assertTrue(planned[path].startswith("---\ntitle: Custom\n---\n"))
        self.assertIn("```markdown\n## Literal heading\n```", planned[path])

    def test_missing_navigation_and_path_collisions_fail_before_writes(self):
        self.write("mkdocs.yml", "site_name: Test\nnav:\n  - API: api/index.md\n")
        with self.assertRaisesRegex(ValueError, "Register the module overview"):
            self.generator.plan(self.root)
        self.write("engine/include/conjure/widgets/a_b.h", "#pragma once\n")
        self.write("engine/include/conjure/widgets/a-b.h", "#pragma once\n")
        with self.assertRaisesRegex(ValueError, "collide"):
            self.generator.plan(self.root)

    def test_unsafe_destination_prevents_all_writes(self):
        inside = self.root / "docs/api/index.md"
        before = inside.read_text(encoding="utf-8")
        outside = self.root.parent / (self.root.name + "-outside.md")
        planned = {inside: "Would replace the index", outside: "Outside content"}
        with patch.object(self.generator, "ROOT", self.root):
            with patch.object(self.generator, "plan", return_value=(planned, {})):
                with self.assertRaisesRegex(ValueError, "outside documentation"):
                    self.generator.main([])
        self.assertEqual(inside.read_text(encoding="utf-8"), before)
        self.assertFalse(outside.exists())


if __name__ == "__main__":
    unittest.main()
