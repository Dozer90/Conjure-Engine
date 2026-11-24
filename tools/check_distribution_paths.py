#!/usr/bin/env python3
"""Distribution source-path check (code sweep F613; the options of cmake/ConjureDistributionPaths.cmake, applied by
cmake/ConjureWarnings.cmake; the managed assemblies of an export, follow-up FU-PDB; Conjure's own checkout as a
subproject, follow-up FU-ROOTS).

__FILE__ (CONJURE_LOG, CONJURE_ASSERT, assert) and std::source_location embed the path the compiler was given, and
debug information and a PDB reference embed more. A Distribution build removes the checkout from those paths
(/d1trimfile on MSVC, -fmacro-prefix-map on GCC and Clang) and carries no debug information, so a shipped binary
does not name the machine that built it. This tool proves it on the built binaries: it reads every byte of the
Distribution conjure_runtime and of the conjure_shared library next to it and fails when one of them contains a
root, that is the source directory or the build directory of the tree, or Conjure's own source or build directory
when a parent project adds Conjure with add_subdirectory() from outside its own directories. Given --binary it checks
any file, and it reads a managed assembly and a portable PDB as well as a native binary: the assemblies an export
ships (Conjure.Managed.dll, Conjure.Bootstrap.dll, the game's scripts) and their .pdb files when the export keeps
them.

A root matches in UTF-8 (narrow strings, debug information, PDB paths) and in UTF-16LE (wide strings), with '/'
and '\\' interchangeable and repeatable between its components ('A:\\\\src' in an escaped string) and ASCII letters
compared without regard to case (cl may change the case of a drive letter, and Windows and macOS volumes are
usually case-insensitive). A match must end the root: 'C:/src/conjure' is found in 'C:/src/conjure/engine/log.cpp'
and in 'C:/src/conjure' itself, not in 'C:/src/conjure_old/log.cpp'. An ELF section that is compressed
(SHF_COMPRESSED or .zdebug*, gcc -gz) cannot be read this way and is a finding of its own.

The debug directory of a PE image (native or .NET) is read: the PDB path of its CodeView entry is named as such in a
finding, and an embedded portable PDB (DebugType=embedded) is decompressed and scanned like a .pdb file. A portable
PDB (a .pdb file, or the embedded one) stores each document path in pieces, a separator and the names between, so
the tool reassembles the paths of its Document table and matches them as well as its bytes; one whose Document
table cannot be read, or an embedded one that cannot be decompressed, is a finding of its own. Not found: a path
stored in pieces anywhere else, compressed in another way, or spelt as a different path to the same directory (an
8.3 short name, a link the build did not use). A finding names its PE or ELF section, "CodeView PDB path" inside
it, "portable PDB document" or "embedded portable PDB"; in a Mach-O or any other file it is reported as "unknown
section".

Modes:
    --build-dir <dir> [--config <name>]   checks bin/conjure_runtime[.exe] and the conjure_shared library of a
                                          configured and built Distribution build directory, against the
                                          CMAKE_HOME_DIRECTORY and the build directory its CMakeCache.txt names,
                                          and against Conjure_SOURCE_DIR and Conjure_BINARY_DIR (the directories
                                          of Conjure's project(), which the Distribution options map too) where
                                          one lies outside those two; a directory inside another root adds nothing,
                                          since that root's pattern finds it wherever it occurs.
                                          The directory must hold a Distribution build (CMAKE_BUILD_TYPE, or
                                          --config for a multi-config generator, binaries in bin/<config>/); any
                                          other configuration exits 77 ("skipped") unless --any-config. A --config
                                          that disagrees with a single-config directory's CMAKE_BUILD_TYPE is a
                                          usage error.
    --binary <file> [...]                 checks the named files (native or managed binaries, portable PDBs, any
                                          other file byte by byte), against the repository that holds this tool

Options: --root <dir> (repeatable: another root to look for, in either mode), --no-shared (skip the conjure_shared
library next to the runtime), --any-config, --max-findings <n> (printed per binary, default 20).

Exit code 0 when no binary contains a root, 1 when one does (one `<binary>: 0x<offset> [<section>] <encoding>:
<string>` line per distinct string), 2 on usage errors (missing directory, cache, binary or root, disagreeing
--config), 77 when the build directory is not a Distribution one.

Tests: python tools/tests/test_check_distribution_paths.py. CI runs them in the style-and-headers job of ci.yml.
The distribution-export job of nightly.yml runs the check over the Distribution tree as the CTest entry
runtime.distribution_paths (its "process|runtime" step, before the export). Its "Managed path check" step, after the
export, runs it over the managed assemblies the export published, once the editor's export publishes any: it does
not yet (project.export names no bootstrap project and the exported test project has scripting disabled), so that
step finds no managed directory and only warns:

    python tools/check_distribution_paths.py --build-dir build/windows-msvc-dist
    python tools/check_distribution_paths.py --binary build/export_out/managed/Game.dll ...

Until then the end-to-end proof for managed code is the [dotnet] case "regression - code sweep FU-PDB ..." of
tests/engine/test_export.cpp, which runs the check over the assemblies and .pdb files of a real export (label slow,
run by the nightly test presets).

tests/runtime/CMakeLists.txt registers both with CTest: runtime.check_distribution_paths_tool (the tests, in every
tree) and runtime.distribution_paths (the check, label "process", in a Distribution tree or on the tree that
CONJURE_DISTRIBUTION_BUILD_DIR names).
"""

