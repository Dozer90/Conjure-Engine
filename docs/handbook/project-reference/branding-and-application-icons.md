# Branding and Application Icons
<!-- doc-template:chapter -->
[Handbook](../index.md) → [Project Reference](index.md)

<!-- doc-content:purpose:start -->
<a id="logo-and-application-icon"></a>

The official Conjure logo, adopted on 13 November 2026, how it is constructed, how it is cut for small sizes,
and how it becomes the editor's application icon on every platform. Read this before changing a drawing, the
icon colour or the generator, and before using the mark anywhere new.
<!-- doc-content:purpose:end -->

<!-- doc-content:body:start -->
<a id="1-the-mark"></a>

## The mark

![The Conjure logo](../../assets/brand/conjure-logo.svg){.conjure-brand-tile width="220" }

A flame that is also a familiar: a fire spell, and a creature casting a star from its mouth. The C of Conjure is
there too, tilted and deliberately half hidden, so the mark flips between readings the way magic should. It has
four pieces: the flame body with the star knocked out of it as an eye, a second flame tongue, and the large
four-point star in the C's mouth. Both stars share one shape family (long axis about 1.5 times the short one,
on the same diagonal, with the same slight twist).

| File | What it is |
|---|---|
| `docs/assets/brand/conjure-logo.svg` | the master on the square icon canvas (2048 units, section 2) |
| `docs/assets/brand/conjure-mark.svg` | the same master cropped tight to the ink, for lockups beside the wordmark |
| `docs/assets/brand/icon/conjure-icon-<N>.svg` | the logo drawn for an N x N pixel grid, N = 16, 20, 24, 32, 40, 48, 64, 128, 256 (section 3) |

The masters are black; the mark is always used in one flat ink chosen for the surface it sits on.

<a id="2-construction-and-use"></a>

## Construction and use

<a id="21-grid"></a>

### Grid

The canvas is 2048 units square. The mark's height fills the middle 1792 (a 1/16 margin at top and bottom).
Horizontally it sits halfway between the centre of its bounding box and the centre of its ink mass, so the heavy
body on the left does not make it look shifted. Every gap between two pieces, the tongue's and the star's, is one
**cut**: 64 units, 1/32 of the canvas, exactly 1 px at 32 px and 2 px at 64 px. The tongue's facing edge runs
parallel to the body, so its gap is even along its length.

<a id="22-clear-space-and-colour"></a>

### Clear space and colour

In layouts, keep at least the eye's height clear on every side (276 units, about 1/6 of the mark's height). App
icons use the canvas margin instead. Lockups pair the tight crop with the wordmark "Conjure", set in Inter
Semibold for now.

| Do | Don't |
|---|---|
| use one flat ink on any surface with enough contrast | add outlines, gradients, glows or shadows |
| use the drawing made for the size (section 3) | use the master below 48 px |
| keep the tilt: it is what lets the C hide | rotate, mirror or straighten the mark |
| keep the star with the creature | colour the star, the eye or the tongue on its own, or change a gap by hand |

<a id="3-small-sizes"></a>

## Small sizes

Below 48 px a single drawing cannot stay legible: the gaps fall under a pixel and fuse, the tongue becomes a
speck and the eye fills in. Each small size therefore has its own cut. A cut only moves or rescales whole pieces
of the master; no curve is redrawn, so every cut is still exactly the drawing.

| Drawn for | Used for | Changes from the master |
|---|---|---|
| 16 px | 16 and 20 px | tongue merged into the body as a second ear; star pulled back 16% from its far tip; eye 1.5 times larger |
| 24 px | 24 px | tongue merged as above; star pulled back 6%; eye 1.25 times larger |
| 32 px | 32 and 40 px | tongue kept, its gap widened from 64 to 84 units; star pulled back 4%; eye 1.15 times larger |
| master | 48 px and up | none |

Every drawing is then fitted to its own pixel grid: the mark is offset by the fraction of a pixel that renders
crispest, and in the small cuts the eye alone is moved by up to half a pixel so its hole opens on whole pixels
(at 16 px it is a clean 2 x 2 opening instead of four grey pixels). That fitting is why each icon size has its
own file even where the shapes are the master's. The cuts make the ink a little taller relative to the canvas
(15/16 at 16 px, 22/24 at 24 px, 30/32 at 32 px) because small icons have no room to spare.

