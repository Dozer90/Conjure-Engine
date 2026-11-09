#!/usr/bin/env python3
"""Self-test for tools/check_docs.py (docs/design/16_documentation.md section 6).

Builds a tiny fake repository in a temporary directory, points the checker at it and asserts that each
rule fires exactly when it should. Runs as the CTest entry `docs.check_tool`; no third-party test
framework is needed.
"""

from __future__ import annotations

import importlib.util
import pathlib
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
CHECKER = HERE.parent / "check_docs.py"


def load_checker():
    specification = importlib.util.spec_from_file_location("check_docs", CHECKER)
    module = importlib.util.module_from_spec(specification)
    assert specification.loader is not None
    specification.loader.exec_module(module)
    return module


def write(path: pathlib.Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def make_fake_repo(root: pathlib.Path) -> None:
    include = root / "engine" / "include" / "conjure"
    write(include / "widgets" / "widgets.h", "#pragma once\n")              # umbrella: excluded
    write(include / "widgets" / "widgets_error.h", "#pragma once\n")        # error enum: excluded
    write(include / "widgets" / "log_category.h", "#pragma once\n")         # plumbing: excluded
    write(include / "widgets" / "detail" / "impl.h", "#pragma once\n")      # detail dir: excluded
    write(include / "widgets" / "widget.h", "#pragma once\n")               # covered below
    write(include / "widgets" / "gadget.h", "#pragma once\n")               # NOT covered -> problem
    write(root / "engine" / "src" / "widgets" / "CMakeLists.txt", "")       # module exists
    write(root / "engine" / "src" / "orphan" / "CMakeLists.txt", "")        # no handbook page -> problem

    write(root / "docs" / "api" / "widgets" / "index.md",
          "# Widgets\n\n📘 **Public API wiki**\n\nInclude `conjure/widgets/widget.h`.\n\n[glossary](../glossary.md)\n")
    write(root / "docs" / "api" / "glossary.md", "# Glossary\n\n📘 **Public API wiki**\n\nTerms.\n")
    write(root / "docs" / "api" / "bad.md", "# Bad page\n\nNo badge here.\n\n[dangling](missing.md)\n\nTODO later\n")
    write(root / "docs" / "handbook" / "modules" / "widgets.md",
          "# Widgets internals\n\n🛠 **Engineering handbook**\n\nInternals.\n")
    long_page = "# Long\n\n🛠 **Engineering handbook**\n" + "\nline\n" * 500
    write(root / "docs" / "handbook" / "long.md", long_page)


def main() -> int:
    checker = load_checker()
    with tempfile.TemporaryDirectory() as temporary:
        root = pathlib.Path(temporary)
        make_fake_repo(root)

        checker.ROOT = root
        checker.DOCS = root / "docs"
        checker.API_DOCS = checker.DOCS / "api"
        checker.HANDBOOK_DOCS = checker.DOCS / "handbook"
        checker.INCLUDE_ROOT = root / "engine" / "include" / "conjure"
        checker.MODULE_SRC_ROOT = root / "engine" / "src"

        problems: list[str] = []
        for page in checker.iter_pages(checker.API_DOCS):
            checker.check_page(page, checker.API_BADGE, problems)
        for page in checker.iter_pages(checker.HANDBOOK_DOCS):
            checker.check_page(page, checker.HANDBOOK_BADGE, problems)
        checker.check_header_coverage(problems)
        checker.check_module_handbook_pages(problems)

        joined = "\n".join(problems)
        expectations = [
            ("docs/api/bad.md: first content line", "missing badge is reported"),
            ("link target does not exist: missing.md", "dangling link is reported"),
            ("docs/api/bad.md: contains TODO", "TODO marker is reported"),
            ("exceeds 400", "over-long page is reported"),
            ("no wiki page references `conjure/widgets/gadget.h`", "uncovered header is reported"),
            ("missing handbook page for module 'orphan'", "module without handbook page is reported"),
        ]
        failures = [why for needle, why in expectations if needle not in joined]

        must_not = [
            ("widgets/widgets.h", "umbrella header must be excluded"),
            ("widgets_error.h", "error header must be excluded"),
            ("log_category.h", "log category header must be excluded"),
            ("detail/impl.h", "detail directory must be excluded"),
            ("conjure/widgets/widget.h", "covered header must not be reported"),
            ("module 'widgets'", "module with a handbook page must not be reported"),
        ]
        failures += [why for needle, why in must_not if needle in joined]

        expected_count = len(expectations)
        if len(problems) != expected_count:
            failures.append(f"expected exactly {expected_count} problems, got {len(problems)}:\n{joined}")

    if failures:
        print("\n".join(f"FAIL: {failure}" for failure in failures))
        return 1
    print("docs.check_tool: all checker rules behave as expected")
    return 0


if __name__ == "__main__":
    sys.exit(main())
