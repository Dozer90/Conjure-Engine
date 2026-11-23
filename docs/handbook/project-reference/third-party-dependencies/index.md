# Third-Party Dependencies
<!-- doc-template:chapter -->
[Handbook](../../index.md) → [Project Reference](../index.md)

<!-- doc-content:purpose:start -->
Dependency maintenance, version pins, licences, and the notices distributed with Conjure.
<!-- doc-content:purpose:end -->

<!-- doc-content:body:start -->
## Dependency Guides

- [JPEG XL](jpeg-xl.md)

<a id="11-licence-and-third-party-notices"></a>

## Licence and third-party notices

Conjure is licensed under the Apache License, Version 2.0. Four files carry
that and the licences of the software it builds on:

| File | Content | Written by |
|---|---|---|
| `LICENSE`, `NOTICE` | the Apache licence text, unchanged; the copyright holders, "the Conjure contributors" | tracked, not generated |
| `THIRD_PARTY_NOTICES.md` | every third-party dependency of every platform with its pin and licence, and the licence text of each pinned one | `tools/third_party_notices.py`, tracked |
| `build/<tree>/bin/THIRD_PARTY_NOTICES.txt` | `NOTICE`, `LICENSE` and the licence text of what this tree links into `conjure_shared` and `conjure_runtime` | the configure writes it, building `conjure_runtime` places it; an export that copies the runtime copies it into the game |

A dependency is registered once, beside its pin in `third_party/CMakeLists.txt`, with
`conjure_third_party_notice(<id> NAME... VERSION <the GIT_TAG> LICENSE_ID... SCOPE RUNTIME|EDITOR|BUILD FETCH <name>
LICENSE_FILES <files in the fetched source>)` (`cmake/ConjureNotices.cmake` documents every argument). The
configure fails when `VERSION` is not the `GIT_TAG`, when a licence file of software the tree uses is missing, and
when a `FetchContent_Declare` of that file has no notice naming it as `FETCH`. Software that is not fetched (a file
tracked under someone else's licence, code that follows a published implementation), and code a fetched source
embeds under a copyright of its own (jsmn in `cgltf.h`), keep their licence text in `third_party/licenses/`; a
notice with `FETCH` names such a file with an absolute path, since its relative ones are read in the fetched source.
libjxl and the three libraries it builds are pinned by archive SHA-256 in `third_party/jxl.cmake`, which
`third_party/CMakeLists.txt` includes; neither the `GIT_TAG` comparison nor the declaration scan reaches that file,
and its own probes check its four notices ([JPEG XL](jpeg-xl.md#7-licences)).

After changing a pin or a notice, regenerate the tracked document and commit it with the change:

```bash
python tools/third_party_notices.py --build-dir build/<tree>            # write THIRD_PARTY_NOTICES.md
python tools/third_party_notices.py --build-dir build/<tree> --check    # what the CTest entry notices.up_to_date runs
```

The tool rewrites the sections of the software the tree uses and keeps the others, so any tree can bring its own
sections up to date; it stops when a kept section was written for another pin or licence than the tree registers,
and then a tree that fetches that software has to run it. A Windows tree
configured with `-DCONJURE_RHI_VULKAN=ON` (`python tools/build.py --build-dir build/<name> --configure-only --
-DCONJURE_RHI_VULKAN=ON`) fetches every pinned dependency at once, the Direct3D 12 headers included, which no
Linux or macOS tree has. `notices.tool` is the self-test of the tool.

> [!NOTE]
>.NET is found on the build machine instead of pinned, so its licence text is in no tracked file: each build
> copies `LICENSE.txt` and `ThirdPartyNotices.txt` of the .NET installation it links into its
> `THIRD_PARTY_NOTICES.txt`, and the configure warns when it finds neither.
<!-- doc-content:body:end -->
