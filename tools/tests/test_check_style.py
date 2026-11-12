#!/usr/bin/env python3
"""Self-test for tools/check_style.py (docs/design/08_testing.md 7.3).

Builds small fake repositories in a temporary directory, runs the checker's main() over them with its output
captured and asserts that each rule fires exactly when it should. Runs as the CTest entry `style.check_tool`;
no third-party test framework is needed.
"""

from __future__ import annotations

import contextlib
import importlib.util
import io
import pathlib
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
CHECKER = HERE.parent / "check_style.py"


def load_checker():
    specification = importlib.util.spec_from_file_location("check_style", CHECKER)
    module = importlib.util.module_from_spec(specification)
    assert specification.loader is not None
    specification.loader.exec_module(module)
    return module


def write(root: pathlib.Path, relative: str, text: str) -> pathlib.Path:
    path = root / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8", newline="\n")
    return path


class run_result:
    def __init__(self, code: int, findings: list[str], errors: str) -> None:
        self.code = code
        self.findings = findings
        self.errors = errors

    def of_rule(self, rule: str) -> list[str]:
        return [line for line in self.findings if f": {rule}: " in line]


def run(checker, root: pathlib.Path, *arguments: str) -> run_result:
    output = io.StringIO()
    errors = io.StringIO()
    with contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors):
        code = checker.main(["--root", str(root), *arguments])
    findings = [line for line in output.getvalue().splitlines() if not line.startswith("check_style:")]
    return run_result(code, findings, errors.getvalue())


#------------------------------------------------------------------------------
# strip_comments
#------------------------------------------------------------------------------

def digit_separator_is_code(checker, root: pathlib.Path) -> None:
    write(root, "tests/foo/test_bar.cpp", "const unsigned long long BITS = 0xFFFF'FFFFull;\nstd::thread worker;\n")
    result = run(checker, root, "tests/foo/test_bar.cpp")
    lines = [line.split(":")[1] for line in result.of_rule("forbidden-in-tests")]
    assert lines == ["2"], result.findings


def raw_string_keeps_later_code(checker, root: pathlib.Path) -> None:
    write(root, "tests/foo/test_raw.cpp", "const char *PATTERN = R\"x(a \" b)x\";\nstd::thread worker;\n")
    result = run(checker, root, "tests/foo/test_raw.cpp")
    lines = [line.split(":")[1] for line in result.of_rule("forbidden-in-tests")]
    assert lines == ["2"], result.findings


def literal_newlines_kept(checker, root: pathlib.Path) -> None:
    text = "auto text = R\"(line one\nline two\nline three)\";\nint after = 0;\nchar quote = 'a;\nint next = 1;\n"
    for keep_strings in (False, True):
        stripped = checker.strip_comments(text, keep_strings=keep_strings)
        assert len(stripped) == len(text), (keep_strings, len(stripped), len(text))
        assert stripped.split("\n")[3] == "int after = 0;", stripped.split("\n")
        # An unterminated character literal ends at the end of its line.
        assert stripped.split("\n")[5] == "int next = 1;", stripped.split("\n")
    assert "line two" not in checker.strip_comments(text)


def enum_after_digit_separator(checker, root: pathlib.Path) -> None:
    write(root, "engine/src/foo/bar.cpp", "int value = 1'000;\nenum class late { TRUE };\n")
    result = run(checker, root, "engine/src/foo/bar.cpp")
    lines = [line.split(":")[1] for line in result.of_rule("windows-macro")]
    assert lines == ["2"], result.findings


#------------------------------------------------------------------------------
# third-party-include
#------------------------------------------------------------------------------

def non_standard_angle_includes(checker, root: pathlib.Path) -> None:
    write(root, "engine/include/conjure/foo/bar.h",
          "#pragma once\n\n#include <spdlog/spdlog.h>\n#include <entt/entt.hpp>\n#include <vk_mem_alloc.h>\n")
    result = run(checker, root, "engine/include/conjure/foo/bar.h")
    assert len(result.of_rule("third-party-include")) == 3, result.findings


def standard_and_glm_allowed(checker, root: pathlib.Path) -> None:
    write(root, "engine/include/conjure/core/math/glm_types.h",
          "#pragma once\n\n#include <glm/glm.hpp>\n\n#include <cstdint>\n")
    write(root, "engine/include/conjure/foo/bar.h",
          "#pragma once\n\n#include \"conjure/core/math/glm_types.h\"\n\n#include <string_view>\n#include <vector>\n")
    result = run(checker, root, "engine/include")
    assert not result.of_rule("third-party-include"), result.findings


def glm_outside_glm_types(checker, root: pathlib.Path) -> None:
    write(root, "engine/include/conjure/foo/bar.h", "#pragma once\n\n#include <glm/vec3.hpp>\n")
    result = run(checker, root, "engine/include/conjure/foo/bar.h")
    assert len(result.of_rule("third-party-include")) == 1, result.findings


