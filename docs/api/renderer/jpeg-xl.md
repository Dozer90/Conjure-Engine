# JPEG XL images
<!-- doc-template:wiki -->
[API](../index.md) → [Renderer](index.md)

<!-- doc-content:purpose:start -->
JPEG XL (ISO/IEC 18181) is a source format for textures and HDR skies: lossless or lossy, 8 to 32 bits per
channel, with colour profiles, alpha, orientation and animation. Drop a `.jxl` file into the assets folder and the
texture importer cooks it like a PNG. `conjure/renderer/jpeg_xl.h` adds what has no other home: an encoder for
screenshots and tools, and the signature check the decoders route by. Everything is in `namespace
conjure::renderer`.
<!-- doc-content:purpose:end -->

|---|---|
| Name | JPEG XL images |
| Module | Renderer |
| `#include` | `conjure/renderer/jpeg_xl.h` |
| Engineering Handbook | [Engineering handbook](../../handbook/engine/renderer.md) |

## Syntax

<!-- doc-content:syntax:start -->
<a id="quick-example"></a>

```cpp
#include "conjure/renderer/jpeg_xl.h"
#include "conjure/renderer/texture_asset.h"

using namespace conjure;

core::result<renderer::texture_asset> round_trip(const core::image &image) {
    renderer::jpeg_xl_encode_options options = {};
    options.effort = 4;                           // lossless is the default
    CONJURE_TRY_ASSIGN(const std::vector<std::byte> file, renderer::encode_jpeg_xl(image, options));
    // renderer::is_jpeg_xl(file) is true; the decode routes on that, not on a file name.
    return renderer::texture_asset::decode(file, renderer::texture_colour_space::SRGB);
}
```
<!-- doc-content:syntax:end -->

## At a glance

