#!/usr/bin/env python3
"""Generates the managed mirror of the native scripting table (docs/design/03_scripting.md D3, 15_bindings.md 3.6).

Reads every CONJURE_NATIVE_FN(group, name, return, (arguments)) invocation of
engine/include/conjure/scripting/native_api.inl (an invocation may continue over several lines, the return type
may be a pointer; // and /* */ comments are ignored, and an invocation the generator cannot read fails the run
instead of being dropped) and writes:

  engine/include/conjure/scripting/native_api_hash.h     CONJURE_NATIVE_API_HASH and the entry counts
  managed/Conjure.Managed/Generated/NativeApi.g.cs       struct NativeApi (delegate* unmanaged fields in table
                                                         order), the NativeFunction enum and the entry names
  docs/generated/native_api_manifest.json                { hash, version, coreEntryCount, entries[...] }
  managed/Conjure.Managed/Interop/BindingStatus.g.cs     enum BindingStatus from the CONJURE_BINDINGS_STATUS
                                                         lines of conjure/bindings/bindings_error.inl
                                                         (15_bindings.md 3.6; skipped when that file is absent)

The hash header also carries CONJURE_NATIVE_API_FOR_EACH_ENTRY_TYPED(X) (X(group, name, return, (arguments))
per entry) and CONJURE_NATIVE_API_FOR_EACH_BINDINGS_ENTRY(X) (the same for the entries after the bindings
marker only): the filtered X-macro conjure/bindings/native_api_entries.h declares and fills the owned
entries with (15_bindings.md B2).

The layout hash is FNV-1a 64 over one line per entry, "<group>_<name> <return> <arguments>\\n", after the
signature normalisation native_entry_descriptors() applies on the C++ side (whitespace collapsed to single
spaces, '*' hugging the parameter name, no space before ',' or ')' and after '('). Both sides therefore
agree on the hash from the same text, and a stale generated file is reported by --check, which the CTest
entry conjure_scripting_api_generated_up_to_date runs.

Usage:
    python tools/generate_native_api.py            # regenerate the four outputs
    python tools/generate_native_api.py --check    # exit 1 when any output differs from what would be generated
"""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent

DEFAULT_INL = ROOT / "engine" / "include" / "conjure" / "scripting" / "native_api.inl"
DEFAULT_HEADER = ROOT / "engine" / "include" / "conjure" / "scripting" / "native_api.h"
DEFAULT_OUT_HASH = ROOT / "engine" / "include" / "conjure" / "scripting" / "native_api_hash.h"
DEFAULT_OUT_CS = ROOT / "managed" / "Conjure.Managed" / "Generated" / "NativeApi.g.cs"
DEFAULT_OUT_MANIFEST = ROOT / "docs" / "generated" / "native_api_manifest.json"
DEFAULT_STATUS_INL = ROOT / "engine" / "include" / "conjure" / "bindings" / "bindings_error.inl"
DEFAULT_OUT_STATUS = ROOT / "managed" / "Conjure.Managed" / "Interop" / "BindingStatus.g.cs"

BINDINGS_MARKER = "// --- bindings (R26) ---"

FNV1A64_OFFSET_BASIS = 0xCBF29CE484222325
FNV1A64_PRIME = 0x00000100000001B3
MASK64 = (1 << 64) - 1

ENTRY_PATTERN = re.compile(
    r"CONJURE_NATIVE_FN\(\s*(\w+)\s*,\s*(\w+)\s*,\s*([A-Za-z_][A-Za-z0-9_ *]*?)\s*,\s*(\((?:[^()]|\([^()]*\))*\))\s*\)",
    re.DOTALL,
)
# Every invocation, readable or not: parse_entries fails when the two counts differ instead of dropping one.
INVOCATION_PATTERN = re.compile(r"\bCONJURE_NATIVE_FN\s*\(")
# Both comment kinds in one pattern: the scan takes whichever starts first, so a '//' inside a block comment
# (a URL, a note on its closing line) neither ends nor extends it.
COMMENT_PATTERN = re.compile(r"//[^\n]*|/\*.*?\*/", re.DOTALL)
CONST_QUALIFIER_PATTERN = re.compile(r"\bconst\b")
VERSION_PATTERN = re.compile(r"#define\s+CONJURE_NATIVE_API_VERSION\s+(\d+)")
STATUS_PATTERN = re.compile(r"CONJURE_BINDINGS_STATUS\(\s*([A-Z][A-Z0-9_]*)\s*\)")