def quoted_non_conjure_include(checker, root: pathlib.Path) -> None:
    # spdlog is on no third-party list: only the allow-list rejects it.
    write(root, "engine/include/conjure/foo/bar.h", "#pragma once\n\n#include \"spdlog/spdlog.h\"\n")
    result = run(checker, root, "engine/include/conjure/foo/bar.h")
    assert len(result.of_rule("third-party-include")) == 1, result.findings


#------------------------------------------------------------------------------
# windows-macro
#------------------------------------------------------------------------------

def anonymous_enums_checked(checker, root: pathlib.Path) -> None:
    write(root, "engine/src/foo/bar.cpp", "enum { ERROR, DELETE };\nenum : int { IN };\n")
    result = run(checker, root, "engine/src/foo/bar.cpp")
    assert len(result.of_rule("windows-macro")) == 3, result.findings


def all_caps_const_checked(checker, root: pathlib.Path) -> None:
    write(root, "engine/src/foo/bar.cpp", "static const int NEAR = 1;\n")
    result = run(checker, root, "engine/src/foo/bar.cpp")
    assert len(result.of_rule("windows-macro")) == 1, result.findings


def camel_case_const_local_is_variable(checker, root: pathlib.Path) -> None:
    write(root, "engine/src/foo/bar.cpp", "float clip() {\n    const float near = 1.0f;\n    return near;\n}\n")
    result = run(checker, root, "engine/src/foo/bar.cpp")
    assert not result.of_rule("windows-macro"), result.findings


def define_of_macro_name(checker, root: pathlib.Path) -> None:
    write(root, "engine/src/foo/bar.cpp", "#define FAR 1\n#define CONJURE_FAR 2\n")
    result = run(checker, root, "engine/src/foo/bar.cpp")
    assert len(result.of_rule("windows-macro")) == 1, result.findings


#------------------------------------------------------------------------------
# tests-mirror-headers
#------------------------------------------------------------------------------

HEADER = "#pragma once\n"
TEST_FILE = "#include \"conjure/testing/doctest.h\"\n"


def one_test_file_two_headers(checker, root: pathlib.Path) -> None:
    write(root, "engine/include/conjure/foo/a/types.h", HEADER)
    write(root, "engine/include/conjure/foo/b/types.h", HEADER)
    write(root, "tests/foo/test_types.cpp", TEST_FILE)
    write(root, "tests/foo/CMakeLists.txt", "conjure_add_test_module(foo SOURCES test_types.cpp)\n")
    result = run(checker, root, "--tests-mirror-headers")
    mirror = result.of_rule("tests-mirror-headers")
    assert len(mirror) == 1 and "already mirrors conjure/foo/a/types.h" in mirror[0], result.findings


def sub_directory_test_wins(checker, root: pathlib.Path) -> None:
    write(root, "engine/include/conjure/foo/a/types.h", HEADER)
    write(root, "engine/include/conjure/foo/b/types.h", HEADER)
    write(root, "tests/foo/a/test_types.cpp", TEST_FILE)
    write(root, "tests/foo/b/test_types.cpp", TEST_FILE)
    write(root, "tests/foo/CMakeLists.txt", "add_subdirectory(a)\nadd_subdirectory(b)\n")
    write(root, "tests/foo/a/CMakeLists.txt", "conjure_add_test_module(foo_a SOURCES test_types.cpp)\n")
    write(root, "tests/foo/b/CMakeLists.txt", "conjure_add_test_module(foo_b SOURCES test_types.cpp)\n")
    result = run(checker, root, "--tests-mirror-headers")
    assert not result.of_rule("tests-mirror-headers"), result.findings

    # A third header of that stem with no test in its own sub-directory cannot borrow one of the others.
    write(root, "engine/include/conjure/foo/c/types.h", HEADER)
    result = run(checker, root, "--tests-mirror-headers")
    mirror = result.of_rule("tests-mirror-headers")
    assert len(mirror) == 1 and "several test_types.cpp" in mirror[0], result.findings


def unlisted_test_file(checker, root: pathlib.Path) -> None:
    write(root, "engine/include/conjure/foo/widget.h", HEADER)
    write(root, "tests/foo/test_widget.cpp", TEST_FILE)
    write(root, "tests/foo/CMakeLists.txt", "conjure_add_test_module(foo SOURCES test_other_widget.cpp)\n")
    result = run(checker, root, "--tests-mirror-headers")
    mirror = result.of_rule("tests-mirror-headers")
    assert len(mirror) == 1 and "not listed in any CMakeLists.txt" in mirror[0], result.findings


