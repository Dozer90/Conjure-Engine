#!/usr/bin/env python3
"""Self-test for tools/check_distribution_paths.py (code sweep F613, follow-ups FU-PDB and FU-ROOTS).

Builds small binaries in a temporary directory: raw bytes, PE images and ELF files (32 and 64 bit, both byte
orders, with compressed and NOBITS sections, and with broken headers), managed PE images (PE32 and PE32+ with a CLR
header and a debug directory holding a CodeView entry and an embedded portable PDB) and portable PDBs (2- and
4-byte heap indices) that do or do not contain a root, and fake build directories (single- and multi-config
CMakeCache.txt, bin/ with a runtime and a shared library, and the cache of a parent project that adds Conjure from a
checkout of its own). It asserts what the tool finds, how it names the place,
and its exit codes. It also runs
cmake/ConjureDistributionPathsProbe.cmake (cmake -P with the CMake that CONJURE_CMAKE_COMMAND names, else the one on
PATH; without either the case prints a MESSAGE and returns), which checks the compile options that keep the roots
out of a Distribution binary. What those options do to a real build is proved by the case "regression - code sweep
F613 ..." of tests/core/foundation/test_log.cpp and by runtime.distribution_paths, this tool over the binaries of a
Distribution tree. Run it directly: `python tools/tests/test_check_distribution_paths.py`; ci.yml runs it in the
style-and-headers job, and CTest as runtime.check_distribution_paths_tool (tests/runtime/CMakeLists.txt, which sets
CONJURE_CMAKE_COMMAND). No third-party test framework is needed.
"""

from __future__ import annotations

import contextlib
import importlib.util
import io
import os
import pathlib
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib
from types import ModuleType
from typing import Callable

HERE = pathlib.Path(__file__).resolve().parent
TOOL = HERE.parent / "check_distribution_paths.py"
REPOSITORY = HERE.parent.parent
OPTIONS_PROBE = REPOSITORY / "cmake" / "ConjureDistributionPathsProbe.cmake"

ELF_SHT_PROGBITS = 1
ELF_SHT_STRTAB = 3
ELF_SHT_NOBITS = 8
ELF_SHF_COMPRESSED = 0x800

PE_DEBUG_CODEVIEW = 2
PE_DEBUG_EMBEDDED_PORTABLE_PDB = 17

notes: list[str] = []


def load_tool() -> ModuleType:
    specification = importlib.util.spec_from_file_location("conjure_check_distribution_paths", TOOL)
    assert specification is not None and specification.loader is not None
    module = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(module)
    return module


def check(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def note(text: str) -> None:
    if text not in notes:
        notes.append(text)


class run_result:
    def __init__(self, code: int, output: str, errors: str) -> None:
        self.code = code
        self.output = output
        self.errors = errors

    def describe(self) -> str:
        return f"exit {self.code}\nstdout:\n{self.output}\nstderr:\n{self.errors}"


def run_tool(tool: ModuleType, arguments: list[str]) -> run_result:
    output = io.StringIO()
    errors = io.StringIO()
    with contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors):
        code = tool.main(arguments)
    return run_result(code, output.getvalue(), errors.getvalue())


def expect_exit(result: run_result, code: int, needles: list[str], context: str) -> None:
    check(result.code == code, f"{context}: expected exit {code}\n{result.describe()}")
    text = result.output + result.errors
    missing = [needle for needle in needles if needle not in text]
    check(not missing, f"{context}: missing {missing}\n{result.describe()}")


# ---------------------------------------------------------------------------------------------------------------
# Fixtures


def pe_image(sections: list[tuple[str, bytes]]) -> bytes:
    """A PE image without an optional header: DOS stub, signature, COFF header, section table, raw data."""
    header_offset = 0x40
    table_offset = header_offset + 24
    data_offset = table_offset + 40 * len(sections)
    dos = bytearray(header_offset)
    dos[0:2] = b"MZ"
    struct.pack_into("<I", dos, 0x3C, header_offset)
    coff = b"PE\x00\x00" + struct.pack("<HHIIIHH", 0x8664, len(sections), 0, 0, 0, 0, 0x22)
    table = bytearray()
    raw = bytearray()
    for name, content in sections:
        offset = data_offset + len(raw)
        table += struct.pack("<8sIIIIIIHHI", name.encode("ascii"), len(content), 0x1000, len(content), offset,
                             0, 0, 0, 0, 0x40000040)
        raw += content
    return bytes(dos) + coff + bytes(table) + bytes(raw)


def elf_image(wide: bool, little: bool, sections: list[tuple[str, int, int, bytes]]) -> bytes:
    """An ELF file: header, the section contents, .shstrtab, then the section header table (index 0 is the null
    section). Each section is (name, type, flags, content); a NOBITS section points at its content's place but
    occupies no file bytes the tool may read."""
    order = "<" if little else ">"
    header_size = 0x40 if wide else 0x34
    entry_format = order + ("IIQQQQIIQQ" if wide else "IIIIIIIIII")
    entry_size = struct.calcsize(entry_format)
    names = bytearray(b"\x00")
    body = bytearray()
    entries: list[tuple[int, int, int, int, int]] = []
    for name, kind, flags, content in sections:
        entries.append((len(names), kind, flags, header_size + len(body), len(content)))
        names += name.encode("ascii") + b"\x00"
        body += content
    names_name = len(names)
    names += b".shstrtab\x00"
    entries.append((names_name, ELF_SHT_STRTAB, 0, header_size + len(body), len(names)))
    body += names
    table_offset = header_size + len(body)
    count = len(entries) + 1
    identification = b"\x7fELF" + bytes([2 if wide else 1, 1 if little else 2, 1]) + bytes(9)
    if wide:
        header = identification + struct.pack(order + "HHIQQQIHHHHHH", 2, 0x3E, 1, 0, 0, table_offset, 0,
                                              header_size, 0, 0, entry_size, count, count - 1)
    else:
        header = identification + struct.pack(order + "HHIIIIIHHHHHH", 2, 3, 1, 0, 0, table_offset, 0,
                                              header_size, 0, 0, entry_size, count, count - 1)
    table = bytearray(struct.pack(entry_format, *([0] * 10)))
    for name_offset, kind, flags, offset, size in entries:
        table += struct.pack(entry_format, name_offset, kind, flags, 0, offset, size, 0, 0, 1, 0)
    return header + bytes(body) + bytes(table)


def compressed_unsigned(value: int) -> bytes:
    """An ECMA-335 compressed unsigned integer."""
    if value < 0x80:
        return bytes([value])
    if value < 0x4000:
        return struct.pack(">H", 0x8000 | value)
    return struct.pack(">I", 0xC0000000 | value)


def padded(data: bytes) -> bytes:
    return data + bytes(-len(data) % 4)


