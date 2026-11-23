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
# third-party-include-in-test and doctest-include
#------------------------------------------------------------------------------

def reported_lines(result: run_result, rule: str, relative: str) -> list[str]:
    """The line numbers of the findings of 'rule' in the file 'relative', in output order."""
    return [line.split(":")[1] for line in result.of_rule(rule) if line.startswith(f"{relative}:")]


def third_party_in_test_reported(checker, root: pathlib.Path) -> None:
    write(root, "tests/foo/test_bar.cpp",
          "#include \"conjure/testing/doctest.h\"\n"     # 1
          "\n"
          "#include <nvrhi/nvrhi.h>\n"                   # 3: a root with a directory
          "#include <imgui.h>\n"                         # 4: a single-file library
          "#include <imgui_internal.h>\n"                # 5
          "#include <stb_image_write.h>\n"               # 6
          "#include <Jolt/Jolt.h>\n"                     # 7
          "#include <spdlog/spdlog.h>\n"                 # 8: on no list, but a directory prefix is third-party
          "#include <d3d12.h>\n"                         # 9: a graphics API, reached through rhi
          "#include <vulkan/vulkan.h>\n"                 # 10
          "#include \"imgui.h\"\n"                       # 11: quoted, still a library
          "#if CONJURE_BUILD_EDITOR\n"
          "    #include <ImGuizmo.h>\n"                  # 13: a conditional include still reaches the library
          "#endif\n")
    write(root, "tests/foo/foo_test_support.h", "#pragma once\n\n#include <jxl/decode.h>\n")
    result = run(checker, root)
    assert reported_lines(result, "third-party-include-in-test", "tests/foo/test_bar.cpp") == [
        "3", "4", "5", "6", "7", "8", "9", "10", "11", "13"], result.findings
    assert reported_lines(result, "third-party-include-in-test", "tests/foo/foo_test_support.h") == ["3"], \
        result.findings
    assert result.code == 1, result.code
    # The message names the include as written and the table that would allow it.
    messages = {line.split(":")[1]: line for line in result.of_rule("third-party-include-in-test")
                if line.startswith("tests/foo/test_bar.cpp:")}
    assert "the third-party header <spdlog/spdlog.h>;" in messages["8"], messages["8"]
    assert "\"third_party_in_tests\" in tools/style_exceptions.json" in messages["8"], messages["8"]
    assert "the third-party header \"imgui.h\";" in messages["11"], messages["11"]


def third_party_in_test_leaves_other_headers(checker, root: pathlib.Path) -> None:
    write(root, "tests/foo/test_bar.cpp",
          "#include \"conjure/testing/doctest.h\"\n"
          "#include \"bar_test_support.h\"\n"
          "\n"
          "#include \"conjure/core/types.h\"\n"
          "#include \"core/jobs/thread_name.h\"\n"
          "\n"
          "#include <cstdint>\n"
          "#include <vector>\n"
          "#if CONJURE_PLATFORM_WINDOWS\n"
          "    #include <Windows.h>\n"
          "    #include <windows.h>\n"
          "    #include <winsock2.h>\n"
          "#else\n"
          "    #include <fcntl.h>\n"
          "    #include <sys/socket.h>\n"
          "    #include <netinet/in.h>\n"
          "    #include <unistd.h>\n"
          "#endif\n"
          "// #include <imgui.h>\n"
          "/* #include <nvrhi/nvrhi.h> */\n"
          # A source held in a multi-line raw string: its include line is text, not a directive.
          "const char *SOURCE = R\"(\n#include <imgui.h>\n)\";\n")
    result = run(checker, root, "tests/foo/test_bar.cpp")
    assert not result.of_rule("third-party-include-in-test"), result.findings


def third_party_outside_tests_not_this_rule(checker, root: pathlib.Path) -> None:
    write(root, "engine/src/foo/bar.cpp", "#include <imgui.h>\n#include <nvrhi/nvrhi.h>\n")
    write(root, "editor/src/ui/panel.cpp", "#include <imgui.h>\n")
    result = run(checker, root)
    assert not result.of_rule("third-party-include-in-test"), result.findings


