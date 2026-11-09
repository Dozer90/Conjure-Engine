# Public API wiki

📘 **Public API wiki** · 🛠 Handbook: [Engineering handbook](../handbook/index.md)

Conjure is a data-oriented 2D/3D game engine written in C++20 with a C# scripting layer, an editor that AI agents
can drive and a standalone runtime for shipping. This wiki is for the people who build games with it: designers,
gameplay programmers, C# scripters and agents driving the editor. It explains what the engine offers and how to use
it; it does not explain how the engine is built inside (that is the 🛠 Engineering handbook, which every page links
to where a sibling exists).

## What Conjure offers

| Layer | What you get | Where it is documented |
|---|---|---|
| Core | numbers, errors, logging, math, containers, strings, memory, time, jobs, JSON, files, reflection | [Core pages](#core-pages) |
| Entities and components | archetype-based ECS with change detection, command buffers and system scheduling | [ECS pages](#ecs-pages) |
| Platform | the game window, per-frame events and the input snapshot | [Platform pages](#platform-pages) |
| Graphics device | the device, swapchain and compiled shaders the renderer builds on | [RHI pages](#rhi-pages) |
| Assets | stable asset references, loading, importers, hot reload, cooking and packs | [Assets pages](#assets-pages) |
| Audio | clips, voices, mix groups, 3D listener, backends | [Audio pages](#audio-pages) |
| Modules | writing a plugin or game module and loading modules into a host | [Modules pages](#modules-pages) |
| Rendering, physics, scene | the renderer with 2D and 3D pipelines, rigid bodies and colliders, scenes and prefabs | wave 3 pages |
| Scripting | the C# API as you see it from a script (`Entity`, `Transform`, `Input`, ...) | wave 4 pages |
| Editor and AI protocol | the editor, and the command protocol agents use to drive it | wave 5 pages |

Pages for later waves appear here as their modules land; the core and wave-2 pages below are complete.

## How the pages are organised

Pages are organised by **what you want to do**, not by header. One page covers one user-facing topic and may span
several headers; small things are grouped (the *Data types* page covers `u8` through `f64`, flag enumerations and
`uuid` together). Every page follows the same shape, so you can skim:

1. a one-paragraph purpose and the header(s) to include,
2. an **At a glance** table (*You want to...* / *Use*),
3. a **Quick example** that compiles as written,
4. concept sections with coloured callouts,
5. an **API summary** of short signatures, and **See also** links.

The coloured callouts mean the same thing on every page: the legend is on the
[landing page](../index.md#callout-boxes-used-throughout). In short, a TIP points at the better function, a
WARNING at a gotcha and a CAUTION at undefined behaviour or data loss.

Signatures in the API summaries are shortened by hand; the header is always the source of truth and every page names
the headers it covers by include path, for example `conjure/core/result.h`.

## Reading order

If you are new to the engine, read the core pages in this order; each builds on the one before:

1. [Data types](core/data-types.md): the number aliases and flag enumerations every signature uses.
2. [Errors and results](core/errors-and-results.md): how every fallible call reports failure (the engine never throws).
3. [Logging and assertions](core/logging-and-assertions.md): how to print, filter and catch programmer errors.
4. [Strings and identifiers](core/strings-and-identifiers.md): hashed names (`string_id`) and the string helpers.
5. [Containers](core/containers.md): handles that detect stale use, inline vectors, flat maps.
6. [Memory](core/memory.md): only when you allocate yourself; arenas, pools and per-frame scratch memory.

Everything else can be read on demand.

## Core pages

| Page | Covers |
|---|---|
| [Data types](core/data-types.md) | `u8`..`u64`, `f32`, `usize`, `byte`, `KIB`/`MIB`/`GIB`, flag enumerations, `non_copyable`, `uuid`, platform and build macros |
| [Errors and results](core/errors-and-results.md) | `result<T>`, `status`, `error`, `error_code`, `make_error`, `CONJURE_TRY`, `CONJURE_TRY_ASSIGN`, error categories |
| [Logging and assertions](core/logging-and-assertions.md) | log categories and levels, sinks, `CONJURE_LOG_*`, `CONJURE_ASSERT`, `CONJURE_VERIFY`, the assert handler |
| [Math](core/math.md) | vectors, matrices, quaternions, transforms, bounding volumes, rays, rectangles |
| [Containers](core/containers.md) | `slot_map`, `small_vector`, `stable_vector`, `flat_hash_map`, `flat_hash_set`, `flat_map` |
| [Strings and identifiers](core/strings-and-identifiers.md) | `string_id`, FNV-1a hashing, `str::` helpers, `bit_vector`, radix sort |
| [Memory](core/memory.md) | allocation counters, `tracked_allocate`, `arena`, `pool_allocator`, `frame_allocator` |
| [Time, events and random](core/time-and-events.md) | clocks and stopwatches, seeded random numbers, events, delegates, the event bus |
| [Jobs and threading](core/jobs.md) | the job system, `parallel_for`, the main thread, dedicated threads |
| [JSON](core/json.md) | parsing, reading and writing JSON documents |
| [Files and the virtual file system](core/files.md) | mounting file systems, reading and writing files, path helpers |
| [Processes, libraries and images](core/system.md) | spawning processes, loading shared libraries, decoding images |
| [Reflection](core/reflection.md) | describing your types so the editor, serialiser and scripts can see them |

## ECS pages

Start with the overview; it gives the mental model in five sentences and a table of the terms the other pages use.

| Page | Covers |
|---|---|
| [Entities, components and systems](ecs/index.md) | the mental model (entities, components, archetypes, systems, phases), which page to read for what, local terms table |
| [Entities and components](ecs/entities-and-components.md) | `CONJURE_COMPONENT`, the component registry, `world` create / destroy / add / get / remove, `entity_handle`, `component_type_desc` for module authors |
| [Queries, views and systems](ecs/queries-views-and-systems.md) | typed `view<...>` iteration, `query` and `chunk_view`, `changed<T>`, `system_desc` and `make_system`, phases and the scheduler |
| [Command buffers and observers](ecs/command-buffers-and-observers.md) | deferred structural changes from systems and jobs, `flush`, observers for added and removed components, the iteration lock, `world_hash` |
| [Archives and serialisation](ecs/archives-and-serialization.md) | `entity_archive` for prefabs, copy and paste and undo, JSON in and out, what archetypes and chunks are when you meet them in tools |

## Platform pages

| Page | Covers |
|---|---|
| [Platform: windows, input and events](platform/index.md) | what the module gives you, who owns the window and the input in a game, backends (GLFW and null), the error codes |
| [Windows and events](platform/windows-and-events.md) | `window_desc`, `create_window()`, pumping events, the event types, monitors, high-DPI sizes, fullscreen, cursor modes, the null backend's `push_event` / `simulate_*` |
| [Input](platform/input.md) | the `input` accumulator and the `input_state` snapshot, pressed / released versus down / held, modifiers, mouse and gamepads, injecting input in tests |

## RHI pages

| Page | Covers |
|---|---|
| [Graphics device layer (RHI)](rhi/index.md) | what the RHI is, who needs it and when to wait for the renderer instead, the render-thread rule |
| [Devices and swapchains](rhi/devices-and-swapchains.md) | `device_desc` and `device::create()`, backends, the null device and `null_device_stats`, `swapchain`, headless mode, device loss, texture formats |
| [Shaders](rhi/shaders.md) | `conjure_add_shaders()` at build time, `index.json`, `shader_library` and shader keys, permutations and define axes, DXIL and SPIR-V blobs |

## Assets pages

| Page | Covers |
|---|---|
| [Assets](assets/index.md) | how assets work from the outside: uuids and `.meta` sidecars, asset paths, sub-assets, the editor and runtime halves of the pipeline |
| [Referencing and loading assets](assets/referencing-and-loading.md) | `asset_ref<T>` and `asset_handle`, `asset_manager` load / flush / collect, batches, states, priorities, fallbacks, publish and retire |
| [Database, importers and hot reload](assets/database-and-import.md) | `asset_database`, `.meta` records, writing an importer and a loader, `asset_type_registry`, side files, the file watcher and hot reload |
| [Cooking, caches and packs](assets/cooking-and-packs.md) | the cooked blob format, the cache, `asset_source`, the cooker, `content.cpak` and its manifest, extension records for plugin modules |

## Audio pages

| Page | Covers |
|---|---|
| [Audio](audio/index.md) | the pieces and how they fit, the `audio_source` and `audio_listener` components and how the engine will sync them |
| [Playing sounds](audio/playing-sounds.md) | `audio_engine`, `play_params` and `voice_id`, one-shot versus retained voices, mix groups, the listener and 3D attenuation, priorities and stealing, devices and device loss |
| [Clips and decoding](audio/clips-and-decoding.md) | `audio_clip` (PCM and streamed), the `conjure.audio.clip` asset type and its import settings, decoders, the backend interface and the null backend |

## Modules pages

| Page | Covers |
|---|---|
| [Modules and plugins](modules/index.md) | what a module is (manifest, native library, C# assemblies, content), how the engine loads them, where to go next |
| [Writing a module](modules/writing-a-module.md) | `module.json`, the entry point, `module_registrar` and extension points, `module_context`, services, static modules |
| [Loading and managing modules](modules/loading-and-managing.md) | `module_manager` discover / load / unload / reload, `module_registry`, consumers, the ABI check, shadow copies, reload scopes, the export layout |

## Cross-cutting

- [Glossary](glossary.md): every term the wiki uses, one line each, with a link to the page that explains it.

> [!NOTE]
> Most core types live in `namespace conjure::core`. The number aliases (`u32`, `usize`, ...), the size constants
> (`KIB`, `MIB`, `GIB`, `CACHE_LINE_SIZE`) and the flag helpers (`has_flag`, `has_any_flag`, `with_flag`,
> `without_flag`, `to_underlying`) live directly in `namespace conjure` so every module can use them unqualified;
> memory, hashing and string helpers live in `conjure::core::memory`, `conjure::core::hash` and
> `conjure::core::str`. Each page states the namespace it uses. Examples on these pages open with
> `using namespace conjure;` and `using namespace conjure::core;` for brevity; in your own headers, qualify.