# C scalar types -> C# (blittable) types.
SCALAR_TYPES = {
    "void": "void",
    "uint8_t": "byte",
    "int8_t": "sbyte",
    "uint16_t": "ushort",
    "int16_t": "short",
    "uint32_t": "uint",
    "int32_t": "int",
    "uint64_t": "ulong",
    "int64_t": "long",
    "float": "float",
    "double": "double",
    "char": "byte",
}

# ABI records whose C# name does not follow the generic conjure_<snake> -> PascalCase rule.
RECORD_TYPES = {
    "conjure_vec2": "Vector2",
    "conjure_vec3": "Vector3",
    "conjure_vec4": "Vector4",
    "conjure_quat_xyzw": "Quaternion",
    "conjure_mat4": "Matrix4x4",
    "conjure_transform": "TransformData",
    "conjure_uuid": "UuidData",
    "conjure_chunk_view": "ChunkViewData",
    "conjure_frame_block": "FrameBlock",
}


class entry:
    """One parsed table entry."""

    def __init__(self, index: int, group: str, name: str, returns: str, arguments: str) -> None:
        self.index = index
        self.group = group
        self.name = name
        self.returns = normalise(returns)
        self.arguments = normalise(arguments)

    @property
    def c_name(self) -> str:
        return f"{self.group}_{self.name}"

    @property
    def signature(self) -> str:
        return f"{self.returns} {self.arguments}"

    @property
    def hashed_line(self) -> str:
        return f"{self.c_name} {self.signature}\n"

    def parameters(self) -> list[tuple[str, str]]:
        """(type, name) pairs; empty for (void)."""
        inner = self.arguments[1:-1].strip()
        if not inner or inner == "void":
            return []
        pairs: list[tuple[str, str]] = []
        for piece in inner.split(","):
            piece = piece.strip()
            match = re.match(r"^(.*?)\s*(\**)([A-Za-z_][A-Za-z0-9_]*)$", piece)
            if not match:
                raise SystemExit(f"cannot split parameter '{piece}' of {self.c_name}")
            base, stars, name = match.groups()
            pairs.append(((base.strip() + " " + stars).strip() if stars else base.strip(), name))
        return pairs


def normalise(text: str) -> str:
    """The signature normalisation shared with native_entry_descriptors()."""
    spaced = text.replace("*", " * ")
    collapsed = " ".join(spaced.split())
    collapsed = collapsed.replace("* ", "*")
    collapsed = collapsed.replace(" ,", ",").replace("( ", "(").replace(" )", ")")
    return collapsed


def fnv1a64(data: bytes, seed: int = FNV1A64_OFFSET_BASIS) -> int:
    value = seed
    for byte in data:
        value = ((value ^ byte) * FNV1A64_PRIME) & MASK64
    return value


def blank_directives(code: str) -> str:
    """'code' with every preprocessor directive line (and its backslash continuations) replaced by spaces of
    the same length: a CONJURE_NATIVE_FN mentioned in an #error or #define is not a table entry."""
    lines = code.split("\n")
    continued = False
    for index, line in enumerate(lines):
        if continued or line.lstrip().startswith("#"):
            continued = line.rstrip().endswith("\\")
            lines[index] = " " * len(line)
    return "\n".join(lines)