def third_party_in_test_exception(checker, root: pathlib.Path) -> None:
    write(root, "tests/rhi/test_null_device.cpp", "#include <nvrhi/nvrhi.h>\n")
    write(root, "tests/rhi/test_other.cpp", "#include <nvrhi/nvrhi.h>\n")
    write(root, "tools/style_exceptions.json",
          "{ \"third_party_in_tests\": {"
          " \"tests/rhi/test_null_device.cpp\": \"the null device is an NVRHI device\" } }")
    # The exception covers its own file only, on a whole-repository run and on a run over the file.
    for arguments in ((), ("tests/rhi",), ("tests/rhi/test_null_device.cpp",)):
        result = run(checker, root, *arguments)
        reported = [line.split(":")[0] for line in result.of_rule("third-party-include-in-test")]
        expected = [] if arguments == ("tests/rhi/test_null_device.cpp",) else ["tests/rhi/test_other.cpp"]
        assert reported == expected, (arguments, result.findings)


def third_party_in_test_stale_exception(checker, root: pathlib.Path) -> None:
    write(root, "tests/foo/test_bar.cpp", "#include <vector>\n")
    write(root, "tests/foo/test_needed.cpp", "#include <imgui.h>\n")
    write(root, "tools/style_exceptions.json",
          "{ \"third_party_in_tests\": {"
          " \"tests/foo/test_bar.cpp\": \"used to include ImGui\","
          " \"tests/foo/test_needed.cpp\": \"drives ImGui\","
          " \"tests/foo/test_gone.cpp\": \"deleted since\" } }")
    result = run(checker, root)
    stale = sorted(line.split(": ", 2)[2] for line in result.of_rule("third-party-include-in-test"))
    assert stale == [
        "exception 'tests/foo/test_bar.cpp' names a file that includes no third-party header; remove it",
        "exception 'tests/foo/test_gone.cpp' names no C++ source under tests/; remove it",
    ], result.findings
    assert all(line.startswith("tools/style_exceptions.json:1:") for line in result.of_rule(
        "third-party-include-in-test")), result.findings

    # A run over some paths knows the files it checked, not the ones it skipped.
    result = run(checker, root, "tests/foo/test_bar.cpp")
    stale = [line.split(": ", 2)[2] for line in result.of_rule("third-party-include-in-test")]
    assert stale == [
        "exception 'tests/foo/test_bar.cpp' names a file that includes no third-party header; remove it",
    ], result.findings
    result = run(checker, root, "tests/foo/test_needed.cpp")
    assert not result.of_rule("third-party-include-in-test"), result.findings


def doctest_include_spellings(checker, root: pathlib.Path) -> None:
    write(root, "tests/foo/test_bar.cpp",
          "#include <doctest/doctest.h>\n#include <doctest.h>\n#include \"doctest/doctest.h\"\n"
          "#include \"doctest.h\"\n#include \"conjure/testing/doctest.h\"\n")
    write(root, "tests/support/include/conjure/testing/doctest.h", "#pragma once\n\n#include <doctest/doctest.h>\n")
    result = run(checker, root)
    assert reported_lines(result, "doctest-include", "tests/foo/test_bar.cpp") == ["1", "2", "3", "4"], \
        result.findings
    # Each message names the include as written, delimiters included.
    endings = [line.split("instead of ")[1] for line in result.of_rule("doctest-include")]
    assert endings == ["<doctest/doctest.h>", "<doctest.h>", "\"doctest/doctest.h\"", "\"doctest.h\""], \
        result.findings
    assert not result.of_rule("third-party-include-in-test"), result.findings
    assert not reported_lines(result, "doctest-include", "tests/support/include/conjure/testing/doctest.h"), \
        result.findings


def raw_string_lines_are_no_directives(checker, root: pathlib.Path) -> None:
    # Every include rule reads the same preprocessor lines: a line of a multi-line raw string that looks like a
    # directive is text. The unmatched #if in the first string must not hide the includes after it either.
    write(root, "tests/foo/test_bar.cpp",
          "#include \"conjure/testing/doctest.h\"\n"     # 1
          "\n"
          "const char *GUARD = R\"(\n#if 1\n)\";\n"      # 3-5
          "const char *SOURCE = R\"(\n"                  # 6
          "#include <vector>\n"                          # 7
          "#include \"conjure/core/types.h\"\n"          # 8: would break the include order
          "#include <doctest/doctest.h>\n"               # 9: would be a doctest include
          "#include <imgui.h>\n"                         # 10: would be a third-party include
          ")\";\n"
          "\n"
          "#include <vector>\n"                          # 13
          "#include \"conjure/core/types.h\"\n")         # 14: a real include-order finding
    write(root, "engine/include/conjure/foo/bar.h",
          "#pragma once\n\nconst char *SOURCE = R\"(\n#include <spdlog/spdlog.h>\n)\";\n")
    result = run(checker, root)
    for rule in ("doctest-include", "third-party-include-in-test", "third-party-include"):
        assert not result.of_rule(rule), (rule, result.findings)
    assert reported_lines(result, "include-order", "tests/foo/test_bar.cpp") == ["14"], result.findings
    assert not reported_lines(result, "include-order", "engine/include/conjure/foo/bar.h"), result.findings