from __future__ import annotations

import argparse
import importlib.util
import os
import pathlib
import re
import struct
import sys
import zlib
from types import ModuleType

ROOT = pathlib.Path(__file__).resolve().parent.parent
SYMBOLS_TOOL = pathlib.Path(__file__).resolve().parent / "check_distribution_symbols.py"

PROGRAM = "check_distribution_paths"

EXIT_CLEAN = 0
EXIT_FINDINGS = 1
EXIT_USAGE = 2
EXIT_SKIPPED = 77

DEFAULT_MAX_FINDINGS = 20
# How far the printed string around a match may reach on either side.
CONTEXT_LIMIT = 260

ENCODING_UTF8 = "utf-8"
ENCODING_UTF16 = "utf-16le"

# The characters that continue a directory name: a root followed by one of them is a different directory.
NAME_CHARACTERS = b"A-Za-z0-9_.\\-"

ELF_SHT_NOBITS = 8
ELF_SHF_COMPRESSED = 0x800

# The PE optional header (PE32 and PE32+): where the data directories start and where their count is.
PE32_MAGIC = 0x10B
PE32_PLUS_MAGIC = 0x20B
PE_DIRECTORY_LAYOUT = {PE32_MAGIC: (92, 96), PE32_PLUS_MAGIC: (108, 112)}
PE_DIRECTORY_DEBUG = 6
PE_DIRECTORY_CLR = 14
PE_DEBUG_ENTRY_SIZE = 28
PE_DEBUG_CODEVIEW = 2
PE_DEBUG_EMBEDDED_PORTABLE_PDB = 17
CODEVIEW_SIGNATURE = b"RSDS"
# "RSDS", the GUID and the age come before the UTF-8 path.
CODEVIEW_PATH_OFFSET = 24
EMBEDDED_PDB_SIGNATURE = b"MPDB"
# "MPDB" and the uncompressed size come before the raw deflate stream.
EMBEDDED_PDB_DATA_OFFSET = 8

# ECMA-335 II.24 metadata, the container of a portable PDB.
METADATA_SIGNATURE = b"BSJB"
METADATA_TABLE_DOCUMENT = 0x30
METADATA_HEAP_GUID_WIDE = 0x02
METADATA_HEAP_BLOB_WIDE = 0x04
METADATA_EXTRA_DATA = 0x40

SECTION_CODEVIEW = "CodeView PDB path"
SECTION_DOCUMENT = "portable PDB document"
SECTION_EMBEDDED = "embedded portable PDB"

# The name in project() of Conjure's root CMakeLists.txt: CMake caches its directories as <name>_SOURCE_DIR and
# <name>_BINARY_DIR, also when a parent project adds Conjure with add_subdirectory().
PROJECT_NAME = "Conjure"


def load_symbols_tool() -> ModuleType:
    """tools/check_distribution_symbols.py, whose cache reading and binary lookup this tool shares. Loaded by path
    so the tool also runs under `python -I`, which leaves the script's directory off sys.path."""
    specification = importlib.util.spec_from_file_location("conjure_check_distribution_symbols", SYMBOLS_TOOL)
    if specification is None or specification.loader is None:
        raise ImportError(f"cannot load {SYMBOLS_TOOL}")
    module = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(module)
    return module


symbols = load_symbols_tool()


