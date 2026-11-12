#!/usr/bin/env python3
"""Generates the editor's application icon from the logo's per-size drawings (docs/handbook/brand.md).

The official Conjure logo is drawn once per icon size: docs/assets/brand/icon/conjure-icon-<N>.svg is the
logo fitted to an N x N pixel grid (below 48 px each size is its own cut of the mark; the handbook explains
how). This tool fills those drawings in ICON_INK and writes:

  editor/resources/conjure_editor.ico          the Windows application icon, compiled into conjure_editor as
                                                the GLFW_ICON resource: 32-bit BMP entries up to 64 px and a
                                                PNG entry at 256 px (ICO_SIZES)
  engine/src/editor/ui/editor_icon_data.cpp    the PNG files editor_ui hands to platform::window::set_icon
                                                (WINDOW_SIZES)

The drawings use a restricted form, checked on read: one <svg> whose viewBox is square and whose width and
height equal the size in the file name, holding exactly one <path> made of absolute M, C and Z commands, every
subpath closed. The path is filled under the nonzero rule, so the eye (wound the other way) is a hole and
overlapping pieces of the same winding merge.

Coverage is exact per sub-scanline: each pixel row is sampled by SUB_ROWS horizontal lines, every curve is
flattened into CURVE_STEPS segments, and the horizontal overlap of each filled span with each pixel is summed.
The result depends only on the drawings and ordinary float arithmetic, so it is the same on every machine.
--check compares PNG payloads after decoding them, so a different zlib build cannot make it fail; the CTest
entry brand.icons_up_to_date runs it.

Usage:
    python tools/generate_brand_icons.py            # regenerate both outputs
    python tools/generate_brand_icons.py --check    # exit 1 when an output differs from what would be generated
"""

from __future__ import annotations

import argparse
import math
import pathlib
import re
import struct
import sys
import xml.etree.ElementTree as ElementTree
import zlib

ROOT = pathlib.Path(__file__).resolve().parent.parent

DRAWINGS = pathlib.PurePosixPath("docs/assets/brand/icon")
ICO_OUTPUT = pathlib.PurePosixPath("editor/resources/conjure_editor.ico")
CPP_OUTPUT = pathlib.PurePosixPath("engine/src/editor/ui/editor_icon_data.cpp")

# Windows draws 16 and 32 px icons at 100 to 200 percent display scaling (20, 24, 40, 48, 64) and Explorer's
# large views at 256. X11 window managers pick from whatever the window offers.
ICO_SIZES = (16, 20, 24, 32, 40, 48, 64, 256)
WINDOW_SIZES = (16, 24, 32, 48, 64, 128, 256)
ICON_SIZES = tuple(sorted(set(ICO_SIZES) | set(WINDOW_SIZES)))
PNG_ENTRY_SIZE = 256                    # ICO entries this large are stored as PNG, smaller ones as BMP

# The Midnight theme accent: one ink that keeps 3:1 contrast against both light and dark taskbars.
ICON_INK = (0x4C, 0x8D, 0xFF)

SUB_ROWS = 16
CURVE_STEPS = 48
BYTES_PER_LINE = 16

SVG_NAMESPACE = "{http://www.w3.org/2000/svg}"
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
NUMBER_PATTERN = r"[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?"
PATH_TOKEN_PATTERN = re.compile(rf"{NUMBER_PATTERN}|[A-Za-z]|[^\s,]")
CPP_ARRAY_PATTERN = re.compile(r"constexpr std::array<u8, (\d+)> PNG_(\d+) = \{([^}]*)\};")

Point = tuple[float, float]


# ---------------------------------------------------------------------------------------------------------------
# Drawings


def is_number(token: str) -> bool:
    return re.fullmatch(NUMBER_PATTERN, token) is not None


