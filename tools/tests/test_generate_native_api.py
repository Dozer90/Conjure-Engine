#!/usr/bin/env python3
"""Self-test for the table parser and the C# type mapping of tools/generate_native_api.py.

Feeds small native_api.inl texts to parse_entries() and C types to cs_type() and asserts that every entry is
read, commented entries are ignored (also when one comment kind holds the other's marker), an entry the generator
cannot read fails the run instead of vanishing from the table, 'const' is dropped only as a word, and an entry of a
core group appended after the bindings marker is a bindings entry (the real table ends with entity_get_tag_text).
Feeds small native_api.h texts to parse_records() and asserts that every conjure_* struct and enumeration is read as
normalised lines (padding members included, default member initialisers and comments left out), that a member,
enumerator, alignment or managed-callback change changes the layout hash while a cosmetic one does not (sweep F445),
and that a record the generator cannot read (a nested definition, a union, a missing ';') fails the run.
Runs as the CTest entry `native_api.generator_tool`; no third-party test framework is needed.
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


def core_group_entry_after_marker_is_bindings(generator) -> None:
    # entity has core entries before the marker and bindings entries after it; an entry appended at the end of
    # the table (entity_get_tag_text) is a bindings entry with the next index, whatever its group.
    entries, core_count = generator.parse_entries(table(
        "CONJURE_NATIVE_FN(entity, get_name, int32_t, (uint64_t entity, char *utf8, int32_t capacity))\n",
        "CONJURE_NATIVE_FN(entity, get_tag, uint64_t, (uint64_t entity))\n"
        "CONJURE_NATIVE_FN(modules, name, int32_t, (int32_t index, char *utf8, int32_t capacity))\n"
        "CONJURE_NATIVE_FN(entity, get_tag_text, int32_t, (uint64_t entity, char *utf8, int32_t capacity))\n"))
    names = [item.c_name for item in entries]
    assert names == ["entity_get_name", "entity_get_tag", "modules_name", "entity_get_tag_text"], names
    assert core_count == 1, core_count
    assert entries[3].index == 3, entries[3].index
    expected = "entity_get_tag_text int32_t (uint64_t entity, char *utf8, int32_t capacity)\n"
    assert entries[3].hashed_line == expected, entries[3].hashed_line


def real_table_ends_with_tag_text(generator) -> None:
    inl_text = generator.DEFAULT_INL.read_text(encoding="utf-8")
    entries, core_count = generator.parse_entries(inl_text)
    last = entries[-1]
    assert last.c_name == "entity_get_tag_text", last.c_name
    assert last.index >= core_count, (last.index, core_count)
    assert last.signature == "int32_t (uint64_t entity, char *utf8, int32_t capacity)", last.signature
    parameters = [generator.cs_type(kind) for kind, _ in last.parameters()]
    assert parameters == ["ulong", "byte*", "int"], parameters


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


RECORD_HEADER = (
    "#pragma once\n"
    "// struct conjure_commented { int x; };\n"
    "#define CONJURE_NATIVE_API_VERSION 2\n"
    "struct conjure_vec3 {\n"
    "    float x;\n"
    "    float  y ;   // a comment\n"
    "    float z;\n"
    "};\n"
    "struct alignas(64) conjure_frame_block {\n"
    "    double time_seconds;\n"
    "    uint8_t padding[8];         /* explicit tail padding */\n"
    "};\n"
    "struct conjure_asset_info {\n"
    "    uint8_t state;\n"
    "    uint8_t pad[3];\n"
    "    const char*name;\n"
    "};\n"
    "enum class conjure_cast_shape : uint8_t {\n"
    "    SPHERE,\n"
    "    BOX = 4,\n"
    "    CAPSULE\n"
    "};\n"
    "struct conjure_native_api {\n"
    "    uint32_t size = 0;\n"
    "    const conjure_frame_block *frame = nullptr;\n"
    "#define CONJURE_NATIVE_FN(group, name, ret, args) ret (*group##_##name) args = nullptr;\n"
    "#include CONJURE_NATIVE_API_TABLE\n"
    "#undef CONJURE_NATIVE_FN\n"
    "};\n"
    "struct conjure_managed_api {\n"
    "    uint64_t layout_hash = 0;\n"
    "    int32_t (*run_phase)(uint8_t phase, float deltaTime) = nullptr;\n"
    "    int32_t ( * shutdown )( void ) = nullptr;\n"
    "};\n"
    "namespace conjure::scripting {\n"
    "    struct native_entry_descriptor {\n"
    "        int index = 0;\n"
    "    };\n"
    "    enum class script_phase : uint8_t {\n"
    "        UPDATE\n"
    "    };\n"
    "}\n"
)


def record_lines(records) -> list[str]:
    return [line for item in records for line in item.hashed_lines]


def records_parsed(generator) -> None:
    records = generator.parse_records(RECORD_HEADER)
    names = [item.name for item in records]
    expected_names = [
        "conjure_vec3", "conjure_frame_block", "conjure_asset_info", "conjure_cast_shape", "conjure_native_api",
        "conjure_managed_api",
    ]
    assert names == expected_names, names
    expected = [
        "struct conjure_vec3 {", "float x;", "float y;", "float z;", "};",
        "struct alignas(64) conjure_frame_block {", "double time_seconds;", "uint8_t padding[8];", "};",
        "struct conjure_asset_info {", "uint8_t state;", "uint8_t pad[3];", "const char *name;", "};",
        "enum class conjure_cast_shape : uint8_t {", "SPHERE,", "BOX = 4,", "CAPSULE,", "};",
        "struct conjure_native_api {", "uint32_t size;", "const conjure_frame_block *frame;", "};",
        "struct conjure_managed_api {", "uint64_t layout_hash;",
        "int32_t (*run_phase)(uint8_t phase, float deltaTime);",
        "int32_t (*shutdown)(void);", "};",
    ]
    lines = record_lines(records)
    assert lines == expected, lines


def regression_f445_hash_covers_records(generator) -> None:
    entries, _ = generator.parse_entries(table("CONJURE_NATIVE_FN(core, log, void, (uint32_t level))\n"))
    records = generator.parse_records(RECORD_HEADER)
    baseline = generator.layout_hash(entries, records)
    assert baseline != generator.layout_hash(entries, []), "the records are not part of the hash"
    changes = {
        "a managed callback signature": ("float deltaTime", "double deltaTime"),
        "two members of the same size swapped": ("    float x;\n    float  y ;", "    float  y ;\n    float x;"),
        "a padding member resized": ("uint8_t pad[3];", "uint8_t pad[7];"),
        "an enumerator value": ("BOX = 4,", "BOX = 5,"),
        "an alignment": ("alignas(64)", "alignas(32)"),
        "a managed callback added": (
            "    int32_t ( * shutdown )", "    int32_t (*reload)(void);\n    int32_t ( * shutdown )"
        ),
        "a native table header member": ("    uint32_t size = 0;", "    uint64_t size = 0;"),
    }
    for title, (before, after) in changes.items():
        assert before in RECORD_HEADER, title
        changed = generator.parse_records(RECORD_HEADER.replace(before, after, 1))
        assert generator.layout_hash(entries, changed) != baseline, f"{title} keeps the hash"
    unchanged = {
        "a default member initialiser": ("uint32_t size = 0;", "uint32_t size = {};"),
        "a comment": ("// a comment", "// another comment"),
        "whitespace": ("    float x;", "  float   x ;"),
        "a struct outside the ABI": ("int index = 0;", "long index = 0;"),
        "a trailing comma after the last enumerator": ("    CAPSULE\n", "    CAPSULE,\n"),
    }
    for title, (before, after) in unchanged.items():
        assert before in RECORD_HEADER, title
        changed = generator.parse_records(RECORD_HEADER.replace(before, after, 1))
        assert generator.layout_hash(entries, changed) == baseline, f"{title} changes the hash"


def unreadable_record_fails(generator) -> None:
    cases = {
        "a nested definition": ("struct conjure_vec3 {\n", "struct conjure_vec3 {\n    struct { int a; } inner;\n",
                                "nested definition"),
        "a union": ("struct conjure_vec3 {", "union conjure_vec3 {", "union conjure_vec3"),
        "a member without ';'": ("    float z;\n};\nstruct alignas", "    float z\n};\nstruct alignas", "no ';'"),
        "an anonymous struct": ("namespace conjure::scripting {\n",
                                "namespace conjure::scripting {\n    struct { int a; } anonymous;\n", "anonymous"),
        "no managed table": ("struct conjure_managed_api {", "struct conjure_managed_api_gone {",
                             "no definition of conjure_managed_api"),
        "a record defined twice": ("struct conjure_asset_info {", "struct conjure_vec3 {", "conjure_vec3 twice"),
        "an empty enumerator": ("    SPHERE,\n", "    SPHERE,,\n", "empty enumerator"),
    }
    for title, (before, after, message) in cases.items():
        assert before in RECORD_HEADER, title
        try:
            generator.parse_records(RECORD_HEADER.replace(before, after, 1))
        except SystemExit as failure:
            assert message in str(failure.code), f"{title}: {failure.code}"
            continue
        raise AssertionError(f"{title} was read without an error")


def real_header_records(generator) -> None:
    header_text = generator.DEFAULT_HEADER.read_text(encoding="utf-8")
    records = generator.parse_records(header_text)
    # Every conjure_* struct or enum definition line of the real header is a hashed record.
    definitions = generator.re.findall(
        r"^(?:struct|enum class)\s+(?:alignas\(\d+\)\s+)?(conjure_\w+)\b[^;\n]*\{", header_text, generator.re.MULTILINE)
    names = [item.name for item in records]
    assert names == definitions, (names, definitions)
    by_name = {item.name: item for item in records}
    managed = by_name["conjure_managed_api"].members
    assert managed[3] == "int32_t (*load_game_assembly)(const char *utf8Path, int32_t length);", managed
    assert managed[-1] == "int32_t (*shutdown)(void);", managed
    assert by_name["conjure_native_api"].members == [
        "uint32_t size;", "uint32_t version;", "uint64_t layout_hash;", "const conjure_frame_block *frame;"
    ], by_name["conjure_native_api"].members
    assert "uint8_t pad[3];" in by_name["conjure_shape_query"].members, by_name["conjure_shape_query"].members
    assert generator.parse_version(header_text) >= 2, generator.parse_version(header_text)


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
        (
            "generate_native_api - a core group's entry after the marker is a bindings entry",
            core_group_entry_after_marker_is_bindings,
        ),
        ("generate_native_api - the real table ends with entity_get_tag_text", real_table_ends_with_tag_text),
        ("generate_native_api - records, enumerations and table members are read as normalised lines", records_parsed),
        (
            "regression - code sweep F445 generate_native_api - a record or managed-table change changes the hash",
            regression_f445_hash_covers_records,
        ),
        ("generate_native_api - a record the generator cannot read fails the run", unreadable_record_fails),
        ("generate_native_api - every conjure_* definition of the real header is hashed", real_header_records),
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
