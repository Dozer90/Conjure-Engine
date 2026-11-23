#!/usr/bin/env python3
"""Distribution stripping check for the standalone runtime (docs/design/04_editor.md 8,
docs/ARCHITECTURE.md 8 and 11, docs/design/06_engine_loop.md 4.2).

A Distribution build ships without the editor: the root CMakeLists.txt forces CONJURE_BUILD_EDITOR off,
Dear ImGui is not fetched, and conjure_runtime links conjure_shared and nothing else from the engine.
This tool proves it on the built binaries. It lists the symbols of the Distribution conjure_runtime (its
own symbol table and its imports) and of the conjure_shared library next to it (its exports and symbol
table) with `dumpbin /SYMBOLS /IMPORTS /EXPORTS` (MSVC) or `llvm-nm` / `nm` (ELF, Mach-O), and fails when
any symbol belongs to the editor or to Dear ImGui:

  conjure::editor     MSVC `...@editor@conjure@@...` and the back-referenced `...@editor@<digit>@...` of a
                      parameter or return type (`?set_editor@application@engine@conjure@@QEAAXPEAVeditor_layer@
                      editor@3@@Z`), Itanium `_ZN7conjure6editor...` and the substituted `NS_6editor...` /
                      `NS<n>_6editor...` of a nested type, an import of a conjure_editor binary
  ImGui / ImGuizmo    MSVC `...@ImGui@@...`, Itanium `_ZN5ImGui...` / `_ZN8ImGuizmo...`, and the ImGui
                      types (ImDrawData, ImFontAtlas, ImGuiIO, ...) in any mangling

The engine's own lower-case names (imgui_layer, imgui_renderer, set_imgui_draw_data) are not findings: the
check is case-sensitive and matches the library's PascalCase identifiers only, and the ImGui-type rule skips
those bridge symbols, whose parameter types name the forward-declared `ImDrawData` (02_renderer.md 7 lists
`renderer::set_imgui_draw_data(ImDrawData *)` in every configuration; R9).

Modes:
    --build-dir <dir> [--config <name>]   checks bin/conjure_runtime[.exe] and the conjure_shared library of a
                                          configured and built Distribution build directory. The directory's
                                          CMakeCache.txt must say CMAKE_BUILD_TYPE=Distribution (single-config
                                          generators) or --config must be Distribution (multi-config generators,
                                          where the binaries live in bin/<config>/); any other configuration
                                          exits 77 ("skipped", CTest's SKIP_RETURN_CODE) unless --any-config.
                                          A --config that disagrees with a single-config directory's
                                          CMAKE_BUILD_TYPE is a usage error.
    --binary <file> [...]                 checks the named binaries, whatever build produced them
    --self-test                           runs the matcher and the cache parser over embedded tool output, and
                                          the lookup of the binaries over a fake build directory

Options: --tool <dumpbin|llvm-nm|nm, a path or a program name looked up on PATH> (default: searched on PATH;
dumpbin on Windows),
--no-shared (skip the conjure_shared library next to the runtime), --max-findings <n> (printed per binary,
default 20), --any-config.

Exit code 0 when every binary is clean, 1 when an editor or ImGui symbol was found, 2 on usage errors
(missing directory or binary, missing tool, tool failure, disagreeing --config), 77 when the build directory is not a
Distribution one. Output is one `<binary>: <symbol>` line per finding.

CI (docs/handbook/ci.md): the Distribution leg builds the preset and then runs the CTest entry that
tests/runtime/CMakeLists.txt registers under the label "process", which the default test presets exclude:

    cmake --preset windows-msvc-dist && cmake --build --preset windows-msvc-dist
    ctest --test-dir build/windows-msvc-dist -L process --output-on-failure

or directly:

    python tools/check_distribution_symbols.py --build-dir build/windows-msvc-dist
"""

from __future__ import annotations

import argparse
import contextlib
import io
import pathlib
import platform
import re
import shutil
import subprocess
import sys
import tempfile
from typing import Callable

ROOT = pathlib.Path(__file__).resolve().parent.parent

EXIT_CLEAN = 0
EXIT_FINDINGS = 1
EXIT_USAGE = 2
EXIT_SKIPPED = 77