def parse_path_data(data: str, name: str) -> list[list[Point]]:
    """Flattens absolute M, C and Z path data into closed polygons (one per subpath)."""
    tokens = PATH_TOKEN_PATTERN.findall(data)
    polygons: list[list[Point]] = []
    current: list[Point] | None = None
    position: Point = (0.0, 0.0)
    command: str | None = None
    index = 0

    def take(count: int) -> list[float]:
        nonlocal index
        values = tokens[index:index + count]
        if len(values) != count or not all(is_number(value) for value in values):
            raise ValueError(f"{name}: command {command} needs {count} numbers")
        index += count
        return [float(value) for value in values]

    while index < len(tokens):
        token = tokens[index]
        if not is_number(token):
            if token not in ("M", "C", "Z"):
                raise ValueError(f"{name}: unsupported path command {token!r}; only absolute M, C and Z are read")
            command = token
            index += 1
            if command == "Z":
                if current is None:
                    raise ValueError(f"{name}: Z without an open subpath")
                current = None
                command = None
            continue
        if command == "M":
            if current is not None:
                raise ValueError(f"{name}: every subpath must end with Z before the next M")
            x, y = take(2)
            position = (x, y)
            current = [position]
            polygons.append(current)
            command = None                          # implicit line-to after M is not part of the form
        elif command == "C":
            if current is None:
                raise ValueError(f"{name}: C before M")
            x1, y1, x2, y2, x3, y3 = take(6)
            x0, y0 = position
            for step in range(1, CURVE_STEPS + 1):
                t = step / CURVE_STEPS
                u = 1.0 - t
                current.append((
                    u * u * u * x0 + 3.0 * u * u * t * x1 + 3.0 * u * t * t * x2 + t * t * t * x3,
                    u * u * u * y0 + 3.0 * u * u * t * y1 + 3.0 * u * t * t * y2 + t * t * t * y3,
                ))
            position = (x3, y3)
        else:
            raise ValueError(f"{name}: number {token!r} outside a command")
    if current is not None:
        raise ValueError(f"{name}: the last subpath is not closed with Z")
    if not polygons:
        raise ValueError(f"{name}: the path is empty")
    return polygons


def parse_drawing(text: str, size: int, name: str) -> tuple[tuple[float, float, float], list[list[Point]]]:
    """Reads one drawing in the restricted form: returns (viewBox x, y, side) and the flattened polygons."""
    try:
        root = ElementTree.fromstring(text)
    except ElementTree.ParseError as error:
        raise ValueError(f"{name}: not well-formed XML: {error}") from error
    if root.tag != SVG_NAMESPACE + "svg":
        raise ValueError(f"{name}: the root element is not an SVG <svg>")
    for attribute in ("width", "height"):
        if root.get(attribute) != str(size):
            raise ValueError(f"{name}: {attribute} must be {size}, the size the drawing is for")
    fields = root.get("viewBox", "").replace(",", " ").split()
    if len(fields) != 4 or not all(is_number(field) for field in fields):
        raise ValueError(f"{name}: viewBox must be four numbers")
    x, y, width, height = (float(field) for field in fields)
    if width <= 0.0 or width != height:
        raise ValueError(f"{name}: viewBox must be square with a positive side")
    children = [element for element in root.iter() if element is not root]
    if len(children) != 1 or children[0].tag != SVG_NAMESPACE + "path":
        raise ValueError(f"{name}: the <svg> must hold exactly one <path> and nothing else")
    return (x, y, width), parse_path_data(children[0].get("d", ""), name)


# ---------------------------------------------------------------------------------------------------------------
# Rasterising


def rasterise(polygons: list[list[Point]], view_box: tuple[float, float, float], size: int) -> list[float]:
    """Coverage in [0, 1+] per pixel, row-major, under the nonzero rule (overlaps may exceed 1; clamp later)."""
    origin_x, origin_y, side = view_box
    scale = size / side
    edges: list[tuple[float, float, float, float, int]] = []   # top, bottom, x at top, dx/dy, winding
    for polygon in polygons:
        mapped = [((x - origin_x) * scale, (y - origin_y) * scale) for x, y in polygon]
        for (x0, y0), (x1, y1) in zip(mapped, mapped[1:] + mapped[:1]):
            if y0 == y1:
                continue
            if y0 < y1:
                edges.append((y0, y1, x0, (x1 - x0) / (y1 - y0), 1))
            else:
                edges.append((y1, y0, x1, (x0 - x1) / (y0 - y1), -1))
    edges.sort(key=lambda edge: edge[0])

    coverage = [0.0] * (size * size)
    weight = 1.0 / SUB_ROWS
    active: list[tuple[float, float, float, float, int]] = []
    next_edge = 0
    for row in range(size):
        base = row * size
        for sub_row in range(SUB_ROWS):
            y = row + (sub_row + 0.5) / SUB_ROWS
            while next_edge < len(edges) and edges[next_edge][0] <= y:
                active.append(edges[next_edge])
                next_edge += 1
            active = [edge for edge in active if edge[1] > y]
            crossings = sorted((top_x + (y - top) * slope, winding) for top, _, top_x, slope, winding in active)
            winding = 0
            for (start, step), (end, _) in zip(crossings, crossings[1:]):
                winding += step
                if winding == 0:
                    continue
                left = max(0.0, start)
                right = min(float(size), end)
                if right <= left:
                    continue
                for column in range(int(left), math.ceil(right)):
                    overlap = min(right, column + 1.0) - max(left, float(column))
                    if overlap > 0.0:
                        coverage[base + column] += overlap * weight
    return coverage