def parse_entries(inl_text: str) -> tuple[list[entry], int]:
    """Entries in table order and the index of the first bindings-owned entry."""
    # The marker is a whole line; the header comment mentions it in passing, so a plain find is not enough.
    marker_match = re.search("^" + re.escape(BINDINGS_MARKER) + r"[ \t]*$", inl_text, re.MULTILINE)
    if marker_match is None:
        raise SystemExit(f"native_api.inl has no '{BINDINGS_MARKER}' marker line")
    marker = marker_match.start()
    # Comments and preprocessor directives are blanked (not removed, newlines kept) so match offsets stay
    # comparable with the marker's position.
    code = COMMENT_PATTERN.sub(lambda found: re.sub(r"[^\n]", " ", found.group(0)), inl_text)
    code = blank_directives(code)
    entries: list[entry] = []
    core_count = -1
    for match in ENTRY_PATTERN.finditer(code):
        if core_count < 0 and match.start() > marker:
            core_count = len(entries)
        entries.append(entry(len(entries), match.group(1), match.group(2), match.group(3), match.group(4)))
    if core_count < 0:
        core_count = len(entries)
    invocations = len(INVOCATION_PATTERN.findall(code))
    if invocations != len(entries):
        raise SystemExit(f"native_api.inl: {invocations} CONJURE_NATIVE_FN invocations but {len(entries)} parsed; "
                         f"an entry uses a form the generator cannot read")
    names = [item.c_name for item in entries]
    duplicates = sorted({name for name in names if names.count(name) > 1})
    if duplicates:
        raise SystemExit(f"duplicate entries in native_api.inl: {', '.join(duplicates)}")
    return entries, core_count


def layout_hash(entries: list[entry]) -> int:
    value = FNV1A64_OFFSET_BASIS
    for item in entries:
        value = fnv1a64(item.hashed_line.encode("utf-8"), value)
    return value


def parse_version(header_text: str) -> int:
    match = VERSION_PATTERN.search(header_text)
    if not match:
        raise SystemExit("native_api.h does not define CONJURE_NATIVE_API_VERSION")
    return int(match.group(1))


def pascal(snake: str) -> str:
    return "".join(part[:1].upper() + part[1:] for part in snake.split("_") if part)


def cs_type(c_type: str) -> str:
    """C# spelling of a C parameter or return type. 'const' is dropped as a word only, so a type name that
    contains it (conjure_constraint_desc) keeps its spelling."""
    stars = c_type.count("*")
    base = " ".join(CONST_QUALIFIER_PATTERN.sub(" ", c_type.replace("*", " ")).split())
    if base in SCALAR_TYPES:
        mapped = SCALAR_TYPES[base]
    elif base.startswith("conjure_"):
        mapped = RECORD_TYPES.get(base, pascal(base[len("conjure_"):]))
    else:
        raise SystemExit(f"no C# mapping for type '{c_type}'")
    if stars == 0 and mapped == "void":
        return "void"
    return mapped + "*" * stars


def split_literal(text: str, width: int) -> list[str]:
    """Splits a string literal at spaces into pieces of at most 'width' characters (the space stays with
    the preceding piece so concatenation reproduces the text)."""
    pieces: list[str] = []
    remaining = text
    while len(remaining) > width:
        cut = remaining.rfind(" ", 0, width)
        cut = width if cut <= 0 else cut + 1
        pieces.append(remaining[:cut])
        remaining = remaining[cut:]
    pieces.append(remaining)
    return pieces


def generate_hash_header(entries: list[entry], core_count: int, hash_value: int) -> str:
    lines = [
        "#pragma once",
        "",
        "// Generated by tools/generate_native_api.py from native_api.inl. Do not edit; regenerate instead.",
        "// CONJURE_NATIVE_API_HASH is the FNV-1a 64 of every normalised entry line (see the generator); the",
        "// managed side bakes the same value into NativeApi.g.cs and the handshake compares them.",
        "// CONJURE_NATIVE_API_FOR_EACH_ENTRY(X) invokes X(group, name, \"<return> <arguments>\") per entry in",
        "// table order: the descriptor table native_entry_descriptors() is built from it.",
        "",
        f"#define CONJURE_NATIVE_API_HASH 0x{hash_value:016X}ull",
        f"#define CONJURE_NATIVE_API_ENTRY_COUNT {len(entries)}",
        f"#define CONJURE_NATIVE_API_CORE_ENTRY_COUNT {core_count}",
        "",
        "// clang-format off",
        "#define CONJURE_NATIVE_API_FOR_EACH_ENTRY(X) \\",
    ]
    for item in entries:
        signature = item.signature.replace("\\", "\\\\").replace('"', '\\"')
        prefix = f"    X({item.group}, {item.name}, "
        pieces = split_literal(signature, 118 - len(prefix) - 4)
        if len(pieces) == 1:
            lines.append(f'{prefix}"{pieces[0]}") \\')
            continue
        # Long signatures continue as adjacent string literals so the generated header keeps to 120 columns.
        lines.append(f'{prefix}"{pieces[0]}" \\')
        for piece in pieces[1:-1]:
            lines.append(f'      "{piece}" \\')
        lines.append(f'      "{pieces[-1]}") \\')
    lines.append("    /* end of table */")
    lines.append("")
    lines.append("// X(group, name, return, (arguments)) per entry in table order: the typed form the bindings' filtered")
    lines.append("// X-macro declares and fills the owned entries with (15_bindings.md B2).")
    lines.append("#define CONJURE_NATIVE_API_FOR_EACH_ENTRY_TYPED(X) \\")
    lines.extend(typed_macro_lines(entries))
    lines.append("    /* end of table */")
    lines.append("")
    lines.append("// The same for the entries after the bindings marker only (the ten bindings groups, R26/R64).")
    lines.append("#define CONJURE_NATIVE_API_FOR_EACH_BINDINGS_ENTRY(X) \\")
    lines.extend(typed_macro_lines(entries[core_count:]))
    lines.append("    /* end of table */")
    lines.append("// clang-format on")
    lines.append("")
    return "\n".join(lines)