PROGRAM = "check_distribution_symbols"
DISTRIBUTION_CONFIG = "Distribution"
RUNTIME_NAMES = ("conjure_runtime.exe", "conjure_runtime")
SHARED_LIBRARY_NAMES = ("conjure_shared.dll", "libconjure_shared.so", "libconjure_shared.dylib")
DEFAULT_MAX_FINDINGS = 20

# (description, pattern). Patterns run over every line of the tool output; a line matching any of them is
# a finding. Case-sensitive on purpose: the engine's snake_case imgui_* names must stay clean.
FORBIDDEN_PATTERNS: tuple[tuple[str, re.Pattern[str]], ...] = (
    # MSVC spells a namespace once and back-references it afterwards with a digit: `editor_layer@editor@3@` is
    # conjure::editor::editor_layer inside a conjure::engine symbol. Any `@editor@<digit>@` counts: no other
    # root namespace in these binaries has an `editor` child.
    ("conjure::editor (MSVC)", re.compile(r"@editor@(?:conjure@|[0-9])@")),
    # Itanium substitutes `conjure` after its first use (S_, or S<n>_ for a longer prefix): `NS_6editor12...`.
    ("conjure::editor (Itanium)", re.compile(r"(?:7conjure|S[0-9A-Z]*_)6editor(?=[0-9L])")),
    ("conjure_editor binary", re.compile(r"\bconjure_editor(?:_core|_ui)?(?:\.dll|\.so|\.dylib)?\b")),
    ("ImGui namespace (MSVC)", re.compile(r"@ImGui@@|@ImGuizmo@@")),
    ("ImGui namespace (Itanium)", re.compile(r"_Z[A-Za-z]*N(?:K|V|KV)?(?:5ImGui|8ImGuizmo)\d*[A-Za-z_]")),
    # ImGui types in any mangling: ImDrawData, ImDrawList, ImFontAtlas, ImGuiIO, ImGuiContext, ImVec2, ...
    # (never preceded by a lower-case letter, so camelCase words ending in "...sIm..." are not matched).
    ("ImGui type", re.compile(r"(?<![a-z])Im(?:Gui|Draw|Font|Vec[24]|Vector|Rect|Guizmo|Pool|TextureID)[A-Za-z0-9_]*")),
)

# The engine's own ImGui bridge symbols (R9): their parameter types name the forward-declared ImDrawData in
# every configuration, so the "ImGui type" rule does not apply to them. The namespace rules still do.
ENGINE_IMGUI_BRIDGE = re.compile(r"set_imgui_draw_data|imgui_layer|imgui_renderer")
IMGUI_TYPE_RULE = "ImGui type"

DUMP_HEADER_PREFIX = "Dump of file"

CACHE_ENTRY = re.compile(r"^(?P<name>[A-Za-z0-9_\-]+):(?P<type>[A-Z]+)=(?P<value>.*)$")


class finding:
    def __init__(self, binary: pathlib.Path, description: str, symbol: str) -> None:
        self.binary = binary
        self.description = description
        self.symbol = symbol

    def format(self) -> str:
        return f"{self.binary.name}: {self.symbol}    [{self.description}]"


#------------------------------------------------------------------------------
# Matching
#------------------------------------------------------------------------------

def scan_symbol_text(binary: pathlib.Path, text: str) -> list[finding]:
    """Every line of tool output that names an editor or ImGui symbol, once per line."""
    findings: list[finding] = []
    seen: set[str] = set()
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line or line in seen:
            continue
        # dumpbin's `Dump of file <path>` header names the binary, not a symbol: a path through a
        # conjure_editor directory is not an editor symbol.
        if line.startswith(DUMP_HEADER_PREFIX):
            continue
        for description, pattern in FORBIDDEN_PATTERNS:
            if pattern.search(line) is not None:
                if description == IMGUI_TYPE_RULE and ENGINE_IMGUI_BRIDGE.search(line) is not None:
                    continue
                seen.add(line)
                findings.append(finding(binary, description, line))
                break
    return findings


#------------------------------------------------------------------------------
# CMake cache
#------------------------------------------------------------------------------

def read_cache(build_dir: pathlib.Path) -> dict[str, str]:
    """The `name=value` entries of CMakeCache.txt (types dropped); empty when the file is missing."""
    cache_path = build_dir / "CMakeCache.txt"
    if not cache_path.is_file():
        return {}
    return parse_cache(cache_path.read_text(encoding="utf-8", errors="replace"))