def portable_pdb(documents: list[tuple[str, list[str]]], wide: bool = False, padding: int = 0) -> bytes:
    """A portable PDB with a Document table and nothing else: each document is (separator, parts), stored the way
    the compiler stores a path, its parts as blobs of their own. 'wide' makes the blob and GUID indices 4 bytes.
    'padding' puts a blob of that many bytes first, so that the parts that follow lie at heap indices that take two
    (0x80 to 0x3FFF) or four (from 0x4000) bytes as compressed integers, as in the large heap of a real PDB."""
    heap = bytearray(b"\x00")

    def add_blob(content: bytes) -> int:
        index = len(heap)
        heap.extend(compressed_unsigned(len(content)) + content)
        return index

    if padding:
        add_blob(bytes(padding))

    rows = bytearray()
    width = 4 if wide else 2
    for separator, parts in documents:
        indices = [add_blob(part.encode("utf-8")) if part else 0 for part in parts]
        name = (separator.encode("utf-8") or b"\x00") + b"".join(compressed_unsigned(index) for index in indices)
        rows += add_blob(name).to_bytes(width, "little") + (1).to_bytes(width, "little")
        rows += (0).to_bytes(width, "little") + (1).to_bytes(width, "little")
    tables = struct.pack("<IBBBBQQI", 0, 2, 0, 0x06 if wide else 0x00, 1, 1 << 0x30, 0, len(documents)) + bytes(rows)
    streams = [("#Pdb", bytes(32)), ("#~", padded(tables)), ("#Blob", padded(bytes(heap))), ("#GUID", bytes(16))]
    version = padded(b"PDB v1.0\x00")
    headers_size = sum(8 + len(padded(name.encode("ascii") + b"\x00")) for name, _ in streams)
    offset = 16 + len(version) + 4 + headers_size
    headers = bytearray()
    body = bytearray()
    for name, content in streams:
        headers += struct.pack("<II", offset + len(body), len(content)) + padded(name.encode("ascii") + b"\x00")
        body += content
    root = b"BSJB" + struct.pack("<HHII", 1, 1, 0, len(version)) + version + struct.pack("<HH", 0, len(streams))
    return root + bytes(headers) + bytes(body)


def deflated(data: bytes) -> bytes:
    compressor = zlib.compressobj(9, zlib.DEFLATED, -15)
    return compressor.compress(data) + compressor.flush()


def managed_pe_image(records: list[tuple[int, bytes]], plus: bool = False, clr: bool = True) -> bytes:
    """A PE image with an optional header (PE32, or PE32+ when 'plus'), one .text section mapped at 0x2000 and
    holding a CLR header (when 'clr'), the debug directory and the data of each (type, data) debug record."""
    text_address = 0x2000
    optional_size = 240 if plus else 224
    data_offset = 0x40 + 24 + optional_size + 40
    clr_size = 72
    directory_size = 28 * len(records)
    payload = bytearray()
    entries = bytearray()
    for kind, content in records:
        relative = clr_size + directory_size + len(payload)
        entries += struct.pack("<IIHHIIII", 0, 0, 0, 0, kind, len(content), text_address + relative,
                               data_offset + relative)
        payload += content
    text = bytes(clr_size) + bytes(entries) + bytes(payload)
    directories = bytearray(16 * 8)
    if clr:
        struct.pack_into("<II", directories, 14 * 8, text_address, clr_size)
    if records:
        struct.pack_into("<II", directories, 6 * 8, text_address + clr_size, directory_size)
    count_offset, directory_offset = (108, 112) if plus else (92, 96)
    optional = bytearray(optional_size)
    struct.pack_into("<H", optional, 0, 0x20B if plus else 0x10B)
    struct.pack_into("<I", optional, count_offset, 16)
    optional[directory_offset:directory_offset + len(directories)] = directories
    dos = bytearray(0x40)
    dos[0:2] = b"MZ"
    struct.pack_into("<I", dos, 0x3C, 0x40)
    coff = b"PE\x00\x00" + struct.pack("<HHIIIHH", 0x8664 if plus else 0x14C, 1, 0, 0, 0, optional_size, 0x2022)
    table = struct.pack("<8sIIIIIIHHI", b".text", len(text), text_address, len(text), data_offset, 0, 0, 0, 0,
                        0x60000020)
    return bytes(dos) + coff + bytes(optional) + table + text


def codeview(path: bytes) -> tuple[int, bytes]:
    return PE_DEBUG_CODEVIEW, b"RSDS" + bytes(16) + struct.pack("<I", 1) + path + b"\x00"


def embedded_pdb(pdb: bytes, size: int | None = None, content: bytes | None = None) -> tuple[int, bytes]:
    stream = deflated(pdb) if content is None else content
    return PE_DEBUG_EMBEDDED_PORTABLE_PDB, b"MPDB" + struct.pack("<I", len(pdb) if size is None else size) + stream


def write(path: pathlib.Path, content: bytes) -> pathlib.Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(content)
    return path


def under(root: pathlib.Path, relative: str) -> bytes:
    """The UTF-8 bytes of a path below a root, spelled as the platform spells the root."""
    return (str(root) + os.sep + relative).encode("utf-8")


def fake_tree(
  temporary: pathlib.Path,
  name: str,
  build_type: str,
  runtime: bytes,
  shared: bytes | None,
  configuration_types: str = "",
  bin_subdirectory: str = "",
  cache_entries: tuple[str, ...] = (),
) -> tuple[pathlib.Path, pathlib.Path]:
    """A source directory and a configured build directory inside it with bin/[<config>/] binaries; 'cache_entries'
    are further CMakeCache.txt lines."""
    source = temporary / name / "source"
    build = source / "build" / "dist"
    source.mkdir(parents=True)
    build.mkdir(parents=True)
    cache = [
        "# This is the CMakeCache file.",
        f"CMAKE_BUILD_TYPE:STRING={build_type}",
        f"CMAKE_CACHEFILE_DIR:INTERNAL={build.as_posix()}",
        f"CMAKE_HOME_DIRECTORY:INTERNAL={source.as_posix()}",
    ]
    if configuration_types:
        cache.append(f"CMAKE_CONFIGURATION_TYPES:STRING={configuration_types}")
    cache.extend(cache_entries)
    (build / "CMakeCache.txt").write_text("\n".join(cache) + "\n", encoding="utf-8")
    bin_dir = build / "bin" / bin_subdirectory if bin_subdirectory else build / "bin"
    write(bin_dir / "conjure_runtime", runtime)
    if shared is not None:
        write(bin_dir / "libconjure_shared.so", shared)
    return source, build


# ---------------------------------------------------------------------------------------------------------------
# Cases


def spellings_are_found(tool: ModuleType, temporary: pathlib.Path) -> None:
    binary = temporary / "spellings.bin"
    root = "C:/Work/Git Repos/conjure"
    spellings = [
        b"C:/Work/Git Repos/conjure/engine/src/log.cpp",
        b"C:\\Work\\Git Repos\\conjure\\engine\\src\\log.cpp",
        b"c:\\work\\git repos\\CONJURE\\engine/include\\conjure/core/log.h",
        b"C:\\\\Work\\\\Git Repos\\\\conjure\\\\x.cpp",
        b"C://Work//Git Repos/conjure/x.cpp",
    ]
    for spelling in spellings:
        findings, places = tool.scan_bytes(binary, b"\x00\x01" + spelling + b"\x00\x02", [root])
        check(len(findings) == 1 and places == 1, f"{spelling!r}: {[item.format() for item in findings]}")
        check(findings[0].text == spelling.decode("utf-8"), f"{spelling!r}: printed {findings[0].text!r}")
        check(findings[0].offset == 2, f"{spelling!r}: offset {findings[0].offset}")


