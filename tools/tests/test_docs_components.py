"""Editor components and sibling handbook sections must survive regeneration and draft creation."""

import argparse
import pathlib
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
sys.path.append(str(pathlib.Path(__file__).resolve().parent))
import create_doc
import docs_components
import docs_model
import regenerate_docs
from nav_outline import nav_tree


class EditorComponentTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = pathlib.Path(temporary.name)
        for header, name in (("editor_application.h", "editor_application"),
                             ("command_registry.h", "command_registry"),
                             ("protocol/access_token.h", "token"),
                             ("ui/editor_workspace.h", "workspace"),
                             ("ui/panels/console_panel.h", "console_panel")):
            self.write("engine/include/conjure/editor/" + header,
                       "namespace conjure::editor { struct " + name + " {}; }\n")
        self.write("engine/src/editor/CMakeLists.txt", "conjure_add_module(editor_core\n)\n")
        self.write("docs/api/editor/index.md", "# Editor API\n")
        for page, title in (("index", "Home"), ("api/index", "API"), ("handbook/index", "Handbook"),
                            ("handbook/editor", "Editor"), ("handbook/editor/editor", "Editor"),
                            ("handbook/getting-started", "Getting started"), ("handbook/engine", "Engine")):
            self.write("docs/" + page + ".md", "# " + title + "\n\nPurpose.\n")
        self.write("mkdocs.yml", "site_name: Test\nnav:\n  - Home: index.md\n  - Public API wiki:\n"
                   "      - api/index.md\n      - Editor:\n          - api/editor/index.md\n"
                   "  - Engineering handbook:\n      - handbook/index.md\n"
                   "      - Getting started: handbook/getting-started.md\n      - Engine:\n"
                   "          - handbook/engine.md\n      - Editor:\n          - handbook/editor.md\n"
                   "          - Internals: handbook/editor/editor.md\n")

    def write(self, name, text):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8", newline="\n")
        return path

    def test_editor_components_breadcrumbs_urls_and_regeneration(self):
        planned, _ = regenerate_docs.plan(self.root)
        scope = planned[self.root / "docs/handbook/editor.md"]
        self.assertIn("## Components", scope)
        self.assertNotIn("## Modules", scope)
        architecture = planned[self.root / "docs/handbook/editor/editor.md"]
        self.assertIn("# Editor Architecture", architecture)
        self.assertIn("| Type | Application |", architecture)
        self.assertNotIn("## Headers", architecture)
        page = self.root / "docs/handbook/editor/editor/protocol/access-token.md"
        self.assertIn("[Protocol](../../protocol.md)", planned[page])
        self.assertNotIn("[Editor](../../editor.md)", planned[page])
        detail = planned[page.with_suffix("") / "token.md"]
        self.assertIn("[Protocol](../../../protocol.md)", detail)
        for component in docs_components.EDITOR_COMPONENTS:
            overview = planned[self.root / "docs" / component.page]
            self.assertIn("kind: component", overview)
            self.assertIn("| Type | Component |", overview)
            self.assertNotIn("## Dependency Graph", overview)
        tree = nav_tree(planned[self.root / "mkdocs.yml"])
        self.assertEqual(tree[1]["Public API wiki"], ["api/index.md", {"Editor": ["api/editor/index.md"]}])
        handbook = tree[2]["Engineering handbook"]
        self.assertEqual([next(iter(item)) for item in handbook], ["Engineering Handbook", "Engine", "Editor"])
        editor = handbook[2]["Editor"]
        protocol = next(item["Protocol"] for item in editor if isinstance(item, dict) and "Protocol" in item)
        self.assertEqual(protocol[0], "handbook/editor/protocol.md")
        self.assertEqual(list(protocol[1]), ["access_token.h"])
        self.assertNotIn("Internals", str(editor))
        self.assertNotIn("Headers", str(editor))
        for path, text in planned.items():
            self.write(path.relative_to(self.root).as_posix(), text)
        self.assertEqual(planned, regenerate_docs.plan(self.root)[0])

    def test_new_editor_headers_require_deliberate_component_ownership(self):
        path = self.write("engine/include/conjure/editor/new_service.h", "struct service {};\n")
        with self.assertRaisesRegex(ValueError, "Assign the editor header"):
            docs_model.SourceIndex(self.root).model(path)

    def test_component_and_header_drafts_use_the_component_hierarchy(self):
        args = argparse.Namespace(kind="component", path="docs/handbook/editor/protocol.md", scope="editor",
                                  component="protocol", module="", header="", title="", symbol="")
        with patch.object(create_doc, "ROOT", self.root):
            path = create_doc.create_page(args)
            self.assertIn("kind: component", path.read_text(encoding="utf-8"))
            with self.assertRaises(FileExistsError):
                create_doc.create_page(args)
            args.kind = "header"
            args.path = "docs/handbook/editor/editor/protocol/access-token.md"
            args.module = "editor"
            args.header = "protocol/access_token.h"
            path = create_doc.create_page(args)
            self.assertIn("[Protocol](../../protocol.md)", path.read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