def parse_cache(text: str) -> dict[str, str]:
    entries: dict[str, str] = {}
    for line in text.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#") or stripped.startswith("//"):
            continue
        match = CACHE_ENTRY.match(stripped)
        if match is None:
            continue
        entries[match.group("name")] = match.group("value")
    return entries


def build_configuration(cache: dict[str, str], requested: str | None) -> tuple[str | None, bool]:
    """The configuration a build directory holds and whether the generator is multi-config.
    Single-config: CMAKE_BUILD_TYPE; None when a --config argument disagrees with it (a usage error).
    Multi-config: the --config argument, or '' when none was given."""
    build_type = cache.get("CMAKE_BUILD_TYPE", "")
    multi_config = not build_type and bool(cache.get("CMAKE_CONFIGURATION_TYPES", ""))
    if multi_config:
        return (requested or ""), True
    if requested and requested != build_type:
        return None, False
    return build_type, False


#------------------------------------------------------------------------------
# Binaries and tools
#------------------------------------------------------------------------------

def find_binaries(bin_dir: pathlib.Path, with_shared: bool) -> tuple[pathlib.Path | None, pathlib.Path | None]:
    runtime = next((bin_dir / name for name in RUNTIME_NAMES if (bin_dir / name).is_file()), None)
    shared = None
    if with_shared:
        shared = next((bin_dir / name for name in SHARED_LIBRARY_NAMES if (bin_dir / name).is_file()), None)
    return runtime, shared


def find_tool(
  explicit: str | None,
  binary: pathlib.Path,
  which: Callable[[str], str | None] = shutil.which,
) -> tuple[pathlib.Path | None, str]:
    """The symbol-listing tool for a binary: (path, 'dumpbin' | 'llvm-nm' | 'nm'); path None when missing.
    An explicit --tool is a path or a program name searched on PATH with 'which'."""
    if explicit:
        candidate: pathlib.Path | None = pathlib.Path(explicit)
        stem = candidate.stem.lower()
        if stem == "dumpbin":
            kind = "dumpbin"
        elif stem.startswith("llvm"):
            kind = "llvm-nm"
        else:
            kind = "nm"
        if not candidate.exists():
            found = which(explicit)
            candidate = pathlib.Path(found) if found else None
        return candidate, kind
    if binary.suffix.lower() in (".dll", ".exe") or platform.system() == "Windows":
        found = which("dumpbin")
        return (pathlib.Path(found) if found else None), "dumpbin"
    found = which("llvm-nm")
    if found:
        return pathlib.Path(found), "llvm-nm"
    found = which("nm")
    return (pathlib.Path(found) if found else None), "nm"


def tool_commands(tool: pathlib.Path, kind: str, binary: pathlib.Path) -> list[tuple[list[str], bool]]:
    """The invocations whose output is scanned, each with whether its failure is fatal. A stripped ELF or
    Mach-O binary has no static symbol table (nm fails with "no symbols"), so only the dynamic listing is
    required there; dumpbin prints every table in one run."""
    if kind == "dumpbin":
        return [([str(tool), "/SYMBOLS", "/IMPORTS", "/EXPORTS", str(binary)], True)]
    if kind == "llvm-nm":
        return [
            ([str(tool), str(binary)], False),
            ([str(tool), "--dynamic", str(binary)], True),
        ]
    if platform.system() == "Darwin":
        return [
            ([str(tool), str(binary)], False),
            ([str(tool), "-g", str(binary)], True),
        ]
    return [
        ([str(tool), str(binary)], False),
        ([str(tool), "-D", str(binary)], True),
    ]


def list_symbols(binary: pathlib.Path, tool: pathlib.Path, kind: str) -> str | None:
    """The combined tool output for a binary, or None when a required invocation failed."""
    output: list[str] = []
    for command, required in tool_commands(tool, kind, binary):
        completed = subprocess.run(command, capture_output=True, text=True, errors="replace")
        if completed.returncode != 0:
            if required:
                print(
                    f"check_distribution_symbols: {' '.join(command)} failed ({completed.returncode}): "
                    f"{completed.stderr.strip()}",
                    file=sys.stderr,
                )
                return None
            continue
        output.append(completed.stdout)
    return "\n".join(output)