def siblings_are_not_found(tool: ModuleType, temporary: pathlib.Path) -> None:
    binary = temporary / "siblings.bin"
    root = "C:/work/conjure"
    for sibling in (b"C:/work/conjure_old/x.cpp", b"C:/work/conjure.old", b"C:/work/conjure-2/x.cpp",
                    b"C:/work/conjureX", "C:/work/conjureé/x.cpp".encode("utf-8"), b"C:/work/conj/x.cpp",
                    b"D:/work/conjure2"):
        findings, places = tool.scan_bytes(binary, b"\x00" + sibling + b"\x00", [root])
        check(not findings and places == 0, f"{sibling!r}: {[item.format() for item in findings]}")


def root_ends_are_found(tool: ModuleType, temporary: pathlib.Path) -> None:
    binary = temporary / "ends.bin"
    root = "/home/ci/conjure"
    for ending in (b"\x00", b"\"", b"'", b":12", b" (debug)", b"\n", b""):
        data = b"\x00/home/ci/conjure" + ending
        findings, places = tool.scan_bytes(binary, data, [root])
        check(len(findings) == 1 and places == 1, f"ending {ending!r}: {[item.format() for item in findings]}")


def posix_root_needs_its_leading_separator(tool: ModuleType, temporary: pathlib.Path) -> None:
    binary = temporary / "posix.bin"
    root = "/home/ci/conjure"
    for text, expected in ((b"/home/ci/conjure/engine/x.cpp", 1), (b"//home//ci/conjure/x.cpp", 1),
                           (b"\\home\\ci\\conjure\\x.cpp", 1), (b"home/ci/conjure/x.cpp", 0),
                           (b"/home/ci/conjure-other/x.cpp", 0)):
        findings, _ = tool.scan_bytes(binary, b"\x00" + text + b"\x00", [root])
        check(len(findings) == expected, f"{text!r}: expected {expected}, {[item.format() for item in findings]}")


def wide_strings_are_found(tool: ModuleType, temporary: pathlib.Path) -> None:
    binary = temporary / "wide.bin"
    root = "C:/work/conjure"
    wide = "C:\\WORK\\conjure\\engine\\log.cpp".encode("utf-16le")
    findings, places = tool.scan_bytes(binary, b"\x00\x00" + wide + b"\x00\x00", [root])
    check(len(findings) == 1 and places == 1, str([item.format() for item in findings]))
    check(findings[0].encoding == tool.ENCODING_UTF16, findings[0].encoding)
    check(findings[0].text == "C:\\WORK\\conjure\\engine\\log.cpp", findings[0].text)
    sibling = "C:/work/conjure_old/log.cpp".encode("utf-16le")
    findings, places = tool.scan_bytes(binary, b"\x00\x00" + sibling + b"\x00\x00", [root])
    check(not findings and places == 0, str([item.format() for item in findings]))


def pe_sections_are_named(tool: ModuleType, temporary: pathlib.Path) -> None:
    root = "C:/work/conjure"
    image = pe_image([(".text", b"\xcc" * 32), (".rdata", b"\x00C:\\work\\conjure\\engine\\log.cpp\x00")])
    binary = write(temporary / "image.dll", image)
    findings, _ = tool.scan_bytes(binary, image, [root])
    check([item.section for item in findings] == [".rdata"], str([item.format() for item in findings]))
    clean = pe_image([(".text", b"\xcc" * 32), (".rdata", b"\x00engine\\log.cpp\x00")])
    findings, places = tool.scan_bytes(binary, clean, [root])
    check(not findings and places == 0, str([item.format() for item in findings]))
    # Bytes after the raw data of the last section (an overlay) belong to no section of a readable image.
    overlay = clean + b"\x00C:/work/conjure/build/generated/abi.h\x00"
    findings, places = tool.scan_bytes(binary, overlay, [root])
    check([item.section for item in findings] == ["headers or unknown section"] and places == 1,
          str([item.format() for item in findings]))


def elf_sections_are_named(tool: ModuleType, temporary: pathlib.Path) -> None:
    root = "/home/ci/conjure"
    for wide in (True, False):
        for little in (True, False):
            layout = f"{'64' if wide else '32'}-bit {'little' if little else 'big'}-endian"
            image = elf_image(wide, little, [
                (".text", ELF_SHT_PROGBITS, 0, b"\x90" * 16),
                (".rodata", ELF_SHT_PROGBITS, 0, b"\x00/home/ci/conjure/engine/log.cpp\x00"),
                (".bss", ELF_SHT_NOBITS, 0, b"\x00\x00\x00\x00"),
                (".debug_str", ELF_SHT_PROGBITS, 0, b"/home/ci/conjure/build/_deps/jolt\x00"),
            ])
            binary = write(temporary / f"elf_{wide}_{little}.so", image)
            sections = [item.name for item in tool.elf_sections(image)]
            check(sections == [".text", ".rodata", ".debug_str", ".shstrtab"], f"{layout}: sections {sections}")
            findings, places = tool.scan_bytes(binary, image, [root])
            named = [item.section for item in findings]
            check(named == [".rodata", ".debug_str"] and places == 2, f"{layout}: {named}, {places} place(s)")


def compressed_sections_are_findings(tool: ModuleType, temporary: pathlib.Path) -> None:
    root = "/home/ci/conjure"
    image = elf_image(True, True, [
        (".rodata", ELF_SHT_PROGBITS, 0, b"\x00engine/log.cpp\x00"),
        (".debug_info", ELF_SHT_PROGBITS, ELF_SHF_COMPRESSED, b"\x01\x00\x00\x00" + bytes(20)),
        (".zdebug_str", ELF_SHT_PROGBITS, 0, b"ZLIB" + bytes(8)),
    ])
    binary = write(temporary / "compressed.so", image)
    findings, places = tool.scan_bytes(binary, image, [root])
    check([(item.section, item.encoding) for item in findings]
          == [(".debug_info", "compressed"), (".zdebug_str", "compressed")] and places == 0,
          str([item.format() for item in findings]))
    expect_exit(run_tool(tool, ["--binary", str(binary), "--root", str(temporary)]), tool.EXIT_FINDINGS,
                ["[.debug_info] compressed", "[.zdebug_str] compressed", "code sweep F613"], "compressed sections")