<!-- doc-content:at_a_glance:start -->
| You want to... | Use |
|---|---|
| Use a `.jxl` file as a texture | put it in the assets folder: `texture_importer` claims `.jxl` |
| Decode one in code | `texture_asset::decode(bytes, colourSpace, generateMips, jobs)` or `load_from_file(path, ...)` |
| Bake an HDR sky from a JPEG XL | name `conjure.renderer.environment` as the file's importer ("import as"); `equirect_image::decode_hdr(bytes, jobs)` in code |
| Write an image as JPEG XL | `encode_jpeg_xl(image, options, jobs)` |
| Take an editor screenshot as JPEG XL | `view.screenshot` with a `.jxl` path (📘 API: [Headless mode and the command model](../editor/headless-and-commands.md#picking-screenshots-camera-console)) |
| Tell a JPEG XL file from other bytes | `is_jpeg_xl(bytes)` |
<!-- doc-content:at_a_glance:end -->

## Variables and Properties

<!-- doc-content:variables:start -->
Types, fields, and state are described in the reference sections below.
<!-- doc-content:variables:end -->

<!-- doc-content:body:start -->
## How a JPEG XL file becomes a texture

Every image decoder entry point (`texture_asset::decode`, the texture importer, glTF images and
`equirect_image::decode_hdr`) looks at the file's first bytes: a JPEG XL signature goes to libjxl, anything else to
the PNG / JPEG / TGA / BMP decoder. A PNG named `.jxl` therefore imports as a PNG, and a JPEG XL named `.png` as a
JPEG XL. What the JPEG XL decoder does on the way to an RGBA8 texture:

| The file has... | The texture gets... |
|---|---|
| A colour profile (sRGB, Display, linear, an ICC profile) | `SRGB`: the colours converted to sRGB by the colour management system. `LINEAR`: the samples as the file stores them, unconverted, because a linear texture holds data such as normals |
| PQ or HLG (HDR) | `SRGB`: linear light with 1.0 at 203 nits (reference white, see [HDR skies](#hdr-skies)), clipped at 1 and sRGB-encoded: reference white is white and brighter highlights clip. `LINEAR`: the PQ or HLG signal as stored |
| 16-bit, half or float samples | 8 bits per channel: decoded to 16 bits and the high byte kept, exactly as a 16-bit PNG. Colour and alpha are judged apart: a 16-bit alpha next to 8-bit colour keeps its high byte too, and that colour, when it is converted, is rounded to the nearest byte |
| Grey, or grey with alpha | the grey in R, G and B |
| No alpha | alpha 255 |
| Premultiplied alpha | straight alpha: the colour is divided by alpha in the encoding the file stores, before it is converted |
| An orientation (rotated or mirrored) | the image turned upright: a portrait photo stored sideways comes out portrait |
| An animation | its first frame; the log warns how many frames were dropped |
| A width or height above `texture_asset::MAX_DIMENSION` (16384) | `INVALID_ARGUMENT`, read from the header before any pixel is allocated |
| More than 16 extra channels, or a layer (frame) wider or taller than 16384 | `INVALID_ARGUMENT`, read from the headers before any frame is decoded |
| A file that needs more decoding memory than its size allows | `INVALID_ARGUMENT` naming the limit (see [Limits](#limits)) |
| A file that ends before the data its headers announce | `renderer_error::DECODE_FAILED`, "the file is truncated" (a header damaged into announcing more data reads the same) |
| A corrupt codestream, or a feature libjxl refuses | `renderer_error::DECODE_FAILED`, "the file is corrupt or uses a feature libjxl refuses" |

> [!NOTE]
> The other formats ignore colour profiles and orientation, as they always have. Only JPEG XL is colour-managed.

`texture_importer` is at version 2 since it reads JPEG XL, so cached textures import again once.

### Limits

A JPEG XL header can announce far more work than its file holds: a few hundred bytes can claim thousands of extra
channels or a layer of 60000 x 60000 pixels. Before any frame is decoded, the decoder refuses an image or a layer
wider or taller than 16384 and more than 16 extra channels (alpha, depth, spot colours and a CMYK black stay well
below; only alpha reaches the texture). While it decodes, libjxl may hold at most 160 MiB (32 MiB, plus 8 MiB for
each of the 16 threads it may run on), plus 64 bytes per sample of the image (width x height x channels); a lossless
1024 x 1024 RGBA file peaked at a seventh of its 416 MiB on nine threads. Anything above is refused with
`INVALID_ARGUMENT`, so a file can never make the import use much more memory than its declared size justifies. The
limit is the same on the calling thread and on any number of workers, so whether a file decodes does not depend on
the machine. The sizes of an ICC profile or a metadata box allocate nothing by themselves.

## HDR skies

`equirect_image::decode_hdr` reads a Radiance `.hdr` or a JPEG XL. A JPEG XL of any sample type (float files
are the usual HDR ones) is converted to linear light with sRGB (Rec. 709) primaries, and values above 1 are kept.
The `.jxl` extension belongs to the texture importer, so a sky stored as JPEG XL reaches `environment_importer`
when its `.meta` names it ("import as": `asset_database::set_importer(id, ENVIRONMENT_IMPORTER_ID)`).
A float JPEG XL holding the same values as a `.hdr` bakes to the same bytes.

PQ and HLG files (HDR exports of photo editors and phones) store display light in nits, while a `.hdr` or a float
file has no unit: its 1.0 is the scene's diffuse white. The decoder puts 1.0 at 203 nits, the reference white
ITU-R BT.2408 sets for both, so a PQ or HLG sky bakes like the same sky stored as floats. The scale is fixed, not
taken from the file's intensity target (its peak brightness, which exporters set differently), so the same signal
always gives the same radiance. HLG is first shown on a display of the file's intensity target (1000 nits unless the
file says otherwise), where 75 % signal is 203 nits and becomes 1.0. A sky converted to PQ by a tool that maps 1.0
to another luminance (libjxl's own default for SDR content is 255 nits) comes back scaled by that luminance / 203;
the environment's intensity setting corrects it. The rescale applies to files whose colour encoding names PQ or HLG,
which is how libjxl and `cjxl` write HDR input; a file that carries an ICC profile describing PQ is converted as
libjxl reads it, with 1.0 at the file's intensity target.

`decode_hdr` hands the bake finite, non-negative radiance: NaN and negative values (a float file can store them, and
saturated wide-gamut colours turn negative in sRGB primaries) become 0, and +inf, or anything above
`MAX_ENVIRONMENT_RADIANCE` (about 5.2e33), becomes that bound, so no sum of the bake overflows. Finite HDR radiance
below it is kept, so a sun far brighter than 65504 (the largest value of the cooked half-float cubemaps) still lights
the irradiance and the rough reflections fully; only the stored cubemap texels clamp at 65504.

## Writing JPEG XL

`encode_jpeg_xl(image, options, jobs)` writes a `core::image` (RGBA8) and returns the file's bytes.

| `jpeg_xl_encode_options` field | Default | Meaning |
|---|---|---|
| `lossless` | `true` | bit-exact; the image decodes back to the same RGBA |
| `distance` | `1.0` | lossy only: the Butteraugli distance in (0, 25]; 1 is visually lossless, larger is smaller and rougher |
| `effort` | `7` | 1 (fastest) to 10 (smallest file); a lossless file is exact at every effort |
| `colourSpace` | `SRGB` | what the file is tagged as: `SRGB` for colour, `LINEAR` (linear sRGB) for data |

A lossy file keeps its alpha lossless, so cut-out edges and masks stay exact. Failures: `INVALID_ARGUMENT` for an
empty image, an effort outside 1..10 or a lossy distance outside (0, 25] (NaN included); `BACKEND_FAILURE` if
libjxl itself fails.

The editor's `view.screenshot` writes its `.jxl` screenshots with this encoder: lossless, `SRGB`, effort 2. On
1280 x 720 frames effort 2 was faster than the PNG encode and wrote a third to a half of its bytes; higher efforts
are much slower and only sometimes smaller.

## Threads and determinism

`jobs` is optional everywhere. With a running job system that has workers, libjxl's work is split across them (16
threads at most); without one, it runs on the calling thread. The bytes are the same either way, for encoding and
decoding, and on every run: a machine decodes the same file to the same texture every time, which the asset cache
relies on. The decoding memory limit is the same either way too; only a file needing within a few MB per thread of
it, far more than its image size explains, can decode on the calling thread and be refused on workers. The texture
and environment importers get the engine's job system from `register_asset_types`.

On another machine the bytes are the same when the file is lossless and comes out in the encoding it stores: an
sRGB file as an `SRGB` texture, any lossless file as a `LINEAR` texture, a linear-sRGB file (floats or integers) as
an HDR sky. They are expected to match, without a guarantee, for an 8-bit file with premultiplied alpha that comes out
that way, and for a lossless file that skcms alone converts between colour spaces (Display to sRGB, an ICC
profile), as skcms avoids the fused multiply-add that rounds differently between CPUs. libjxl picks its SIMD code for
the CPU it runs on, so a lossy file, or a lossless one converted with libjxl's own transfer functions (every
conversion from or to PQ or HLG, whatever the colour space, and between sRGB and linear light in the same colour
space: an sRGB, PQ or HLG sky, a PQ or HLG `SRGB` texture, a linear-sRGB file as an `SRGB` texture), may come out
one rounding step apart on another machine. A cooked texture or a baked sky made from such a file is then
consistent on each machine but not byte-identical between them; keep HDR skies lossless and linear when every
machine must bake the same `.cenv`.
<!-- doc-content:body:end -->

## Functions

<!-- doc-content:functions:start -->
<a id="api-summary"></a>

| Signature (short) | Purpose |
|---|---|
| `bool is_jpeg_xl(span<const byte> bytes) noexcept` | a codestream (`FF 0A`) or container signature; too few bytes read as false |
| `result<vector<byte>> encode_jpeg_xl(const core::image &, const jpeg_xl_encode_options & = {}, job_system * = nullptr)` | write a JPEG XL file |
| `static result<texture_asset> texture_asset::decode(span<const byte>, texture_colour_space, bool generateMips = true, job_system * = nullptr)` | decode any supported image |
| `static result<equirect_image> equirect_image::decode_hdr(span<const byte>, job_system * = nullptr)` | decode a `.hdr` or JPEG XL to linear RGB floats |
| `texture_importer(job_system *jobs)` | the importer decoding on a job system |
<!-- doc-content:functions:end -->

------------------------------------------

**See also:**

<!-- doc-content:related:start -->
- 📘 API: [Renderer assets](renderer-assets.md), [Database, importers and hot reload](../assets/database-and-import.md)
- 🛠 Handbook: [JPEG XL](../../handbook/project-reference/third-party-dependencies/jpeg-xl.md), [Renderer module](../../handbook/engine/renderer.md)
<!-- doc-content:related:end -->

<!-- doc-legacy-anchors:start -->
<a id="see-also"></a>
<!-- doc-legacy-anchors:end -->
