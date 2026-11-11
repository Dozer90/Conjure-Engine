#!/usr/bin/env python3
"""Skip discipline for CI (docs/design/08_testing.md D12, 3.2, 7.3).

A test that cannot run on a machine exits with 77 (SKIP_RETURN_CODE), which CTest reports as *Not Run*
instead of a failure. On a developer machine that is the right outcome; on a CI runner that was set up
for the test (the .NET SDK is installed, the window tests are tagged) a skip hides a broken set-up. This
tool reads the JUnit reports `ctest --output-junit` wrote (one per ctest run of the job: the preset run and
the label run) and the test labels from `ctest --show-only=json-v1`, and fails when a skipped test carries
neither an allowed label nor an allowed name tag:

    python tools/ctest_check_skips.py --build-dir build/<preset> --junit build/<preset>/junit.xml \\
        --junit build/<preset>/junit_labels.xml --allow-tag window --allow-tag downloaded --allow-tag audio_device

    CONJURE_TEST_ALLOWED_SKIPS=golden          # labels whose skips are expected (comma separated); CI
                                               # sets `golden` on pull requests and nothing nightly

Name tags are the bracketed words of a case name (`[window]`, `[downloaded]`, `[gpu]`, `[audio_device]`): a
window test skips wherever no display exists, a downloaded-asset test wherever tools/download_test_assets.py
has nothing to fetch, a `[gpu]` test wherever no software adapter exists (D3D12 WARP is Windows only, so CI
allows the tag on Linux and macOS and not on Windows), an `[audio_device]` test wherever no playback endpoint
exists (every hosted runner). A `[dotnet]` skip is never allowed on CI, which installs the SDK.

Exit code 0 when every skip is allowed (or there is none), 1 when a skip is not, 2 on usage errors
(missing files, unreadable JUnit or JSON). --self-test runs the matcher over embedded fixtures and, when
ctest is on the PATH, reads a generated CTestTestfile.cmake through `ctest --show-only=json-v1` so a schema
change of CTest is caught.
"""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as element_tree

EXIT_CLEAN = 0
EXIT_UNEXPECTED_SKIPS = 1
EXIT_USAGE = 2

ALLOWED_LABELS_VARIABLE = "CONJURE_TEST_ALLOWED_SKIPS"
NAME_TAG = re.compile(r"\[([A-Za-z0-9_]+)\]")


class skipped_test:
    def __init__(self, name: str, labels: list[str], reason: str) -> None:
        self.name = name
        self.labels = labels
        self.reason = reason

    def tags(self) -> set[str]:
        return {match.group(1).lower() for match in NAME_TAG.finditer(self.name)}


#------------------------------------------------------------------------------
# Parsing
#------------------------------------------------------------------------------

def parse_junit(text: str) -> list[tuple[str, str]]:
    """(name, reason) of every skipped test case of a CTest JUnit report."""
    root = element_tree.fromstring(text)
    skipped: list[tuple[str, str]] = []
    for testcase in root.iter("testcase"):
        marker = testcase.find("skipped")
        if marker is None:
            continue
        name = testcase.get("name", "")
        reason = marker.get("message", "") or (marker.text or "").strip()
        skipped.append((name, reason))
    return skipped


def parse_labels(text: str) -> dict[str, list[str]]:
    """Test name -> labels from `ctest --show-only=json-v1`."""
    document = json.loads(text)
    labels_by_name: dict[str, list[str]] = {}
    for test in document.get("tests", []):
        labels: list[str] = []
        for entry in test.get("properties", []):
            if entry.get("name") == "LABELS":
                value = entry.get("value", [])
                labels = list(value) if isinstance(value, list) else [str(value)]
        labels_by_name[test.get("name", "")] = labels
    return labels_by_name


def collect_skips(junit_text: str, labels_text: str) -> list[skipped_test]:
    labels_by_name = parse_labels(labels_text)
    return [skipped_test(name, labels_by_name.get(name, []), reason) for name, reason in parse_junit(junit_text)]


def collect_skips_of_reports(junit_texts: list[str], labels_text: str) -> list[skipped_test]:
    """The skips of several JUnit reports (one ctest run each) against one label listing."""
    skips: list[skipped_test] = []
    for junit_text in junit_texts:
        skips.extend(collect_skips(junit_text, labels_text))
    return skips