def benchmark_headers_mirrored(checker, root: pathlib.Path) -> None:
    write(root, "tests/benchmarks/include/conjure/bench/runner.h", HEADER)
    result = run(checker, root, "--tests-mirror-headers")
    mirror = result.of_rule("tests-mirror-headers")
    assert len(mirror) == 1 and "tests/benchmarks/tests/**/test_runner.cpp" in mirror[0], result.findings

    write(root, "tests/benchmarks/tests/test_runner.cpp", TEST_FILE)
    write(root, "tests/benchmarks/tests/CMakeLists.txt",
          "conjure_add_test_module(benchmarks SOURCES test_runner.cpp)\n")
    result = run(checker, root, "--tests-mirror-headers")
    assert not result.of_rule("tests-mirror-headers"), result.findings


#------------------------------------------------------------------------------
# Usage errors
#------------------------------------------------------------------------------

def missing_path(checker, root: pathlib.Path) -> None:
    write(root, "engine/src/foo/bar.cpp", "int value = 0;\n")
    result = run(checker, root, "engine/src/foo/missing.cpp")
    assert result.code == 2 and "does not exist" in result.errors, (result.code, result.errors)


def path_outside_root(checker, root: pathlib.Path) -> None:
    repository = root / "repository"
    write(repository, "engine/src/foo/bar.cpp", "int value = 0;\n")
    outside = write(root, "elsewhere/bar.cpp", "int value = 0;\n")
    result = run(checker, repository, str(outside))
    assert result.code == 2 and "outside" in result.errors, (result.code, result.errors)


def non_source_argument(checker, root: pathlib.Path) -> None:
    write(root, "docs/readme.md", "# Readme\n")
    result = run(checker, root, "docs/readme.md")
    assert result.code == 2 and "not a C++ source file" in result.errors, (result.code, result.errors)


def no_source_under_paths(checker, root: pathlib.Path) -> None:
    write(root, "docs/readme.md", "# Readme\n")
    result = run(checker, root, "docs")
    assert result.code == 2 and "no C++ source file" in result.errors, (result.code, result.errors)


def malformed_exceptions(checker, root: pathlib.Path) -> None:
    write(root, "engine/src/foo/bar.cpp", "int value = 0;\n")
    write(root, "tools/style_exceptions.json", "{ \"forbidden_in_tests\": ")
    result = run(checker, root)
    assert result.code == 2 and "style_exceptions.json" in result.errors, (result.code, result.errors)

    write(root, "tools/style_exceptions.json", "{ \"forbidden_in_tests\": [\"tests/a.cpp\"] }")
    result = run(checker, root)
    assert result.code == 2 and "must map paths to reasons" in result.errors, (result.code, result.errors)


CASES = [
    ("strip_comments - a digit separator does not start a character literal", digit_separator_is_code),
    ("strip_comments - a raw string holding a quote keeps the code after it", raw_string_keeps_later_code),
    ("strip_comments - newlines inside literals are kept", literal_newlines_kept),
    ("windows-macro - an enum after a digit separator is still checked", enum_after_digit_separator),
    ("third-party-include - any non-standard angle include in a public header is reported",
     non_standard_angle_includes),
    ("third-party-include - standard headers and glm in glm_types.h are allowed", standard_and_glm_allowed),
    ("third-party-include - glm outside glm_types.h is reported", glm_outside_glm_types),
    ("third-party-include - a quoted non-Conjure include in a public header is reported", quoted_non_conjure_include),
    ("windows-macro - enumerators of anonymous enums are checked", anonymous_enums_checked),
    ("windows-macro - an ALL_CAPS const constant is checked", all_caps_const_checked),
    ("windows-macro - a camelCase const local is a variable, not a constant", camel_case_const_local_is_variable),
    ("windows-macro - a #define of a Windows macro name is reported", define_of_macro_name),
    ("tests-mirror-headers - one test file cannot mirror two headers", one_test_file_two_headers),
    ("tests-mirror-headers - the test in the header's sub-directory wins", sub_directory_test_wins),
    ("tests-mirror-headers - a test file no CMakeLists.txt lists does not count", unlisted_test_file),
    ("tests-mirror-headers - benchmark harness headers need a test file", benchmark_headers_mirrored),
    ("check_style - a missing path argument is a usage error", missing_path),
    ("check_style - a path outside --root is a usage error", path_outside_root),
    ("check_style - a non-source file argument is a usage error", non_source_argument),
    ("check_style - paths that match no source file are a usage error", no_source_under_paths),
    ("check_style - malformed style_exceptions.json is a usage error", malformed_exceptions),
]


def main() -> int:
    checker = load_checker()
    failures: list[str] = []
    with tempfile.TemporaryDirectory(prefix="check_style_") as directory:
        for index, (title, case) in enumerate(CASES):
            root = pathlib.Path(directory) / f"case_{index:02d}"
            root.mkdir()
            try:
                case(checker, root)
            except (Exception, SystemExit) as failure:
                failures.append(f"{title}: {type(failure).__name__}: {failure}")
                continue
            print(f"ok: {title}")

    if failures:
        print("\n".join(f"FAIL: {failure}" for failure in failures))
        return 1
    print(f"style.check_tool: {len(CASES)} case(s) passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
