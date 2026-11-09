# Glossary

📘 **Public API wiki**

Every term the wiki uses without explaining it in place, one line each, with the page that explains it. Terms are
alphabetical; engine type names are in code spans.

| Term | Meaning | Where |
|---|---|---|
| ABI signature / ABI check | The record a module's `conjure_module_describe()` returns (compiler, standard library, configuration, architecture, runtime flags, engine version, header hash); `compare_abi` must match the host field for field or the load fails with `ABI_MISMATCH` | [Loading and managing modules](modules/loading-and-managing.md) |
| Alignment | The address multiple a block of memory must start at; a power of two. Allocators take it as a parameter, `align_up()` rounds sizes to it | [Memory](core/memory.md) |
| Archetype | The set of component types an entity carries; every entity belongs to exactly one, and entities of one archetype are stored together in chunks | [Entities, components and systems](ecs/index.md), [Archives and serialisation](ecs/archives-and-serialization.md) |
| Arena | A bump allocator: allocations are cheap and never freed one by one; `reset()` recycles everything at once | [Memory](core/memory.md) |
| Assertion | A check for a programmer error that can never fail in a correct program (`CONJURE_ASSERT`); compiled out of Distribution builds | [Logging and assertions](core/logging-and-assertions.md) |
| Assert handler | The function called when an assertion fails; returns `BREAK`, `CONTINUE` or `ABORT` | [Logging and assertions](core/logging-and-assertions.md) |
| Asset / `asset_id` | One unit of content identified by a `uuid` that stays stable when the file moves or is renamed | [Assets](assets/index.md) |
| Asset path | `mount://relative/path#sub`: a human-readable way to name an asset, with the mounts `assets://` (the project) and `engine://` (built-in content) | [Assets](assets/index.md) |
| `asset_ref<T>` / `asset_handle` | A non-owning 24-byte reference you store in components, and the owning strong reference that keeps an asset loaded | [Referencing and loading assets](assets/referencing-and-loading.md) |
| Attenuation model | How a 3D voice gets quieter with distance: `INVERSE`, `LINEAR`, `EXPONENTIAL` or `NONE` | [Playing sounds](audio/playing-sounds.md#3d-sound-and-the-listener) |
| Backend (audio) | The driver behind `audio_engine`: miniaudio for a real device, the null backend for tests and servers | [Clips and decoding](audio/clips-and-decoding.md#backends) |
| Backend (graphics) | The graphics API behind `device`: D3D12, Vulkan or the null device | [Devices and swapchains](rhi/devices-and-swapchains.md) |
| Backend (platform) | `backend_kind`: GLFW for a real window, null for a virtual one | [Windows and events](platform/windows-and-events.md), [Platform](platform/index.md) |
| Backward-shift deletion | How `flat_hash_map` erases without tombstones: later elements of the same probe run move one slot back | [Containers](core/containers.md) |
| Binary triplet | The directory a module's native library lives in: `bin/<platform>-<arch>-<config>` | [Writing a module](modules/writing-a-module.md) |
| Build configuration | One of Debug, Development or Distribution; decides whether asserts, trace logging and developer tools exist | [Data types](core/data-types.md) |
| Cache (assets) | `.conjure/cache/<platform>/blobs`: where the editor keeps cooked blobs, validated by content hash | [Cooking, caches and packs](assets/cooking-and-packs.md) |
| Category (error) | Which system an `error` came from, named like `"core"` or `"inventory"`; bound to an enumeration with `CONJURE_DECLARE_ERROR_CATEGORY` | [Errors and results](core/errors-and-results.md) |
| Category (log) | A named logging channel with its own minimum level, such as `LOG_CORE` | [Logging and assertions](core/logging-and-assertions.md) |
| Change detection / `changed<T>` | A query filter that visits only the chunks whose `T` column was written since a version you remember; chunk-granular, never per entity | [Queries, views and systems](ecs/queries-views-and-systems.md) |
| Chunk | A fixed 16 KiB block holding the entities of one archetype as one column per component; the unit of iteration and of parallel work | [Entities, components and systems](ecs/index.md), [Archives and serialisation](ecs/archives-and-serialization.md) |
| Clip | The samples of one sound, held in memory (`PCM`) or decoded while playing (`STREAMED`) | [Clips and decoding](audio/clips-and-decoding.md) |
| Closure (assets) | The set of assets a pack ships: the requested roots plus everything they depend on | [Cooking, caches and packs](assets/cooking-and-packs.md) |
| Coalesced resize | One `framebuffer_resized_event` per pump, the last size of the batch, however many times the window changed | [Windows and events](platform/windows-and-events.md) |
| Command buffer / flush | Structural changes (create, destroy, add, remove) recorded while the world is locked and applied together at `flush` | [Command buffers and observers](ecs/command-buffers-and-observers.md) |
| Component / Tag component | A plain struct of data attached to an entity, declared with `CONJURE_COMPONENT`; a tag is an empty component that only marks an entity | [Entities and components](ecs/entities-and-components.md) |
| Component id / component mask | The dense id a component type gets when registered, and the bit set of ids that describes an archetype or a query | [Entities and components](ecs/entities-and-components.md) |
| Consumer (modules) | A `module_consumer`: the subsystem that validates and applies the records a module registers under its extension point | [Loading and managing modules](modules/loading-and-managing.md) |
| Cooked blob | The `.cab` output of an importer: an 80-byte header plus the payload the loader reads | [Cooking, caches and packs](assets/cooking-and-packs.md) |
| Cumulative counters | The allocation counters only ever grow; live allocations are `allocationCount - freeCount` | [Memory](core/memory.md) |
| Cursor mode | `NORMAL`, `HIDDEN`, `DISABLED` (hidden and unbounded, for camera look) or `CAPTURED` (visible but confined to the window) | [Windows and events](platform/windows-and-events.md) |
| Data-oriented design (DOD) | Laying data out for how it is processed (dense arrays, bit masks, arenas) rather than around objects; the engine's internals work this way and every wiki page shows the simple facade first and the data-oriented escape hatch second | [Containers](core/containers.md), [Memory](core/memory.md), [Entities, components and systems](ecs/index.md) |
| Dense storage | Elements packed contiguously with no holes, as in `slot_map`, so iteration is cache friendly | [Containers](core/containers.md) |
| Device loss | The GPU or audio device went away (driver reset, unplugged headphones); the engine reports it and recreates what it can | [Devices and swapchains](rhi/devices-and-swapchains.md#device-loss), [Playing sounds](audio/playing-sounds.md) |
| Distribution build | The shipping configuration: no asserts, no editor, no developer tooling; `CONJURE_DISTRIBUTION` is 1 | [Data types](core/data-types.md) |
| Doppler factor | How much a moving voice's pitch shifts relative to the listener; 0 turns the effect off | [Playing sounds](audio/playing-sounds.md) |
| DXIL / SPIR-V | The compiled shader formats for D3D12 and Vulkan; one blob of each per permutation, listed in `index.json` | [Shaders](rhi/shaders.md#the-index-and-the-blob-columns) |
| Edge / level (input) | *Pressed* and *released* are edges (true for one frame); *down* and *held* are levels (true while the button stays down) | [Input](platform/input.md) |
| Entity | A small id (index and generation) that components are attached to; it carries no data of its own | [Entities and components](ecs/entities-and-components.md) |
| Entity archive / prefab | A typed in-memory copy of a group of entities that can be instantiated into a world again and serialised to JSON | [Archives and serialisation](ecs/archives-and-serialization.md) |
| `error` | A recoverable failure: category, code and message; carried by `result<T>` instead of being thrown | [Errors and results](core/errors-and-results.md) |
| `error_code` | The generic error enumeration any system may use (`NOT_FOUND`, `INVALID_ARGUMENT`, `IO_FAILURE`, ...) | [Errors and results](core/errors-and-results.md) |
| Extension point / descriptor | A `string_id` name (`EXTENSION_POINT`) under which a module registers plain data records with `register_extension`; the consuming subsystem defines the record type | [Writing a module](modules/writing-a-module.md) |
| Extension record (assets) | `importer_extension_desc` / `asset_type_extension_desc`: how a plugin module offers importers and asset types | [Cooking, caches and packs](assets/cooking-and-packs.md) |
| Fallback (assets) | The placeholder payload of an asset type, returned while an asset is loading or after it failed | [Referencing and loading assets](assets/referencing-and-loading.md) |
| Flag enumeration | An `enum class` whose values are single bits, combined with `\|` after `CONJURE_ENUM_FLAGS(type);` and tested with `has_flag()` | [Data types](core/data-types.md) |
| FNV-1a | The hash function behind `string_id`, error categories and asset identity (`hash::fnv1a64`, `hash::fnv1a32`) | [Strings and identifiers](core/strings-and-identifiers.md), [Assets](assets/index.md) |
| Frame allocator | Per-frame scratch memory usable from every thread; everything it hands out dies at the next `reset()` | [Memory](core/memory.md) |
| Generation | The counter stored in a slot and in every `slot_key` issued for it; bumped on erase so stale keys stop matching | [Containers](core/containers.md) |
| Handle | A small value (a `slot_key`, a `uuid`, a `string_id`) that refers to something without being a pointer to it | [Containers](core/containers.md), [Data types](core/data-types.md) |
| Headless (graphics) | A device with no window (`native_window` kind `NONE`): rendering to textures only, as tests and servers do | [Devices and swapchains](rhi/devices-and-swapchains.md#windows-and-headless-mode) |
| High-water mark | The largest value a statistic has reached, such as `frame_allocator::high_water_bytes()`; used to size budgets | [Memory](core/memory.md) |
| Hot reload | Re-importing a changed source file while the editor runs and swapping the new payload into every reference | [Database, importers and hot reload](assets/database-and-import.md) |
| Importer | Turns source bytes into a cooked payload, deterministically, on a worker thread | [Database, importers and hot reload](assets/database-and-import.md) |
| Interning | Recording the text of a `string_id` in the process-wide table so the hash can be shown as a name again | [Strings and identifiers](core/strings-and-identifiers.md) |
| Iteration lock / `iteration_scope` | While a world is locked for iteration, direct structural changes are refused (`STRUCTURE_LOCKED`); use a command buffer | [Command buffers and observers](ecs/command-buffers-and-observers.md) |
| Level (log) | `TRACE`, `DEBUG`, `INFO`, `WARN`, `ERR`, `FATAL` or `OFF`; a message is delivered when it is at or above both the category and global level | [Logging and assertions](core/logging-and-assertions.md) |
| Listener | The one point in space 3D voices are heard from: position, orientation and velocity | [Playing sounds](audio/playing-sounds.md#3d-sound-and-the-listener) |
| Load factor | `size() / capacity()` of a hash table; `flat_hash_map` rehashes when an insertion would push it above 0.875 | [Containers](core/containers.md) |
| Loader / `load_cpu` / finalise | The two stages that turn a cooked blob into a payload: decoding on a worker, then finalising on the main thread | [Database, importers and hot reload](assets/database-and-import.md) |
| Logical size / framebuffer size | A window's size in screen points versus in pixels; they differ by the content scale on high-DPI displays | [Windows and events](platform/windows-and-events.md) |
| Main thread (platform) | Every platform call happens on the main thread; `snapshot()` of the input is the one exception | [Platform](platform/index.md), [Jobs and threading](core/jobs.md) |
| Manifest (`module.json`) | The file that names a module, its version, engine range, dependencies, binary and assemblies | [Writing a module](modules/writing-a-module.md) |
| `.meta` sidecar | The file next to a source asset that holds its uuid, type and import settings | [Assets](assets/index.md), [Database, importers and hot reload](assets/database-and-import.md) |
| Mix group / effective gain | A named bus voices play through; a voice's effective gain is its own gain times every group on the path to the master | [Playing sounds](audio/playing-sounds.md#mix-groups) |
| Mixer block | The fixed number of frames the mixer renders per callback step (`MIX_BLOCK_FRAMES`, 256) | [Playing sounds](audio/playing-sounds.md) |
| Modifiers (input) | `key_modifier_flags`: Shift, Control, Alt and Super state carried with key and mouse events | [Input](platform/input.md) |
| Module | Two meanings: an engine module is a library built into the engine (core, ecs, renderer, ...); a plugin or game module is a package (`module.json`, native library, C# assemblies, content) the engine loads at start-up | [Modules and plugins](modules/index.md) |
| `module_id` / stale id | The handle a loaded module gets; after an unload the id stops matching, like a stale `slot_key` | [Loading and managing modules](modules/loading-and-managing.md) |
| Null backend (audio) | A backend with no device and a virtual clock, driven by `update()` and `end_frame()`; what tests and servers use | [Clips and decoding](audio/clips-and-decoding.md#backends) |
| Null backend (platform) | A virtual window driven by `push_event` / `simulate_*`, so window and input code runs in tests | [Windows and events](platform/windows-and-events.md) |
| Null device (graphics) | A device that records calls in `null_device_stats` instead of talking to a GPU | [Devices and swapchains](rhi/devices-and-swapchains.md#testing-without-a-gpu) |
| Null id / null key | The all-zero `string_id`, `uuid` or `slot_key`, meaning "none"; test with `is_null()` or `is_valid()` | [Strings and identifiers](core/strings-and-identifiers.md), [Containers](core/containers.md) |
| Observer | A callback that runs when a component is added to or removed from an entity, including through a command buffer flush | [Command buffers and observers](ecs/command-buffers-and-observers.md) |
| One-shot / retained voice | A voice that frees itself when the clip ends versus one you keep a `voice_id` for and stop or change yourself | [Playing sounds](audio/playing-sounds.md) |
| Owner thread (ECS) | The thread a world belongs to; structural changes are allowed only there and only outside iteration | [Command buffers and observers](ecs/command-buffers-and-observers.md) |
| Pack / `content.cpak` / `manifest.json` | The exported game's content: one pack file with its manifest, read by the runtime instead of the cache | [Cooking, caches and packs](assets/cooking-and-packs.md) |
| Payload / publish | The loaded object behind an asset and the atomic pointer swap that makes a new one visible to every reference | [Referencing and loading assets](assets/referencing-and-loading.md) |
| Phase / stage | A phase is one step of the frame (`FRAME_BEGIN` ... `FRAME_END`); within a phase the scheduler packs systems that do not conflict into stages that run in parallel | [Queries, views and systems](ecs/queries-views-and-systems.md) |
| Pool allocator | Fixed-size blocks carved from slabs, freed individually; for many objects of one type | [Memory](core/memory.md) |
| Priming | `voice_state::PRIMING`: a voice that has started but is waiting for its first streamed chunk or for a stolen slot to fade out | [Playing sounds](audio/playing-sounds.md) |
| Probe run | In an open-addressing hash table, the consecutive occupied slots a lookup walks from a key's home bucket | [Containers](core/containers.md) |
| Push constants | A few bytes of per-draw data handed to a shader without a buffer; `CONJURE_PUSH_CONSTANTS` declares them once for root constants on D3D12 and a push-constant block on Vulkan | [Shaders](rhi/shaders.md) |
| Radix sort | A stable, allocation-free sort for unsigned integer keys with optional payload indices (`radix_sort_u64`) | [Strings and identifiers](core/strings-and-identifiers.md) |
| Reload scope | `FULL` closes and reopens the binaries of a module and its loaded dependents; `MANAGED_ONLY` involves only the consumers that take part in managed reloads (the scripting side) | [Loading and managing modules](modules/loading-and-managing.md) |
| Render thread | There is none in v1: every RHI call happens on the main thread | [Graphics device layer (RHI)](rhi/index.md) |
| `result<T>` | Either a value or an `error`; the engine's replacement for exceptions. Check it before reading the value | [Errors and results](core/errors-and-results.md) |
| Retire / `RETIRE_FRAMES` | A replaced payload is kept for `RETIRE_FRAMES` (2) frames before it is destroyed, so pointers read this frame do not dangle | [Referencing and loading assets](assets/referencing-and-loading.md) |
| RHI | The render hardware interface: the thin layer (device, swapchain, shader library) between the engine and the graphics API | [Graphics device layer (RHI)](rhi/index.md) |
| Scratch memory | Memory needed only briefly (one frame, one load) and released all at once; arenas and the frame allocator provide it | [Memory](core/memory.md) |
| Service (modules) | A typed object a module publishes with `CONJURE_MODULE_SERVICE` and others look up with `try_service` | [Writing a module](modules/writing-a-module.md) |
| Shader key / permutation | A `(name, entry, defines)` triple names one compiled shader; a define axis in `conjure_add_shaders()` produces one permutation per combination | [Shaders](rhi/shaders.md) |
| Shadow copy | The copy of a module's library the host loads so the original can be rebuilt and reloaded while the game runs | [Loading and managing modules](modules/loading-and-managing.md) |
| Side file | A file an importer reads next to the source; hashed and watched like the source, so a change to it re-imports the owner | [Database, importers and hot reload](assets/database-and-import.md) |
| Sink | A destination for log messages: console, file or a callback of your own | [Logging and assertions](core/logging-and-assertions.md) |
| Slab | One heap block of a `pool_allocator` holding `blocksPerSlab` blocks and their free bitmap | [Memory](core/memory.md) |
| Slot (assets) | `asset_slot`: the one record per uuid every reference points at; never freed, so references stay valid | [Referencing and loading assets](assets/referencing-and-loading.md) |
| `slot_key` / `slot_map` | A generational handle and the dense container it addresses; stale keys are detected, not dereferenced | [Containers](core/containers.md) |
| Snapshot (input) | The `input_state` published by `end_frame()`: what the game queries, identical for the whole frame | [Input](platform/input.md) |
| Software adapter / WARP | A CPU implementation of the graphics API (`preferSoftwareAdapter`), used to run GPU tests on machines without one | [Devices and swapchains](rhi/devices-and-swapchains.md) |
| Stable storage | Storage whose element addresses never change while the element exists, as in `stable_vector` | [Containers](core/containers.md) |
| Stale (assets) | A record whose source content hash no longer matches its cooked blob; re-imported on the next scan | [Database, importers and hot reload](assets/database-and-import.md) |
| Static module | A module compiled into the host and declared with `CONJURE_DECLARE_STATIC_MODULE`, or one with a manifest and no binary | [Writing a module](modules/writing-a-module.md) |
| `status` | `result<void>`: success or an `error`, no value | [Errors and results](core/errors-and-results.md) |
| `string_id` | A name reduced to its 64-bit FNV-1a hash; compared by hash, printed by name once interned | [Strings and identifiers](core/strings-and-identifiers.md) |
| Structural change | Creating or destroying an entity, or adding or removing a component: anything that moves an entity between archetypes | [Command buffers and observers](ecs/command-buffers-and-observers.md) |
| Sub-asset / sub path | A named part of one source file (`mesh/Body`), addressed with `#` in an asset path | [Assets](assets/index.md) |
| Swapchain / back buffer | The images a window shows; you render into the current back buffer and `present()` it | [Devices and swapchains](rhi/devices-and-swapchains.md) |
| System | A function the scheduler runs once per frame over every entity matching its query, in a declared phase | [Queries, views and systems](ecs/queries-views-and-systems.md) |
| Terminal state (assets) | `LOADED` or `FAILED`: a load that will not change again until something reloads it | [Referencing and loading assets](assets/referencing-and-loading.md) |
| Tracked allocation | A heap block obtained through `tracked_allocate`, counted in the process-wide counters and released with `tracked_free` | [Memory](core/memory.md) |
| Transparent comparator | A comparator such as `std::less<>` that lets a `flat_map<std::string, V>` be searched with a `std::string_view` | [Containers](core/containers.md) |
| Trivially destructible | A type whose destructor does nothing, which is what arenas and the frame allocator require of the objects they hold | [Memory](core/memory.md) |
| Unload policy | `REFUSE_IF_IN_USE` fails an unload while something depends on the module; `REMOVE_INSTANCES` strips what it registered | [Loading and managing modules](modules/loading-and-managing.md) |
| `unresolved_components` | Component names in an archive or JSON that no registered type matches; reported, not silently dropped | [Archives and serialisation](ecs/archives-and-serialization.md) |
| `uuid` | A 128-bit identifier that stays stable across sessions and machines; used for assets and prefabs | [Data types](core/data-types.md), [Assets](assets/index.md) |
| Verify | `CONJURE_VERIFY`: an assertion whose expression still runs in Distribution builds; for checked calls with side effects | [Logging and assertions](core/logging-and-assertions.md) |
| Version (ECS) | The world's monotonic counter stamped on a chunk column when it is written; what `changed<T>` compares against | [Queries, views and systems](ecs/queries-views-and-systems.md) |
| Voice | One playing instance of a clip, addressed by a `voice_id` | [Playing sounds](audio/playing-sounds.md) |
| Voice stealing / priority | When every voice is busy, the lowest-priority voice is faded out and reused for the new sound | [Playing sounds](audio/playing-sounds.md) |
| Wave | A stage of the engine's development plan; wave 1 is core, wave 2 adds ECS, platform, RHI, assets, audio and modules, later waves add rendering, physics, scripting and the editor | [Public API wiki](index.md) |
| Windowed fullscreen | A borderless window covering the monitor at the desktop resolution, switched without a mode change | [Windows and events](platform/windows-and-events.md) |
| `world_hash` | A deterministic hash of a whole world, used to prove two runs produced the same state | [Command buffers and observers](ecs/command-buffers-and-observers.md) |
