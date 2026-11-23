"""Moved handbook routes must remain valid, local, direct, and unchanged by regeneration."""

from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_docs
import docs_redirects
import regenerate_docs


class RedirectTests(unittest.TestCase):
    def test_metadata_and_fragment_routes(self):
        source = ('---\nredirect_to: development/build.md\n'
                  'redirect_fragments: {"3-presets":"development/build.md#presets"}\n---\n# Moved\n')
        self.assertEqual(docs_redirects.metadata(source),
                         ("development/build.md", {"3-presets": "development/build.md#presets"}))
        self.assertIsNone(docs_redirects.metadata("# Plain chapter\n"))
        with self.assertRaises(ValueError):
            docs_redirects.metadata("---\nredirect_to: new.md\nredirect_fragments: []\n---\n")

    def test_checker_rejects_external_missing_chained_and_missing_anchor_routes(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            docs = root / "docs"
            docs.mkdir()
            target = docs / "new.md"
            target.write_text("# New\n\n## Presets\n", encoding="utf-8")
            alias = docs / "old.md"
            with patch.object(check_docs, "ROOT", root), patch.object(check_docs, "DOCS", docs):
                for destination, valid in (("new.md#presets", True), ("new.md#unknown", False),
                                           ("missing.md", False), ("https://example.com", False), ("old.md", False)):
                    alias.write_text("---\nredirect_to: " + destination + "\n---\n# Moved\n", encoding="utf-8")
                    problems = []
                    check_docs.check_page(alias, problems)
                    self.assertEqual(not problems, valid, (destination, problems))

    def test_regeneration_preserves_alias_metadata_and_parent_overview_breadcrumbs(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            book = root / "docs/handbook"
            (book / "development").mkdir(parents=True)
            (root / "mkdocs.yml").write_text("site_name: Test\nnav: []\n", encoding="utf-8")
            (book / "index.md").write_text("# Handbook\n", encoding="utf-8")
            (book / "development/index.md").write_text("# Development\n", encoding="utf-8")
            chapter = book / "development/build.md"
            chapter.write_text("# Build\n\nHow to build.\n", encoding="utf-8")
            alias = book / "old.md"
            source = "---\nredirect_to: development/build.md\n---\n# Moved\n\n[Build](development/build.md)\n"
            alias.write_text(source, encoding="utf-8")
            planned, _ = regenerate_docs.plan(root)
            self.assertEqual(planned[alias], source)
            self.assertIn("[Development](index.md)", planned[chapter])


if __name__ == "__main__":
    unittest.main()