def broken_headers_give_no_sections(tool: ModuleType, temporary: pathlib.Path) -> None:
    root = "/home/ci/conjure"
    payload = b"\x00/home/ci/conjure/x.cpp\x00"
    good_elf = elf_image(True, True, [(".rodata", ELF_SHT_PROGBITS, 0, payload)])
    table_beyond_end = bytearray(good_elf)
    struct.pack_into("<Q", table_beyond_end, 0x28, len(good_elf) + 4096)
    names_out_of_range = bytearray(good_elf)
    struct.pack_into("<H", names_out_of_range, 0x3E, 99)
    bad_pe = bytearray(pe_image([(".rdata", payload)]))
    struct.pack_into("<I", bad_pe, 0x3C, len(bad_pe) + 16)
    cases = {
        "truncated ELF header": b"\x7fELF\x02\x01\x01" + payload,
        "ELF section table beyond the end": bytes(table_beyond_end) + payload,
        "ELF name table index out of range": bytes(names_out_of_range) + payload,
        "ELF of an unknown class": b"\x7fELF\x07\x01\x01" + bytes(0x40) + payload,
        "PE header beyond the end": bytes(bad_pe),
        "truncated PE": b"MZ" + payload,
    }
    for label, data in cases.items():
        binary = write(temporary / "broken.bin", data)
        check(tool.binary_sections(data) == [], f"{label}: sections were read")
        findings, places = tool.scan_bytes(binary, data, [root])
        check(places >= 1, f"{label}: the root was not found")
        check(all(item.section == "unknown section" for item in findings),
              f"{label}: {[item.format() for item in findings]}")


def codeview_paths_are_read(tool: ModuleType, temporary: pathlib.Path) -> None:
    root = "C:/work/conjure"
    dirty = b"C:\\work\\conjure\\build\\managed_artifacts\\obj\\Game\\release\\Game.pdb"
    for plus in (False, True):
        for clr in (True, False):
            layout = f"{'PE32+' if plus else 'PE32'}, {'managed' if clr else 'native'}"
            image = managed_pe_image([codeview(dirty)], plus, clr)
            binary = write(temporary / f"codeview_{plus}_{clr}.dll", image)
            check(tool.is_managed(image) == clr, f"{layout}: is_managed")
            ranges = tool.codeview_ranges(image)
            check(len(ranges) == 1 and image[ranges[0][0]:ranges[0][1]] == dirty, f"{layout}: ranges {ranges}")
            findings, places = tool.scan_bytes(binary, image, [root])
            check([(item.section, item.text) for item in findings]
                  == [(".text, CodeView PDB path", dirty.decode("utf-8"))] and places == 1,
                  f"{layout}: {[item.format() for item in findings]}")
            clean = managed_pe_image([codeview(b"/_/build/managed_artifacts/obj/Game/release/Game.pdb")], plus, clr)
            findings, places = tool.scan_bytes(binary, clean, [root])
            check(not findings and places == 0, f"{layout}: {[item.format() for item in findings]}")
    binary = write(temporary / "Game.dll", managed_pe_image([codeview(dirty)]))
    expect_exit(run_tool(tool, ["--binary", str(binary), "--root", str(temporary)]), tool.EXIT_CLEAN, ["clean"],
                "a managed assembly without the root")
    rooted = write(temporary / "rooted" / "Game.dll",
                   managed_pe_image([codeview(under(temporary, "rooted/obj/Game.pdb"))]))
    expect_exit(run_tool(tool, ["--binary", str(rooted), "--root", str(temporary)]), tool.EXIT_FINDINGS,
                ["[.text, CodeView PDB path] utf-8", "follow-up FU-PDB"], "a managed assembly naming the root")
    # The hint about the export's mapping is for managed code only: a native image gets the F613 advice alone.
    native = write(temporary / "rooted" / "native.dll",
                   managed_pe_image([codeview(under(temporary, "rooted/obj/native.pdb"))], clr=False))
    result = run_tool(tool, ["--binary", str(native), "--root", str(temporary)])
    expect_exit(result, tool.EXIT_FINDINGS, ["[.text, CodeView PDB path] utf-8", "code sweep F613"],
                "a native image naming the root")
    check("follow-up FU-PDB" not in result.output + result.errors,
          f"a native image got the managed hint\n{result.describe()}")


def broken_debug_directories_give_no_records(tool: ModuleType, temporary: pathlib.Path) -> None:
    good = managed_pe_image([codeview(b"C:\\work\\conjure\\Game.pdb")])
    check(len(tool.pe_debug_records(good)) == 1, "the intact image has one debug record")
    no_optional_header = pe_image([(".text", b"\x00RSDS" + bytes(30))])
    unknown_magic = bytearray(good)
    struct.pack_into("<H", unknown_magic, 0x40 + 24, 0x107)
    directory_beyond_end = bytearray(good)
    struct.pack_into("<I", directory_beyond_end, 0x40 + 24 + 96 + 6 * 8, 0x7FFF0000)
    data_beyond_end = bytearray(good)
    entry = 0x40 + 24 + 224 + 40 + 72
    struct.pack_into("<I", data_beyond_end, entry + 24, len(good) + 64)
    struct.pack_into("<I", data_beyond_end, entry + 20, 0x7FFF0000)
    few_directories = bytearray(good)
    struct.pack_into("<I", few_directories, 0x40 + 24 + 92, 6)
    cases = {
        "a PE image without an optional header": no_optional_header,
        "an optional header of an unknown kind": bytes(unknown_magic),
        "a debug directory no section maps": bytes(directory_beyond_end),
        "debug data beyond the end of the file": bytes(data_beyond_end),
        "fewer data directories than the debug directory's index": bytes(few_directories),
        "no PE image at all": b"\x7fELF" + bytes(60),
    }
    for label, data in cases.items():
        check(tool.pe_debug_records(data) == [], f"{label}: records were read")
        check(tool.codeview_ranges(data) == [], f"{label}: CodeView ranges were read")
        check(tool.embedded_portable_pdbs(data) == [], f"{label}: embedded PDBs were read")


def portable_pdb_documents_are_findings(tool: ModuleType, temporary: pathlib.Path) -> None:
    documents = [
        ("\\", ["C:", "work", "conjure", "Game", "Player.cs"]),
        ("/", ["", "_", "Game", "Mover.cs"]),
        ("/", ["", "home", "ci", "conjure", "obj", "Game.AssemblyInfo.cs"]),
        ("\\", ["C:", "work", "conjure_old", "Old.cs"]),
        ("", ["Unrooted.cs"]),
    ]
    # The first part follows the padding blob and its compressed length: index 1 without padding (one byte as a
    # compressed integer), 0x103 after 0x100 bytes (two bytes), 0x4105 after 0x4100 bytes (four bytes, and the
    # padding's own length takes four too).
    layouts = [(wide, padding) for wide in (False, True) for padding in (0, 0x100, 0x4100)]
    for wide, padding in layouts:
        layout = f"{'4-byte' if wide else '2-byte'} heap indices, parts after a {padding:#x}-byte blob"
        pdb = portable_pdb(documents, wide, padding)
        read = [path for _, path in tool.portable_pdb_documents(pdb)]
        check(read == ["C:\\work\\conjure\\Game\\Player.cs", "/_/Game/Mover.cs",
                       "/home/ci/conjure/obj/Game.AssemblyInfo.cs", "C:\\work\\conjure_old\\Old.cs",
                       "Unrooted.cs"], f"{layout}: documents {read}")
        binary = write(temporary / f"Game_{wide}_{padding}.pdb", pdb)
        findings, places = tool.scan_bytes(binary, pdb, ["C:/work/conjure", "/home/ci/conjure"])
        check([(item.section, item.text) for item in findings]
              == [("portable PDB document", "C:\\work\\conjure\\Game\\Player.cs"),
                  ("portable PDB document", "/home/ci/conjure/obj/Game.AssemblyInfo.cs")] and places == 2,
              f"{layout}: {[item.format() for item in findings]}")
        # The pieces alone hold no root: the same bytes that are not read as a PDB give no finding.
        findings, places = tool.scan_bytes(binary, b"\x00" + pdb, ["C:/work/conjure", "/home/ci/conjure"])
        check(not findings and places == 0, f"{layout}: {[item.format() for item in findings]}")
    path = str(temporary / "Game" / "Player.cs")
    rooted = write(temporary / "rooted.pdb", portable_pdb([(os.sep, path.split(os.sep))]))
    expect_exit(run_tool(tool, ["--binary", str(rooted), "--root", str(temporary)]), tool.EXIT_FINDINGS,
                ["[portable PDB document] utf-8", "Player.cs", "follow-up FU-PDB"], "a .pdb naming the root")
    clean = write(temporary / "clean.pdb", portable_pdb([("/", ["", "_", "Game", "Player.cs"])]))
    expect_exit(run_tool(tool, ["--binary", str(clean), "--root", str(temporary)]), tool.EXIT_CLEAN, ["clean"],
                "a mapped .pdb")