def regression_third_party_in_tests(checker, root: pathlib.Path) -> None:
    # Code sweep F546: 08_testing.md 5.1 forbade third-party headers in tests, yet nothing reported the 18
    # files that included NVRHI, Dear ImGui or stb_image_write.
    write(root, "tests/editor/ui/test_gizmo_controller.cpp",
          "#include \"conjure/testing/doctest.h\"\n\n#include <imgui.h>\n")
    result = run(checker, root)
    reported = [line.split(": ")[0] for line in result.of_rule("third-party-include-in-test")]
    assert reported == ["tests/editor/ui/test_gizmo_controller.cpp:3"], result.findings
    assert result.code == 1, result.code


#------------------------------------------------------------------------------
# include-order
#------------------------------------------------------------------------------

def quoted_jpeg_xl_headers(checker, root: pathlib.Path) -> None:
    write(root, "engine/src/foo/bar.cpp",
          "#include \"jxl/decode.h\"\n#include \"hwy/base.h\"\n\n#include <jxl/encode.h>\n\n#include <vector>\n")
    result = run(checker, root, "engine/src/foo/bar.cpp")
    quoted = [line for line in result.of_rule("include-order") if "must use angle brackets" in line]
    assert [line.split(":")[1] for line in quoted] == ["1", "2"], result.findings


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
# thread-outside-jobs
#------------------------------------------------------------------------------

def thread_outside_jobs_reported(checker, root: pathlib.Path) -> None:
    write(root, "engine/src/foo/bar.cpp", "int value = 0;\nstd::thread worker;\nstd::jthread other;\n")
    write(root, "engine/include/conjure/foo/bar.h", "#pragma once\n\nstruct bar {\n    std::thread mThread;\n};\n")
    write(root, "editor/src/main.cpp", "std::vector<std::thread> pool;\n")
    write(root, "runtime/src/main.cpp", "auto reader = std::jthread([] {});\n")
    result = run(checker, root)
    reported = sorted(line.split(": ")[0] for line in result.of_rule("thread-outside-jobs"))
    assert reported == [
        "editor/src/main.cpp:1", "engine/include/conjure/foo/bar.h:4", "engine/src/foo/bar.cpp:2",
        "engine/src/foo/bar.cpp:3", "runtime/src/main.cpp:1",
    ], result.findings
    assert result.code == 1, result.code


def thread_in_jobs_allowed(checker, root: pathlib.Path) -> None:
    write(root, "engine/src/core/jobs/dedicated_thread.cpp", "std::jthread thread;\nstd::vector<std::thread> pool;\n")
    write(root, "engine/src/core/jobs/detail/worker.cpp", "std::thread worker;\n")
    write(root, "engine/include/conjure/core/jobs/dedicated_thread.h", "#pragma once\n\nstd::jthread mThread;\n")
    result = run(checker, root)
    assert not result.of_rule("thread-outside-jobs"), result.findings

    # Only the two jobs folders are allowed: a folder that merely starts with the name, the jobs folder of
    # another module and the other core areas are not.
    write(root, "engine/src/core/jobs_extra/pool.cpp", "std::thread worker;\n")
    write(root, "engine/src/foo/jobs/pool.cpp", "std::thread worker;\n")
    write(root, "engine/src/core/system/process.cpp", "std::thread reader;\n")
    write(root, "engine/include/conjure/core/system/process.h", "#pragma once\n\nstd::thread mReader;\n")
    result = run(checker, root)
    assert len(result.of_rule("thread-outside-jobs")) == 4, result.findings


def thread_names_that_create_no_thread(checker, root: pathlib.Path) -> None:
    write(root, "engine/src/foo/bar.cpp",
          "std::thread::id owner = std::this_thread::get_id();\n"
          "unsigned count = std::thread::hardware_concurrency();\n"
          "void wait() { std::this_thread::yield(); }\n"
          "my::thread worker;\nstd::threading value;\n")
    result = run(checker, root, "engine/src/foo/bar.cpp")
    assert not result.of_rule("thread-outside-jobs"), result.findings