def to_rgba(coverage: list[float], ink: tuple[int, int, int]) -> bytes:
    """Straight-alpha RGBA: every pixel is the ink, its alpha the clamped coverage."""
    pixels = bytearray()
    for value in coverage:
        pixels += bytes((ink[0], ink[1], ink[2], round(min(value, 1.0) * 255.0)))
    return bytes(pixels)


def render_icons(root: pathlib.Path) -> dict[int, bytes]:
    images: dict[int, bytes] = {}
    for size in ICON_SIZES:
        path = root / DRAWINGS / f"conjure-icon-{size}.svg"
        try:
            text = path.read_text(encoding="utf-8")
        except OSError as error:
            raise ValueError(f"{path}: cannot read the drawing: {error}") from error
        view_box, polygons = parse_drawing(text, size, path.name)
        images[size] = to_rgba(rasterise(polygons, view_box, size), ICON_INK)
    return images


# ---------------------------------------------------------------------------------------------------------------
# PNG


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload))


def encode_png(size: int, rgba: bytes) -> bytes:
    stride = size * 4
    raw = b"".join(b"\x00" + rgba[row * stride:(row + 1) * stride] for row in range(size))
    header = struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)
    return (PNG_SIGNATURE + png_chunk(b"IHDR", header) + png_chunk(b"IDAT", zlib.compress(raw, 9))
            + png_chunk(b"IEND", b""))


def paeth(left: int, up: int, up_left: int) -> int:
    estimate = left + up - up_left
    distance_left = abs(estimate - left)
    distance_up = abs(estimate - up)
    distance_up_left = abs(estimate - up_left)
    if distance_left <= distance_up and distance_left <= distance_up_left:
        return left
    if distance_up <= distance_up_left:
        return up
    return up_left


def decode_png(data: bytes) -> tuple[int, int, bytes]:
    """Decodes an 8-bit RGBA, non-interlaced PNG (any filter) to (width, height, rgba); ValueError otherwise."""
    if not data.startswith(PNG_SIGNATURE):
        raise ValueError("not a PNG")
    offset = len(PNG_SIGNATURE)
    header: tuple[int, ...] | None = None
    compressed = bytearray()
    while offset + 12 <= len(data):
        length, kind = struct.unpack(">I4s", data[offset:offset + 8])
        payload = data[offset + 8:offset + 8 + length]
        (crc,) = struct.unpack(">I", data[offset + 8 + length:offset + 12 + length])
        if len(payload) != length or crc != zlib.crc32(kind + payload):
            raise ValueError(f"PNG chunk {kind!r} is truncated or fails its CRC")
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", payload)
        elif kind == b"IDAT":
            compressed += payload
        elif kind == b"IEND":
            break
        offset += 12 + length
    if header is None:
        raise ValueError("PNG without IHDR")
    width, height, depth, colour, _, _, interlace = header
    if depth != 8 or colour != 6 or interlace != 0:
        raise ValueError("only 8-bit RGBA, non-interlaced PNGs are read")
    raw = zlib.decompress(bytes(compressed))
    stride = width * 4
    if len(raw) != height * (stride + 1):
        raise ValueError("PNG image data has the wrong length")
    pixels = bytearray()
    previous = bytearray(stride)
    for row in range(height):
        start = row * (stride + 1)
        kind = raw[start]
        line = bytearray(raw[start + 1:start + 1 + stride])
        for index in range(stride):
            left = line[index - 4] if index >= 4 else 0
            up = previous[index]
            up_left = previous[index - 4] if index >= 4 else 0
            if kind == 1:
                line[index] = (line[index] + left) & 0xFF
            elif kind == 2:
                line[index] = (line[index] + up) & 0xFF
            elif kind == 3:
                line[index] = (line[index] + ((left + up) >> 1)) & 0xFF
            elif kind == 4:
                line[index] = (line[index] + paeth(left, up, up_left)) & 0xFF
            elif kind != 0:
                raise ValueError(f"PNG row {row} has unknown filter {kind}")
        pixels += line
        previous = line
    return width, height, bytes(pixels)