def check_binaries(binaries: list[pathlib.Path], tool_argument: str | None, max_findings: int) -> int:
    all_findings: list[finding] = []
    for binary in binaries:
        tool, kind = find_tool(tool_argument, binary)
        if tool is None:
            wanted = tool_argument or ("dumpbin" if kind == "dumpbin" else "llvm-nm or nm")
            print(f"check_distribution_symbols: symbol-listing tool not found ({wanted}); pass --tool", file=sys.stderr)
            return EXIT_USAGE
        text = list_symbols(binary, tool, kind)
        if text is None:
            return EXIT_USAGE
        findings = scan_symbol_text(binary, text)
        line_count = sum(1 for line in text.splitlines() if line.strip())
        verdict = "clean" if not findings else f"{len(findings)} editor / ImGui symbol(s)"
        print(f"check_distribution_symbols: {binary.name}: {line_count} symbol line(s) scanned, {verdict}")
        for item in findings[:max_findings]:
            print(f"  {item.format()}")
        if len(findings) > max_findings:
            print(f"  ... {len(findings) - max_findings} more")
        all_findings.extend(findings)

    if all_findings:
        print(
            "check_distribution_symbols: the Distribution runtime must not reference the editor or Dear ImGui "
            "(docs/design/04_editor.md 8): only conjure_editor links the editor libraries and ImGui is fetched "
            "for editor builds only.",
            file=sys.stderr,
        )
        return EXIT_FINDINGS
    return EXIT_CLEAN


def read_build_cache(build_dir: pathlib.Path, program: str) -> dict[str, str] | None:
    """The CMakeCache.txt entries of a build directory; None, after a usage message prefixed with <program>, when it
    is no directory or holds no cache. Shared with tools/check_distribution_paths.py."""
    if not build_dir.is_dir():
        print(f"{program}: {build_dir} is not a directory", file=sys.stderr)
        return None
    cache = read_cache(build_dir)
    if not cache:
        print(f"{program}: {build_dir} has no CMakeCache.txt (not a configured build directory)", file=sys.stderr)
        return None
    return cache


def resolve_distribution_binaries(
  build_dir: pathlib.Path,
  cache: dict[str, str],
  config_argument: str | None,
  any_config: bool,
  with_shared: bool,
  program: str,
) -> tuple[int, list[pathlib.Path]]:
    """The binaries a check of a Distribution build directory reads: bin/conjure_runtime and, with <with_shared>,
    the conjure_shared library next to it (in bin/<config>/ under a multi-config generator). Returns EXIT_CLEAN and
    the binaries, or after a message prefixed with <program> EXIT_USAGE (disagreeing or missing --config, missing
    binary) or EXIT_SKIPPED (not a Distribution build, unless <any_config>) and no binaries. Shared with
    tools/check_distribution_paths.py."""
    configuration, multi_config = build_configuration(cache, config_argument)
    if configuration is None:
        print(
            f"{program}: --config {config_argument} disagrees with "
            f"CMAKE_BUILD_TYPE={cache.get('CMAKE_BUILD_TYPE', '')} of the single-config build directory {build_dir}",
            file=sys.stderr,
        )
        return EXIT_USAGE, []
    if multi_config and not configuration:
        print(f"{program}: a multi-config build directory needs --config <name>", file=sys.stderr)
        return EXIT_USAGE, []
    if configuration != DISTRIBUTION_CONFIG and not any_config:
        print(
            f"{program}: {build_dir} is a '{configuration or 'unknown'}' build, not a "
            f"{DISTRIBUTION_CONFIG} one; the check runs on Distribution binaries only (pass --any-config to force)"
        )
        return EXIT_SKIPPED, []

    bin_dir = build_dir / "bin"
    if multi_config and (bin_dir / configuration).is_dir():
        bin_dir = bin_dir / configuration
    runtime, shared = find_binaries(bin_dir, with_shared)
    if runtime is None:
        print(f"{program}: no conjure_runtime executable in {bin_dir} (is the build complete?)", file=sys.stderr)
        return EXIT_USAGE, []
    binaries = [runtime]
    if with_shared:
        if shared is None:
            print(f"{program}: no conjure_shared library in {bin_dir} (pass --no-shared to skip it)", file=sys.stderr)
            return EXIT_USAGE, []
        binaries.append(shared)
    return EXIT_CLEAN, binaries


