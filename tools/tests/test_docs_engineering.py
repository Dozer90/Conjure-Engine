"""Source-driven engineering layouts, declarations, links, and conditional sections."""

from __future__ import annotations

import argparse
import html
from html.parser import HTMLParser
from pathlib import Path
import re
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch
import xml.etree.ElementTree as etree

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.append(str(Path(__file__).resolve().parent))
import create_doc
import docs_graph
import docs_engineering
import docs_model
import docs_render
import regenerate_docs
from nav_outline import nav_tree


class Codes(HTMLParser):
    def __init__(self):
        super().__init__()
        self.values = []
        self.current = None

    def handle_starttag(self, tag, attrs):
        if tag == "code":
            self.current = []

    def handle_endtag(self, tag):
        if tag == "code" and self.current is not None:
            self.values.append("".join(self.current))
            self.current = None

    def handle_data(self, value):
        if self.current is not None:
            self.current.append(value)


class EngineeringTests(unittest.TestCase):
    def test_empty_sections_preserve_markers_examples_and_restrictions(self):
        text = ('# Widget\n\n## Classes\n\n| File | Description |\n|---|---|\n\n'
                '## Notes\n\n<!-- doc-content:notes:start -->\n\n<!-- doc-content:notes:end -->\n\n'
                '## Constructors\n\n> [!IMPORTANT]\n> Copying is unsupported.\n\n'
                '## Example\n\n```markdown\n## Literal\n| A | B |\n|---|---|\n```\n')
        output = create_doc.prune_empty_sections(text)
        self.assertNotIn("## Classes", output)
        self.assertNotIn("## Notes", output)
        self.assertIn("<!-- doc-content:notes:start -->", output)
        self.assertIn("## Constructors", output)
        self.assertIn("## Example\n\n```markdown\n## Literal", output)
        self.assertEqual(output, create_doc.prune_empty_sections(output))
        scope = create_doc.render_template("scope", {"title": "Engine"})
        self.assertNotIn("## Modules", scope)

    def test_enum_only_header_has_full_definitions_and_logical_declaration_groups(self):
        self.write("engine/include/conjure/widgets/choices.h", '''namespace conjure::widgets {
            enum class mode : unsigned;
            enum class mode : unsigned {
                Z = 8,
                A = Z << 1, // docs/design/12_audio.md 4.3
                COUNT
            };
            using mode_count = unsigned;
            inline constexpr mode_count LIMIT = 3;
            inline constexpr const char *LABEL = "mode";
            inline const char *label = "mutable pointer";
            inline mode_count active = 0;
        }''')
        planned, _ = regenerate_docs.plan(self.root)
        page = planned[self.root / "docs/handbook/engine/widgets/choices.md"]
        for section in ("Structs", "Classes", "Functions", "Other Declarations", "Implementation Notes"):
            self.assertNotIn("## " + section, page)
        for section in ("Enumerations", "Type Aliases", "Constants", "Variables"):
            self.assertIn("## " + section, page)
        self.assertEqual(page.count("### `mode` {#symbol-mode}"), 1)
        self.assertIn("enum class mode : unsigned {\n    Z = 8,\n    A = Z << 1,", page)
        self.assertIn("COUNT\n};", page)
        self.assertIn('id="variable-LIMIT"', page)
        self.assertNotIn("docs/design", page)
        self.assertNotIn("| <a id=\"symbol-mode-z\"", page)
        self.assertLess(page.index("Z = 8"), page.index("A = Z"))
        constants = page.split("## Constants\n", 1)[1].split("## Variables\n", 1)[0]
        self.assertIn("<code>LABEL</code>", constants)
        self.assertNotIn("<code>label</code>", constants)
        anchors = re.findall(r'<a id="(variable-[^"]+)"', page)
        self.assertEqual(len(anchors), len(set(anchors)))
        base = planned[self.root / "docs/handbook/engine/widgets/base/base.md"]
        for section in ("Constructors", "Destructors", "Variables", "Functions", "Operator Overloads"):
            self.assertNotIn("## " + section, base)
        device = planned[self.root / "docs/handbook/engine/widgets/widget/device.md"]
        self.assertIn("Copy constructor and assignment has been deleted.", device)

    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.write("engine/include/conjure/widgets/base.h", "namespace conjure::widgets { struct base {}; }")
        self.header = self.write("engine/include/conjure/widgets/widget.h", '''namespace conjure::widgets {
            using count = unsigned;
            struct base;
            struct options { count sampleRate = 48000; bool enabled = true; };
            class device : public base {
                friend struct helper;
                struct impl;
                void *mState = nullptr;
            public:
                explicit device(options settings);
                device(const device &) = delete;
                device &operator=(const device &) = delete;
                i32 operator+(i32 rhs) const;
                [[nodiscard]] count play(i32 sound, bool loop = false);
                [[nodiscard]] count play(const options &settings);
                void stop() noexcept;
            };
        }''')
        self.write("engine/src/widgets/widget.cpp", "// implementation\n")
        self.write("engine/src/widgets/CMakeLists.txt", "conjure_add_module(widgets\n PUBLIC_DEPS conjure::core\n)\n")
        self.write("engine/src/core/CMakeLists.txt", "conjure_add_module(core\n)\n")
        self.write("docs/api/index.md", "# API\n")
        self.write("docs/api/widgets/example.md", "# Widgets\n\n`conjure/widgets/widget.h` `conjure/widgets/base.h`\n")
        self.write("docs/handbook/index.md", "# Handbook\n")
        self.write("docs/handbook/engine.md", "# Engine\n\nEngine services.\n\n## Modules\n")
        self.write("docs/handbook/engine/widgets.md", "# Widgets\n\nWidget services.\n")
        self.write("docs/handbook/engine/core.md", "# Core\n\nCore services.\n")
        self.write("mkdocs.yml", "site_name: Test\nnav:\n  - API: api/index.md\n  - Engine:\n"
                   "      - Core: handbook/engine/core.md\n      - Widgets: handbook/engine/widgets.md\n")
        self.index = docs_model.SourceIndex(self.root)
        self.model = self.index.model(self.header)
        self.entity = next(row for row in self.model.types if row.name == "device")
        self.page = self.model.type_pages["device"]

    def write(self, name, value):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(value, encoding="utf-8")
        return path

    def test_type_pages_cover_definitions_and_prefer_definitions_over_forwards(self):
        self.assertEqual(set(self.model.type_pages), {"device", "options"})
        target = self.model.type_reference("base")
        self.assertEqual(target, Path("handbook/engine/widgets/base/base.md"))
        self.assertEqual(self.model.type_reference("impl", "device"),
                         docs_model.DocTarget(Path("handbook/engine/widgets/widget.md")))

    def test_variable_columns_keep_names_types_and_written_defaults(self):
        variables = self.model.members(self.entity, "variables")
        self.assertEqual([row.name for row in variables], ["device::mState"])
        output = docs_render.variable_table(variables, self.page, self.model, "device", {})
        self.assertEqual(re.findall(r"<th>(.*?)</th>", output),
                         ["Access", "Name", "Type", "Default", "Description"])
        parser = Codes()
        parser.feed(output)
        self.assertEqual(parser.values, ["mState", "void *", "nullptr"])
        self.assertIn("<td>private</td>", output)

    def test_constructors_keep_argument_lines_and_summarise_deleted_copy_operations(self):
        output = docs_render.constructor_section(self.model.members(self.entity, "constructors"), self.page,
            self.model, "device", {}, [row for row in self.model.members(self.entity, "functions")
                                      if "= delete" in row.signature])
        self.assertNotIn("<wbr>", output)
        self.assertNotIn("= delete", output)
        self.assertRegex(output, r'</table>\s*<div class="admonition important"')
        self.assertIn("Copy constructor and assignment has been deleted.", output)
        parser = Codes()
        parser.feed(output)
        self.assertIn("explicit device(\n    options settings);", parser.values)

    def test_functions_group_overloads_in_collapsible_h4s_and_include_them_in_the_toc(self):
        source = docs_render.function_sections(self.entity, self.page, self.model, {})
        self.assertNotIn("operator", source)
        self.assertEqual(source.count("#### `play`"), 1)
        self.assertNotIn("Details", source)
        self.assertEqual(source.count('#### `play` {#function-play}'), 1)
        self.assertNotRegex(source, r'<details\b[^>]*\bopen\b')
        self.assertEqual(source.count('id="function-play-'), 2)

    def test_supported_operators_have_a_separate_conditional_section(self):
        source = docs_render.function_sections(self.entity, self.page, self.model, {}, operators=True)
        self.assertTrue(source.startswith("## Operator Overloads\n"))
        self.assertIn("#### `operator+`", source)
        self.assertNotIn("operator=", source)
        empty = next(row for row in self.model.types if row.name == "options")
        self.assertEqual(docs_render.function_sections(empty, self.page, self.model, {}, operators=True), "")
        self.assertNotEqual(docs_render.callable_anchor("operator+"), docs_render.callable_anchor("operator-"))

    def test_argument_formatting_preserves_templates_defaults_and_qualifiers(self):
        signature = 'void run(std::pair<i32, i32> pair = {1, 2}, std::string_view text = "a,b") const noexcept;'
        self.assertEqual(docs_render.format_function(signature, "run"),
                         'void run(\n    std::pair<i32, i32> pair = {1, 2},\n'
                         '    std::string_view text = "a,b") const noexcept;')

    def test_link_targets_handle_header_symbols_relative_html_and_local_pages(self):
        target = self.model.type_reference("count")
        self.assertEqual(docs_render.href(self.page, target, True), "../widget.html#symbol-count")
        output = docs_render.prose("Call play() with `options` or nullptr.", self.page, self.model, "device")
        parser = Codes()
        parser.feed(output)
        self.assertEqual(parser.values, ["play()", "options", "nullptr"])

    def test_inheritance_and_derived_classes_use_actual_definitions(self):
        self.assertEqual(self.model.ancestor_chains(self.entity), [["base", "device"]])
        output = docs_render.inheritance(self.entity, self.page, self.model)
        self.assertIn("## Inheritance Hierarchy", output)
        self.assertNotIn("## Derived Classes", output)

    def test_dependency_graph_keeps_line_style_and_uses_filled_small_arrows(self):
        diagram, edges = docs_graph.graph(self.model)
        self.assertEqual(edges, [("core", "widgets", "public")])
        root = etree.fromstring(diagram)
        ns = {"s": "http://www.w3.org/2000/svg"}
        marker = root.find("s:defs/s:marker", ns)
        self.assertEqual(marker.get("markerUnits"), "userSpaceOnUse")
        self.assertEqual(marker.find("s:path", ns).get("fill"), "#8c8c8c")
        self.assertEqual(root.find("s:path", ns).get("stroke-width"), "2.5")
        self.assertTrue(root.findall('s:circle[@data-port]', ns))
        self.assertNotIn('fill="#272727"', diagram)
        self.assertNotIn('Dependency → consumer · solid:', diagram)
        hole = root.find('s:defs/s:mask[@id="node-core"]/s:circle', ns)
        self.assertEqual((hole.get("r"), hole.get("fill")), ("8", "black"))
        self.assertTrue(root.find('.//s:rect[@mask="url(#node-core)"]', ns) is not None)

    @staticmethod
    def graph_model(dependencies, module="editor"):
        return SimpleNamespace(dependencies=dependencies, module=module, scope="editor",
                               reference_page=lambda page: None)

    @staticmethod
    def segment_enters_box(a, b, box):
        low, high = 0.0, 1.0
        left, top, right, bottom = box
        for start, end, minimum, maximum in ((a[0], b[0], left, right), (a[1], b[1], top, bottom)):
            delta = end - start
            if delta == 0:
                if not minimum < start < maximum:
                    return False
            else:
                limits = sorted(((minimum - start) / delta, (maximum - start) / delta))
                low, high = max(low, limits[0]), min(high, limits[1])
        return high - low > 1e-8

    def assert_routes_clear(self, model):
        drawing = docs_graph.layout(model, docs_graph.relationships(model))
        routes = drawing.routes + [(None, model.module, access, points)
                                   for access, points in drawing.trunks.items()]
        for source, target, access, points in routes:
            for name, (x, y) in drawing.positions.items():
                if name in (source, target):
                    continue
                half = drawing.widths[name] / 2 + 2
                box = (x - half, y - 30, x + half, y + 30)
                for a, b in zip(points, points[1:]):
                    self.assertFalse(self.segment_enters_box(a, b, box), (source, target, name))
        for i, (source, target, access, points) in enumerate(routes):
            for other_source, other_target, other_access, other in routes[:i]:
                if ((source, access) == (other_source, other_access)
                        or (target, access) == (other_target, other_access)):
                    continue
                for a, b in zip(points, points[1:]):
                    for c, d in zip(other, other[1:]):
                        if a[0] == b[0] == c[0] == d[0]:
                            end = min(max(a[1], b[1]), max(c[1], d[1]))
                            start = max(min(a[1], b[1]), min(c[1], d[1]))
                            self.assertLessEqual(end, start, (source, target, other_source, other_target))
                        if a[1] == b[1] == c[1] == d[1]:
                            end = min(max(a[0], b[0]), max(c[0], d[0]))
                            start = max(min(a[0], b[0]), min(c[0], d[0]))
                            self.assertLessEqual(end, start, (source, target, other_source, other_target))
        return drawing

    def test_large_graph_groups_direct_dependencies_without_extra_context_edges(self):
        private = {"bindings", "physics", "renderer", "rhi", "scripting"}
        names = ["assets", "bindings", "core", "ecs", "engine", "modules", "physics", "platform",
                 "renderer", "rhi", "scene", "scripting"]
        dependencies = {name: [("core", "public")] for name in names if name != "core"}
        dependencies["editor"] = [(name, "private" if name in private else "public") for name in names]
        model = self.graph_model(dependencies)
        drawing = self.assert_routes_clear(model)
        diagram, edges = docs_graph.graph(model)
        root = etree.fromstring(diagram)
        ns = {"s": "http://www.w3.org/2000/svg"}
        paths = root.findall('s:path[@data-dependency]', ns)
        self.assertEqual(len(paths), 12)
        self.assertEqual(len(paths), len(edges))
        for path in paths:
            self.assertIsNone(path.get("marker-end"))
            expected = "7 5" if path.get("data-dependency") in private else None
            self.assertEqual(path.get("stroke-dasharray"), expected)
        trunks = root.findall('s:path[@data-trunk]', ns)
        self.assertEqual(len(trunks), 2)
        for trunk in trunks:
            self.assertEqual(trunk.get("marker-end"), "url(#arrow)")
            expected = "7 5" if trunk.get("data-trunk") == "private" else None
            self.assertEqual(trunk.get("stroke-dasharray"), expected)
        self.assertEqual({label for _, _, label in drawing.headings}, {"Public", "Private"})
        for access in ("public", "private"):
            group = sorted((source for source, _, kind in edges if kind == access), key=str.casefold)
            self.assertEqual(group, sorted(group, key=lambda name: drawing.positions[name][1]))
        self.assertEqual(drawing.positions["scene"][1], drawing.positions["scripting"][1])
        self.assertEqual(diagram, docs_graph.graph(model)[0])
        self.assertNotIn(("physics", "scene", "private"), edges)
        self.assertNotIn(("core", "assets", "public"), edges)

    def test_graph_consumers_and_large_groups_avoid_unrelated_nodes(self):
        names = [f"module_{i:02}" for i in range(30)]
        dependencies = {name: [("core", "public")] for name in names}
        dependencies["editor"] = [(name, "private" if i % 2 else "public") for i, name in enumerate(names)]
        for i in range(12):
            dependencies[f"consumer_{i:02}"] = [("editor", "private" if i % 2 else "public")]
        self.assert_routes_clear(self.graph_model(dependencies))
        self.assert_routes_clear(self.graph_model(dependencies, "core"))
        self.assert_routes_clear(self.graph_model({}, "standalone"))

    def test_consumers_sit_close_below_module_without_repeated_headings(self):
        model = self.graph_model({"editor": [("core", "public")],
                                  "zebra": [("editor", "private")], "alpha": [("editor", "public")]})
        drawing = self.assert_routes_clear(model)
        self.assertNotEqual(drawing.ports["editor", "public"], drawing.ports["editor", "private"])
        self.assertEqual(drawing.positions["alpha"][1], drawing.positions["editor"][1] + 96)
        self.assertEqual(drawing.positions["zebra"][1], drawing.positions["editor"][1] + 96)
        self.assertEqual(drawing.headings, [(144, 30, "Public")])
        diagram, _ = docs_graph.graph(model)
        self.assertIn('mask="url(#node-editor)"', diagram)
        self.assertNotIn(">Consumers</text>", diagram)
        simple, _ = docs_graph.graph(self.graph_model({"editor": [("core", "public")]}))
        self.assertNotIn(">Consumers</text>", simple)

    def test_arrow_strokes_stop_at_the_base_and_tips_meet_target_borders(self):
        model = self.graph_model({"rhi": [("core", "public"), ("thirdparty::nvrhi", "private")],
                                  "renderer": [("rhi", "public")], "editor": [("rhi", "private")],
                                  "engine": [("rhi", "private")]}, "rhi")
        drawing = self.assert_routes_clear(model)
        root = etree.fromstring(docs_graph.graph(model)[0])
        ns = {"s": "http://www.w3.org/2000/svg"}
        self.assertEqual(root.find("s:defs/s:marker", ns).get("refX"), "0")
        for path in root.findall('s:path[@marker-end]', ns):
            points = [(float(x), float(y)) for x, y in re.findall(r'[ML](-?[\d.]+),(-?[\d.]+)', path.get("d"))]
            before, end = points[-2:]
            dx, dy = end[0] - before[0], end[1] - before[1]
            length = (dx * dx + dy * dy) ** 0.5
            tip = (end[0] + dx * 8 / length, end[1] + dy * 8 / length)
            target = path.get("data-consumer")
            x, y = drawing.positions[target]
            if path.get("data-trunk"):
                self.assertAlmostEqual(tip[1], y - 29.5)
                self.assertAlmostEqual(end[1], y - 37.5)
            else:
                direction = 1 if x < drawing.positions[model.module][0] else -1
                self.assertAlmostEqual(tip[0], x + direction * 113.5)
                self.assertAlmostEqual(end[0], x + direction * 121.5)
                self.assertAlmostEqual(tip[1], y)

    def test_connectors_start_at_dot_centres_without_solid_stems(self):
        model = self.graph_model({"editor": [("core", "public"), ("physics", "private")],
                                  "consumer": [("editor", "private")]})
        drawing = docs_graph.layout(model, docs_graph.relationships(model))
        for source, _, access, points in drawing.routes:
            self.assertEqual(points[0], drawing.ports[source, access])
        root = etree.fromstring(docs_graph.graph(model)[0])
        ns = {"s": "http://www.w3.org/2000/svg"}
        self.assertFalse(root.findall('s:path[@data-port]', ns))
        for dot in root.findall('s:circle[@data-port]', ns):
            source, access = dot.get("data-port"), dot.get("data-access")
            center = (float(dot.get("cx")), float(dot.get("cy")))
            self.assertEqual(center, drawing.ports[source, access])
            self.assertEqual(dot.get("r"), "4")
            route = root.find(f's:path[@data-dependency="{source}"]', ns)
            if access == "private":
                self.assertEqual(route.get("stroke-dasharray"), "7 5")
            holes = root.findall(f's:defs/s:mask[@id="node-{source}"]/s:circle', ns)
            hole = next(hole for hole in holes if (float(hole.get("cx")), float(hole.get("cy"))) == center)
            self.assertGreaterEqual(float(hole.get("r")), float(dot.get("r")) + 4)

    def test_new_type_draft_matches_its_source_and_never_overwrites(self):
        args = argparse.Namespace(kind="type", path="docs/handbook/engine/widgets/widget/device.md",
                                  scope="engine", module="widgets", header="widget.h", title="", symbol="device")
        with patch.object(create_doc, "ROOT", self.root):
            path = create_doc.create_page(args)
            self.assertIn("# device", path.read_text(encoding="utf-8"))
            with self.assertRaises(FileExistsError):
                create_doc.create_page(args)
            args.symbol = "impl"
            with self.assertRaisesRegex(ValueError, "fully defined"):
                create_doc.create_page(args)

    def test_new_type_can_be_created_before_navigation_is_registered(self):
        self.write("mkdocs.yml", "site_name: Test\nnav: []\n")
        args = argparse.Namespace(kind="type", path="docs/handbook/engine/widgets/widget/device.md",
                                  scope="engine", module="widgets", header="widget.h", title="", symbol="device")
        with patch.object(create_doc, "ROOT", self.root):
            path = create_doc.create_page(args)
        self.assertIn("# device", path.read_text(encoding="utf-8"))

    def test_scope_navigation_sorts_modules_without_reordering_api_topics(self):
        api = ("nav:\n  - Public API wiki:\n      - Engine:\n"
               "          - Zebra: api/widgets/zebra.md\n          - Alpha: api/widgets/alpha.md\n")
        text = api + ("  - Engineering handbook:\n      - Engine:\n          - handbook/engine.md\n"
                      "          - Widgets: handbook/engine/widgets.md\n          - Core: handbook/engine/core.md\n")
        result = docs_engineering.navigation(text, self.root, self.index)
        self.assertTrue(result.startswith(api))
        self.assertLess(result.index("- Core:"), result.index("- Widgets:"))

    def test_headers_are_direct_module_children_and_types_remain_header_children(self):
        original = (self.root / "mkdocs.yml").read_text(encoding="utf-8")
        result = docs_engineering.navigation(original, self.root, self.index)
        tree = nav_tree(result)[1]["Engine"]
        widgets = next(item["Widgets"] for item in tree if "Widgets" in item)
        self.assertEqual(widgets, ["handbook/engine/widgets.md",
            {"base.h": ["handbook/engine/widgets/base.md", {"base": "handbook/engine/widgets/base/base.md"}]},
            {"widget.h": ["handbook/engine/widgets/widget.md",
                {"device": "handbook/engine/widgets/widget/device.md"},
                {"options": "handbook/engine/widgets/widget/options.md"}]}])
        self.assertNotIn("- Headers:", result)
        self.assertEqual(result, docs_engineering.navigation(result, self.root, self.index))

    def test_legacy_module_chapters_require_review_instead_of_embedding_old_indexes(self):
        old = "# Widgets\n\nPurpose.\n\n## Contents\n\n- [Internals](#internals)\n\n## Internals\n\nAlgorithm.\n"
        with self.assertRaisesRegex(ValueError, "authored migration"):
            docs_engineering.module_regions(old, regenerate_docs)
        approved = "# Widgets\n\n" + regenerate_docs.content_region("breakdown", "### Internals\n\nAlgorithm.")
        regions = docs_engineering.module_regions(approved, regenerate_docs)
        self.assertEqual(regions["breakdown"], "### Internals\n\nAlgorithm.")


if __name__ == "__main__":
    unittest.main()
