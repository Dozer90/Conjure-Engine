# JPEG XL
<!-- doc-template:chapter -->
[Handbook](../../index.md) → [Project Reference](../index.md) → [Third-Party Dependencies](index.md)

<!-- doc-content:purpose:start -->
<a id="libjxl-jpeg-xl"></a>

This chapter covers JPEG XL in the engine: what the format is used for and what it is not, then the dependency:
`third_party/jxl.cmake` pins libjxl, the reference implementation of JPEG XL (ISO/IEC 18181), and the three
libraries it compiles from source, configures only its static libraries and exposes them as
`conjure::thirdparty::jxl`; `third_party/jxl_checks.cmake` checks the result when the engine is configured, and the
renderer's JPEG XL codec built on it. Read it before linking libjxl into a module, bumping the pin, changing how it
is built or changing the codec. The general rules for third-party code are in
[Building and Configuration](../../development/building-and-configuration.md#8-third-party-dependencies).
<!-- doc-content:purpose:end -->

<!-- doc-content:body:start -->
<a id="1-jpeg-xl-in-the-engine"></a>

## JPEG XL in the engine

JPEG XL is a still-image format, lossless or lossy, with 8 to 32 bits per channel (integer or float), alpha, colour
profiles, HDR transfer curves (PQ, HLG) and animation. The engine adopts it as a source and interchange format:

| Use | Entry point | Why JPEG XL |
|---|---|---|
| Source textures | `texture_importer` claims `.jxl`; glTF images and `texture_asset::decode` recognise the signature | lossless masks, heightmaps and painted textures smaller in source control than PNG; lossy for photographic sources; the file's colour profile is honoured |
| HDR environments | `environment_importer` (by "import as"), `equirect_image::decode_hdr` | half or float samples in linear light, lossless or lossy, without the 8-bit mantissas and shared exponent of a Radiance `.hdr` |
| Screenshots | the editor's `view.screenshot` with a `.jxl` path or `"format": "jxl"` | the same pixels as the PNG; at effort 2, on 1280 x 720 test frames in a Debug build, a third to a half of its bytes and a faster encode. PNG stays the default |

It is not a GPU format. A JPEG XL file is entropy-coded and has to be decoded to pixels on the CPU, while a GPU
samples fixed-rate block formats it decodes itself. The importers therefore decode it once, at import: a cooked
texture is the same RGBA8 `.ctex` (sRGB or UNORM, with its mip chain) as one cooked from a PNG, and the
block-compressed formats a later cook step is to produce (BCn and ASTC; the RHI already reserves BC1, BC5 and BC7)
will be reached the same way. Cooked packs and the asset cache hold cooked blobs only, so a cooked project
never decodes JPEG XL at run time. Golden images, their diffs and the editor's icons stay PNG: reviewers and tools
open them everywhere.

Which tools write `.jxl` (November 2026): Photoshop (26.8 and later), GIMP (3.0 and later), Krita, Affinity
Photo 2 and Paint.NET (5.1.5 and later) open and save it, and so do Lightroom Classic and darktable. Blender (whose
ImBuf support is an open pull request) and Substance 3D Painter (absent from its export formats) do not, so baked
and painted 3D textures still arrive as PNG or TGA and import as before; convert them to JPEG XL only when their
size matters.

> [!WARNING]
> Decoding an image parses whatever bytes the file holds, and an image decoder is attack surface: a crafted file can
> reach a decoder bug. libjxl's recent releases fixed decoder use-after-free, uninitialised-memory and overflow bugs,
> and Brotli's decoder had a critical out-of-bounds write (GHSA-9v8h-qqgm-4m35, fixed in Brotli v1.2.1). v0.13.0 is
> pinned because it carries the latest of those fixes; bump the pin (section 8) when a release fixes another. The
> decoder runs wherever sources are imported: the editor, tools, and a runtime started on an uncooked project. Its
> guards bound the resources a file can ask for (section 9.3): the 2 GiB input cap, `MAX_DIMENSION` for the image
> and for every layer and the extra-channel cap, all read from the headers before a frame is decoded, and a memory
> budget derived from the image's size that libjxl's allocations count against. They bound memory, not decoder bugs:
> they do not make a malformed file safe, so treat `.jxl` files from mods or other users as untrusted input.

<a id="2-pins"></a>

## Pins

The release tarball of libjxl and the commits its `deps.sh` and `.gitmodules` name. Each is a GitHub archive pinned
by URL and SHA-256, so FetchContent refuses bytes that differ. The commit each archive was made from is recorded in
the archive itself (the comment of its pax header), and every one matches its pin.

| Archive | Pin | SHA-256 | Size | Licence |
|---|---|---|---|---|
| libjxl | tag `v0.13.0` (commit `5a7da834`) | `24d18c9af2d1f5a17730b3f8d3f70a5c16e5ac303f5d9d5fe9dc00cf7ca82af2` | 1,734,982 B | BSD-3-Clause, plus Google's patent grant (`PATENTS`) |
| Highway | commit `457c8917` (1.2.0) | `5124b0501c98d9930dbb065bfa1a5bbbd59ce0f12facb7e1e33aaef01a5f1f1a` | 2,235,690 B | Apache-2.0 OR BSD-3-Clause |
| Brotli | commit `db24a98a` (v1.2.1) | `113b9685dfc14dd2f3360d077283266eec2fbdfb0fe3e5600f11be151da0d68e` | 646,758 B | MIT |
| skcms | commit `96d9171c` (2025_09_16) | `9bb4b5bba0b7c04f6c2bce9ff713d61e23c9a20c4945161ae16290498ad74627` | 10,061,812 B | BSD-3-Clause |

The libjxl hash is also the one Homebrew and MacPorts publish for the tarball. libjxl v0.13.0 moved to Brotli v1.2.1
for its decoder fix of advisory GHSA-9v8h-qqgm-4m35. Nothing else is downloaded: no lcms2, libpng, sjpeg, googletest,
test data or OpenEXR.

<a id="3-layout-of-the-tree"></a>

## Layout of the tree

The GitHub archive of libjxl carries no submodules, and libjxl's `third_party/CMakeLists.txt` looks for Highway,
Brotli and skcms only inside its own tree. The three are therefore unpacked into its `third_party/` directory after
libjxl itself (unpacking libjxl replaces its whole tree):

```
build/<preset>/_deps/libjxl-0.13.0-src/                     libjxl
build/<preset>/_deps/libjxl-0.13.0-src/third_party/highway/  FetchContent name libjxl_highway
build/<preset>/_deps/libjxl-0.13.0-src/third_party/brotli/   libjxl_brotli
build/<preset>/_deps/libjxl-0.13.0-src/third_party/skcms/    libjxl_skcms
build/<preset>/_deps/libjxl-build/                           libjxl's build directory
```

The tree is versioned. A new libjxl version unpacks into a new directory, and because the directory is part of each
nested declaration (ExternalProject re-runs a download step whose source directory changed), the three are unpacked
into it again even when their own pin stayed.

FetchContent unpacks a tree only when its download step runs, and that step depends on its script, its URL and its
stamps, never on the tree, so a tree that disappears is not unpacked again:

- libjxl's tree deleted by hand stays missing, and so does every tree of a configure with
  `FETCHCONTENT_FULLY_DISCONNECTED` ON, which unpacks nothing;
- one of the three deleted by hand stays missing;
- libjxl's tree unpacked again at the same version (its sub-build or stamps removed, or its URL changed) has lost the
  three, whose steps do not run again.

`conjure_jxl_check_tree` stops the configure in each case and names the cause and the remedy: delete the stamp
directories `_deps/libjxl*-subbuild/*-populate-prefix/src/*-populate-stamp` and configure with
`FETCHCONTENT_FULLY_DISCONNECTED` OFF. FetchContent configures and builds every sub-build on every configure, so the
steps run again, and an archive already in its sub-build whose SHA-256 matches is unpacked without a download (tried
on a Debug tree: skcms deleted by hand, then its stamp directory). Deleting `_deps/libjxl*` also works, but it throws
away the four archives, the only local copies, and the next configure downloads them all.

<a id="31-a-libjxl-clone"></a>

### A libjxl clone

`FETCHCONTENT_SOURCE_DIR_LIBJXL` set to a libjxl clone (to work on the decoder) builds the clone instead of the
pinned archive. `jxl.cmake` then unpacks nothing into it: an unpack step deletes its target directory first, which
would take the clone's Highway, Brotli and skcms submodules with their `.git` files and local edits. The clone brings
the three itself (`git submodule update --init third_party/highway third_party/brotli third_party/skcms`), nothing
declares them (their licence notices read libjxl's tree, section 7), and the configure stops naming that command when
one is missing. The clone must be at the pinned tag: `conjure_jxl_check_build` compares the version its
`lib/CMakeLists.txt` declares with `CONJURE_JXL_VERSION`.
`FETCHCONTENT_SOURCE_DIR_LIBJXL_HIGHWAY`, `_BROTLI` and `_SKCMS` stop the configure (`conjure_jxl_check_overrides`):
libjxl builds the three from its own `third_party/` directory only, so one placed elsewhere would be missing there.

<a id="4-configuration"></a>

## Configuration

`conjure_add_libjxl()` adds libjxl from a function, so these settings reach libjxl's directories and nothing else of
the engine:

| Setting | Why |
|---|---|
| `BUILD_SHARED_LIBS OFF`, `BUILD_TESTING OFF` (variables) | static libraries, no upstream tests; the engine's own cache entries are untouched |
| `CMAKE_POLICY_DEFAULT_CMP0077` and `CMP0102` `NEW` | Highway declares CMake 3.10: without them its `option()` calls ignore the two variables and its `mark_as_advanced(BUILD_SHARED_LIBS)` writes the entry into the engine's cache |
| `CMAKE_DISABLE_FIND_PACKAGE_` `HWY`, `Brotli`, `LCMS2`, `PNG` | libjxl falls back to an installed copy when its tree lacks one, and looks for libpng for its tools; no installed library can stand in for a pin |
| `JPEGXL_VERSION` | otherwise libjxl's tools directory asks git for a version, and the checkout around the build tree is the engine's |
| `PROVISION_DEPENDENCIES OFF` | it runs `deps.sh` at configure time, which downloads without checking any hash |
| `JPEGXL_ENABLE_*` `OFF` | tools, devtools, Doxygen, man pages, benchmark, examples, JNI, sjpeg, OpenEXR, viewers, plugins, fuzzers, tcmalloc, coverage |
| `JPEGXL_ENABLE_SKCMS`, `_TRANSCODE_JPEG`, `_BOXES` `ON` | skcms is the colour management system. JPEG reconstruction (lossless JPEG recompression) and box decoding stay at libjxl's defaults, and they link Brotli's decoder into `jxl`. The engine decodes pixels only and asks for neither (no `JxlDecoderSetJPEGBuffer`, no box event): libjxl skips `jbrd`, `Exif`, `xml ` and `brob` boxes, and a JPEG that `cjxl` transcoded decodes to pixels like any other file. A codestream inside the container (`jxlc`, or split across `jxlp` boxes after a metadata box) is decoded and tested |
| `JPEGXL_STATIC`, `_BUNDLE_LIBPNG`, `_FORCE_SYSTEM_*` and the others `OFF` | pinned to their defaults; `JPEGXL_STATIC` would also set the MSVC runtime |
| `HWY_ENABLE_INSTALL OFF` | Highway's install rules; libjxl and Brotli have no such switch, and nothing is installed from an engine tree |

libjxl compiles itself as C++17 in its own directory and adds no warning flags under MSVC. The directory is `SYSTEM`
and `EXCLUDE_FROM_ALL`: only what an engine target links is built (`conjure_renderer` links
`conjure::thirdparty::jxl`, so every build with `CONJURE_WITH_RENDERER` builds it), and its headers never trigger the
engine's warnings.

> [!NOTE]
> With cl the only output of the libjxl build is Highway's command-line note on its seven sources: Highway's
> own `/EHs-c-` overrides the `/EHsc` every target gets. libjxl compiled without `NDEBUG` (the Debug configuration)
> prints the reason of a failed encoder or decoder call to stderr; Development and Distribution define `NDEBUG`.

Highway picks its SIMD code at run time, and skcms compiles its AVX2 and AVX-512 files only under Clang on x86-64,
with the flag on that one file. No instruction-set flag reaches an engine object, which section 5 checks.

<a id="5-checks"></a>

## Checks

Tests never include third-party headers, so the pin is checked when the engine is
configured. `third_party/jxl_checks.cmake` stops the configure when:

- `conjure_jxl_check_overrides` finds `FETCHCONTENT_SOURCE_DIR_LIBJXL_HIGHWAY`, `_BROTLI` or `_SKCMS` set (section
  3.1);
- `conjure_jxl_check_tree` finds libjxl's tree missing, or Highway, Brotli or skcms missing from it (section 3), with
  the submodule command instead of the stamp remedy for a clone;
- `conjure_jxl_check_build` finds that `jxl` or `jxl_cms` does not exist, declares a version other than
  `CONJURE_JXL_VERSION` (libjxl sets it in its own `lib/CMakeLists.txt`, so it names the source that was unpacked,
  whatever the URL said) or is not a static library;
- the link closure of `jxl` holds a target other than `jxl`, `jxl_cms`, `jxl_base`, `hwy`, `brotlienc`, `brotlidec`,
  `brotlicommon` and `Threads::Threads`: an optional dependency (lcms2, libpng, tcmalloc) that entered through an
  upstream option that changed name;
- a target of that closure other than `Threads::Threads` passes compile options to its consumers;
- configuring libjxl created a `BUILD_SHARED_LIBS` cache entry. The check removes the entry before it stops the
  configure: CMake saves the cache of a failed configure, and the next one would find the entry already there and
  take it for the engine's own, so the check fires on every configure until the leak is fixed.

The checks run in `third_party/`, the directory that adds libjxl's tree, and there an imported target that
`find_package()` or `pkg_check_modules()` creates inside libjxl's directories is not a target unless it was made
`GLOBAL`: it is visible only in the directory that created it and below. `if(TARGET)` cannot tell such a name
(`PNG::PNG`, `PkgConfig::TCMallocMinimal`, or the `Threads::Threads` that libjxl's own `find_package(Threads)`
creates, the only one in the pinned build's closure) from a plain library name such as `m`. The link-closure check
therefore collects the `IMPORTED_TARGETS` of every directory of libjxl's tree, and a closure name among them other
than `Threads::Threads` stops the configure by its name: its compile options cannot be read from `third_party/`. An
`ALIAS` such a directory makes of one (`tcmalloc::minimal`) is scoped like it since CMake 3.18 but is in no
`IMPORTED_TARGETS` list, so any other name with `::` that is no target in `third_party/` stops the configure too;
`Threads::Threads` and CMake's `::@` markers around a link item added from another directory pass. Real targets and
`GLOBAL` imports (`lcms2` from libjxl's `FindLCMS2`) are read as before.

The checks live apart from the download so that `tests/third_party/` can run them on stand-ins. Its CTest entries
(labels `build;third_party`) configure a throwaway project without a language, afresh unless an entry repeats the
configure of an earlier one, and never download. The stand-in of libjxl's tree has the real scoping: the stand-ins
of the libraries libjxl builds are `GLOBAL` imported targets, visible everywhere like real ones, while
`Threads::Threads` is created in the stand-in's root and the hidden dependency in its `lib/`, both invisible to the
directory that runs the checks. The entries from the pinned build to the cache entry add the stand-in with
`add_subdirectory()` and run the tree and build checks from there, as `jxl.cmake` does; the two tree entries call
`conjure_jxl_check_tree` on a partial or empty directory and add nothing. The last five include
`cmake/ConjureNotices.cmake` and then `jxl.cmake` itself with `FETCHCONTENT_FULLY_DISCONNECTED` ON: `unpacked_tree`
and `without_renderer` with the stand-in where `jxl.cmake` declares libjxl's tree (the archives as if unpacked), the
others with `FETCHCONTENT_SOURCE_DIR_LIBJXL` naming it. The first three go through `jxl.cmake`'s
`add_subdirectory()`; the last two stop at the check that refuses them, before anything is declared or added. The
first three resolve the four notices with `_conjure_notice_resolve()`, the step `conjure_write_third_party_notices()`
takes for each notice: with `CONJURE_WITH_RENDERER` ON each must be used, every licence file found in libjxl's tree,
and without the renderer none may be (section 7).

| Entry | Passes when |
|---|---|
| `third_party.jxl_check_accepts_the_pinned_build` | stand-ins shaped like the real build pass both checks, with `Threads::Threads` hidden in libjxl's tree and visible from an earlier `find_package(Threads)` |
| `third_party.jxl_version_mismatch_fails_configure` | `jxl` at 0.12.0 stops the configure |
| `third_party.jxl_shared_library_fails_configure` | a shared `jxl_cms` stops it |
| `third_party.jxl_missing_library_fails_configure` | a missing `jxl_cms` stops it |
| `third_party.jxl_unpinned_dependency_fails_configure` | `lcms2` (`GLOBAL`) two levels down, behind `$<LINK_ONLY:...>`, stops it |
| `third_party.jxl_hidden_imported_dependency_fails_configure` | `PkgConfig::TCMallocMinimal`, an imported target of the stand-in's `lib/` that `jxl` links, stops it |
| `third_party.jxl_hidden_alias_fails_configure` | `tcmalloc::minimal`, a non-`GLOBAL` `ALIAS` of that imported target made and linked in the same `lib/`, stops it by its `::` |
| `third_party.jxl_compile_options_fail_configure` | `/arch:AVX2` on `hwy` stops it |
| `third_party.jxl_shared_libs_cache_entry_fails_configure` | a `BUILD_SHARED_LIBS` entry that a libjxl directory wrote stops it |
| `third_party.jxl_shared_libs_cache_entry_fails_configure_again` | configured again in the same tree without `--fresh` (after the entry above), the entry stops it again |
| `third_party.jxl_incomplete_tree_fails_configure` | a tree without skcms stops it, naming the stamp remedy |
| `third_party.jxl_missing_tree_fails_configure` | a tree without `CMakeLists.txt` stops it, naming deletion by hand and `FETCHCONTENT_FULLY_DISCONNECTED` |
| `third_party.jxl_unpacked_tree_ships_the_notices` | `jxl.cmake` declares Highway, Brotli and skcms in libjxl's `third_party/` directory, adds the tree, and the four notices are used, so they ship |
| `third_party.jxl_source_override_keeps_the_clone` | `jxl.cmake` adds the clone and declares none of `libjxl_highway`, `libjxl_brotli` and `libjxl_skcms` (none is populated: disconnected, a declared one would be, without the unpack over the clone's submodule a real configure runs), and the four notices are used (section 7) |
| `third_party.jxl_notices_unused_without_renderer` | with `CONJURE_WITH_RENDERER` OFF the tree is added as before and none of the four notices is used (section 7) |
| `third_party.jxl_nested_source_override_fails_configure` | `FETCHCONTENT_SOURCE_DIR_LIBJXL_HIGHWAY` stops it |
| `third_party.jxl_clone_without_submodules_fails_configure` | a clone without skcms stops it, naming `git submodule update --init` |

<a id="6-the-target"></a>

## The target

`conjure::thirdparty::jxl` is an interface target over libjxl's `jxl`: the encoder and the decoder, with `jxl_cms`
(skcms) linked publicly and Highway and Brotli as link-only dependencies. Its usage requirements add
`JXL_STATIC_DEFINE` and `JXL_CMS_STATIC_DEFINE` and put the generated `jxl/` headers, Highway's headers and libjxl's
source root (whose internal headers engine code never includes) on the include path; `tools/check_style.py` lists
`jxl/` and `hwy/` as third-party roots, so they are included with angle brackets.
`CONJURE_JXL_VERSION` (a cache entry) holds the pinned version for the rest of the build.

> [!WARNING]
> libjxl also defines `jxl_dec`, a decoder-only library with the same symbols as `jxl`. A binary never links both.
> `jxl_threads` (a `std::thread` runner) is configured but not exposed: work runs on `core::jobs`, and a
> `JxlParallelRunner` over the job system is the way to decode on several threads.

<a id="7-licences"></a>

## Licences

The notices of are registered next to the pins with `conjure_third_party_notice()`
(`cmake/ConjureNotices.cmake`), each library's own licence file taken from its pinned archive: libjxl's `LICENSE` and
`PATENTS`, Highway's `LICENSE` and `LICENSE-BSD3`, Brotli's `LICENSE` and skcms's `LICENSE`, all with scope `RUNTIME`.
All four name `FETCH libjxl` and read the three's files from libjxl's tree (`third_party/highway/LICENSE`,...),
which holds them whether the archives were unpacked into it or a clone's submodules bring them (section 3.1). A
clone's build declares none of the three, and a notice whose `FETCH` content is not populated is left out of the
runtime notices without a warning, so a `FETCH` of their own names would drop three licence texts from a game
exported from such a build. libjxl is configured in every tree but only `conjure_renderer` links it, so the four say
`USED ${CONJURE_WITH_RENDERER}`: a tree without the renderer lists them in its manifest as unused and leaves them out
of its `THIRD_PARTY_NOTICES.txt`. Where the renderer is built, building `conjure_runtime` places the four licence
texts in `bin/THIRD_PARTY_NOTICES.txt` and export step 5 copies that file into the game.

Each `VERSION` reads the pin the download uses: `v${CONJURE_JXL_VERSION}` for libjxl (the tag of its URL) and
`CONJURE_JXL_HIGHWAY_COMMIT`, `CONJURE_JXL_BROTLI_COMMIT` and `CONJURE_JXL_SKCMS_COMMIT` for the three (the commits
of their URLs), so a bump cannot leave a notice naming the old pin. That is the only guard of the four `VERSION`s:
the configure compares a notice's `VERSION` with a `GIT_TAG` in the file that registers it
(`_conjure_notice_check_pin`), and `jxl.cmake` pins by `URL` and `URL_HASH`. Nor does the scan of
`conjure_write_third_party_notices()` for a `FetchContent_Declare` without a notice reach `jxl.cmake`: it reads
`third_party/CMakeLists.txt`, the file that calls it, and not the files that file includes. A scan of `jxl.cmake`
would have to accept `libjxl_highway`, `libjxl_brotli` and `libjxl_skcms`, whose notices name `FETCH libjxl`;
instead the probes of section 5 resolve the four notices through `_conjure_notice_resolve()` and fail unless each is
used and finds its licence files. `THIRD_PARTY_NOTICES.md` is regenerated from the registrations with
`tools/third_party_notices.py` ([Getting started](index.md#11-licence-and-third-party-notices)).

<a id="8-bumping-the-pin"></a>

## Bumping the pin

1. Take the new tag's tarball and the commits its `deps.sh` names for Highway, Brotli and skcms.
2. Download each archive once and compute its SHA-256. GitHub archive bytes depend on the URL form (a tag, a short
   or a full commit name the top directory differently), so hash the exact URL the declaration uses.
3. Change `CONJURE_JXL_VERSION`, the three `CONJURE_JXL_*_COMMIT`, the four `URL_HASH` and the release names in
   the notices' `NOTE` together. The URLs and the notices' `VERSION` read the version and the commits.
4. Read the new tag's option list in its top-level `CMakeLists.txt` and its `third_party/CMakeLists.txt`: a new
   option that adds a dependency shows up as a link-closure failure (section 5) until it is turned off.
5. Configure and build a target that links `conjure::thirdparty::jxl`, then run `ctest -R third_party` and the
   renderer module, `ctest -L renderer`: the codec's cases are in `test_jpeg_xl.cpp`, and the texture and
   environment suites decode JPEG XL too.
6. Regenerate `THIRD_PARTY_NOTICES.md` (`python tools/third_party_notices.py --build-dir <tree>`, section 7) and
   commit it with the pin: the four sections name the old pins until then, and `notices.up_to_date` fails.

<a id="9-the-renderers-codec"></a>

## The renderer's codec

`conjure_renderer` is the only consumer: `engine/src/renderer/import/jpeg_xl_*` decode JPEG XL for the texture and
environment importers, glTF images and `texture_asset::decode`, and encode for `encode_jpeg_xl`
(`conjure/renderer/jpeg_xl.h`), which the editor's screenshots call (lossless, effort 2) without linking libjxl
themselves; `core::image` stays PNG-only. The behaviour every caller sees is in
📘 API: [JPEG XL images](../../../api/renderer/jpeg-xl.md); this section explains how it
sits on libjxl.

| Piece | How |
|---|---|
| Allocation | `jpeg_xl_memory_manager(budget)`: a `JxlMemoryManager` over `core::memory::tracked_allocate` with a 16-byte size header, so the blocks keep malloc's alignment and `tracked_free` gets its size back. A decode passes a `jpeg_xl_memory_budget` as the manager's opaque pointer: every block counts against it until freed, and a request that would pass its limit returns `nullptr`, which libjxl turns into a clean `JXL_DEC_ERROR` (section 9.3). libjxl's own `std::vector` growth goes through `operator new` as everywhere else and is not counted: it holds bookkeeping (TOCs, frame lists), not planes |
| Threads | `run_jpeg_xl_on_jobs`: a `JxlParallelRunner` over `core::jobs::job_system::parallel_for`. Each range is split into `min(values, workers + 1, JPEG_XL_MAX_THREADS)` contiguous tasks (16 at most: the memory budget allows for that many, section 9.3); task `k` runs its values with thread id `k`, so libjxl's per-thread storage is never shared and the split does not depend on scheduling. Without a running system with workers no runner is set and libjxl stays on the caller. `jxl_threads` is never linked |
| Colour | `JxlDecoderSetCms(*JxlGetDefaultCms())` (skcms). `SRGB` requests an sRGB encoded profile (grey for grey) unless the data already is one; the HDR path requests linear sRGB floats; `LINEAR` requests nothing for a lossless file and the original encoded profile for a lossy one. A PQ or HLG original (read with `JxlDecoderGetColorAsEncodedProfile(..., TARGET_ORIGINAL, ...)`) is requested as linear sRGB floats for `SRGB` too, and the engine rescales it (section 9.3). libjxl's unpremultiply runs in its output stage, after every colour stage, so a converted file would come out as f(c * a) / a. It is on only when nothing converts after the division (an sRGB file as `SRGB`, any file as `LINEAR`; a lossy one is rendered back into its encoded profile first) and the output has alpha, which the HDR path's RGB floats have not; `JxlDecoderSetUnpremultiplyAlpha` is refused once the decode has started, so the header pass (`read_headers`) decides it from the colour encoding. Otherwise a premultiplied file is output as RGBA floats in the encoding it stores (lossless: as stored; lossy: rendered back into its encoded profile, or into linear sRGB and converted to its ICC profile), divided by `max(alpha, 2^-26)` as libjxl would, then converted to the wanted profile through the CMS interface; that float frame costs 16 bytes a pixel, which the integer way does not allocate (tested). That profile's ICC comes from a decoder over a four-byte header-only XYB codestream (`FF 0A 41 06`): libjxl writes the ICC of an encoded colour space only as a decoder's output profile, and only an XYB image's output follows the request |
| XYB into ICC | libjxl 0.13.0 cannot render a lossy (XYB) image into an ICC output profile: a debug build stops in `lib/jxl/render_pipeline/stage_from_linear.cc` ("Invalid target encoding"). A `LINEAR` decode of such a file asks for linear sRGB floats instead and converts them to the original ICC profile with the CMS interface (`set_fields_from_icc`, `init`, `run` per row, `destroy`); `test_jpeg_xl.cpp` covers RGB and grey profiles, 8 and 16 bits. The engine's steps after libjxl (convert to the original, unpremultiply, convert to the wanted profile, rescale PQ and HLG, apply the sRGB curve) run in that order in `finish_in_engine` |
| Depth | Output is `JXL_TYPE_UINT8` when colour and alpha are both 8-bit; when either is deeper, `JXL_TYPE_FLOAT` through an image-out callback (`quantise_scanline`) that quantises each channel by its own depth as `finish_in_engine` does, so no 16- or 32-bit frame is allocated: a deep channel keeps the high byte of its 16-bit rounding, an 8-bit one is rounded. Colour and alpha depths are separate fields of the basic info (`bits_per_sample`, `alpha_bits`). libjxl dithers its 8-bit output, which is why a deep alpha must not take that way, and the high byte of its 16-bit output rounds v x 255 at v instead of at one half, a step low for dark and high for bright 8-bit colour that libjxl converted (tested, Display and lossy). A 16-bit file and the same 16-bit PNG decode to the same bytes (tested) |
| Frames | events `BASIC_INFO`, `COLOR_ENCODING`, `FRAME`, `FULL_IMAGE`; later frames are skipped with `JxlDecoderSkipCurrentFrame` at their `NEED_IMAGE_OUT_BUFFER` and counted for the warning. A header-only pass runs first (`read_headers`, section 9.3) |
| Errors | The whole file is handed to `JxlDecoderSetInput` and the input is left open, so `NEED_MORE_INPUT` means the file ends before the data its headers announce: `DECODE_FAILED`, "the file is truncated" (after `JxlDecoderCloseInput` libjxl reports that as `JXL_DEC_ERROR` instead, which made truncation and corruption one message). `JXL_DEC_ERROR` is a corrupt file or a refused feature: `DECODE_FAILED`, "the file is corrupt or uses a feature libjxl refuses", unless the memory budget refused an allocation, which is `INVALID_ARGUMENT` naming the limit. A container whose codestream is complete decodes even when a later box claims more bytes than the file holds: no box event is asked for. The encoder validates its input itself (`INVALID_ARGUMENT`); a libjxl refusal after that is `BACKEND_FAILURE` naming the step and `JxlEncoderGetError` |

<a id="91-determinism"></a>

### Determinism

Decoding and encoding give the same bytes on every run and with 0, 1 or 8 workers (`test_jpeg_xl.cpp`: encoding
a 320 x 288 image, decoding a 300 x 280 one, both spanning several groups), which the asset cache needs. The memory
limit does not depend on the workers either (section 9.3).

Across machines, what decides is which code does the arithmetic:

| Decode | Same bytes on every x64 CPU? | Why |
|---|---|---|
| Lossless, output in the encoding it stores (an sRGB file as `SRGB`, any lossless file as `LINEAR`, a linear-sRGB file on the HDR path), unless libjxl divides 8-bit premultiplied colour | yes | integer-coded. libjxl's output stage converts to float and scales, and for 8-bit output adds an ordered dither of at most 0.49219 of a step before it rounds: single correctly rounded operations on samples that stay off a rounding boundary. The engine's own steps (the division on the HDR path, quantising a deep channel) are scalar code |
| Lossless, converted by skcms alone: between primaries or through an ICC profile, with no PQ or HLG side and no sRGB-linear pair of the same primaries (Display as `SRGB` or on the HDR path); or 8-bit premultiplied colour that libjxl divides (section 9) | expected | skcms's code paths avoid fused multiply-add (its CPU check says so; MSVC builds only its baseline path). libjxl's 8-bit output stage, which also divides, is Highway code that is correctly rounded operation by operation, but a converted or divided sample can sit on a rounding boundary, where a compiler that fuses the scale with the dither (GCC may on Highway's FMA targets; MSVC does not) rounds differently |
| Lossless, with a PQ or HLG side whatever the primaries, or between sRGB and linear with the same primaries (a PQ or HLG sky or `SRGB` texture, a sky in sRGB, premultiplied or not, a linear-sRGB file as `SRGB`) | not guaranteed | `JxlCmsInit` (`jxl_cms.cc`) applies these transfer functions itself (`ExtraTF`, and HLG's OOTF) in code dispatched per CPU (`HWY_DYNAMIC_DISPATCH`), before skcms or instead of it, whether libjxl's pipeline calls the CMS or the engine's `finish_in_engine` does |
| Lossy (VarDCT, XYB) | not guaranteed | the inverse transforms and the XYB stage are Highway kernels |

`JxlCmsInit` reads an ICC profile as the encoding it recognises in it, so an ICC-tagged file falls in the row of that
encoding. Highway picks its code per CPU when it runs, and its `MulAdd` is a fused multiply-add on AVX2 and a separate
multiply and add on SSE4, which round differently. So a decode in the last two rows may come out one rounding step
apart on another CPU, which can change a cooked texture or, through an HDR sky, the baked `.cenv`.
Byte-identical output across machines is therefore not guaranteed for these paths. Nothing tests a decode
under two Highway targets: every test runs on one CPU.

<a id="92-binary-cost"></a>

### Binary cost

libjxl is linked in every configuration, Distribution included: `assets_subsystem` builds the asset database and
runs the importers whenever a project is not cooked, and the runtime runs such projects, so leaving the decoder out
of Distribution would make a `.jxl` source import in Development and fail in Distribution. Measured with MSVC
`/O2` (CMake's Release flags, which Distribution derives from) on probe DLLs that call the same libjxl entry points
as the codec: the decoder with skcms adds about 1.4 MB (1,453,568 B against an empty DLL of 9,728 B), decoder and
encoder about 3.7 MB (3,782,656 B). `conjure_shared` exports `encode_jpeg_xl`, so the whole 3.7 MB ships. Leaving
the encoder out of Distribution would save about 2.3 MB but would also leave the Distribution unit suite without
the encoder its JPEG XL fixtures are written with.

<a id="93-resource-limits-and-hdr-transfer"></a>

### Resource limits and HDR transfer

A header can announce far more work than its file holds, and libjxl checks frame sizes only for overflow. The
decoder therefore bounds what a file may ask for, with the constants in `jpeg_xl_codec.h`:

| Limit | Where | Why |
|---|---|---|
| Image `xsize` and `ysize` at most `MAX_DIMENSION` | `JXL_DEC_BASIC_INFO`, before any frame | the output's own size |
| `num_extra_channels` at most `JPEG_XL_MAX_EXTRA_CHANNELS` (16) | `JXL_DEC_BASIC_INFO` | the field allows 4096, each a plane of the frame's size (`DecodeGlobalInfo` creates one per channel), while only alpha is output |
| Every layer at most `MAX_DIMENSION` | `read_headers`: a second decoder with coalescing off and only `BASIC_INFO`, `COLOR_ENCODING` and `FRAME` subscribed, so each layer reports its upsampled size in `JxlDecoderGetFrameHeader` and no frame state is built (`InitFrameOutput` runs only for `FULL_IMAGE`) | a frame's `frame_size` reaches about 2^30 per side whatever the image size; with coalescing on, `layer_info` reports the image's size instead |
| libjxl's live bytes at most 32 MiB + 8 MiB x `JPEG_XL_MAX_THREADS` (16) + 64 B x `xsize` x `ysize` x (colour + extra channels) | `jpeg_xl_memory_budget`, set from the basic info (160 MiB before it), the same inline and on any number of workers | bounds what no header check sees: reference-only and DC frames (never reported as `FRAME`), the frame state `InitFrameOutput` allocates before `FRAME` is returned, an ICC profile (libjxl caps it at 256 MiB and grows its buffers only as its data arrives) |

Measured with the budget's counter in a Debug build: under 5 MB for small images on one thread, 6 to 12 MB for
512 x 512 RGBA files on nine threads, and 60 MB for a lossless 1024 x 1024 RGBA file (effort 1) on nine threads,
about 14 bytes per sample plus 5 MB per thread that has a group; reference frames of an animation add up to four
4-byte planes per sample. `run_jpeg_xl_on_jobs` never hands libjxl more threads than values or than 16, so a small
image never gets many threads' storage. The thread term counts 16 threads even inline: it used to count the threads at
hand, so a 16 x 16 image whose frame state needs about 94 MB was refused inline (40 MiB) and read on with eight
workers (104 MiB). Now a decode on workers only adds the few MB of each thread that has work, so only a file needing
within that of its limit, far more state than its image size explains, can decode inline and fail on workers
(`test_jpeg_xl.cpp` decodes a 16384^2 frame and a 32768^2 reference-only frame of a 16 x 16 image inline and on 1, 2
and 8 workers). Box sizes allocate nothing: no box or JPEG reconstruction event is subscribed, and a
codestream split across `jxlp` boxes is copied only as far as the input reaches. A refusal is `INVALID_ARGUMENT`:
the texel and channel limits name themselves, and the budget names its limit in MiB when `exceeded` is set after
`JXL_DEC_ERROR`.

PQ and HLG are display-referred. libjxl outputs them with 1.0 at the original intensity target (`TF_PQ(intensity
target)` in the CMS and the to-linear stage, the XYB stage's scale for lossy files; its encoder defaults the target
to 10000 nits for PQ and 1000 for HLG) and tone-maps only when a desired intensity target differs, which the decoder
never sets. HLG goes through libjxl's OOTF for a display of the intensity target. The engine multiplies the linear
result by `intensity_target / JPEG_XL_REFERENCE_WHITE_NITS` (203, BT.2408's reference white for PQ and HLG), so
203 nits is 1.0 like a float file's diffuse white, whatever target the exporter wrote. The HDR path keeps values
above 1; `SRGB` clips at 1 and applies the sRGB curve, so reference white is white. libjxl's own tone mapper is not
used for `SRGB`: its BT.2408 curve, aimed at an SDR display, compresses everything from a few tens of nits up and
leaves reference white near half intensity, and it runs before the output stage, where it could not follow the
engine's division by alpha. `LINEAR` keeps the signal as stored. `equirect_image::decode_hdr` then reads NaN and
negative samples as 0 and anything above `MAX_ENVIRONMENT_RADIANCE` (about 5.2e33, +inf included) as that bound, which
keeps the bake's float sums finite; brighter finite radiance than a half holds reaches the bake unclipped, and only the
cooked cubemaps clamp to 65504 (`equirect_image::sample` reads texels the same way).

## Related

- [Building and Configuration](../../development/building-and-configuration.md#8-third-party-dependencies): the rules every pinned dependency follows
- [Testing and Benchmarks](../../development/testing-and-benchmarks.md#10-labels): the `build` and `third_party` labels
- [Repository Tour](../../getting-started/repository-tour.md): where `third_party/` and `_deps` sit
- `third_party/jxl.cmake`, `third_party/jxl_checks.cmake`, `tests/third_party/CMakeLists.txt`

<a id="contents"></a>
<!-- doc-content:body:end -->