def typed_macro_lines(entries: list[entry]) -> list[str]:
    """One `X(group, name, return, (arguments)) \\` line per entry; an argument list that would pass 120
    columns continues on the next line (whitespace inside a macro argument is insignificant)."""
    lines: list[str] = []
    for item in entries:
        prefix = f"    X({item.group}, {item.name}, {item.returns}, "
        single = f"{prefix}{item.arguments}) \\"
        if len(single) <= 120:
            lines.append(single)
            continue
        lines.append(f"{prefix}\\")
        # Long argument lists wrap after a comma; the inner pieces end with ", \\" so a comma never
        # starts a continuation line.
        pieces = split_arguments(item.arguments, 110)
        for piece in pieces[:-1]:
            lines.append(f"      {piece} \\")
        lines.append(f"      {pieces[-1]})) \\")
    return lines


def split_arguments(arguments: str, width: int) -> list[str]:
    """Splits "(a, b, c)" at ", " boundaries into pieces of at most 'width' characters, each piece but the
    last keeping its trailing comma."""
    parameters = arguments[1:-1].split(", ")
    pieces: list[str] = []
    current = "("
    for index, parameter in enumerate(parameters):
        candidate = parameter + ("," if index + 1 < len(parameters) else "")
        separator = "" if current == "(" else " "
        if len(current) + len(separator) + len(candidate) > width and current != "(":
            pieces.append(current)
            current = candidate
        else:
            current = current + separator + candidate
    pieces.append(current)
    return pieces