def unreadable_portable_pdbs_are_findings(tool: ModuleType, temporary: pathlib.Path) -> None:
    pdb = portable_pdb([("/", ["", "_", "Player.cs"])])
    no_tables = bytearray(pdb)
    no_tables[no_tables.index(b"#~\x00")] = ord("X")
    # The heap of ("/", ["", "X"]) is 0x00, then the part "X" at index 1 (01 'X'), then the name at index 3: its
    # length 3, the separator, the empty part 0 and the part 1, whose index becomes 0xE0, a prefix ECMA-335 defines
    # no compressed integer for.
    single = portable_pdb([("/", ["", "X"])])
    name = b"\x03/\x00\x01"
    check(single.count(name) == 1, "the name blob of the invalid-index fixture is not where it was expected")
    invalid_index = single.replace(name, b"\x03/\x00\xe0")
    cases = {
        "a truncated stream table": pdb[:24],
        "no #~ stream": bytes(no_tables),
        "a stream beyond the end": pdb[:-8],
        "only the signature": b"BSJB",
        "a part index with the invalid compressed prefix 0xE0": invalid_index,
    }
    for label, data in cases.items():
        binary = write(temporary / "broken.pdb", data)
        findings, _ = tool.scan_bytes(binary, data, ["/home/ci/conjure"])
        check([(item.section, item.encoding) for item in findings] == [("portable PDB document", "unreadable")],
              f"{label}: {[item.format() for item in findings]}")
        expect_exit(run_tool(tool, ["--binary", str(binary), "--root", str(temporary)]), tool.EXIT_FINDINGS,
                    ["cannot be read"], label)


def embedded_portable_pdbs_are_scanned(tool: ModuleType, temporary: pathlib.Path) -> None:
    root = "C:/work/conjure"
    dirty = portable_pdb([("\\", ["C:", "work", "conjure", "Game", "Player.cs"])])
    clean = portable_pdb([("/", ["", "_", "Game", "Player.cs"])])
    image = managed_pe_image([codeview(b"Game.pdb"), embedded_pdb(dirty)])
    binary = write(temporary / "embedded.dll", image)
    records = tool.pe_debug_records(image)
    check([record.kind for record in records] == [PE_DEBUG_CODEVIEW, PE_DEBUG_EMBEDDED_PORTABLE_PDB],
          f"records {[record.kind for record in records]}")
    embedded = tool.embedded_portable_pdbs(image)
    check(len(embedded) == 1 and embedded[0] == (records[1].offset, dirty), "the embedded PDB decompresses")
    findings, places = tool.scan_bytes(binary, image, [root])
    check([(item.offset, item.section, item.text) for item in findings]
          == [(records[1].offset, "embedded portable PDB, portable PDB document",
               "C:\\work\\conjure\\Game\\Player.cs")] and places == 1,
          str([item.format() for item in findings]))
    findings, places = tool.scan_bytes(binary, managed_pe_image([embedded_pdb(clean)]), [root])
    check(not findings and places == 0, str([item.format() for item in findings]))
    broken = {
        "a stream that does not inflate": embedded_pdb(clean, content=b"\xff\xff\xff\xff"),
        "a size that disagrees with the stream": embedded_pdb(clean, size=len(clean) + 1),
        "a record without the MPDB signature": (PE_DEBUG_EMBEDDED_PORTABLE_PDB, b"XPDB" + bytes(12)),
    }
    for label, record in broken.items():
        data = managed_pe_image([record])
        findings, _ = tool.scan_bytes(binary, data, [root])
        check([(item.section, item.encoding) for item in findings] == [("embedded portable PDB", "compressed")],
              f"{label}: {[item.format() for item in findings]}")


def repeated_strings_are_reported_once(tool: ModuleType, temporary: pathlib.Path) -> None:
    binary = temporary / "repeated.bin"
    text = b"/home/ci/conjure/build/dist/generated/abi.h"
    data = b"\x00" + text + b"\x00" + text + b"\x00"
    findings, places = tool.scan_bytes(binary, data, ["/home/ci/conjure", "/home/ci/conjure/build/dist"])
    check(len(findings) == 1, str([item.format() for item in findings]))
    # The build directory starts at the same place as the source directory, so the two roots add no place.
    check(places == 2, f"{places} place(s)")


def binary_mode_uses_the_repository(tool: ModuleType, temporary: pathlib.Path) -> None:
    dirty = write(temporary / "binary_mode" / "dirty.bin", b"\x00" + under(REPOSITORY, "engine/src/log.cpp"))
    clean = write(temporary / "binary_mode" / "clean.bin", b"\x00engine/src/log.cpp\x00")
    expect_exit(run_tool(tool, ["--binary", str(dirty)]), tool.EXIT_FINDINGS, ["dirty.bin: 0x1 [unknown section]"],
                "dirty binary")
    expect_exit(run_tool(tool, ["--binary", str(clean)]), tool.EXIT_CLEAN, ["clean.bin", "clean"], "clean binary")
    both = run_tool(tool, ["--binary", str(clean), "--binary", str(dirty)])
    expect_exit(both, tool.EXIT_FINDINGS, [f"clean.bin: {clean.stat().st_size} byte(s) scanned, clean", "dirty.bin"],
                "two binaries")


def extra_roots_are_checked(tool: ModuleType, temporary: pathlib.Path) -> None:
    other = temporary / "extra" / "elsewhere"
    other.mkdir(parents=True)
    binary = write(temporary / "extra" / "app.bin", b"\x00" + under(other, "deps/lib.c") + b"\x00")
    expect_exit(run_tool(tool, ["--binary", str(binary)]), tool.EXIT_CLEAN, ["clean"], "without --root")
    expect_exit(run_tool(tool, ["--binary", str(binary), "--root", str(other)]), tool.EXIT_FINDINGS,
                ["deps"], "with --root")
    expect_exit(run_tool(tool, ["--binary", str(binary), "--root", str(other / "missing")]), tool.EXIT_USAGE,
                ["is not a directory"], "missing --root")
    expect_exit(run_tool(tool, ["--binary", str(binary), "--root", ""]), tool.EXIT_USAGE,
                ["is not a directory"], "empty --root")