def run_build_dir(
  build_dir_argument: str,
  config_argument: str | None,
  any_config: bool,
  with_shared: bool,
  tool_argument: str | None,
  max_findings: int,
) -> int:
    build_dir = pathlib.Path(build_dir_argument)
    cache = read_build_cache(build_dir, PROGRAM)
    if cache is None:
        return EXIT_USAGE
    code, binaries = resolve_distribution_binaries(build_dir, cache, config_argument, any_config, with_shared, PROGRAM)
    if code != EXIT_CLEAN:
        return code
    return check_binaries(binaries, tool_argument, max_findings)


def run_binaries(binary_arguments: list[str], tool_argument: str | None, max_findings: int) -> int:
    binaries: list[pathlib.Path] = []
    for argument in binary_arguments:
        binary = pathlib.Path(argument)
        if not binary.is_file():
            print(f"check_distribution_symbols: {binary} does not exist", file=sys.stderr)
            return EXIT_USAGE
        binaries.append(binary)
    return check_binaries(binaries, tool_argument, max_findings)


#------------------------------------------------------------------------------
# Self-test
#------------------------------------------------------------------------------

# The fixtures are real tool output: the dumpbin lines come from MSVC 14.5x, the nm lines from clang (Itanium
# ABI). The back-referenced symbols (`@editor@3@`, `@editor@2@`, `NS_6editor`) were produced by compiling a
# snippet in which engine and scene code names conjure::editor types.
def fixture(*lines: str) -> str:
    return "\n".join(lines) + "\n"


SELF_TEST_CLEAN_DUMPBIN = fixture(
    "Microsoft (R) COFF/PE Dumper Version 14.51.36231.0",
    "Copyright (C) Microsoft Corporation.  All rights reserved.",
    "",
    "",
    "Dump of file conjure_runtime.exe",
    "",
    "File Type: EXECUTABLE IMAGE",
    "",
    "  Section contains the following imports:",
    "",
    "    conjure_shared.dll",
    "             140004000 Import Address Table",
    "             140005A10 Import Name Table",
    "                     0 time date stamp",
    "                     0 Index of first forwarder reference",
    "",
    "                        12A ?initialise@application@engine@conjure@@QEAA?AV?$result@X@core@2@XZ",
    "                        2B1 ??0game_layer@engine@conjure@@QEAA@XZ",
    "                        3C4 ??0imgui_layer@engine@conjure@@QEAA@XZ",
    "                        410 ?set_imgui_draw_data@renderer@renderer@conjure@@QEAAXPEAUImDrawData@@@Z",
    "                        5F2 ?from_command_line@application_desc@engine@conjure@@SA?AV?$result@"
    "Uapplication_desc@engine@conjure@@@core@3@V?$span@$$CBV?$basic_string_view@DU?$char_traits@D@std@@@std@@"
    "$0?0@std@@AEBUengine_config@23@@Z",
    "                        601 ?run@application@engine@conjure@@QEAAHXZ",
    "                        602 ?load_editor_settings@project@engine@conjure@@QEAA_NXZ",
    "",
    "    KERNEL32.dll",
    "             140004100 Import Address Table",
    "                        1F4 GetLastError",
    "",
    "  Summary",
    "",
    "        1000 .data",
    "        3000 .rdata",
)

SELF_TEST_DIRTY_DUMPBIN = fixture(
    "Dump of file conjure_runtime.exe",
    "",
    "File Type: EXECUTABLE IMAGE",
    "",
    "  Section contains the following imports:",
    "",
    "    conjure_shared.dll",
    "                        12A ?initialise@application@engine@conjure@@QEAA?AV?$result@X@core@2@XZ",
    "                        2C0 ?dispatch@command_dispatcher@editor@conjure@@QEAA?AUcommand_outcome@23@"
    "AEBUcommand_request@23@@Z",
    "                        2C1 ?NewFrame@ImGui@@YAXXZ",
    "                        2C2 ?AddDrawCmd@ImDrawList@@QEAAXXZ",
    "                        2C3 ?BeginFrame@ImGuizmo@@YAXXZ",
    "                        2C4 ??0ImFontAtlas@@QEAA@XZ",
    "                        2C5 ?GetIO@ImGui@@YAAEAUImGuiIO@@XZ",
    "                        2C6 ?set_editor@application@engine@conjure@@QEAAXPEAVeditor_layer@editor@3@@Z",
    "                        2C7 ?inspect@scene@1conjure@@QEAAXAEBUcommand_request@editor@2@@Z",
    "",
    "    conjure_editor_ui.dll",
    "                        001 ?draw@editor_workspace@editor@conjure@@QEAAXXZ",
)