#------------------------------------------------------------------------------
# Policy
#------------------------------------------------------------------------------

def unexpected_skips(skips: list[skipped_test], allowed_labels: set[str], allowed_tags: set[str]) -> list[skipped_test]:
    lowered_labels = {label.lower() for label in allowed_labels if label}
    lowered_tags = {tag.lower() for tag in allowed_tags if tag}
    unexpected: list[skipped_test] = []
    for skip in skips:
        if any(label.lower() in lowered_labels for label in skip.labels):
            continue
        if skip.tags() & lowered_tags:
            continue
        unexpected.append(skip)
    return unexpected


def allowed_labels_from_environment(environment: dict[str, str]) -> set[str]:
    raw = environment.get(ALLOWED_LABELS_VARIABLE, "")
    return {part.strip() for part in raw.split(",") if part.strip()}


#------------------------------------------------------------------------------
# Running
#------------------------------------------------------------------------------

def show_only_json(build_dir: pathlib.Path) -> str | None:
    command = ["ctest", "--test-dir", str(build_dir), "--show-only=json-v1"]
    completed = subprocess.run(command, capture_output=True, text=True, errors="replace")
    if completed.returncode != 0:
        print(f"ctest_check_skips: {' '.join(command)} failed ({completed.returncode}): {completed.stderr.strip()}",
              file=sys.stderr)
        return None
    return completed.stdout


def run_check(
    build_dir: pathlib.Path, junits: list[pathlib.Path], allowed_labels: set[str], allowed_tags: set[str]
) -> int:
    for junit in junits:
        if not junit.is_file():
            print(f"ctest_check_skips: JUnit report {junit} does not exist (run ctest with --output-junit)",
                  file=sys.stderr)
            return EXIT_USAGE
    if not build_dir.is_dir():
        print(f"ctest_check_skips: {build_dir} is not a directory", file=sys.stderr)
        return EXIT_USAGE
    labels_text = show_only_json(build_dir)
    if labels_text is None:
        return EXIT_USAGE
    try:
        junit_texts = [junit.read_text(encoding="utf-8", errors="replace") for junit in junits]
        skips = collect_skips_of_reports(junit_texts, labels_text)
    except (element_tree.ParseError, json.JSONDecodeError) as failure:
        print(f"ctest_check_skips: cannot read the reports: {failure}", file=sys.stderr)
        return EXIT_USAGE

    unexpected = unexpected_skips(skips, allowed_labels, allowed_tags)
    print(
        f"ctest_check_skips: {len(skips)} skipped test(s) in {len(junits)} report(s), {len(unexpected)} unexpected "
        f"(allowed labels: {', '.join(sorted(allowed_labels)) or 'none'}; allowed tags: "
        f"{', '.join(sorted(allowed_tags)) or 'none'})"
    )
    for skip in unexpected:
        labels = ", ".join(skip.labels) or "no labels"
        print(f"  {skip.name} [{labels}]: {skip.reason or 'skipped'}")
    if unexpected:
        print(
            "ctest_check_skips: a skipped test on CI means the runner lacks something it was set up to have "
            f"(docs/design/08_testing.md D12); fix the set-up or allow the label through {ALLOWED_LABELS_VARIABLE}.",
            file=sys.stderr,
        )
        return EXIT_UNEXPECTED_SKIPS
    return EXIT_CLEAN


#------------------------------------------------------------------------------
# Self-test
#------------------------------------------------------------------------------

SELF_TEST_JUNIT = """<?xml version="1.0" encoding="UTF-8"?>
<testsuite name="all" tests="7" failures="0" disabled="0" skipped="6" hostname="ci" time="1.0">
  <testcase name="platform.backend_conformance - title round trips&lt;glfw [window]&gt;" classname="all" time="0.0">
    <skipped message="precondition REAL_WINDOW not met"/>
  </testcase>
  <testcase name="audio.decoder - [downloaded] ogg decodes" classname="all" time="0.0">
    <skipped message="precondition DOWNLOADED_ASSETS not met"/>
  </testcase>
  <testcase name="scripting.host - starts [dotnet]" classname="all" time="0.0">
    <skipped/>
  </testcase>
  <testcase name="golden.pbr_spheres - matches the reference" classname="all" time="0.0">
    <skipped message="golden images disabled"/>
  </testcase>
  <testcase name="rhi.device - creates on the software adapter [gpu]" classname="all" time="0.0">
    <skipped message="precondition SOFTWARE_GPU_ADAPTER not met"/>
  </testcase>
  <testcase name="audio.miniaudio_backend - opens the default device [audio_device]" classname="all" time="0.0">
    <skipped message="precondition AUDIO_DEVICE not met"/>
  </testcase>
  <testcase name="core_math.vec3 - adds" classname="all" time="0.0"/>
</testsuite>
"""

