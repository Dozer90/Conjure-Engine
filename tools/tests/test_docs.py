#!/usr/bin/env python3
"""Standard-library tests for documentation commands and all four draft templates."""

from __future__ import annotations

import argparse
import contextlib
import importlib.util
import io
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

TOOLS = pathlib.Path(__file__).resolve().parent.parent


def load(name):
    spec = importlib.util.spec_from_file_location(name, TOOLS / (name + ".py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class DocumentationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = pathlib.Path(self.temporary.name)
        self.driver = load("docs")
        self.creator = load("create_doc")
        self.creator.ROOT = self.root
        self.checker = load("check_docs")
        self.checker.ROOT = self.root
        self.checker.DOCS = self.root / "docs"
        self.checker.API_DOCS = self.root / "docs/api"
        self.checker.HANDBOOK_DOCS = self.root / "docs/handbook"
        self.checker.INCLUDE_ROOT = self.root / "engine/include/conjure"
        self.checker.MODULE_SRC_ROOT = self.root / "engine/src"
        self.write("docs/api/index.md", "# API\n")
        self.write("docs/handbook/index.md", "# Handbook\n")
        self.write("engine/include/conjure/widgets/widget.h", "#pragma once\n")
        self.write("engine/src/widgets/widget.cpp", "// Implementation\n")

    def write(self, name, text):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
        return path

    def args(self, kind, path, **changes):
        values = dict(kind=kind, path=path, scope="engine", module="widgets", header="", title="Widget")
        values.update(changes)
        return argparse.Namespace(**values)

    def test_four_templates_produce_consistent_scope_pages(self):
        pages = []
        pages.append(self.creator.create_page(self.args("scope", "docs/handbook/engine.md", module="")))
        pages.append(self.creator.create_page(self.args("module", "docs/handbook/engine/widgets.md")))
        pages.append(self.creator.create_page(self.args("header", "docs/handbook/engine/widgets/widget.md",
                                                        header="widget.h")))
        pages.append(self.creator.create_page(self.args("wiki", "docs/api/widgets/widget.md", header="widget.h")))
        self.assertEqual(self.checker.collect_problems(), [])
        for page in pages:
            text = page.read_text(encoding="utf-8")
            self.assertNotIn("$", text)
            self.assertNotIn("**Public API wiki**", text)
            self.assertNotIn("**Engineering handbook**", text)
        text = pages[-1].read_text(encoding="utf-8")
        self.assertIn("`conjure/widgets/widget.h`", text)
        self.assertIn("../../handbook/engine/widgets/widget.md", text)
        text = pages[2].read_text(encoding="utf-8")
        self.assertIn("../../engine.md", text)
        self.assertIn("../widgets.md", text)
        self.assertIn("| File Path | `engine/include/conjure/widgets/widget.h` |", text)

    def test_editor_scope_and_nested_header(self):
        self.write("editor/include/conjure/tools/area/handle.h", "#pragma once\n")
        self.write("docs/handbook/editor.md", "# Editor\n")
        self.write("docs/handbook/editor/tools.md", "# Tools\n")
        args = self.args("header", "docs/handbook/editor/tools/area/handle.md",
                         scope="editor", module="tools", header="area/handle.h")
        page = self.creator.create_page(args)
        problems = []
        self.checker.check_page(page, problems)
        self.assertEqual(problems, [])
        self.assertIn("editor/include/conjure/tools/area/handle.h", page.read_text(encoding="utf-8"))

    def test_no_overwrite_and_no_escape_or_wrong_layout(self):
        existing = self.write("docs/handbook/engine.md", "Keep this text\n")
        with self.assertRaises(FileExistsError):
            self.creator.create_page(self.args("scope", "docs/handbook/engine.md", module=""))
        self.assertEqual(existing.read_text(encoding="utf-8"), "Keep this text\n")
        for path in ("../escape.md", "docs/../../escape.md", "docs/handbook/modules/widgets.md",
                     "docs/handbook/engine/Bad_Name.md"):
            with self.assertRaises(ValueError):
                self.creator.create_page(self.args("module", path))
        with self.assertRaises(ValueError):
            self.creator.create_page(self.args("wiki", "docs/api/widgets/unknown.md", header="missing.h"))

    def test_command_dispatch_and_exit_codes(self):
        for action, suffix in (("check", ["-I", str(self.driver.ROOT / "tools/check_docs.py")]),
                               ("build", ["-m", "mkdocs", "build", "--strict"]),
                               ("serve", ["-m", "mkdocs", "serve"])):
            with patch.object(self.driver, "run", return_value=7) as run:
                with patch.object(self.driver, "ensure_toolchain", return_value=pathlib.Path("docs-python")):
                    self.assertEqual(self.driver.main([action]), 7)
            self.assertEqual(run.call_args.args[0][1:], suffix)
        with patch.object(self.driver, "run", return_value=0) as run:
            self.assertEqual(self.driver.main(["new", "--help"]), 0)
        self.assertEqual(run.call_args.args[0][-1], "--help")
        with patch.object(self.driver, "run", return_value=4) as run:
            self.assertEqual(self.driver.main(["regenerate", "--check"]), 4)
        self.assertEqual(run.call_args.args[0][-2:],
                         [str(self.driver.ROOT / "tools/regenerate_docs.py"), "--check"])
        with patch.object(self.driver, "run", return_value=0) as run:
            with patch.object(self.driver, "ensure_toolchain", return_value=pathlib.Path("docs-python")):
                self.assertEqual(self.driver.main(["assets", "--format"]), 0)
        self.assertEqual(run.call_args.args[0],
                         ["docs-python", "-I", str(self.driver.ROOT / "tools/docs_assets.py"), "--format"])
        with contextlib.redirect_stdout(io.StringIO()):
            for argv in ([], ["unknown"], ["build", "--strict"]):
                self.assertEqual(self.driver.main(argv), 2)

    def test_new_reaches_sibling_tools_in_isolated_mode(self):
        # A real 'docs.py new' in a copy of tools/: docs.py starts create_doc.py with python -I, which leaves tools/
        # off sys.path, and loading the tools in this process (setUp) must not hide that.
        tools = self.root / "tools"
        shutil.copytree(TOOLS / "docs_templates", tools / "docs_templates")
        for script in TOOLS.glob("*.py"):
            shutil.copyfile(script, tools / script.name)
        result = subprocess.run([sys.executable, "-I", str(tools / "docs.py"), "new", "scope",
                                 "docs/handbook/engine.md", "--title", "Engine"],
                                cwd=self.root, capture_output=True, text=True, encoding="utf-8")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("Created draft: docs/handbook/engine.md", result.stdout)
        self.assertIn("# Engine", (self.root / "docs/handbook/engine.md").read_text(encoding="utf-8"))

    def test_install_creation_failure_and_missing_toolchain(self):
        with patch.object(pathlib.Path, "exists", return_value=False):
            with patch.object(self.driver, "run", side_effect=[0, 5]) as run:
                self.assertEqual(self.driver.main(["install"]), 5)
                self.assertEqual(run.call_args_list[0].args[0][1:3], ["-m", "venv"])
                self.assertEqual(run.call_args_list[1].args[0][1:4], ["-m", "pip", "install"])
            with patch.object(self.driver, "run", return_value=9) as run:
                self.assertEqual(self.driver.main(["install"]), 9)
                self.assertEqual(run.call_count, 1)
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
                self.driver.ensure_toolchain()
            self.assertEqual(error.exception.code, 2)


if __name__ == "__main__":
    unittest.main()