SELF_TEST_CLEAN_NM = fixture(
    "0000000000012340 T _ZN7conjure6engine11applicationC1ENS0_16application_descE",
    "0000000000012380 T _ZN7conjure6engine11application10initialiseEv",
    "0000000000013000 T _ZN7conjure6engine11imgui_layerC1Ev",
    "0000000000013100 T _ZN7conjure8renderer8renderer19set_imgui_draw_dataEP10ImDrawData",
    "                 U _ZN7conjure6engine11application3runEv",
    "                 U __cxa_finalize",
    "0000000000045000 D _ZN7conjure4core8LOG_COREE",
    "0000000000046000 T _ZNK7conjure5scene5scene7name_ofENS_3ecs6entityE",
    "0000000000046100 T _ZNK7conjure6engine7project20load_editor_settingsEv",
)

SELF_TEST_DIRTY_NM = fixture(
    "0000000000012340 T _ZN7conjure6engine11applicationC1ENS0_16application_descE",
    "                 U _ZN7conjure6editor18command_dispatcher8dispatchERKNS0_15command_requestE",
    "                 U _ZN5ImGui8NewFrameEv",
    "                 U _ZN10ImDrawList10AddDrawCmdEv",
    "                 U _ZN8ImGuizmo10BeginFrameEv",
    "                 U _ZNK7conjure6editor10undo_stack8can_undoEv",
    "0000000000013000 T _ZN11ImFontAtlasC1Ev",
    "0000000000013100 T _ZN7conjure6engine11application10set_editorEPNS_6editor12editor_layerE",
    "0000000000013200 T _ZN7conjure5scene5scene7inspectERKNS_6editor15command_requestE",
)

SELF_TEST_SINGLE_CONFIG_CACHE = fixture(
    "# This is the CMakeCache file.",
    "//Build type",
    "CMAKE_BUILD_TYPE:STRING=Distribution",
    "CONJURE_BUILD_EDITOR:BOOL=ON",
    "CMAKE_BUILD_TYPE-STRINGS:INTERNAL=Debug;Development;Distribution",
)

SELF_TEST_MULTI_CONFIG_CACHE = fixture(
    "CMAKE_BUILD_TYPE:STRING=",
    "CMAKE_CONFIGURATION_TYPES:STRING=Debug;Development;Distribution",
)