def thread_in_comment_or_string(checker, root: pathlib.Path) -> None:
    write(root, "engine/src/foo/bar.cpp",
          "// std::thread is confined to core::jobs\n/* std::jthread too */\n"
          "const char *TEXT_LINE = \"std::thread worker;\";\nstd::thread real;\n")
    result = run(checker, root, "engine/src/foo/bar.cpp")
    lines = [line.split(":")[1] for line in result.of_rule("thread-outside-jobs")]
    assert lines == ["4"], result.findings


def thread_in_tests_is_the_other_rule(checker, root: pathlib.Path) -> None:
    write(root, "tests/foo/test_bar.cpp", "std::thread worker;\n")
    write(root, "tests/foo/test_allowed.cpp", "std::thread worker;\n")
    write(root, "tools/style_exceptions.json",
          "{ \"forbidden_in_tests\": { \"tests/foo/test_allowed.cpp\": \"the area sits below core::jobs\" } }")
    result = run(checker, root)
    assert not result.of_rule("thread-outside-jobs"), result.findings
    forbidden = result.of_rule("forbidden-in-tests")
    assert len(forbidden) == 1 and forbidden[0].startswith("tests/foo/test_bar.cpp:1:"), result.findings


def forbidden_in_tests_stale_exception(checker, root: pathlib.Path) -> None:
    write(root, "tests/foo/test_allowed.cpp", "std::thread worker;\n")
    write(root, "tools/style_exceptions.json",
          "{ \"forbidden_in_tests\": { \"tests/foo/test_allowed.cpp\": \"the area sits below core::jobs\","
          " \"tests/foo/test_gone.cpp\": \"deleted since\" } }")
    result = run(checker, root)
    assert result.of_rule("forbidden-in-tests") == [
        "tools/style_exceptions.json:1: forbidden-in-tests: exception 'tests/foo/test_gone.cpp' names a file that "
        "does not exist; remove it"], result.findings
    # A run over some paths cannot tell a missing file from one it skipped.
    result = run(checker, root, "tests/foo/test_allowed.cpp")
    assert not result.of_rule("forbidden-in-tests"), result.findings


def thread_created_without_the_type_reported(checker, root: pathlib.Path) -> None:
    write(root, "engine/src/foo/bar.cpp",
          "auto pending = std::async(std::launch::async, [] {});\n"
          "int started = pthread_create(&handle, nullptr, &entry, nullptr);\n")
    write(root, "editor/src/main.cpp", "HANDLE reader = ::CreateThread (nullptr, 0, &entry, nullptr, 0, nullptr);\n")
    write(root, "runtime/src/main.cpp",
          "auto first = _beginthread(&entry, 0, nullptr);\n"
          "auto second = _beginthreadex(nullptr, 0, &entry, nullptr, 0, nullptr);\n")
    # No folder is allowed these, the jobs folders included: core::jobs creates std::thread / std::jthread objects.
    write(root, "engine/src/core/jobs/job_system.cpp", "std::jthread worker;\nauto pending = std::async([] {});\n")
    write(root, "engine/include/conjure/core/jobs/pool.h",
          "#pragma once\n\nint start() { return pthread_create(nullptr, nullptr, nullptr, nullptr); }\n")
    result = run(checker, root)
    # Each finding is "<path>:<line>: thread-outside-jobs: <name> creates a thread ...".
    reported = sorted(line.split(" creates a thread ")[0] for line in result.of_rule("thread-outside-jobs"))
    assert reported == [
        "editor/src/main.cpp:1: thread-outside-jobs: CreateThread",
        "engine/include/conjure/core/jobs/pool.h:3: thread-outside-jobs: pthread_create",
        "engine/src/core/jobs/job_system.cpp:2: thread-outside-jobs: std::async",
        "engine/src/foo/bar.cpp:1: thread-outside-jobs: std::async",
        "engine/src/foo/bar.cpp:2: thread-outside-jobs: pthread_create",
        "runtime/src/main.cpp:1: thread-outside-jobs: _beginthread",
        "runtime/src/main.cpp:2: thread-outside-jobs: _beginthreadex",
    ], result.findings
    assert result.code == 1, result.code


def thread_call_names_that_create_no_thread(checker, root: pathlib.Path) -> None:
    write(root, "engine/src/foo/bar.cpp",
          "// std::async and pthread_create( are not used here\n"
          "const char *TEXT_LINE = \"CreateThread(_beginthreadex(\";\n"
          "auto value = std::async_value;\nint named = my_pthread_create(nullptr);\n"
          "auto work = CreateThreadpoolWork(&entry, nullptr, nullptr);\nvoid on_begin_thread();\n"
          "auto pending = my::async([] {});\nauto entry = &pthread_create;\n")
    # Test code has its own rule, which does not look for these.
    write(root, "tests/foo/test_bar.cpp", "auto pending = std::async([] {});\nint started = pthread_create();\n")
    result = run(checker, root)
    assert not result.of_rule("thread-outside-jobs"), result.findings
    assert not result.of_rule("forbidden-in-tests"), result.findings


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