# The label run of a job (ctest -L "downloaded|process") writes its own report: a process case that exits 77.
SELF_TEST_JUNIT_LABEL_RUN = """<?xml version="1.0" encoding="UTF-8"?>
<testsuite name="all" tests="2" failures="0" disabled="0" skipped="1" hostname="ci" time="1.0">
  <testcase name="editor.editor_process - runs a command file [process]" classname="all" time="0.0">
    <skipped message="precondition EDITOR_EXECUTABLE not met"/>
  </testcase>
  <testcase name="runtime.distribution_symbols" classname="all" time="0.0"/>
</testsuite>
"""

SELF_TEST_LABELS = json.dumps({
    "tests": [
        {"name": "platform.backend_conformance - title round trips<glfw [window]>",
         "properties": [{"name": "LABELS", "value": ["unit", "platform"]}]},
        {"name": "audio.decoder - [downloaded] ogg decodes",
         "properties": [{"name": "LABELS", "value": ["unit", "audio", "downloaded"]}]},
        {"name": "scripting.host - starts [dotnet]",
         "properties": [{"name": "LABELS", "value": ["unit", "scripting", "dotnet"]}]},
        {"name": "golden.pbr_spheres - matches the reference",
         "properties": [{"name": "LABELS", "value": ["golden"]}]},
        {"name": "rhi.device - creates on the software adapter [gpu]",
         "properties": [{"name": "LABELS", "value": ["unit", "rhi"]}]},
        {"name": "audio.miniaudio_backend - opens the default device [audio_device]",
         "properties": [{"name": "LABELS", "value": ["unit", "audio"]}]},
        {"name": "editor.editor_process - runs a command file [process]",
         "properties": [{"name": "LABELS", "value": ["unit", "editor", "process"]}]},
        {"name": "runtime.distribution_symbols", "properties": [{"name": "LABELS", "value": ["process", "runtime"]}]},
        {"name": "core_math.vec3 - adds", "properties": [{"name": "LABELS", "value": ["unit", "core_math"]}]},
    ]
})

# A minimal CTestTestfile.cmake: `ctest --show-only=json-v1` over it exercises the real schema parse_labels reads.
SELF_TEST_CTEST_FILE = """add_test(probe.one "cmake" "-E" "true")
set_tests_properties(probe.one PROPERTIES LABELS "unit;probe" TIMEOUT 10)
add_test(probe.two "cmake" "-E" "true")
"""

# The pull-request tag policies of .github/workflows/ci.yml per leg.
PR_TAGS_WINDOWS = {"window", "downloaded", "audio_device"}
PR_TAGS_POSIX = PR_TAGS_WINDOWS | {"gpu"}


def self_test_ctest_schema(expect) -> None:
    """Runs `ctest --show-only=json-v1` on a generated test file and checks the schema parse_labels expects."""
    if shutil.which("ctest") is None:
        print("self-test note: ctest is not on the PATH; the --show-only=json-v1 schema case did not run")
        return
    with tempfile.TemporaryDirectory(prefix="ctest_check_skips_") as directory:
        build_dir = pathlib.Path(directory)
        (build_dir / "CTestTestfile.cmake").write_text(SELF_TEST_CTEST_FILE, encoding="utf-8")
        text = show_only_json(build_dir)
        expect("ctest --show-only=json-v1 runs over a generated CTestTestfile.cmake", text is not None)
        if text is None:
            return
        labels = parse_labels(text)
        # CTest sorts the LABELS it reports, so the order is not the one the test file wrote.
        expect("parse_labels reads LABELS from the real schema", sorted(labels.get("probe.one", [])) == ["probe", "unit"],
               str(labels))
        expect("a test without labels is listed with none", labels.get("probe.two") == [], str(labels))