> [!NOTE]
> The merged tongue overlaps the body. The drawings are filled under the nonzero rule (the SVG default), so
> same-wound pieces merge and the eye, wound the other way, stays a hole. A tool that fills with the even-odd
> rule would punch the overlap out.

<a id="4-the-application-icon"></a>

## The application icon

<a id="41-colour-and-sizes"></a>

### Colour and sizes

The icon uses the Midnight theme accent, `#4C8DFF`, which keeps at least 3:1 contrast against both light and
dark taskbars and title bars. The generator writes two outputs from the drawings:

| Output | Sizes | Consumer |
|---|---|---|
| `editor/resources/conjure_editor.ico` | 16, 20, 24, 32, 40, 48, 64 as 32-bit BMP entries; 256 as PNG | the Windows resource |
| `engine/src/editor/ui/editor_icon_data.cpp` | 16, 24, 32, 48, 64, 128, 256 as PNG files | `window::set_icon` at run time |

<a id="42-windows"></a>

### Windows

`editor/resources/conjure_editor.rc` compiles the.ico into `conjure_editor` as the icon resource named
`GLFW_ICON`. Explorer and the taskbar show an executable's first icon group, and GLFW gives every window of the
process the `GLFW_ICON` icon when it creates the window class, so the icon is in place before the first frame.
`editor/CMakeLists.txt` adds the script on Windows only, puts `editor/resources` on the resource compiler's
include path for that one source, and lists the.ico as a dependency so a regenerated icon relinks.

<a id="43-every-platform-at-run-time"></a>

### Every platform at run time

`conjure/editor/ui/editor_icon.h` decodes the embedded PNGs (`editor_icon_images()`) and sets them on a window
(`apply_editor_icon()`); `editor_layer::on_attach` calls it for the engine's window. The icon is cosmetic, so a
failure never fails the attach:

| Platform | Result |
|---|---|
| Windows | the same images replace the class icon |
| X11 | the window manager shows the icon (it picks a size from the seven) |
| macOS, Wayland | `FEATURE_UNAVAILABLE`, one INFO line: windows there take their icon from the application bundle or the desktop file |

`window::set_icon` is the platform call underneath; its contract (validation by `validate_window_icon`, an empty
span restoring the default icon) is on the wiki page 📘 API: [Windows and events](../../api/platform/windows-and-events.md).
The editor ships no macOS application bundle or Linux desktop file yet, so those platforms show the system
default until it does. `conjure_runtime` sets no icon: a game's icon is the game's.

<a id="5-changing-a-drawing"></a>

## Changing a drawing

1. Edit `docs/assets/brand/icon/conjure-icon-<N>.svg` in a vector editor and keep the restricted form the
   generator reads: one `<svg>` whose `viewBox` is square and whose `width` and `height` equal N, holding exactly
   one `<path>` of absolute `M`, `C` and `Z` commands, every subpath closed. Anything else is rejected with the
   file and the reason.
2. Run `python tools/generate_brand_icons.py`. It rasterises every drawing in the icon ink and rewrites the.ico
   and the generated C++.
3. Rebuild and run the tests. `brand.icons_up_to_date` (`generate_brand_icons.py --check`) fails while an
   output differs from what the drawings produce; `brand.icons_tool` is the generator's self-test.

> [!IMPORTANT]
> Never edit `conjure_editor.ico` or `editor_icon_data.cpp` by hand: the check reports the difference and the
> next regeneration discards it.

The rasteriser is exact per sub-scanline: 16 horizontal samples per pixel row, each curve flattened into 48
segments, the overlap of every filled span with every pixel summed, coverage clamped to 1 and written as the
alpha of the ink. It uses only ordinary float arithmetic, so its output is the same on every machine. The check
compares PNG payloads after decoding them, so a different zlib build cannot make it fail, and compares the BMP
entries and the C++ text byte for byte.

## Related

- 📘 API: [Windows and events](../../api/platform/windows-and-events.md) (`window::set_icon`)
- 📘 API: [The workspace UI](../../api/editor/workspace-ui.md) (`conjure/editor/ui/editor_icon.h`)
- [Modules: platform](../engine/platform.md), [Modules: editor](../editor/editor.md)
- `tools/generate_brand_icons.py` and `tools/tests/test_generate_brand_icons.py`

<a id="contents"></a>
<!-- doc-content:body:end -->
