# Referencing and loading assets

📘 **Public API wiki** · 🛠 Handbook: [Assets module](../../handbook/modules/assets.md)

How to refer to an asset from your data, keep it loaded while you use it and read it every frame. Headers:
`conjure/assets/asset_ref.h` (`asset_ref<T>`, `asset_handle`), `conjure/assets/asset_manager.h`
(`asset_manager`, batches, events), `conjure/assets/asset_slot.h` (what a reference points at) and
`conjure/assets/asset_types.h` (ids, states, priorities, flags). Everything is in `namespace conjure::assets`.

## At a glance

| You want to... | Use |
|---|---|
| Store a reference to an asset in a component | `asset_ref<T>` by value; it serialises as the uuid |
| Make a reference usable (point it at the manager's slot) | `manager.bind<T>(id)` |
| Read the asset | `ref.get()`: `T *`, or `nullptr` until it is loaded |
| Read it, or a placeholder while it is not ready | `manager.get_or_fallback(ref)` |
| Start loading and keep the asset resident | `asset_handle handle = manager.load(id)` |
| Load several things and wait for all of them | `manager.load_batch(ids)`, then `batch.is_complete()` |
| Load now, on the main thread, accepting a hitch | `manager.load_blocking(id)` |
| Know whether something changed since you last looked | compare `ref.version()` with the value you cached |
| Unload what nobody holds any more | `manager.collect()` |
| Be told when assets load, fail or reload | `manager.subscribe(callback)` |

## Quick example

A component holds an `asset_ref`; a system holds the `asset_handle` that keeps the payload resident. Copied
from the pattern in `tests/assets/test_binary_asset.cpp`.

```cpp
#include "conjure/assets/asset_manager.h"
#include "conjure/assets/binary_asset.h"

using namespace conjure;
using namespace conjure::assets;

struct loot_table_component {
    asset_ref<binary_asset> table = {};      // 24 bytes, trivially copyable: lives in the ECS chunk
};

class loot_system {
    private:
        asset_handle mTable = {};            // owning: keeps the payload resident across collect()
        usize mBytesSeen = 0;

    public:
        void start(asset_manager &manager, loot_table_component &component, const asset_id &tableId) {
            component.table = manager.bind<binary_asset>(tableId);   // binds, never loads
            mTable = manager.load(tableId);                            // UNLOADED -> QUEUED
        }

        void update(const loot_table_component &component) {
            const binary_asset *table = component.table.get();         // one atomic load, any thread
            if (table == nullptr) {
                return;                                                // still loading, or failed
            }
            mBytesSeen = table->bytes.size();
        }
};
```

## Two kinds of reference

`asset_ref<T>` is **non-owning**: a uuid plus a pointer to the asset's slot, 24 bytes, trivially copyable, no
reference count. Components store it by value and the engine copies, clones and serialises them by the thousand.
It never keeps anything loaded.

`asset_handle` is **owning**: every live handle is one strong reference on the slot, and `collect()` never
unloads a slot with a strong reference. It is type-erased, so scene managers and batches can hold handles of any
type; `handle.ref<T>()` and `handle.get<T>()` give typed access and log an ERROR once if `T` is the wrong type.

> [!TIP]
> Put `asset_ref<T>` in components and `asset_handle` in the system that decides residency (the scene manager
> holds a handle for everything a scene uses). If a payload disappears under a component, nobody owned it.

> [!WARNING]
> A `T *` from `get()` is valid until the end of the **next** frame. Publishing a new payload (first load or
> reload) retires the old one for `RETIRE_FRAMES` (2) frames, then destroys it. Work that spans frames holds the
> `asset_ref` and calls `get()` again; caches keyed by `(id, version)` compare `ref.version()`.

### `asset_ref<T>`

| Signature (short) | Purpose |
|---|---|
| `asset_ref()` / `explicit asset_ref(const asset_id &)` | unbound: carries the uuid, `get()` is `nullptr` |
| `asset_ref(const asset_id &, const asset_slot *)` | bound (what `bind<T>()` returns) |
| `const asset_id &id()` / `bool is_bound()` / `const asset_slot *slot()` | identity |
| `T *get()` | the payload while LOADED, else `nullptr`; one acquire load |
| `asset_state state()` | `UNLOADED` when unbound |
| `u32 version()` | changes on every publish and unload; 0 when unbound |
| `operator==` | by uuid, bound or not |

A reflected `asset_ref<T>` field writes its uuid as a string and reads back **unbound**; the scene module binds
every such field after load with `asset_manager::bind_slot()`. You never bind component fields by hand after
deserialisation. See 📘 API: [Reflection](../core/reflection.md).

### `asset_handle`

| Signature (short) | Purpose |
|---|---|
| `asset_handle()` / `explicit asset_handle(asset_slot *)` | unbound, or one strong reference on a slot |
| copy, move, destroy (`noexcept`) | adjust the strong count atomically; usable on any thread |
| `asset_id id()` / `asset_type_id type()` / `bool is_bound()` / `asset_slot *slot()` | identity |
| `asset_state state()` / `bool is_terminal()` / `u32 version()` | `is_terminal()` is true for LOADED, FAILED and unbound |
| `asset_ref<T> ref()` / `T *get()` | typed access; a type mismatch yields an unbound ref and logs once per (uuid, type) until an `asset_manager` is destroyed (the record is process-wide) |
| `void reset()` | release the reference |

## The manager

`asset_manager` owns every slot and payload and runs the load pipeline. Construct it once with an
`asset_manager_desc`: a `core::jobs::job_system *` (required; 0 workers runs loads inline, which tests use), the
primary `asset_source` (a cache in the editor, a pack at runtime), `maxJobsInFlight` (8), the per-`update()`
`finaliseBudget` (2 ms) and `blockingTimeout` (30 s). Register asset types through `manager.types()` before
loading. The engine ticks it for you: `begin_frame()`, `update()` and `end_frame()` once per frame.

```cpp
#include "conjure/assets/asset_manager.h"

using namespace conjure;
using namespace conjure::assets;

void tick(asset_manager &manager, u64 frameIndex) {
    manager.begin_frame(frameIndex);
    manager.update();      // dispatch queued loads, finalise under the budget, retire payloads, fire events
    // ... systems run here; pointers from get() stay valid until the end of the next frame ...
    manager.end_frame();
}
```

> [!WARNING]
> Every call that changes the manager (`bind`, `load`, `reload`, `collect`, `update`, `subscribe`, ...) is
> main-thread only. Safe from any thread: `asset_ref` copy, `get()`, `state()`, `version()` and `asset_handle`
> copy, move and destroy.

### The load state machine

```
UNLOADED --load()--> QUEUED --dispatch--> LOADING --completion--> FINALISING --finalise--> LOADED
                                                                        |                     |
                                                                      error                 reload()
                                                                        v                     v
                                                                     FAILED <--error-- RELOADING --ok--> LOADED
```

| From | Call | To |
|---|---|---|
| `UNLOADED` | `load(id, priority)` | `QUEUED`; the handle is returned at once |
| `QUEUED` | `load(id, HIGH)` again | stays queued, moves up to the higher priority |
| `LOADED`, `FAILED` | `load(id)` | nothing changes; you still get a handle |
| `FAILED` | `retry_failed(id)` | `QUEUED` |
| `LOADED` | `reload(id)` | `RELOADING`; the old payload stays published until the new one is ready |
| `RELOADING` | failure | back to `LOADED` with the old payload; event `RELOAD_FAILED` |
| `LOADED`, zero handles, not pinned | `collect()` | `UNLOADED`; event `UNLOADED` |

`LOADED` and `FAILED` are **terminal** (`is_terminal()`). A dependent is finalised only once every dependency it
lists is terminal, and dependencies are loaded for you: loading a model loads its materials and textures, in
that order, and the model's slot holds handles to them until it is unloaded. A failed dependency does not fail
the dependent; the dependent reads the type's fallback through its ref and a WARN is logged once.

### Loading several assets

```cpp
#include "conjure/assets/asset_manager.h"

#include <span>

using namespace conjure;
using namespace conjure::assets;

class scene_activation {
    private:
        asset_load_batch mBatch = {};

    public:
        void begin(asset_manager &manager, std::span<const asset_id> sceneAssets) {
            mBatch = manager.load_batch(sceneAssets, asset_load_priority::HIGH);
        }

        [[nodiscard]] bool ready() const noexcept {
            return mBatch.is_complete();           // every handle LOADED, RELOADING or FAILED; progress() is 0..1
        }

        [[nodiscard]] usize failures() const noexcept {
            return mBatch.size() - mBatch.loaded_count();
        }
};
```

`asset_load_batch` holds the handles (`handles()`, `size()`), so the assets stay resident for as long as the
batch lives; assign a fresh `asset_load_batch()` to release them. A member that hot-reloads keeps its previous
payload published, so a complete batch stays complete (and `loaded_count()` counts it) while it is `RELOADING`.

> [!TIP]
> Use `HIGH` for what the player sees next and `LOW` for prefetching. Within a level loads are first in, first
> out, and `flush()` runs `update()` until everything queued is terminal (tools and tests, never in a frame).

### Loading now

`load_blocking(id)` runs the whole pipeline inline on the main thread, dependencies first, and returns the
`LOADED` handle or the error (the slot is then `FAILED`). It waits up to `blockingTimeout` for a job that already
owns the slot (or one of its dependencies) and fails with `TIMEOUT` otherwise; the slot then stays in the normal
pipeline, which finishes it on a later `update()`. Inside a frame it is allowed but logs one WARN per frame: it
is a hitch, not a hazard.

### Fallbacks, errors and queries

| Signature (short) | Purpose |
|---|---|
| `T *fallback<T>()` | the type's placeholder (magenta texture, silent clip), created once and pinned; `nullptr` while the type is unregistered or if it has none |
| `T *get_or_fallback(const asset_ref<T> &)` | `ref.get()` while loaded, else the fallback |
| `const core::error *last_error(id)` | why a slot is FAILED (or why its last reload failed, until `collect()` unloads it); valid until the slot changes state |
| `asset_state state_of(id)` | `UNLOADED` for unknown uuids |
| `std::vector<asset_id> failed_ids(asset_error_code)` | every FAILED slot with that code; main thread |
| `std::optional<asset_id> find(std::string_view assetPath)` | `"mount://path#sub"` to uuid through the sources |
| `asset_manager_stats stats()` | `slots`, `loaded`, `pending`, `failed`, `retired`, `jobsInFlight`, `bytesResident` (of the published payloads), `frameIndex` |

An unknown uuid ends `FAILED` with `NOT_FOUND` and one ERROR log per uuid; a corrupt blob, a missing loader or a
loader failure end `FAILED` with `CACHE_CORRUPT`, `NO_LOADER`, `LOAD_FAILED` or `FINALISE_FAILED` and a WARN.
`get()` returns `nullptr` in every failed case, so a frame never crashes because a file is bad.

### Events

Events are delivered on the main thread at the end of `update()`, in order, once each. `unsubscribe()` takes
effect at once, even from inside a callback: a removed subscriber receives none of the remaining events; a
subscriber added during delivery starts with the next `update()`.

```cpp
#include "conjure/assets/asset_manager.h"

#include <vector>

using namespace conjure;
using namespace conjure::assets;

class asset_watch {
    private:
        asset_subscription mSubscription = {};
        std::vector<asset_id> mFailed = {};

    public:
        void attach(asset_manager &manager) {
            mSubscription = manager.subscribe([this](const asset_event &event) {
                if (event.kind == asset_event_kind::FAILED && event.error != nullptr) {
                    mFailed.push_back(event.id);           // event.error is valid during the callback only
                }
            });
        }

        void detach(asset_manager &manager) {
            manager.unsubscribe(mSubscription);
        }
};
```

| `asset_event_kind` | When |
|---|---|
| `LOADED` | first publish of a payload |
| `RELOADED` | a reload published a new payload; `event.version` is the new version |
| `RELOAD_FAILED` | a reload failed; the old payload is still published; `event.error` set |
| `FAILED` | a load ended FAILED; `event.error` set |
| `UNLOADED` | `collect()` dropped the payload (fires on the following `update()`) |

### Unloading and shutdown

Nothing unloads by itself. `collect()` unloads every `LOADED` slot with zero handles that is not pinned,
releases the handles it held on its dependencies and repeats until nothing changes; it returns how many it
unloaded. The engine calls it after a scene is unloaded. `pin(id, true)` protects a slot (engine built-ins and
fallbacks are pinned). `shutdown()` refuses new work (`load()` then ends `FAILED` with `SHUTTING_DOWN`), waits for
jobs in flight and returns queued slots to `UNLOADED`.

> [!CAUTION]
> Destroy every `asset_handle` before the `asset_manager`. The destructor asserts (and logs in Distribution)
> when a strong reference is still alive, and a handle that outlives the manager points at freed memory.

## The slot

`asset_slot` (`conjure/assets/asset_slot.h`) is what both reference types point at: one per uuid, created on
first `bind` or `load`, never freed before the manager shuts down, so the pointer stays valid for the manager's
lifetime. It is read-only outside the module: `object` (the published payload or `nullptr`), `version`,
`state`, `strongRefs`, `type`, `id` and `index`, all atomics apart from `id` and `index`. `bind_slot(id)` is the
untyped bind the scene bind pass uses; `make_handle(id)` binds and returns a handle without loading. A slot bound
before its asset exists learns its type later: an untyped bind (`bind_slot`, `make_handle`), `load` or `pin` of a
uuid that no source can type asks the sources again on each call, until one can.

## Ids, states and flags

`conjure/assets/asset_types.h` holds the vocabulary every other header shares.

| Name | Meaning |
|---|---|
| `asset_id` (`core::uuid`) | identity; `asset_id{}` is "no asset" and never gets a slot |
| `asset_type_id`, `make_asset_type_id("conjure.renderer.texture")` | 64-bit FNV-1a of the type name; stored in blobs and packs |
| `asset_state`, `is_terminal(state)`, `to_string(state)` | `UNLOADED`, `QUEUED`, `LOADING`, `FINALISING`, `LOADED`, `RELOADING`, `FAILED` |
| `asset_load_priority` | `LOW`, `NORMAL`, `HIGH`; FIFO within a level |
| `asset_type_flags` | `RELOAD_WITH_DEPENDENCIES`, `EXCLUDE_FROM_PACK`, `PINNED_FALLBACK` (a flag enumeration) |
| `RETIRE_FRAMES` (2), `MAX_ASSET_TYPES` (256), `DEFAULT_MAX_JOBS_IN_FLIGHT` (8), `DEFAULT_WATCH_INTERVAL` (500 ms) | constants |
| `asset_type_traits<T>`, `CONJURE_DECLARE_ASSET_TYPE(type, "name")`, `declared_asset_type` | how a C++ type is tied to its asset type name; see [Database, importers and hot reload](database-and-import.md) |

## API summary

| Signature (short) | Purpose |
|---|---|
| `explicit asset_manager(asset_manager_desc)` | `jobs`, `source`, `maxJobsInFlight`, `finaliseBudget`, `blockingTimeout` |
| `asset_type_registry &types()` | register types here before loading |
| `void add_source(unique_ptr<asset_source>)` / `void swap_source(unique_ptr<asset_source>)` | plugin packs searched after the primary; replace the primary (loaded payloads stay) |
| `asset_ref<T> bind<T>(id)` / `const asset_slot *bind_slot(id)` / `asset_handle make_handle(id)` | bind without loading |
| `asset_handle load(id, priority = NORMAL)` / `asset_load_batch load_batch(span<const asset_id>, priority)` | queue loads |
| `result<asset_handle> load_blocking(id)` | inline load, dependencies first |
| `void reload(id)` / `void retry_failed(id)` / `void pin(id, bool)` / `bool is_pinned(id)` | state changes |
| `T *fallback<T>()` / `T *get_or_fallback(ref)` / `const error *last_error(id)` / `asset_state state_of(id)` | queries |
| `void begin_frame(u64)` / `void update()` / `void end_frame()` / `u32 collect()` / `void flush()` / `void shutdown()` | frame and lifetime |
| `asset_subscription subscribe(asset_event_callback)` / `void unsubscribe(asset_subscription)` | events |
| `asset_manager_stats stats()` / `u64 frame_index()` / `bool is_shut_down()` | statistics |
| `asset_load_batch`: `is_complete()`, `progress()`, `loaded_count()`, `handles()`, `size()` | a group of handles |

## See also

- 📘 API: [Assets](index.md): how the pipeline fits together
- 📘 API: [Database, importers and hot reload](database-and-import.md): declaring your own asset type
- 📘 API: [Cooking, caches and packs](cooking-and-packs.md): where the blobs the manager reads come from
- 📘 API: [Jobs and threading](../core/jobs.md), [Reflection](../core/reflection.md), [Glossary](../glossary.md)
- 🛠 Handbook: [Assets module](../../handbook/modules/assets.md): the pipeline internals, publish and retire