# ---------------------------------------------------------------------------------------------------------------
# ICO


def encode_bmp_entry(size: int, rgba: bytes) -> bytes:
    """A 32-bit BGRA DIB, rows bottom-up, followed by the 1-bit AND mask (set where the pixel is transparent)."""
    mask_stride = ((size + 31) // 32) * 4
    header = struct.pack(
        "<IiiHHIIiiII", 40, size, size * 2, 1, 32, 0, size * size * 4 + mask_stride * size, 0, 0, 0, 0)
    pixels = bytearray()
    mask = bytearray()
    for row in range(size - 1, -1, -1):
        bits = bytearray(mask_stride)
        for column in range(size):
            offset = (row * size + column) * 4
            red, green, blue, alpha = rgba[offset:offset + 4]
            pixels += bytes((blue, green, red, alpha))
            if alpha == 0:
                bits[column // 8] |= 0x80 >> (column % 8)
        mask += bits
    return header + bytes(pixels) + bytes(mask)


def encode_ico(images: dict[int, bytes]) -> bytes:
    payloads = [
        encode_png(size, images[size]) if size >= PNG_ENTRY_SIZE else encode_bmp_entry(size, images[size])
        for size in ICO_SIZES
    ]
    directory = struct.pack("<HHH", 0, 1, len(ICO_SIZES))
    offset = len(directory) + 16 * len(ICO_SIZES)
    entries = bytearray()
    for size, payload in zip(ICO_SIZES, payloads):
        dimension = 0 if size >= 256 else size
        entries += struct.pack("<BBBBHHII", dimension, dimension, 0, 0, 1, 32, len(payload), offset)
        offset += len(payload)
    return directory + bytes(entries) + b"".join(payloads)


def decode_ico(data: bytes) -> list[tuple[tuple[int, ...], bytes]]:
    """The entries of an ICO: (width, height, colours, reserved, planes, bit count) and the payload."""
    if len(data) < 6:
        raise ValueError("ICO shorter than its header")
    reserved, kind, count = struct.unpack("<HHH", data[:6])
    if reserved != 0 or kind != 1:
        raise ValueError("not an ICO file")
    entries = []
    for index in range(count):
        start = 6 + 16 * index
        if start + 16 > len(data):
            raise ValueError("ICO directory is truncated")
        width, height, colours, entry_reserved, planes, bits, length, offset = struct.unpack(
            "<BBBBHHII", data[start:start + 16])
        if offset + length > len(data):
            raise ValueError(f"ICO entry {index} points past the end of the file")
        entries.append(((width, height, colours, entry_reserved, planes, bits), data[offset:offset + length]))
    return entries


# ---------------------------------------------------------------------------------------------------------------
# C++


def render_cpp(pngs: list[tuple[int, bytes]]) -> str:
    lines = [
        "// Generated by tools/generate_brand_icons.py from docs/assets/brand/icon; do not edit. After changing a",
        "// drawing, run `python tools/generate_brand_icons.py` (docs/handbook/brand.md).",
        "",
        '#include "editor/ui/editor_icon_data.h"',
        "",
        "#include <array>",
        "",
        "namespace conjure::editor::ui::icon_data {",
        "",
        "    namespace {",
    ]
    for size, data in pngs:
        lines.append(f"        constexpr std::array<u8, {len(data)}> PNG_{size} = {{")
        for start in range(0, len(data), BYTES_PER_LINE):
            values = data[start:start + BYTES_PER_LINE]
            lines.append("            " + ", ".join(f"0x{value:02X}" for value in values) + ",")
        lines.append("        };")
        lines.append("")
    lines.append(f"        constexpr std::array<icon_png, {len(pngs)}> FILES = {{")
    for size, _ in pngs:
        lines.append(f"            icon_png{{{size}, PNG_{size}}},")
    lines += [
        "        };",
        "    }",
        "",
        "    std::span<const icon_png> files() noexcept {",
        "        return FILES;",
        "    }",
        "}",
        "",
    ]
    return "\n".join(lines)


def parse_cpp(text: str) -> list[tuple[int, bytes]]:
    pngs = []
    for match in CPP_ARRAY_PATTERN.finditer(text):
        values = [int(token, 16) for token in re.findall(r"0x([0-9A-Fa-f]{2})", match.group(3))]
        if len(values) != int(match.group(1)):
            raise ValueError(f"PNG_{match.group(2)} declares {match.group(1)} bytes but holds {len(values)}")
        pngs.append((int(match.group(2)), bytes(values)))
    return pngs


# ---------------------------------------------------------------------------------------------------------------
# Generate and check


def generate(root: pathlib.Path) -> None:
    images = render_icons(root)
    ico = root / ICO_OUTPUT
    ico.parent.mkdir(parents=True, exist_ok=True)
    ico.write_bytes(encode_ico(images))
    cpp = root / CPP_OUTPUT
    cpp.parent.mkdir(parents=True, exist_ok=True)
    cpp.write_bytes(render_cpp([(size, encode_png(size, images[size])) for size in WINDOW_SIZES]).encode("utf-8"))


def png_matches(data: bytes, size: int, rgba: bytes) -> bool:
    try:
        return decode_png(data) == (size, size, rgba)
    except (ValueError, zlib.error):
        return False


def check(root: pathlib.Path) -> list[str]:
    """Every way the outputs differ from what generate() would write; empty when they are up to date."""
    images = render_icons(root)
    problems: list[str] = []

    ico_path = root / ICO_OUTPUT
    if not ico_path.is_file():
        problems.append(f"{ICO_OUTPUT}: missing")
    else:
        try:
            actual = decode_ico(ico_path.read_bytes())
        except ValueError as error:
            actual = []
            problems.append(f"{ICO_OUTPUT}: {error}")
        expected = decode_ico(encode_ico(images))
        if actual and len(actual) != len(expected):
            problems.append(f"{ICO_OUTPUT}: {len(actual)} entries, expected {len(expected)}")
        elif actual:
            for size, (want_header, want), (have_header, have) in zip(ICO_SIZES, expected, actual):
                if have_header != want_header:
                    problems.append(f"{ICO_OUTPUT}: the {size} px entry's directory fields differ")
                elif size >= PNG_ENTRY_SIZE and not png_matches(have, size, images[size]):
                    problems.append(f"{ICO_OUTPUT}: the {size} px PNG entry's pixels differ")
                elif size < PNG_ENTRY_SIZE and have != want:
                    problems.append(f"{ICO_OUTPUT}: the {size} px BMP entry differs")

    cpp_path = root / CPP_OUTPUT
    if not cpp_path.is_file():
        problems.append(f"{CPP_OUTPUT}: missing")
        return problems
    text = cpp_path.read_bytes().decode("utf-8", errors="replace")
    try:
        pngs = parse_cpp(text)
    except ValueError as error:
        problems.append(f"{CPP_OUTPUT}: {error}")
        return problems
    if render_cpp(pngs) != text:
        problems.append(f"{CPP_OUTPUT}: not in the generated form (edited by hand?)")
    if [size for size, _ in pngs] != list(WINDOW_SIZES):
        problems.append(f"{CPP_OUTPUT}: sizes {[size for size, _ in pngs]}, expected {list(WINDOW_SIZES)}")
    else:
        for size, data in pngs:
            if not png_matches(data, size, images[size]):
                problems.append(f"{CPP_OUTPUT}: the {size} px PNG's pixels differ")
    return problems


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Generate the editor's application icon from the logo drawings.")
    parser.add_argument("--check", action="store_true", help="exit 1 when an output is not up to date")
    parser.add_argument("--root", type=pathlib.Path, default=ROOT, help="repository root (default: this checkout)")
    arguments = parser.parse_args(argv)
    try:
        if arguments.check:
            problems = check(arguments.root)
            for problem in problems:
                print(f"{problem}; run python tools/generate_brand_icons.py", file=sys.stderr)
            return 1 if problems else 0
        generate(arguments.root)
    except ValueError as error:
        print(f"generate_brand_icons: {error}", file=sys.stderr)
        return 2
    print(f"wrote {ICO_OUTPUT} and {CPP_OUTPUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