def generate_cs(entries: list[entry], core_count: int, hash_value: int, version: int) -> str:
    lines: list[str] = []
    lines.append("// <auto-generated>")
    lines.append("// Generated by tools/generate_native_api.py from engine/include/conjure/scripting/native_api.inl.")
    lines.append("// Do not edit; regenerate instead. The CTest entry conjure_scripting_api_generated_up_to_date diffs it.")
    lines.append("// </auto-generated>")
    lines.append("#nullable enable")
    lines.append("using System.Numerics;")
    lines.append("using System.Runtime.InteropServices;")
    lines.append("")
    lines.append("namespace Conjure.Interop;")
    lines.append("")
    lines.append("/// <summary>")
    lines.append("/// Mirror of conjure_native_api: the header validated by the handshake followed by one unmanaged")
    lines.append("/// function pointer per table entry, in table order.")
    lines.append("/// </summary>")
    lines.append("[StructLayout(LayoutKind.Sequential)]")
    lines.append("public unsafe struct NativeApi")
    lines.append("{")
    lines.append(f"    public const uint ExpectedVersion = {version};")
    lines.append(f"    public const ulong ExpectedLayoutHash = 0x{hash_value:016X}UL;")
    lines.append(f"    public const int EntryCount = {len(entries)};")
    lines.append(f"    public const int CoreEntryCount = {core_count};")
    lines.append("")
    lines.append("    public uint Size;")
    lines.append("    public uint Version;")
    lines.append("    public ulong LayoutHash;")
    lines.append("    public FrameBlock* Frame;")
    lines.append("")
    for item in entries:
        parameter_types = [cs_type(kind) for kind, _ in item.parameters()]
        generic = ", ".join(parameter_types + [cs_type(item.returns)])
        lines.append(f"    public delegate* unmanaged<{generic}> {pascal(item.c_name)};")
    lines.append("")
    lines.append("    /// <summary>")
    lines.append("    /// The function pointer of entry 'function' as an untyped pointer (null when native left it unfilled).")
    lines.append("    /// </summary>")
    lines.append("    public readonly void* Pointer(NativeFunction function)")
    lines.append("    {")
    lines.append("        fixed (NativeApi* self = &this)")
    lines.append("        {")
    lines.append("            void** table = (void**)((byte*)self + HeaderSize);")
    lines.append("            int index = (int)function;")
    lines.append("            return index >= 0 && index < EntryCount ? table[index] : null;")
    lines.append("        }")
    lines.append("    }")
    lines.append("")
    lines.append("    /// <summary>")
    lines.append("    /// Bytes before the first function pointer: Size, Version, LayoutHash and Frame.")
    lines.append("    /// </summary>")
    lines.append("    public const int HeaderSize = 24;")
    lines.append("}")
    lines.append("")
    lines.append("/// <summary>")
    lines.append("/// Index of every entry in the table (and in NativeApi).")
    lines.append("/// </summary>")
    lines.append("public enum NativeFunction")
    lines.append("{")
    for item in entries:
        lines.append(f"    {pascal(item.c_name)} = {item.index},")
    lines.append("}")
    lines.append("")
    lines.append("/// <summary>")
    lines.append("/// The C names of the entries, indexed by NativeFunction; used in diagnostics such as the")
    lines.append("/// NotSupportedException raised for an entry native did not provide.")
    lines.append("/// </summary>")
    lines.append("public static class NativeFunctionNames")
    lines.append("{")
    lines.append("    public static readonly string[] Names =")
    lines.append("    {")
    for item in entries:
        lines.append(f"        \"{item.c_name}\",")
    lines.append("    };")
    lines.append("")
    lines.append("    public static string Of(NativeFunction function)")
    lines.append("    {")
    lines.append("        int index = (int)function;")
    lines.append("        return index >= 0 && index < Names.Length ? Names[index] : $\"entry#{index}\";")
    lines.append("    }")
    lines.append("}")
    lines.append("")
    return "\n".join(lines)


def parse_statuses(inl_text: str) -> list[str]:
    """The CONJURE_BINDINGS_STATUS names in order; the first must be NONE and the names must be unique."""
    code = re.sub(r"//[^\n]*", "", inl_text)
    names = STATUS_PATTERN.findall(code)
    if not names or names[0] != "NONE":
        raise SystemExit("bindings_error.inl must start with CONJURE_BINDINGS_STATUS(NONE)")
    if len(set(names)) != len(names):
        raise SystemExit("bindings_error.inl lists a status twice")
    return names


def generate_status_cs(statuses: list[str]) -> str:
    lines: list[str] = []
    lines.append("// <auto-generated>")
    lines.append("// Generated by tools/generate_native_api.py --status from engine/include/conjure/bindings/bindings_error.inl.")
    lines.append("// Do not edit; regenerate instead. The CTest entry conjure_scripting_api_generated_up_to_date diffs it.")
    lines.append("// </auto-generated>")
    lines.append("#nullable enable")
    lines.append("")
    lines.append("namespace Conjure.Interop;")
    lines.append("")
    lines.append("/// <summary>")
    lines.append("/// conjure::bindings::bindings_error: the code behind a negative status of a bindings entry (the status")
    lines.append("/// is -(int)code, 15_bindings.md B4). Append-only; not part of the ABI layout hash.")
    lines.append("/// </summary>")
    lines.append("public enum BindingStatus : ushort")
    lines.append("{")
    for index, name in enumerate(statuses):
        lines.append(f"    {pascal(name.lower())} = {index},")
    lines.append("}")
    lines.append("")
    lines.append("/// <summary>")
    lines.append("/// The enumerator names, indexed by BindingStatus; used in exception messages.")
    lines.append("/// </summary>")
    lines.append("public static class BindingStatusNames")
    lines.append("{")
    lines.append("    public static readonly string[] Names =")
    lines.append("    {")
    for name in statuses:
        lines.append(f'        "{name}",')
    lines.append("    };")
    lines.append("")
    lines.append("    public static string Of(BindingStatus status)")
    lines.append("    {")
    lines.append("        int index = (int)status;")
    lines.append('        return index >= 0 && index < Names.Length ? Names[index] : $"status#{index}";')
    lines.append("    }")
    lines.append("}")
    lines.append("")
    return "\n".join(lines)


