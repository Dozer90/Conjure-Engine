#!/usr/bin/env python3
"""Self-test for tools/generate_brand_icons.py (docs/handbook/brand.md).

Exercises the rasteriser on shapes with known coverage, the drawing parser's restricted form, the PNG and ICO
codecs, and generate/check end to end in a temporary repository. Runs as the CTest entry `brand.icons_tool`; no
third-party test framework is needed.
"""

from __future__ import annotations

import contextlib
import importlib.util
import io
import pathlib
import struct
import sys
import tempfile
import zlib

HERE = pathlib.Path(__file__).resolve().parent
TOOL = HERE.parent / "generate_brand_icons.py"

failures: list[str] = []


def load_tool():
    specification = importlib.util.spec_from_file_location("generate_brand_icons", TOOL)
    module = importlib.util.module_from_spec(specification)
    assert specification.loader is not None
    specification.loader.exec_module(module)
    return module


def quiet_main(tool, arguments: list[str]) -> int:
    """Runs the tool's command line with its messages captured; the failure cases print on purpose."""
    with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
        return tool.main(arguments)


def expect(condition: bool, message: str) -> None:
    if not condition:
        failures.append(message)


def expect_rejected(tool, text: str, size: int, fragment: str) -> None:
    try:
        tool.parse_drawing(text, size, "case.svg")
    except ValueError as error:
        expect(fragment in str(error), f"rejection should mention {fragment!r}, got {error}")
        return
    failures.append(f"a drawing needing {fragment!r} was accepted")


def line(x0: float, y0: float, x1: float, y1: float) -> str:
    """A straight segment as a cubic, the only curve command the form has."""
    return f"C{x0 + (x1 - x0) / 3} {y0 + (y1 - y0) / 3} {x0 + 2 * (x1 - x0) / 3} {y0 + 2 * (y1 - y0) / 3} {x1} {y1}"


def square(x0: float, y0: float, x1: float, y1: float, clockwise: bool = True) -> str:
    corners = [(x0, y0), (x1, y0), (x1, y1), (x0, y1)] if clockwise else [(x0, y0), (x0, y1), (x1, y1), (x1, y0)]
    data = f"M{corners[0][0]} {corners[0][1]}"
    for (ax, ay), (bx, by) in zip(corners, corners[1:] + corners[:1]):
        data += line(ax, ay, bx, by)
    return data + "Z"


def drawing(size: int, data: str, view_box: str | None = None) -> str:
    box = view_box if view_box is not None else f"0 0 {size} {size}"
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{box}" width="{size}" height="{size}">'
            f'<path fill="#000000" d="{data}"/></svg>')


def test_rasteriser(tool) -> None:
    view_box, polygons = tool.parse_drawing(drawing(4, square(1, 1, 3, 3)), 4, "square.svg")
    coverage = tool.rasterise(polygons, view_box, 4)
    for y in range(4):
        for x in range(4):
            inside = 1 <= x < 3 and 1 <= y < 3
            expect(abs(coverage[y * 4 + x] - (1.0 if inside else 0.0)) < 1e-9, f"square coverage at {x},{y}")

    view_box, polygons = tool.parse_drawing(drawing(2, square(0.5, 0, 1.5, 2)), 2, "half.svg")
    coverage = tool.rasterise(polygons, view_box, 2)
    expect(all(abs(value - 0.5) < 1e-9 for value in coverage), f"half-pixel edges give 0.5, got {coverage}")

    view_box, polygons = tool.parse_drawing(drawing(8, square(0, 0, 16, 16), "0 0 16 16"), 8, "scaled.svg")
    expect(all(abs(value - 1.0) < 1e-9 for value in tool.rasterise(polygons, view_box, 8)), "viewBox scaling")

    hole = drawing(6, square(0, 0, 6, 6) + square(2, 2, 4, 4, clockwise=False))
    view_box, polygons = tool.parse_drawing(hole, 6, "hole.svg")
    coverage = tool.rasterise(polygons, view_box, 6)
    expect(abs(coverage[2 * 6 + 2]) < 1e-9 and abs(coverage[0] - 1.0) < 1e-9,
           "an opposite winding cuts a hole (nonzero rule)")

    overlap = drawing(4, square(0, 0, 3, 4) + square(1, 0, 4, 4))
    view_box, polygons = tool.parse_drawing(overlap, 4, "overlap.svg")
    rgba = tool.to_rgba(tool.rasterise(polygons, view_box, 4), tool.ICON_INK)
    expect(rgba[1 * 4 + 3] == 255, "same-winding overlaps merge and clamp to opaque")
    expect(rgba[0:3] == bytes(tool.ICON_INK), "every pixel carries the ink, alpha carries the coverage")


def test_parser(tool) -> None:
    good = square(0, 0, 4, 4)
    expect_rejected(tool, drawing(4, good.replace("C", "c", 1)), 4, "only absolute M, C and Z")
    expect_rejected(tool, drawing(4, "M0 0L4 0L4 4Z"), 4, "only absolute M, C and Z")
    expect_rejected(tool, drawing(4, good[:-1]), 4, "not closed")
    expect_rejected(tool, drawing(4, good, "0 0 4 5"), 4, "square")
    expect_rejected(tool, drawing(4, good), 8, "width must be 8")
    expect_rejected(tool, drawing(4, good).replace("</svg>", "<path d=\"M0 0Z\"/></svg>"), 4, "exactly one <path>")
    expect_rejected(tool, "<svg", 4, "not well-formed")
    expect_rejected(tool, drawing(4, "C1 1 2 2 3 3Z"), 4, "C before M")