def build_dir_checks_runtime_and_shared(tool: ModuleType, temporary: pathlib.Path) -> None:
    clean_source, clean_build = fake_tree(temporary, "clean", "Distribution", b"\x00main.cpp\x00", b"\x00log.cpp\x00")
    expect_exit(run_tool(tool, ["--build-dir", str(clean_build)]), tool.EXIT_CLEAN,
                ["conjure_runtime: 10 byte(s) scanned, clean", "libconjure_shared.so: 9 byte(s) scanned, clean"],
                "clean tree")

    source = temporary / "dirty_shared" / "source"
    shared = b"\x00" + under(source, "engine/src/core/log.cpp") + b"\x00"
    _, build = fake_tree(temporary, "dirty_shared", "Distribution", b"\x00main.cpp\x00", shared)
    expect_exit(run_tool(tool, ["--build-dir", str(build)]), tool.EXIT_FINDINGS,
                ["conjure_runtime: 10 byte(s) scanned, clean", "libconjure_shared.so: 0x1 [unknown section]"],
                "dirty shared library")
    expect_exit(run_tool(tool, ["--build-dir", str(build), "--no-shared"]), tool.EXIT_CLEAN, ["clean"],
                "--no-shared skips the shared library")

    outside = temporary / "outside_build"
    outside.mkdir()
    (outside / "CMakeCache.txt").write_text(
        f"CMAKE_BUILD_TYPE:STRING=Distribution\nCMAKE_HOME_DIRECTORY:INTERNAL={clean_source.as_posix()}\n",
        encoding="utf-8")
    write(outside / "bin" / "conjure_runtime.exe", b"\x00" + under(outside, "generated/abi.cpp") + b"\x00")
    write(outside / "bin" / "conjure_shared.dll", b"\x00")
    expect_exit(run_tool(tool, ["--build-dir", str(outside)]), tool.EXIT_FINDINGS, ["generated"],
                "a build directory outside the source directory is a root of its own")


def roots_line(result: run_result) -> str:
    """The line on which the tool lists the roots it looks for."""
    prefix = "check_distribution_paths: roots "
    lines = [line for line in result.output.splitlines() if line.startswith(prefix)]
    check(len(lines) == 1, f"expected one roots line\n{result.describe()}")
    return lines[0][len(prefix):]


def project_entries(source: pathlib.Path, binary: pathlib.Path) -> tuple[str, ...]:
    """The CMakeCache.txt lines CMake writes for Conjure's project(), as STATIC entries."""
    return (f"Conjure_SOURCE_DIR:STATIC={source.as_posix()}", f"Conjure_BINARY_DIR:STATIC={binary.as_posix()}")


def subproject_checkout_is_a_root(tool: ModuleType, temporary: pathlib.Path) -> None:
    """A parent project in <case>/sibling/source adds Conjure from <case>/sibling/conjure, a checkout beside its own,
    with Conjure's build directory inside the parent's: cmake/ConjureDistributionPaths.cmake maps Conjure's source
    directory there, so the check must look for it as well."""
    conjure = temporary / "sibling" / "conjure"
    conjure.mkdir(parents=True)
    runtime = b"\x00" + under(conjure, "engine/src/core/log.cpp") + b"\x00"
    inner_build = temporary / "sibling" / "source" / "build" / "dist" / "conjure_inner_build"
    entries = project_entries(conjure, inner_build)
    _, build = fake_tree(temporary, "sibling", "Distribution", runtime, b"\x00", cache_entries=entries)
    result = run_tool(tool, ["--build-dir", str(build)])
    expect_exit(result, tool.EXIT_FINDINGS, ["conjure_runtime: 0x1 [unknown section]", "core"],
                "a sibling checkout of Conjure")
    roots = roots_line(result)
    check(str(conjure) in roots, f"Conjure's checkout is not a root: {roots}")
    check("conjure_inner_build" not in roots, f"a build directory inside the parent's is listed again: {roots}")


def roots_without_project_entries(tool: ModuleType, build: pathlib.Path) -> str:
    """The roots the tool lists for a fake tree once Conjure's project entries are taken out of its cache: the same
    paths spelled the same way (and resolved the same way, an 8.3 or linked temporary directory included), which
    is what the roots with the entries must equal when they add nothing."""
    cache = build / "CMakeCache.txt"
    lines = cache.read_text(encoding="utf-8").splitlines()
    kept = [line for line in lines if not line.startswith("Conjure_")]
    check(len(kept) < len(lines), f"{cache} holds no project entry to take out")
    cache.write_text("\n".join(kept) + "\n", encoding="utf-8")
    return roots_line(run_tool(tool, ["--build-dir", str(build)]))


def subproject_roots_are_added_only_outside(tool: ModuleType, temporary: pathlib.Path) -> None:
    clean = b"\x00main.cpp\x00"

    # Conjure's project() at the top of the tree (Conjure built on its own) and Conjure vendored inside the parent:
    # both directories lie inside the tree's roots, so the roots stay those of the tree.
    layouts: list[tuple[str, str, str]] = [
        ("top_level", "", ""),
        ("vendored", "third_party/conjure", "third_party/conjure"),
    ]
    for name, source_relative, binary_relative in layouts:
        source = temporary / name / "source"
        binary = source / "build" / "dist"
        entries = project_entries(source / source_relative if source_relative else source,
                                  binary / binary_relative if binary_relative else binary)
        _, build = fake_tree(temporary, name, "Distribution", clean, b"\x00", cache_entries=entries)
        roots = roots_line(run_tool(tool, ["--build-dir", str(build)]))
        expected = roots_without_project_entries(tool, build)
        check(roots == expected, f"{name}: roots {roots}, expected {expected}")

    # A build directory of Conjure's outside every other root is a root of its own.
    outside_build = temporary / "elsewhere" / "conjure_build"
    outside_build.mkdir(parents=True)
    runtime = b"\x00" + under(outside_build, "generated/version.cpp") + b"\x00"
    entries = project_entries(temporary / "outside" / "source", outside_build)
    _, build = fake_tree(temporary, "outside", "Distribution", runtime, b"\x00", cache_entries=entries)
    result = run_tool(tool, ["--build-dir", str(build)])
    expect_exit(result, tool.EXIT_FINDINGS, ["version.cpp"], "Conjure's build directory outside the tree")
    check(str(outside_build) in roots_line(result), result.describe())

    # Empty entries add nothing.
    entries = ("Conjure_SOURCE_DIR:STATIC=", "Conjure_BINARY_DIR:STATIC=")
    _, build = fake_tree(temporary, "empty_entries", "Distribution", clean, b"\x00", cache_entries=entries)
    result = run_tool(tool, ["--build-dir", str(build)])
    expect_exit(result, tool.EXIT_CLEAN, ["clean"], "empty project entries")
    roots = roots_line(result)
    expected = roots_without_project_entries(tool, build)
    check(roots == expected, f"empty entries: roots {roots}, expected {expected}")