def run_self_test() -> int:
    failures = 0
    cases = 0
    binary = pathlib.Path("conjure_runtime.exe")

    def expect(title: str, condition: bool, detail: str = "") -> None:
        nonlocal failures, cases
        cases += 1
        if condition:
            print(f"self-test ok: {title}")
        else:
            failures += 1
            print(f"self-test FAILED: {title}{(': ' + detail) if detail else ''}")

    clean_dumpbin = scan_symbol_text(binary, SELF_TEST_CLEAN_DUMPBIN)
    expect("clean dumpbin output (imgui_layer and set_imgui_draw_data are not ImGui)", not clean_dumpbin,
           "; ".join(item.format() for item in clean_dumpbin))

    dirty_dumpbin = scan_symbol_text(binary, SELF_TEST_DIRTY_DUMPBIN)
    dirty_dumpbin_descriptions = sorted({item.description for item in dirty_dumpbin})
    expect("dirty dumpbin output reports every editor and ImGui import", len(dirty_dumpbin) == 10,
           f"found {len(dirty_dumpbin)}: {[item.symbol for item in dirty_dumpbin]}")
    expect("dirty dumpbin output names the editor namespace, the ImGui namespace, an ImGui type and the editor "
           "dll",
           dirty_dumpbin_descriptions == [
               "ImGui namespace (MSVC)", "ImGui type", "conjure::editor (MSVC)", "conjure_editor binary"],
           str(dirty_dumpbin_descriptions))
    back_referenced_msvc = [
        item.symbol for item in dirty_dumpbin
        if item.description == "conjure::editor (MSVC)" and "@editor@conjure@@" not in item.symbol
    ]
    expect("dirty dumpbin output reports an editor type back-referenced in a non-editor symbol",
           len(back_referenced_msvc) == 2 and all("@editor@" in symbol for symbol in back_referenced_msvc),
           str(back_referenced_msvc))

    clean_nm = scan_symbol_text(binary, SELF_TEST_CLEAN_NM)
    expect("clean nm output", not clean_nm, "; ".join(item.format() for item in clean_nm))

    dirty_nm = scan_symbol_text(binary, SELF_TEST_DIRTY_NM)
    expect("dirty nm output reports every editor and ImGui symbol", len(dirty_nm) == 8,
           f"found {len(dirty_nm)}: {[item.symbol for item in dirty_nm]}")
    expect("dirty nm output names the Itanium editor and ImGui manglings",
           {"conjure::editor (Itanium)", "ImGui namespace (Itanium)", "ImGui type"}
           <= {item.description for item in dirty_nm},
           str(sorted({item.description for item in dirty_nm})))
    substituted_itanium = [
        item.symbol for item in dirty_nm
        if item.description == "conjure::editor (Itanium)" and "NS_6editor" in item.symbol
    ]
    expect("dirty nm output reports an editor type substituted into a non-editor symbol",
           len(substituted_itanium) == 2, str(substituted_itanium))

    bridge_only = scan_symbol_text(
        binary, "  ?render@imgui_renderer@renderer@conjure@@QEAAXPEAUImDrawData@@PEAVIFramebuffer@nvrhi@@@Z\n")
    expect("the engine's bridge symbols are exempt from the ImGui-type rule only", not bridge_only,
           "; ".join(item.format() for item in bridge_only))
    bridge_in_editor = scan_symbol_text(binary, "  ?draw@imgui_panel@editor@conjure@@QEAAXPEAUImDrawData@@@Z\n")
    expect("an editor symbol with a bridge-like name is still a finding",
           [item.description for item in bridge_in_editor] == ["conjure::editor (MSVC)"],
           str([item.description for item in bridge_in_editor]))

    duplicate = scan_symbol_text(binary, "  ?NewFrame@ImGui@@YAXXZ\n  ?NewFrame@ImGui@@YAXXZ\n")
    expect("a repeated line is one finding", len(duplicate) == 1, str(len(duplicate)))

    header_only = scan_symbol_text(
        binary, "Dump of file D:\\work\\conjure_editor\\build\\bin\\conjure_runtime.exe\n"
                "Dump of file /home/ci/conjure_editor/build/bin/conjure_shared.dll\n")
    expect("a Dump of file header naming a conjure_editor directory is not a finding", not header_only,
           "; ".join(item.format() for item in header_only))

    def fake_which(name: str) -> str | None:
        return "/usr/lib/llvm/bin/llvm-nm" if name == "llvm-nm" else None

    resolved = find_tool("llvm-nm", binary, which=fake_which)
    expect("an explicit tool name is resolved on PATH",
           resolved == (pathlib.Path("/usr/lib/llvm/bin/llvm-nm"), "llvm-nm"), str(resolved))
    unresolved = find_tool("no_such_listing_tool", binary, which=fake_which)
    expect("an explicit tool name that is not on PATH is not found", unresolved[0] is None, str(unresolved))

    single = parse_cache(SELF_TEST_SINGLE_CONFIG_CACHE)
    expect("cache parser reads CMAKE_BUILD_TYPE", single.get("CMAKE_BUILD_TYPE") == "Distribution", str(single))
    expect("single-config build reports its configuration",
           build_configuration(single, None) == ("Distribution", False), str(build_configuration(single, None)))
    expect("single-config build with a disagreeing --config is a usage error",
           build_configuration(single, "Debug") == (None, False), str(build_configuration(single, "Debug")))
    expect("single-config build with an agreeing --config reports its configuration",
           build_configuration(single, "Distribution") == ("Distribution", False),
           str(build_configuration(single, "Distribution")))

    multi = parse_cache(SELF_TEST_MULTI_CONFIG_CACHE)
    expect("multi-config build without --config has no configuration",
           build_configuration(multi, None) == ("", True), str(build_configuration(multi, None)))
    expect("multi-config build takes --config",
           build_configuration(multi, "Distribution") == ("Distribution", True),
           str(build_configuration(multi, "Distribution")))

    # The lookup of the binaries both Distribution checks share, over a fake build directory.
    with tempfile.TemporaryDirectory(prefix="distribution_symbols_") as directory:
        build_dir = pathlib.Path(directory)
        (build_dir / "CMakeCache.txt").write_text(SELF_TEST_SINGLE_CONFIG_CACHE, encoding="utf-8")
        (build_dir / "bin").mkdir()
        (build_dir / "bin" / "conjure_runtime").write_bytes(b"\x00")
        messages = io.StringIO()
        with contextlib.redirect_stdout(messages), contextlib.redirect_stderr(messages):
            cache = read_build_cache(build_dir, PROGRAM)
            missing_shared = resolve_distribution_binaries(build_dir, cache or {}, None, False, True, PROGRAM)
            (build_dir / "bin" / "libconjure_shared.so").write_bytes(b"\x00")
            found = resolve_distribution_binaries(build_dir, cache or {}, None, False, True, PROGRAM)
            skipped = resolve_distribution_binaries(build_dir, {"CMAKE_BUILD_TYPE": "Debug"}, None, False, True,
                                                    PROGRAM)
            no_cache = read_build_cache(build_dir / "bin", PROGRAM)
        bin_dir = build_dir / "bin"
        expect("the shared lookup finds the runtime and the shared library of a Distribution build directory",
               found == (EXIT_CLEAN, [bin_dir / "conjure_runtime", bin_dir / "libconjure_shared.so"]), str(found))
        expect("the shared lookup reports a missing shared library as a usage error, named by the caller",
               missing_shared == (EXIT_USAGE, [])
               and f"{PROGRAM}: no conjure_shared library in" in messages.getvalue(), messages.getvalue())
        expect("the shared lookup skips a build directory that is not Distribution", skipped == (EXIT_SKIPPED, []),
               str(skipped))
        expect("the shared cache reader refuses a directory without CMakeCache.txt",
               no_cache is None and "has no CMakeCache.txt" in messages.getvalue(), messages.getvalue())

    print(f"check_distribution_symbols: self-test {cases} case(s), {failures} failure(s)")
    return EXIT_FINDINGS if failures else EXIT_CLEAN