def mirror_stale_exception(checker, root: pathlib.Path) -> None:
    write(root, "engine/include/conjure/foo/widget.h", HEADER)
    write(root, "tools/style_exceptions.json",
          "{ \"tests_mirror_headers\": { \"conjure/foo/widget.h\": \"tested through its owner\","
          " \"conjure/foo/gone.h\": \"deleted since\" } }")
    result = run(checker, root, "--tests-mirror-headers")
    assert result.of_rule("tests-mirror-headers") == [
        "tools/style_exceptions.json:1: tests-mirror-headers: exception 'conjure/foo/gone.h' names a header that "
        "does not exist; remove it"], result.findings


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

    for table in ("forbidden_in_tests", "tests_mirror_headers", "third_party_in_tests"):
        for value in ("[\"tests/a.cpp\"]", "{ \"tests/a.cpp\": 1 }", "\"tests/a.cpp\""):
            write(root, "tools/style_exceptions.json", f"{{ \"{table}\": {value} }}")
            result = run(checker, root)
            assert result.code == 2 and f"{table} must map paths to reasons" in result.errors, \
                (table, value, result.code, result.errors)


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
    ("third-party-include-in-test - every library include in test code is reported, conditional and quoted ones too",
     third_party_in_test_reported),
    ("third-party-include-in-test - Conjure, helper, standard and operating-system headers are not reported",
     third_party_in_test_leaves_other_headers),
    ("third-party-include-in-test - code outside tests/ is not checked", third_party_outside_tests_not_this_rule),
    ("third-party-include-in-test - a file listed under third_party_in_tests is not reported",
     third_party_in_test_exception),
    ("third-party-include-in-test - an entry naming a missing file or one without a library include is stale",
     third_party_in_test_stale_exception),
    ("doctest-include - doctest/ and a bare doctest.h are reported in either quote style", doctest_include_spellings),
    ("include rules - a directive-like line inside a multi-line raw string is text",
     raw_string_lines_are_no_directives),
    ("regression - code sweep F546 a third-party include in a test is reported", regression_third_party_in_tests),
    ("include-order - a quoted libjxl or Highway header must use angle brackets", quoted_jpeg_xl_headers),
    ("windows-macro - enumerators of anonymous enums are checked", anonymous_enums_checked),
    ("windows-macro - an ALL_CAPS const constant is checked", all_caps_const_checked),
    ("windows-macro - a camelCase const local is a variable, not a constant", camel_case_const_local_is_variable),
    ("windows-macro - a #define of a Windows macro name is reported", define_of_macro_name),
    ("thread-outside-jobs - a std::thread or std::jthread in engine, editor or runtime code is reported",
     thread_outside_jobs_reported),
    ("thread-outside-jobs - the two jobs folders of core are the only allowed places", thread_in_jobs_allowed),
    ("thread-outside-jobs - std::thread::id, hardware_concurrency and std::this_thread are not reported",
     thread_names_that_create_no_thread),
    ("thread-outside-jobs - a comment or a string literal is not reported", thread_in_comment_or_string),
    ("thread-outside-jobs - test code is left to forbidden-in-tests and its exception table",
     thread_in_tests_is_the_other_rule),
    ("forbidden-in-tests - an entry naming a missing file is stale on a whole-repository run",
     forbidden_in_tests_stale_exception),
    ("thread-outside-jobs - std::async and the raw OS thread calls are reported in every folder",
     thread_created_without_the_type_reported),
    ("thread-outside-jobs - a name that only resembles a thread call, a comment and a string are not reported",
     thread_call_names_that_create_no_thread),
    ("tests-mirror-headers - one test file cannot mirror two headers", one_test_file_two_headers),
    ("tests-mirror-headers - the test in the header's sub-directory wins", sub_directory_test_wins),
    ("tests-mirror-headers - a test file no CMakeLists.txt lists does not count", unlisted_test_file),
    ("tests-mirror-headers - benchmark harness headers need a test file", benchmark_headers_mirrored),
    ("tests-mirror-headers - an entry naming a missing header is stale", mirror_stale_exception),
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