def outer_roots_keep_what_lies_outside(tool: ModuleType, temporary: pathlib.Path) -> None:
    """outer_roots and lies_inside compare strings only, so stand-in paths of both families run on every platform."""
    del temporary
    cases: list[tuple[str, list[str], list[str], list[str]]] = [
        ("outside", ["C:/work/conjure"], ["C:/work/game"], ["C:/work/conjure"]),
        ("equal to a root", ["C:/work/game"], ["C:/work/game"], []),
        ("inside a root", ["C:/work/game/third_party/conjure"], ["C:/work/game"], []),
        ("inside, other separators and case", ["c:\\WORK\\Game\\conjure\\"], ["C:/work/game"], []),
        ("a sibling that starts like a root", ["C:/work/game_old"], ["C:/work/game"], ["C:/work/game_old"]),
        ("a POSIX checkout beside the parent", ["/home/ci/conjure", "/home/ci/game/build/conjure"],
         ["/home/ci/game", "/home/ci/game/build"], ["/home/ci/conjure"]),
        ("a candidate inside another candidate", ["/src/conjure/build", "/src/conjure"], ["/game"], ["/src/conjure"]),
        ("the same candidate twice", ["/src/conjure", "/src/conjure/"], ["/game"], ["/src/conjure"]),
        ("a root without a leading separator finds one with it", ["/work/game/conjure"], ["work/game"], []),
        ("a root with a leading separator does not find one without", ["work/game/conjure"], ["/work/game"],
         ["work/game/conjure"]),
        ("a root without components covers nothing", ["/src/conjure"], ["/"], ["/src/conjure"]),
        ("no candidates", [], ["/game"], []),
    ]
    for title, candidates, roots, expected in cases:
        kept = tool.outer_roots(candidates, roots)
        check(kept == expected, f"{title}: outer_roots({candidates}, {roots}) = {kept}, expected {expected}")
    entries = {"Conjure_SOURCE_DIR": "/a", "Conjure_BINARY_DIR": ""}
    check(tool.project_directories(entries) == ["/a"], f"project_directories({entries})")
    check(tool.project_directories({"Game_SOURCE_DIR": "/a"}) == [], "project_directories reads another project")


def build_dir_needs_distribution(tool: ModuleType, temporary: pathlib.Path) -> None:
    source = temporary / "debug_tree" / "source"
    dirty = b"\x00" + under(source, "engine/x.cpp") + b"\x00"
    _, build = fake_tree(temporary, "debug_tree", "Debug", dirty, b"\x00")
    expect_exit(run_tool(tool, ["--build-dir", str(build)]), tool.EXIT_SKIPPED, ["'Debug' build"], "Debug tree")
    expect_exit(run_tool(tool, ["--build-dir", str(build), "--any-config"]), tool.EXIT_FINDINGS, ["engine"],
                "--any-config")
    expect_exit(run_tool(tool, ["--build-dir", str(build), "--config", "Distribution"]), tool.EXIT_USAGE,
                ["disagrees with CMAKE_BUILD_TYPE=Debug"], "disagreeing --config")


def multi_config_reads_its_configuration(tool: ModuleType, temporary: pathlib.Path) -> None:
    source = temporary / "multi" / "source"
    dirty = b"\x00" + under(source, "engine/x.cpp") + b"\x00"
    _, build = fake_tree(temporary, "multi", "", dirty, b"\x00", "Debug;Development;Distribution", "Distribution")
    expect_exit(run_tool(tool, ["--build-dir", str(build)]), tool.EXIT_USAGE, ["needs --config"], "no --config")
    expect_exit(run_tool(tool, ["--build-dir", str(build), "--config", "Distribution"]), tool.EXIT_FINDINGS,
                ["conjure_runtime: 0x1"], "bin/Distribution")
    expect_exit(run_tool(tool, ["--build-dir", str(build), "--config", "Debug"]), tool.EXIT_SKIPPED,
                ["'Debug' build"], "--config Debug")


def build_dir_usage_errors(tool: ModuleType, temporary: pathlib.Path) -> None:
    missing = temporary / "usage" / "missing"
    expect_exit(run_tool(tool, ["--build-dir", str(missing)]), tool.EXIT_USAGE, ["is not a directory"],
                "missing directory")
    empty = temporary / "usage" / "empty"
    empty.mkdir(parents=True)
    expect_exit(run_tool(tool, ["--build-dir", str(empty)]), tool.EXIT_USAGE, ["has no CMakeCache.txt"], "no cache")
    nameless = temporary / "usage" / "nameless"
    nameless.mkdir()
    (nameless / "CMakeCache.txt").write_text("CMAKE_BUILD_TYPE:STRING=Distribution\n", encoding="utf-8")
    expect_exit(run_tool(tool, ["--build-dir", str(nameless)]), tool.EXIT_USAGE, ["names no CMAKE_HOME_DIRECTORY"],
                "cache without CMAKE_HOME_DIRECTORY")
    _, no_shared = fake_tree(temporary, "usage_no_shared", "Distribution", b"\x00", None)
    expect_exit(run_tool(tool, ["--build-dir", str(no_shared)]), tool.EXIT_USAGE, ["no conjure_shared library"],
                "missing shared library")
    expect_exit(run_tool(tool, ["--build-dir", str(no_shared), "--no-shared"]), tool.EXIT_CLEAN, ["clean"],
                "missing shared library with --no-shared")
    (no_shared / "bin" / "conjure_runtime").unlink()
    expect_exit(run_tool(tool, ["--build-dir", str(no_shared)]), tool.EXIT_USAGE, ["no conjure_runtime executable"],
                "missing runtime")


def argument_errors(tool: ModuleType, temporary: pathlib.Path) -> None:
    binary = write(temporary / "arguments" / "a.bin", b"\x00")
    expect_exit(run_tool(tool, []), tool.EXIT_USAGE, ["pass --build-dir <dir> or --binary <file>"], "no mode")
    expect_exit(run_tool(tool, ["--build-dir", str(temporary), "--binary", str(binary)]), tool.EXIT_USAGE,
                ["are exclusive"], "both modes")
    expect_exit(run_tool(tool, ["--binary", str(binary), "--max-findings", "0"]), tool.EXIT_USAGE,
                ["--max-findings must be positive"], "--max-findings 0")
    expect_exit(run_tool(tool, ["--binary", str(temporary / "arguments" / "missing.bin")]), tool.EXIT_USAGE,
                ["does not exist"], "missing binary")


def unreadable_binary_is_a_usage_error(tool: ModuleType, temporary: pathlib.Path) -> None:
    # A directory passes no read on any platform (IsADirectoryError on POSIX, PermissionError on Windows).
    directory = temporary / "unreadable.so"
    directory.mkdir()
    output = io.StringIO()
    errors = io.StringIO()
    with contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors):
        code = tool.check_binaries([directory], [str(temporary)], tool.DEFAULT_MAX_FINDINGS)
    result = run_result(code, output.getvalue(), errors.getvalue())
    expect_exit(result, tool.EXIT_USAGE, [f"cannot read {directory}"], "a directory given as the binary")