#------------------------------------------------------------------------------
# Entry point
#------------------------------------------------------------------------------

def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build-dir", default=None, help="a configured and built Distribution build directory")
    parser.add_argument("--config", default=None,
                        help="the configuration of a multi-config build directory (binaries in bin/<config>/)")
    parser.add_argument("--binary", action="append", default=[], metavar="FILE",
                        help="a binary to check instead of a build directory (repeatable)")
    parser.add_argument("--tool", default=None,
                        help="dumpbin, llvm-nm or nm: a path or a program name looked up on PATH "
                             "(default: searched on PATH)")
    parser.add_argument("--no-shared", action="store_true",
                        help="check the runtime executable only, not the conjure_shared library next to it")
    parser.add_argument("--any-config", action="store_true",
                        help="check a build directory even when it is not a Distribution build")
    parser.add_argument("--max-findings", type=int, default=DEFAULT_MAX_FINDINGS,
                        help=f"findings printed per binary (default: {DEFAULT_MAX_FINDINGS})")
    parser.add_argument("--self-test", action="store_true",
                        help="run the matcher over embedded tool output and the binary lookup over a fake build "
                             "directory")
    args = parser.parse_args(argv)

    if args.max_findings < 1:
        print("check_distribution_symbols: --max-findings must be positive", file=sys.stderr)
        return EXIT_USAGE
    if args.self_test:
        return run_self_test()
    if args.build_dir and args.binary:
        print("check_distribution_symbols: --build-dir and --binary are exclusive", file=sys.stderr)
        return EXIT_USAGE
    if args.binary:
        return run_binaries(args.binary, args.tool, args.max_findings)
    if args.build_dir:
        return run_build_dir(args.build_dir, args.config, args.any_config, not args.no_shared, args.tool,
                             args.max_findings)
    parser.print_usage(sys.stderr)
    print("check_distribution_symbols: pass --build-dir <dir>, --binary <file> or --self-test", file=sys.stderr)
    return EXIT_USAGE


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