class finding:
    def __init__(self, binary: pathlib.Path, offset: int, section: str, encoding: str, text: str) -> None:
        self.binary = binary
        self.offset = offset
        self.section = section
        self.encoding = encoding
        self.text = text

    def format(self) -> str:
        return f"{self.binary.name}: 0x{self.offset:x} [{self.section}] {self.encoding}: {self.text}"


#------------------------------------------------------------------------------
# Roots
#------------------------------------------------------------------------------

def root_components(root: str) -> tuple[bool, list[str]]:
    """Whether the root starts with a separator (a POSIX or UNC path), and its non-empty components."""
    leading = root[:1] in ("/", "\\")
    components = [component for component in re.split(r"[\\/]+", root) if component]
    return leading, components


def root_pattern(root: str, encoding: str) -> re.Pattern[bytes] | None:
    """The pattern that finds a root in a binary in one encoding; None for a root without components."""
    leading, components = root_components(root)
    if not components:
        return None
    if encoding == ENCODING_UTF16:
        separator = b"(?:[\\\\/]\x00)+"
        boundary = b"(?![" + NAME_CHARACTERS + b"]\x00)"
    else:
        separator = b"[\\\\/]+"
        boundary = b"(?![" + NAME_CHARACTERS + b"\x80-\xff])"
    parts = [re.escape(component.encode(encoding)) for component in components]
    body = (separator if leading else b"") + separator.join(parts)
    return re.compile(body + boundary, re.IGNORECASE)


def ascii_lower(text: str) -> str:
    """The text with its ASCII letters in lower case, the only letters the patterns compare without case."""
    return "".join(character.lower() if character.isascii() else character for character in text)


def lies_inside(root: str, other: str) -> bool:
    """Whether a root equals or lies inside another, compared the way the patterns compare (components, separators
    of either kind, ASCII letters without regard to case; another root without a leading separator also finds the
    root with one). False for another root without components, which has no pattern."""
    leading, components = root_components(root)
    other_leading, other_components = root_components(other)
    if not other_components or (other_leading and not leading) or len(other_components) > len(components):
        return False
    return all(ascii_lower(mine) == ascii_lower(theirs) for mine, theirs in zip(components, other_components))


def outer_roots(candidates: list[str], roots: list[str]) -> list[str]:
    """The candidates that add a root to 'roots', as cmake/ConjureDistributionPaths.cmake keeps only the outer of
    its roots: a candidate that equals or lies inside one of 'roots' or inside another candidate, or that equals an
    earlier candidate, is dropped (the outer root's pattern finds the inner one wherever it occurs)."""
    kept: list[str] = []
    for index, candidate in enumerate(candidates):
        if any(lies_inside(candidate, root) for root in roots):
            continue
        covered = False
        for other_index, other in enumerate(candidates):
            if other_index == index or not lies_inside(candidate, other):
                continue
            if not lies_inside(other, candidate) or other_index < index:
                covered = True
                break
        if not covered:
            kept.append(candidate)
    return kept


def project_directories(cache: dict[str, str]) -> list[str]:
    """Conjure's own source and build directories as a CMakeCache.txt names them (empty entries left out)."""
    names = (f"{PROJECT_NAME}_SOURCE_DIR", f"{PROJECT_NAME}_BINARY_DIR")
    return [cache[name] for name in names if cache.get(name, "")]


def expand_roots(roots: list[str]) -> list[str]:
    """Each root made absolute and, when that differs, with its links resolved. A root that differs from an earlier
    one only in separators or the case of ASCII letters is dropped: its pattern would find the same places."""
    expanded: list[str] = []
    keys: set[tuple[bool, tuple[str, ...]]] = set()
    for root in roots:
        for candidate in (os.path.abspath(root), str(pathlib.Path(root).resolve())):
            leading, components = root_components(candidate)
            key = (leading, tuple(ascii_lower(component) for component in components))
            if key in keys:
                continue
            keys.add(key)
            expanded.append(candidate)
    return expanded


#------------------------------------------------------------------------------
# Sections
#------------------------------------------------------------------------------

class section:
    def __init__(self, name: str, offset: int, size: int, compressed: bool, address: int = 0,
                 address_size: int = 0) -> None:
        self.name = name
        self.offset = offset
        self.size = size
        self.compressed = compressed
        # Where a PE section is mapped (its RVA and virtual size); 0 for an ELF section.
        self.address = address
        self.address_size = address_size