def run_self_test() -> int:
    failures = 0
    cases = 0

    def expect(title: str, condition: bool, detail: str = "") -> None:
        nonlocal failures, cases
        cases += 1
        if condition:
            print(f"self-test ok: {title}")
        else:
            failures += 1
            print(f"self-test FAILED: {title}{(': ' + detail) if detail else ''}")

    skips = collect_skips(SELF_TEST_JUNIT, SELF_TEST_LABELS)
    expect("six skipped cases are read from the JUnit report", len(skips) == 6, str(len(skips)))
    expect("labels are joined by name", skips[1].labels == ["unit", "audio", "downloaded"], str(skips[1].labels))
    expect("name tags are extracted", skips[0].tags() == {"window"}, str(skips[0].tags()))

    none_allowed = unexpected_skips(skips, set(), set())
    expect("without allowances every skip is unexpected", len(none_allowed) == 6, str(len(none_allowed)))

    pull_request_posix = unexpected_skips(skips, {"golden"}, PR_TAGS_POSIX)
    expect("the Linux and macOS pull-request policy leaves only the dotnet skip",
           [skip.name for skip in pull_request_posix] == ["scripting.host - starts [dotnet]"],
           str([skip.name for skip in pull_request_posix]))

    pull_request_windows = unexpected_skips(skips, {"golden"}, PR_TAGS_WINDOWS)
    expect("the Windows pull-request policy flags the dotnet and the gpu skip (WARP exists there)",
           sorted(skip.name for skip in pull_request_windows)
           == ["rhi.device - creates on the software adapter [gpu]", "scripting.host - starts [dotnet]"],
           str([skip.name for skip in pull_request_windows]))

    nightly = unexpected_skips(skips, set(), {"window", "audio_device", "gpu"})
    expect("the nightly policy flags downloaded, dotnet and golden skips", len(nightly) == 3, str(len(nightly)))

    both_runs = collect_skips_of_reports([SELF_TEST_JUNIT, SELF_TEST_JUNIT_LABEL_RUN], SELF_TEST_LABELS)
    expect("the label run's report is examined too", len(both_runs) == 7, str(len(both_runs)))
    process_skip = unexpected_skips(both_runs, {"golden"}, PR_TAGS_POSIX)
    expect("a process case that exited 77 is unexpected",
           "editor.editor_process - runs a command file [process]" in [skip.name for skip in process_skip],
           str([skip.name for skip in process_skip]))

    labels = allowed_labels_from_environment({ALLOWED_LABELS_VARIABLE: "golden, slow,"})
    expect("the environment variable is split on commas", labels == {"golden", "slow"}, str(labels))
    expect("an absent variable allows nothing", allowed_labels_from_environment({}) == set())

    self_test_ctest_schema(expect)

    print(f"ctest_check_skips: self-test {cases} case(s), {failures} failure(s)")
    return EXIT_UNEXPECTED_SKIPS if failures else EXIT_CLEAN


#------------------------------------------------------------------------------
# Entry point
#------------------------------------------------------------------------------

def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build-dir", default=None, help="the configured build directory ctest ran in")
    parser.add_argument("--junit", action="append", default=[], metavar="FILE",
                        help="a report ctest --output-junit wrote (repeatable: one per ctest run of the job)")
    parser.add_argument("--allow-label", action="append", default=[], metavar="LABEL",
                        help=f"a label whose skips are allowed (in addition to {ALLOWED_LABELS_VARIABLE})")
    parser.add_argument("--allow-tag", action="append", default=[], metavar="TAG",
                        help="a bracketed case-name tag whose skips are allowed, e.g. window")
    parser.add_argument("--self-test", action="store_true", help="run the matcher over embedded reports")
    args = parser.parse_args(argv)

    if args.self_test:
        return run_self_test()
    if not args.build_dir or not args.junit:
        parser.print_usage(sys.stderr)
        print("ctest_check_skips: pass --build-dir and --junit, or --self-test", file=sys.stderr)
        return EXIT_USAGE
    allowed_labels = allowed_labels_from_environment(dict(os.environ)) | set(args.allow_label)
    junits = [pathlib.Path(junit) for junit in args.junit]
    return run_check(pathlib.Path(args.build_dir), junits, allowed_labels, set(args.allow_tag))


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
