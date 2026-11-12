#!/usr/bin/env python3
"""Self-test for the table parser and the C# type mapping of tools/generate_native_api.py.

Feeds small native_api.inl texts to parse_entries() and C types to cs_type() and asserts that every entry is
read, commented entries are ignored (also when one comment kind holds the other's marker), an entry the generator
cannot read fails the run instead of vanishing from the table, and 'const' is dropped only as a word. Runs as the
CTest entry `native_api.generator_tool`; no third-party test framework is needed.
"""

from __future__ import annotations

import importlib.util
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
GENERATOR = HERE.parent / "generate_native_api.py"

PREAMBLE = (
    "// Generated table; the bindings marker below splits core and bindings entries.\n"
    "#ifndef CONJURE_NATIVE_FN\n"
    "#error define CONJURE_NATIVE_FN(group, name, return, (arguments)) before including this file\n"
    "#endif\n"
)
MARKER = "// --- bindings (R26) ---\n"


def load_generator():
    specification = importlib.util.spec_from_file_location("generate_native_api", GENERATOR)
    module = importlib.util.module_from_spec(specification)
    assert specification.loader is not None
    specification.loader.exec_module(module)
    return module


def table(core: str, bindings: str = "") -> str:
    return PREAMBLE + core + MARKER + bindings


def pointer_return_parsed(generator) -> None:
    entries, core_count = generator.parse_entries(table(
        "CONJURE_NATIVE_FN(core, log, void, (uint32_t level, const char *text))\n",
        "CONJURE_NATIVE_FN(asset, name_of, const char *, (uint64_t id))\n"))
    assert [item.c_name for item in entries] == ["core_log", "asset_name_of"], [item.c_name for item in entries]
    assert core_count == 1, core_count
    assert entries[1].returns == "const char *", entries[1].returns
    assert generator.cs_type(entries[1].returns) == "byte*", generator.cs_type(entries[1].returns)


def block_commented_entry_ignored(generator) -> None:
    entries, core_count = generator.parse_entries(table(
        "CONJURE_NATIVE_FN(core, log, void, (uint32_t level))\n"
        "/* CONJURE_NATIVE_FN(core, retired, void, (void))\n"
        "   CONJURE_NATIVE_FN(core, retired_too, int32_t, (void)) */\n"
        "// CONJURE_NATIVE_FN(core, commented, void, (void))\n",
        "CONJURE_NATIVE_FN(asset, count, uint32_t, (void))\n"))
    assert [item.c_name for item in entries] == ["core_log", "asset_count"], [item.c_name for item in entries]
    assert core_count == 1, core_count


def slashes_inside_block_comment(generator) -> None:
    # A '//' that ends the first block comment's line must not hide its '*/' and swallow the entry after it.
    entries, _ = generator.parse_entries(table(
        "/* see https://example.invalid */\n"
        "CONJURE_NATIVE_FN(core, kept, void, (void))\n"
        "/* other */\n"))
    assert [item.c_name for item in entries] == ["core_kept"], [item.c_name for item in entries]
    # A '//' on a block comment's closing line must not leave the commented-out entry live.
    entries, _ = generator.parse_entries(table(
        "/* see https://example.invalid */\n"
        "CONJURE_NATIVE_FN(core, kept, void, (void))\n"
        "/* retired:\n"
        "   CONJURE_NATIVE_FN(core, old, void, (void)) // gone */\n"))
    names = [item.c_name for item in entries]
    assert "core_kept" in names and "core_old" not in names, names
    # A '/*' inside a line comment must not open a block comment that swallows the entries after it.
    entries, _ = generator.parse_entries(table(
        "// see /* below\n"
        "CONJURE_NATIVE_FN(core, kept, void, (void))\n"
        "/* other */\n"))
    assert [item.c_name for item in entries] == ["core_kept"], [item.c_name for item in entries]


def unparseable_entry_fails(generator) -> None:
    try:
        generator.parse_entries(table(
            "CONJURE_NATIVE_FN(core, log, void, (uint32_t level))\n"
            "CONJURE_NATIVE_FN(core, frame_index, std::uint32_t, (void))\n"))
    except SystemExit as failure:
        assert "2 CONJURE_NATIVE_FN invocations but 1 parsed" in str(failure.code), failure.code
        return
    raise AssertionError("an entry with a std::uint32_t return was dropped without an error")


def const_removed_as_word(generator) -> None:
    cases = {
        "conjure_constraint_desc *": "ConstraintDesc*",
        "const conjure_constant_buffer *": "ConstantBuffer*",
        "const char *": "byte*",
        "const conjure_vec3*": "Vector3*",
        "uint32_t": "uint",
    }
    for c_type, expected in cases.items():
        mapped = generator.cs_type(c_type)
        assert mapped == expected, f"cs_type({c_type!r}) == {mapped!r}, expected {expected!r}"


def main() -> int:
    generator = load_generator()
    cases = [
        ("generate_native_api - a pointer return entry is parsed", pointer_return_parsed),
        ("generate_native_api - a block-commented entry is ignored", block_commented_entry_ignored),
        (
            "generate_native_api - a comment marker inside the other comment kind neither ends nor extends it",
            slashes_inside_block_comment,
        ),
        ("generate_native_api - an unparseable entry fails instead of being dropped", unparseable_entry_fails),
        ("generate_native_api - const is removed only as a word", const_removed_as_word),
    ]
    failures: list[str] = []
    for title, case in cases:
        try:
            case(generator)
        except (Exception, SystemExit) as failure:
            failures.append(f"{title}: {type(failure).__name__}: {failure}")
            continue
        print(f"ok: {title}")

    if failures:
        print("\n".join(f"FAIL: {failure}" for failure in failures))
        return 1
    print(f"native_api.generator_tool: {len(cases)} case(s) passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