def pe_header(data: bytes) -> int | None:
    """The file offset of a PE image's "PE\\0\\0" signature; None when the file is no readable PE image."""
    if len(data) < 0x40 or data[:2] != b"MZ":
        return None
    header = struct.unpack_from("<I", data, 0x3C)[0]
    if header + 24 > len(data) or data[header:header + 4] != b"PE\x00\x00":
        return None
    return header


def pe_sections(data: bytes) -> list[section]:
    """The sections of a PE image (raw data ranges and where they are mapped); empty when the headers are not
    readable."""
    header = pe_header(data)
    if header is None:
        return []
    count, optional_size = struct.unpack_from("<H12xH", data, header + 6)
    table = header + 24 + optional_size
    sections: list[section] = []
    for index in range(count):
        entry = table + index * 40
        if entry + 40 > len(data):
            return sections
        raw_name = data[entry:entry + 8].split(b"\x00", 1)[0].decode("ascii", errors="replace")
        address_size, address, raw_size, raw_offset = struct.unpack_from("<IIII", data, entry + 8)
        sections.append(section(raw_name, raw_offset, raw_size, False, address, address_size))
    return sections


def pe_directory(data: bytes, index: int) -> tuple[int, int] | None:
    """The (RVA, size) of a data directory of a PE image; None when the image has no such directory or the
    optional header is not readable."""
    header = pe_header(data)
    if header is None:
        return None
    optional_size = struct.unpack_from("<H", data, header + 20)[0]
    optional = header + 24
    if optional_size < 2 or optional + 2 > len(data):
        return None
    layout = PE_DIRECTORY_LAYOUT.get(struct.unpack_from("<H", data, optional)[0])
    if layout is None:
        return None
    count_offset, directories = layout
    entry = directories + index * 8
    if entry + 8 > optional_size or optional + entry + 8 > len(data):
        return None
    if index >= struct.unpack_from("<I", data, optional + count_offset)[0]:
        return None
    address, size = struct.unpack_from("<II", data, optional + entry)
    if address == 0 or size == 0:
        return None
    return address, size


def pe_offset(sections: list[section], address: int) -> int | None:
    """The file offset of an RVA; None when no section maps it."""
    for item in sections:
        if item.address <= address < item.address + max(item.address_size, item.size):
            return item.offset + address - item.address
    return None


class debug_record:
    def __init__(self, kind: int, offset: int, size: int) -> None:
        # The IMAGE_DEBUG_TYPE_* of the entry and where its data lies in the file.
        self.kind = kind
        self.offset = offset
        self.size = size