def max_findings_limits_output(tool: ModuleType, temporary: pathlib.Path) -> None:
    root = temporary / "limit"
    root.mkdir()
    data = b"".join(b"\x00" + under(root, f"file_{index}.cpp") for index in range(3)) + b"\x00"
    binary = write(temporary / "limit.bin", data)
    result = run_tool(tool, ["--binary", str(binary), "--root", str(root), "--max-findings", "2"])
    expect_exit(result, tool.EXIT_FINDINGS, ["file_0.cpp", "file_1.cpp", "... 1 more", "3 finding(s)"],
                "--max-findings 2")
    check("file_2.cpp" not in result.output, result.describe())


def roots_are_expanded_and_deduplicated(tool: ModuleType, temporary: pathlib.Path) -> None:
    target = temporary / "expand" / "real"
    target.mkdir(parents=True)
    spelled_twice = [str(target), str(target).upper() if os.name == "nt" else str(target), str(target) + os.sep]
    check(len(tool.expand_roots(spelled_twice)) == len(tool.expand_roots([str(target)])),
          str(tool.expand_roots(spelled_twice)))
    link = temporary / "expand" / "link"
    try:
        os.symlink(target, link, target_is_directory=True)
    except (OSError, NotImplementedError) as error:
        note(f"roots reached through a link: not exercised, no directory link could be made here ({error})")
        return
    expanded = tool.expand_roots([str(link)])
    check(str(link) in expanded and str(target.resolve()) in expanded, str(expanded))
    binary = write(temporary / "expand" / "linked.bin", b"\x00" + under(target.resolve(), "x.cpp") + b"\x00")
    expect_exit(run_tool(tool, ["--binary", str(binary), "--root", str(link)]), tool.EXIT_FINDINGS, ["x.cpp"],
                "a binary naming the link's target")


def distribution_path_options_are_computed(tool: ModuleType, temporary: pathlib.Path) -> None:
    """cmake/ConjureDistributionPathsProbe.cmake in script mode: the options of every compiler family and root layout
    the function of cmake/ConjureDistributionPaths.cmake tells apart, on stand-in roots."""
    cmake = os.environ.get("CONJURE_CMAKE_COMMAND") or shutil.which("cmake")
    if cmake is None:
        note("the Distribution path options (cmake/ConjureDistributionPathsProbe.cmake): not checked, no cmake on PATH "
             "and no CONJURE_CMAKE_COMMAND")
        return
    completed = subprocess.run([cmake, "-P", str(OPTIONS_PROBE)], cwd=temporary, capture_output=True,
                               encoding="utf-8", errors="replace", timeout=120, check=False)
    result = run_result(completed.returncode, completed.stdout, completed.stderr)
    expect_exit(result, 0, ["distribution paths probe: passed"], "the options probe")
    # CMake wraps a warning at blanks, so the text is compared with its white space folded.
    warning = " ".join(completed.stderr.split())
    check("'/src/a=b' contains '=', which a -fmacro-prefix-map cannot express" in warning,
          f"the warning about a root with '=' is missing\n{result.describe()}")


def main() -> int:
    tool = load_tool()
    failures: list[str] = []
    cases: list[tuple[str, Callable[[ModuleType, pathlib.Path], None]]] = [
        ("check_distribution_paths - a root is found whatever its separators and the case of its letters",
         spellings_are_found),
        ("check_distribution_paths - a sibling directory that only starts like the root is not a finding",
         siblings_are_not_found),
        ("check_distribution_paths - the root itself and a root before a non-name character are findings",
         root_ends_are_found),
        ("check_distribution_paths - a POSIX root needs its leading separator", posix_root_needs_its_leading_separator),
        ("check_distribution_paths - a root in a UTF-16LE wide string is a finding", wide_strings_are_found),
        ("check_distribution_paths - a finding names the PE section that holds it", pe_sections_are_named),
        ("check_distribution_paths - a finding names the ELF section, 32 and 64 bit, both byte orders",
         elf_sections_are_named),
        ("check_distribution_paths - a compressed ELF section is a finding", compressed_sections_are_findings),
        ("check_distribution_paths - broken headers give no sections and the scan still runs",
         broken_headers_give_no_sections),
        ("check_distribution_paths - a managed PE's CodeView PDB path is read and named as such, PE32 and PE32+",
         codeview_paths_are_read),
        ("check_distribution_paths - broken PE debug directories give no records and the scan still runs",
         broken_debug_directories_give_no_records),
        ("regression - code sweep FU-PDB the document paths a portable PDB stores in pieces are findings",
         portable_pdb_documents_are_findings),
        ("check_distribution_paths - a portable PDB whose Document table cannot be read is a finding",
         unreadable_portable_pdbs_are_findings),
        ("check_distribution_paths - an embedded portable PDB is decompressed and scanned, or is a finding",
         embedded_portable_pdbs_are_scanned),
        ("check_distribution_paths - a repeated string is reported once and a shared place counted once",
         repeated_strings_are_reported_once),
        ("check_distribution_paths - --binary looks for the repository that holds the tool",
         binary_mode_uses_the_repository),
        ("check_distribution_paths - --root adds a directory and must exist", extra_roots_are_checked),
        ("check_distribution_paths - --build-dir checks the runtime and the shared library against both roots",
         build_dir_checks_runtime_and_shared),
        ("regression - code sweep FU-ROOTS a checkout of Conjure beside its parent project is a root",
         subproject_checkout_is_a_root),
        ("check_distribution_paths - Conjure's own directories add a root only where they lie outside the tree's",
         subproject_roots_are_added_only_outside),
        ("check_distribution_paths - outer_roots keeps the candidates outside the roots and each other",
         outer_roots_keep_what_lies_outside),
        ("check_distribution_paths - --build-dir skips a tree that is not Distribution unless --any-config",
         build_dir_needs_distribution),
        ("check_distribution_paths - a multi-config tree is checked in bin/<config>",
         multi_config_reads_its_configuration),
        ("check_distribution_paths - --build-dir reports what is missing as a usage error", build_dir_usage_errors),
        ("check_distribution_paths - argument errors are usage errors", argument_errors),
        ("check_distribution_paths - a binary that cannot be read is a usage error",
         unreadable_binary_is_a_usage_error),
        ("check_distribution_paths - --max-findings limits the printed strings", max_findings_limits_output),
        ("check_distribution_paths - roots are made absolute, resolved and deduplicated",
         roots_are_expanded_and_deduplicated),
        ("Distribution path options - each compiler family, root layout and special character gives its options",
         distribution_path_options_are_computed),
    ]
    with tempfile.TemporaryDirectory(prefix="distribution_paths_") as directory:
        temporary = pathlib.Path(directory)
        for index, (title, case) in enumerate(cases):
            case_directory = temporary / f"case_{index}"
            case_directory.mkdir()
            try:
                case(tool, case_directory)
            except Exception as failure:
                failures.append(f"{title}: {type(failure).__name__}: {failure}")
                continue
            print(f"ok: {title}")

    for text in notes:
        print(f"MESSAGE: {text}")
    if failures:
        print("\n".join(f"FAIL: {failure}" for failure in failures))
        return 1
    print(f"check_distribution_paths self-test: {len(cases)} case(s) passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