def generate_manifest(entries: list[entry], core_count: int, hash_value: int, version: int) -> str:
    manifest = {
        "hash": f"0x{hash_value:016X}",
        "version": version,
        "entryCount": len(entries),
        "coreEntryCount": core_count,
        "entries": [
            {
                "index": item.index,
                "group": item.group,
                "name": item.name,
                "returns": item.returns,
                "args": [{"type": kind, "name": name} for kind, name in item.parameters()],
                "signature": item.signature,
            }
            for item in entries
        ],
    }
    return json.dumps(manifest, indent=2) + "\n"


def write_if_changed(path: pathlib.Path, content: str) -> bool:
    path.parent.mkdir(parents=True, exist_ok=True)
    existing = path.read_text(encoding="utf-8") if path.exists() else None
    if existing == content:
        return False
    path.write_text(content, encoding="utf-8", newline="\n")
    return True


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--inl", default=str(DEFAULT_INL), help="native_api.inl to read")
    parser.add_argument("--header", default=str(DEFAULT_HEADER), help="native_api.h (CONJURE_NATIVE_API_VERSION)")
    parser.add_argument("--out-hash", default=str(DEFAULT_OUT_HASH), help="generated native_api_hash.h")
    parser.add_argument("--out-cs", default=str(DEFAULT_OUT_CS), help="generated NativeApi.g.cs")
    parser.add_argument("--out-manifest", default=str(DEFAULT_OUT_MANIFEST), help="generated manifest JSON")
    parser.add_argument(
        "--status", default=str(DEFAULT_STATUS_INL), help="bindings_error.inl to read (skipped when absent)"
    )
    parser.add_argument("--out-status", default=str(DEFAULT_OUT_STATUS), help="generated BindingStatus.g.cs")
    parser.add_argument("--check", action="store_true", help="compare the outputs instead of writing them")
    args = parser.parse_args(argv)

    inl_text = pathlib.Path(args.inl).read_text(encoding="utf-8")
    header_text = pathlib.Path(args.header).read_text(encoding="utf-8")
    entries, core_count = parse_entries(inl_text)
    version = parse_version(header_text)
    hash_value = layout_hash(entries)

    outputs = {
        pathlib.Path(args.out_hash): generate_hash_header(entries, core_count, hash_value),
        pathlib.Path(args.out_cs): generate_cs(entries, core_count, hash_value, version),
        pathlib.Path(args.out_manifest): generate_manifest(entries, core_count, hash_value, version),
    }
    status_path = pathlib.Path(args.status)
    if status_path.exists():
        statuses = parse_statuses(status_path.read_text(encoding="utf-8"))
        outputs[pathlib.Path(args.out_status)] = generate_status_cs(statuses)

    if args.check:
        stale = []
        for path, content in outputs.items():
            existing = path.read_text(encoding="utf-8").replace("\r\n", "\n") if path.exists() else None
            if existing != content:
                stale.append(path)
        if stale:
            for path in stale:
                print(f"{path}: out of date; run python tools/generate_native_api.py", file=sys.stderr)
            return 1
        print(f"native API generated files are up to date ({len(entries)} entries, hash 0x{hash_value:016X})")
        return 0

    for path, content in outputs.items():
        changed = write_if_changed(path, content)
        print(f"{'wrote' if changed else 'unchanged'} {path}")
    print(f"{len(entries)} entries ({core_count} core), version {version}, hash 0x{hash_value:016X}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