def pe_debug_records(data: bytes) -> list[debug_record]:
    """The entries of a PE image's debug directory whose data lies in the file; empty without one."""
    directory = pe_directory(data, PE_DIRECTORY_DEBUG)
    if directory is None:
        return []
    sections = pe_sections(data)
    start = pe_offset(sections, directory[0])
    if start is None:
        return []
    records: list[debug_record] = []
    for index in range(directory[1] // PE_DEBUG_ENTRY_SIZE):
        entry = start + index * PE_DEBUG_ENTRY_SIZE
        if entry + PE_DEBUG_ENTRY_SIZE > len(data):
            break
        kind, size, address, pointer = struct.unpack_from("<12xIIII", data, entry)
        offset = pointer if pointer else pe_offset(sections, address)
        if offset is None or size == 0 or offset + size > len(data):
            continue
        records.append(debug_record(kind, offset, size))
    return records


def is_managed(data: bytes) -> bool:
    """True for a .NET assembly (a PE image with a CLR header) and for a portable PDB."""
    return data[:4] == METADATA_SIGNATURE or pe_directory(data, PE_DIRECTORY_CLR) is not None


def codeview_ranges(data: bytes) -> list[tuple[int, int]]:
    """The byte ranges of the PDB paths of a PE image's CodeView entries (RSDS)."""
    ranges: list[tuple[int, int]] = []
    for record in pe_debug_records(data):
        if record.kind != PE_DEBUG_CODEVIEW or record.size <= CODEVIEW_PATH_OFFSET:
            continue
        if data[record.offset:record.offset + 4] != CODEVIEW_SIGNATURE:
            continue
        start = record.offset + CODEVIEW_PATH_OFFSET
        end = data.find(b"\x00", start, record.offset + record.size)
        ranges.append((start, end if end >= 0 else record.offset + record.size))
    return ranges


#------------------------------------------------------------------------------
# Portable PDB
#------------------------------------------------------------------------------

def compressed_unsigned(data: bytes, position: int) -> tuple[int, int]:
    """An ECMA-335 compressed unsigned integer at a position: its value and the position after it."""
    first = data[position]
    if first & 0x80 == 0:
        return first, position + 1
    if first & 0xC0 == 0x80:
        return ((first & 0x3F) << 8) | data[position + 1], position + 2
    if first & 0xE0 == 0xC0:
        value = struct.unpack_from(">I", data, position)[0] & 0x1FFFFFFF
        return value, position + 4
    raise ValueError(f"invalid compressed integer 0x{first:02x} at 0x{position:x}")


def metadata_streams(data: bytes) -> dict[str, tuple[int, int]]:
    """The streams of ECMA-335 metadata that starts at the beginning of 'data': name -> (offset, size)."""
    if data[:4] != METADATA_SIGNATURE:
        raise ValueError("no metadata signature")
    version_length = struct.unpack_from("<I", data, 12)[0]
    position = 16 + version_length
    count = struct.unpack_from("<H", data, position + 2)[0]
    position += 4
    streams: dict[str, tuple[int, int]] = {}
    for _ in range(count):
        offset, size = struct.unpack_from("<II", data, position)
        end = data.index(b"\x00", position + 8)
        name = data[position + 8:end].decode("ascii")
        # The name and its terminator are padded to a multiple of four bytes.
        position = position + 8 + ((end - position - 8 + 1 + 3) & ~3)
        if offset + size > len(data):
            raise ValueError(f"stream {name} lies beyond the end")
        streams[name] = (offset, size)
    return streams


def blob(data: bytes, heap: int, index: int) -> bytes:
    """A blob of the #Blob heap starting at 'heap'."""
    size, start = compressed_unsigned(data, heap + index)
    if start + size > len(data):
        raise ValueError(f"blob 0x{index:x} lies beyond the end")
    return data[start:start + size]


def portable_pdb_documents(data: bytes) -> list[tuple[int, str]]:
    """The documents of a portable PDB (the metadata at the start of 'data'): (offset of the name's blob, full path
    as the compiler recorded it). Raises ValueError (or struct.error, IndexError) when the tables cannot be read."""
    streams = metadata_streams(data)
    if "#~" not in streams or "#Blob" not in streams:
        raise ValueError("no #~ or #Blob stream")
    tables = streams["#~"][0]
    heap = streams["#Blob"][0]
    heap_sizes = data[tables + 6]
    valid = struct.unpack_from("<Q", data, tables + 8)[0]
    position = tables + 24
    rows: dict[int, int] = {}
    for table in range(64):
        if valid >> table & 1:
            rows[table] = struct.unpack_from("<I", data, position)[0]
            position += 4
    if heap_sizes & METADATA_EXTRA_DATA:
        position += 4
    # A portable PDB holds no type-system tables (their row counts are in #Pdb), and Document is the first of the
    # debug tables, so its rows come first.
    if any(table < METADATA_TABLE_DOCUMENT for table in rows):
        raise ValueError("type-system tables in a portable PDB")
    blob_width = 4 if heap_sizes & METADATA_HEAP_BLOB_WIDE else 2
    guid_width = 4 if heap_sizes & METADATA_HEAP_GUID_WIDE else 2
    documents: list[tuple[int, str]] = []
    for _ in range(rows.get(METADATA_TABLE_DOCUMENT, 0)):
        name_index = int.from_bytes(data[position:position + blob_width], "little")
        position += 2 * blob_width + 2 * guid_width
        name = blob(data, heap, name_index)
        separator = name[:1].decode("utf-8") if name[:1] != b"\x00" else ""
        parts: list[str] = []
        cursor = 1
        while cursor < len(name):
            part, cursor = compressed_unsigned(name, cursor)
            parts.append(blob(data, heap, part).decode("utf-8") if part else "")
        documents.append((heap + name_index, separator.join(parts)))
    return documents


def embedded_portable_pdbs(data: bytes) -> list[tuple[int, bytes | None]]:
    """The embedded portable PDBs of a PE image: (file offset of the debug entry's data, the decompressed PDB, or
    None when it cannot be decompressed)."""
    embedded: list[tuple[int, bytes | None]] = []
    for record in pe_debug_records(data):
        if record.kind != PE_DEBUG_EMBEDDED_PORTABLE_PDB:
            continue
        raw = data[record.offset:record.offset + record.size]
        if raw[:4] != EMBEDDED_PDB_SIGNATURE or len(raw) < EMBEDDED_PDB_DATA_OFFSET:
            embedded.append((record.offset, None))
            continue
        expected = struct.unpack_from("<I", raw, 4)[0]
        try:
            pdb = zlib.decompress(raw[EMBEDDED_PDB_DATA_OFFSET:], -15)
        except zlib.error:
            embedded.append((record.offset, None))
            continue
        embedded.append((record.offset, pdb if len(pdb) == expected else None))
    return embedded


def elf_sections(data: bytes) -> list[section]:
    """The sections of an ELF file (32 or 64 bit, either byte order) that occupy file bytes; empty when the
    headers are not readable."""
    if len(data) < 0x34 or data[:4] != b"\x7fELF" or data[4] not in (1, 2) or data[5] not in (1, 2):
        return []
    wide = data[4] == 2
    order = "<" if data[5] == 1 else ">"
    if wide:
        if len(data) < 0x40:
            return []
        table, entry_size, count, names_index = struct.unpack_from(order + "Q10xHHH", data, 0x28)
        header_format = order + "IIQQQQ"
    else:
        table, entry_size, count, names_index = struct.unpack_from(order + "I10xHHH", data, 0x20)
        header_format = order + "IIIIII"
    if entry_size < struct.calcsize(header_format) or table + count * entry_size > len(data):
        return []
    headers = [struct.unpack_from(header_format, data, table + index * entry_size) for index in range(count)]
    if names_index >= count:
        return []
    names_offset = headers[names_index][4]
    sections: list[section] = []
    for name_index, kind, flags, _, offset, size in headers:
        if kind == ELF_SHT_NOBITS or size == 0:
            continue
        start = names_offset + name_index
        end = data.find(b"\x00", start)
        name = data[start:end].decode("ascii", errors="replace") if 0 <= start < end else "?"
        compressed = bool(flags & ELF_SHF_COMPRESSED) or name.startswith(".zdebug")
        sections.append(section(name, offset, size, compressed))
    return sections


def binary_sections(data: bytes) -> list[section]:
    return pe_sections(data) or elf_sections(data)


def section_name(sections: list[section], offset: int, codeview: list[tuple[int, int]] | None = None) -> str:
    name = "headers or unknown section" if sections else "unknown section"
    for item in sections:
        if item.offset <= offset < item.offset + item.size:
            name = item.name
            break
    for start, end in codeview or []:
        if start <= offset < end:
            return f"{name}, {SECTION_CODEVIEW}"
    return name


#------------------------------------------------------------------------------
# Scanning
#------------------------------------------------------------------------------

def surrounding_text(data: bytes, start: int, end: int, encoding: str) -> str:
    """The printable string a match sits in, at most CONTEXT_LIMIT bytes beyond the match on either side."""
    step = 2 if encoding == ENCODING_UTF16 else 1

    def printable(position: int) -> bool:
        if position < 0 or position + step > len(data):
            return False
        if step == 2:
            return data[position + 1] == 0 and 0x20 <= data[position] < 0x7F
        return data[position] >= 0x20 and data[position] != 0x7F

    left = start
    while start - left < CONTEXT_LIMIT and printable(left - step):
        left -= step
    right = end
    while right - end < CONTEXT_LIMIT and printable(right):
        right += step
    return data[left:right].decode(encoding, errors="replace")


def scan_documents(binary: pathlib.Path, pdb: bytes, roots: list[str], label: str,
                   offset: int | None) -> tuple[list[finding], set[tuple[int, str]]]:
    """The findings of the Document table of a portable PDB: each document whose reassembled path contains a root,
    or one finding when the table cannot be read. 'offset' replaces the place of each finding (an embedded PDB has
    no file offsets of its own); with the places a root starts."""
    try:
        documents = portable_pdb_documents(pdb)
    except (ValueError, struct.error, IndexError, UnicodeDecodeError) as error:
        place = 0 if offset is None else offset
        return [finding(binary, place, label, "unreadable",
                        f"the Document table of a portable PDB cannot be read ({error}), so its paths cannot be "
                        "checked")], set()
    findings: list[finding] = []
    places: set[tuple[int, str]] = set()
    for blob_offset, path in documents:
        encoded = path.encode("utf-8")
        place = blob_offset if offset is None else offset
        for root in roots:
            pattern = root_pattern(root, ENCODING_UTF8)
            if pattern is not None and pattern.search(encoded):
                places.add((blob_offset, label))
                findings.append(finding(binary, place, label, ENCODING_UTF8, path))
                break
    return findings, places


def scan_embedded(binary: pathlib.Path, data: bytes, roots: list[str]) -> tuple[list[finding], set[tuple[int, str]]]:
    """The findings of the portable PDBs embedded in a PE image, each placed at its debug entry: its bytes and its
    documents, or one finding when it cannot be decompressed."""
    findings: list[finding] = []
    places: set[tuple[int, str]] = set()
    for offset, pdb in embedded_portable_pdbs(data):
        if pdb is None:
            findings.append(finding(binary, offset, SECTION_EMBEDDED, "compressed",
                                    "an embedded portable PDB that cannot be decompressed cannot be scanned; export "
                                    "without debug symbols or with DebugType=portable"))
            continue
        inner, inner_places = scan_places(binary, pdb, roots)
        for item in inner:
            item.offset = offset
            item.section = f"{SECTION_EMBEDDED}, {item.section}"
        findings.extend(inner)
        places.update((offset + place, f"{SECTION_EMBEDDED}:{key}") for place, key in inner_places)
    return findings, places


def scan_places(binary: pathlib.Path, data: bytes, roots: list[str]) -> tuple[list[finding], set[tuple[int, str]]]:
    """The findings of a file and the places a root starts in it (offset and encoding or table): its bytes, the
    documents of a portable PDB and the PDBs a PE image embeds."""
    sections = binary_sections(data)
    codeview = codeview_ranges(data)
    findings: list[finding] = []
    for item in sections:
        if item.compressed:
            findings.append(finding(binary, item.offset, item.name, "compressed",
                                    "a compressed section cannot be scanned; build Distribution without -gz "
                                    "and without debug information, or strip the binary"))
    seen: set[tuple[str, str]] = set()
    places: set[tuple[int, str]] = set()
    for root in roots:
        for encoding in (ENCODING_UTF8, ENCODING_UTF16):
            pattern = root_pattern(root, encoding)
            if pattern is None:
                continue
            for match in pattern.finditer(data):
                places.add((match.start(), encoding))
                text = surrounding_text(data, match.start(), match.end(), encoding)
                name = section_name(sections, match.start(), codeview)
                if (text, name) in seen:
                    continue
                seen.add((text, name))
                findings.append(finding(binary, match.start(), name, encoding, text))
    if data[:4] == METADATA_SIGNATURE:
        documents, document_places = scan_documents(binary, data, roots, SECTION_DOCUMENT, None)
        findings.extend(documents)
        places.update(document_places)
    embedded, embedded_places = scan_embedded(binary, data, roots)
    findings.extend(embedded)
    places.update(embedded_places)
    findings.sort(key=lambda item: item.offset)
    return findings, places


def scan_bytes(binary: pathlib.Path, data: bytes, roots: list[str]) -> tuple[list[finding], int]:
    """The findings of a binary: each distinct string that contains a root (once per section), each document of a
    portable PDB (the file, or one a PE image embeds) whose path contains one, and each part that cannot be read (a
    compressed ELF section, an embedded PDB that does not decompress, an unreadable Document table); with the number
    of places a root starts (a place two roots share counts once)."""
    findings, places = scan_places(binary, data, roots)
    return findings, len(places)


def check_binaries(binaries: list[pathlib.Path], roots: list[str], max_findings: int) -> int:
    print("check_distribution_paths: roots " + ", ".join(roots))
    total = 0
    managed_findings = False
    for binary in binaries:
        try:
            data = binary.read_bytes()
        except OSError as error:
            print(f"check_distribution_paths: cannot read {binary}: {error}", file=sys.stderr)
            return EXIT_USAGE
        findings, matches = scan_bytes(binary, data, roots)
        managed_findings = managed_findings or (bool(findings) and is_managed(data))
        verdict = "clean" if not findings else f"{len(findings)} finding(s), a root at {matches} place(s)"
        print(f"check_distribution_paths: {binary.name}: {len(data)} byte(s) scanned, {verdict}")
        for item in findings[:max_findings]:
            print(f"  {item.format()}")
        if len(findings) > max_findings:
            print(f"  ... {len(findings) - max_findings} more")
        total += len(findings)

    if total:
        sys.stdout.flush()
        print(
            "check_distribution_paths: a Distribution binary must not name the checkout that built it (code sweep "
            "F613). Compile it with the Distribution path options of cmake/ConjureDistributionPaths.cmake (a target "
            "that replaces its directory's compile options loses them) and without debug information (-g, /DEBUG and "
            "a PDB path bring the roots back).",
            file=sys.stderr,
        )
        if managed_findings:
            print(
                "check_distribution_paths: a managed assembly or portable PDB gets its paths mapped only by the "
                "export's dotnet commands (engine/src/engine/export_commands.cpp: a CI build that imports the source "
                "roots file, follow-up FU-PDB); one built any other way keeps the paths of the machine.",
                file=sys.stderr,
            )
        return EXIT_FINDINGS
    return EXIT_CLEAN


#------------------------------------------------------------------------------
# Modes
#------------------------------------------------------------------------------

def checked_roots(arguments: list[str]) -> list[str] | None:
    """The roots of the --root arguments, each an existing directory; None (after a message) otherwise."""
    roots: list[str] = []
    for argument in arguments:
        if not argument or not pathlib.Path(argument).is_dir():
            print(f"check_distribution_paths: --root {argument!r} is not a directory", file=sys.stderr)
            return None
        roots.append(argument)
    return roots


def run_build_dir(
  build_dir_argument: str,
  config_argument: str | None,
  any_config: bool,
  with_shared: bool,
  extra_roots: list[str],
  max_findings: int,
) -> int:
    build_dir = pathlib.Path(build_dir_argument)
    cache = symbols.read_build_cache(build_dir, PROGRAM)
    if cache is None:
        return EXIT_USAGE
    source_dir = cache.get("CMAKE_HOME_DIRECTORY", "")
    if not source_dir:
        print(f"{PROGRAM}: the CMakeCache.txt of {build_dir} names no CMAKE_HOME_DIRECTORY", file=sys.stderr)
        return EXIT_USAGE
    code, binaries = symbols.resolve_distribution_binaries(build_dir, cache, config_argument, any_config, with_shared,
                                                           PROGRAM)
    if code != EXIT_CLEAN:
        return code
    cache_dir = cache.get("CMAKE_CACHEFILE_DIR", "") or str(build_dir)
    tree_roots = expand_roots([source_dir, cache_dir, str(build_dir)])
    project_roots = outer_roots(expand_roots(project_directories(cache)), tree_roots)
    roots = expand_roots(tree_roots + project_roots + extra_roots)
    return check_binaries(binaries, roots, max_findings)


def run_binaries(binary_arguments: list[str], extra_roots: list[str], max_findings: int) -> int:
    binaries: list[pathlib.Path] = []
    for argument in binary_arguments:
        binary = pathlib.Path(argument)
        if not binary.is_file():
            print(f"check_distribution_paths: {binary} does not exist", file=sys.stderr)
            return EXIT_USAGE
        binaries.append(binary)
    roots = expand_roots([str(ROOT)] + extra_roots)
    return check_binaries(binaries, roots, max_findings)


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
    parser.add_argument("--root", action="append", default=[], metavar="DIR",
                        help="another directory whose path must not appear in the binaries (repeatable)")
    parser.add_argument("--no-shared", action="store_true",
                        help="check the runtime executable only, not the conjure_shared library next to it")
    parser.add_argument("--any-config", action="store_true",
                        help="check a build directory even when it is not a Distribution build")
    parser.add_argument("--max-findings", type=int, default=DEFAULT_MAX_FINDINGS,
                        help=f"distinct strings printed per binary (default: {DEFAULT_MAX_FINDINGS})")
    args = parser.parse_args(argv)

    if args.max_findings < 1:
        print("check_distribution_paths: --max-findings must be positive", file=sys.stderr)
        return EXIT_USAGE
    extra_roots = checked_roots(args.root)
    if extra_roots is None:
        return EXIT_USAGE
    if args.build_dir and args.binary:
        print("check_distribution_paths: --build-dir and --binary are exclusive", file=sys.stderr)
        return EXIT_USAGE
    if args.binary:
        return run_binaries(args.binary, extra_roots, args.max_findings)
    if args.build_dir:
        return run_build_dir(args.build_dir, args.config, args.any_config, not args.no_shared, extra_roots,
                             args.max_findings)
    parser.print_usage(sys.stderr)
    print("check_distribution_paths: pass --build-dir <dir> or --binary <file>", file=sys.stderr)
    return EXIT_USAGE


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