def test_png(tool) -> None:
    rgba = bytes(range(256)) * 4                        # 16 x 16 RGBA
    width, height, decoded = tool.decode_png(tool.encode_png(16, rgba))
    expect((width, height, decoded) == (16, 16, rgba), "PNG round trip")

    # A hand-filtered image: row 0 with Sub (1), row 1 with Up (2); 1 x 2 pixels.
    raw = bytes([1, 10, 20, 30, 40, 2, 1, 1, 1, 1])
    header = struct.pack(">IIBBBBB", 1, 2, 8, 6, 0, 0, 0)
    data =(tool.PNG_SIGNATURE + tool.png_chunk(b"IHDR", header) + tool.png_chunk(b"IDAT", zlib.compress(raw))
            + tool.png_chunk(b"IEND", b""))
    expect(tool.decode_png(data) == (1, 2, bytes([10, 20, 30, 40, 11, 21, 31, 41])), "PNG Sub and Up filters")

    broken = bytearray(tool.encode_png(4, bytes(64)))
    broken[-5] ^= 0xFF                                  # inside the IEND CRC
    try:
        tool.decode_png(bytes(broken))
        failures.append("a PNG with a bad CRC decoded")
    except ValueError:
        pass


def test_ico(tool) -> None:
    images = {size: b"".join(bytes((76, 141, 255, 0 if index % 3 else 255)) for index in range(size * size))
              for size in tool.ICON_SIZES}
    entries = tool.decode_ico(tool.encode_ico(images))
    expect(len(entries) == len(tool.ICO_SIZES), "one ICO entry per size")
    for size, (header, payload) in zip(tool.ICO_SIZES, entries):
        dimension = 0 if size >= 256 else size
        expect(header == (dimension, dimension, 0, 0, 1, 32), f"{size} px directory entry {header}")
        if size >= tool.PNG_ENTRY_SIZE:
            expect(tool.decode_png(payload) == (size, size, images[size]), f"{size} px PNG entry")
            continue
        info = struct.unpack("<IiiHHIIiiII", payload[:40])
        expect(info[:5] == (40, size, size * 2, 1, 32), f"{size} px BITMAPINFOHEADER {info[:5]}")
        bottom_left = payload[40:44]                    # first stored pixel: bottom row, first column, BGRA
        index = (size - 1) * size
        expect(bottom_left == bytes((255, 141, 76, 0 if index % 3 else 255)), f"{size} px rows are bottom-up BGRA")
        mask = payload[40 + size * size * 4:]
        stride = ((size + 31) // 32) * 4
        expect(len(mask) == stride * size, f"{size} px AND mask size")
        expect(bool(mask[0] & 0x80) == (index % 3 != 0), f"{size} px AND mask marks transparent pixels")


def test_generate_and_check(tool) -> None:
    with tempfile.TemporaryDirectory() as temporary:
        root = pathlib.Path(temporary)
        for size in tool.ICON_SIZES:
            path = root / tool.DRAWINGS / f"conjure-icon-{size}.svg"
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(drawing(size, square(1, 1, size - 1, size - 2)), encoding="utf-8")

        expect(quiet_main(tool, ["--check", "--root", str(root)]) == 1, "check fails before anything is generated")
        expect(quiet_main(tool, ["--root", str(root)]) == 0, "generate succeeds")
        expect(tool.check(root) == [], f"freshly generated outputs are up to date: {tool.check(root)}")
        cpp = (root / tool.CPP_OUTPUT).read_bytes()
        expect(b"\r\n" not in cpp, "the generated C++ uses LF line endings")
        expect(max(len(text) for text in cpp.decode().splitlines()) <= 120, "the generated C++ fits 120 columns")

        ico_path = root / tool.ICO_OUTPUT
        ico = bytearray(ico_path.read_bytes())
        ico[6 + 16 * len(tool.ICO_SIZES) + 40] ^= 0x01  # first pixel byte of the 16 px BMP entry
        ico_path.write_bytes(bytes(ico))
        expect(any("16 px BMP entry" in problem for problem in tool.check(root)), "a changed ICO pixel is found")

        tool.generate(root)
        cpp_path = root / tool.CPP_OUTPUT
        cpp_path.write_text(cpp_path.read_text(encoding="utf-8").replace("    }\n}", "    }\n}\n// edit"),
                            encoding="utf-8")
        expect(any("generated form" in problem for problem in tool.check(root)), "a hand edit is found")

        tool.generate(root)
        drawing_path = root / tool.DRAWINGS / "conjure-icon-32.svg"
        drawing_path.write_text(drawing(32, square(2, 1, 31, 30)), encoding="utf-8")
        problems = tool.check(root)
        expect(any("32 px" in problem for problem in problems), f"a changed drawing is found: {problems}")

        drawing_path.unlink()
        expect(quiet_main(tool, ["--check", "--root", str(root)]) == 2, "a missing drawing is a usage error")


def main() -> int:
    tool = load_tool()
    test_rasteriser(tool)
    test_parser(tool)
    test_png(tool)
    test_ico(tool)
    test_generate_and_check(tool)
    for failure in failures:
        print(f"FAIL: {failure}", file=sys.stderr)
    if failures:
        return 1
    print("generate_brand_icons self-test passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
