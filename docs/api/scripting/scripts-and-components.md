# Scripts, components and the runtime

📘 **Public API wiki** · 🛠 Handbook: [Scripting module](../../handbook/modules/scripting.md)

How scripts are attached to entities, how their fields are read and written, how the runtime runs them through
the frame, and what the store and the class registry hold. Headers: `conjure/scripting/script_runtime.h`,
`conjure/scripting/script_component.h`, `conjure/scripting/script_class_registry.h`,
`conjure/scripting/script_fields.h` and `conjure/scripting/script_system.h`. Everything is in `namespace
conjure::scripting`.

## At a glance

| You want to... | Use |
|---|---|
| Register the component type once | `register_script_component(registry)` |
| Attach a script to an entity | `runtime.add_script(entity, classHash, fieldsJson)` |
| Remove one | `runtime.remove_script(entity, entryId)` |
| List an entity's scripts | `runtime.scripts_of(entity)`, `runtime.entry(id)` |
| Read a script's fields (inspector) | `runtime.collect_entry_fields(id)` |
| Write one field | `runtime.set_field(id, fieldHash, value)` |
| Save / load all scripts of an entity as JSON | `runtime.serialize_entity_scripts(entity)` / `deserialize_entity_scripts(entity, value)` |
| Start and stop play | `runtime.set_mode(script_mode::PLAY)` / `set_mode(script_mode::EDIT)` |
| Run the phases by hand (tools, tests) | `runtime.run_phase(script_phase::UPDATE, deltaTime)`; `flush_pending()` |
| Hand contacts to scripts | `runtime.set_collision_source(fn)` or `dispatch_collision_events(events)` |
| Add the six script systems to the scheduler | `register_script_systems(scheduler, runtime, reloadController)` |
| Look up a class or a field | `runtime.classes().find(hash)`, `script_class_info::find_field(name)` |
| Convert fields between JSON and the blob | `json_to_blob(...)`, `blob_to_json(...)`, `field_blob_writer`, `field_blob_reader` |

## Quick example

```cpp
#include "conjure/scripting/script_runtime.h"

using namespace conjure;

core::result<void> give_player_script(scripting::script_runtime &runtime, ecs::entity player) {
    CONJURE_TRY_ASSIGN(const core::json::document fields, core::json::document::parse("{\"hp\": 4}"));
    const scripting::script_entry_id id =
      runtime.add_script(player, scripting::script_name_hash("Game.Player"), fields.root());
    if (!id.is_valid()) {
        return core::error(core::error_code::INVALID_STATE, "player is dead or carries too many scripts");
    }
    // In PLAY mode the instance exists after the next flush (run_phase flushes; so do the systems).
    runtime.flush_pending();
    CONJURE_TRY_ASSIGN(const core::json::document live, runtime.collect_entry_fields(id));
    CONJURE_TRY_ASSIGN(const core::json::document ninety, core::json::document::parse("90"));
    return runtime.set_field(id, scripting::script_name_hash("hp"), ninety.root());
}
```

## The component and the store

`script_component {u32 setHandle}` is 4 bytes and points into the runtime's `script_store`. A `script_set` is the
entity plus up to `MAX_SCRIPTS_PER_ENTITY` (8) `script_entry {classHash, managedSlot, fields, flags}` values;
`script_entry_id {setHandle, entryIndex}` names one. The component registers with hooks that deep-copy the set
on copy, free it on destruction and write / read the set as `{"scripts":[{"class","hash","fields"}]}` JSON, so
every ECS path (archives, prefabs, scene files, snapshots, `duplicate_subtree`) carries scripts without
aliasing a store entry. `script_entry_flags`: `STARTED` (`OnStart` ran), `FAULTED` (a callback was disabled
after three consecutive faults, or every callback because `OnCreate` threw; logged as a WARN either way),
`CLASS_MISSING` (no class for the hash in the active registry; the JSON is preserved).

The set lives in the runtime's store, so the hooks need a live runtime: without one, writing a component that has
a set fails with `INVALID_STATE` rather than saving an empty script list, and reading one fails the same way.

> [!CAUTION]
> Never copy a `script_component` by hand into another entity: two entities would own one set. Go through the
> ECS (archives, `scene::duplicate_subtree`, `add_script`), whose copy hook clones the set.

## Modes and the frame

In `script_mode::EDIT` the JSON is authoritative and no managed instance exists; in `PLAY` the live instances
are authoritative and the JSON is a snapshot refreshed on demand. `set_mode(PLAY)` creates every entry of the
attached world in one batch (`OnCreate` for each), `set_mode(EDIT)` destroys them with `OnDestroy` and clears
`STARTED` and `FAULTED`, so the next play runs `OnStart` again.

`run_phase(phase, deltaTime)` writes the frame block, makes one managed call for the phase
(`FIXED_UPDATE`, `UPDATE` or `LATE_UPDATE`), force-closes any query scope a script left open (WARN), flushes the
deferred structural requests and runs `flush_pending()` while the phase is still current. `flush_pending`
processes the pending destroys (`OnDestroy`, entity destruction) and creates (`OnCreate`) until the queues are
empty. `dispatch_collision_events(events)` builds one record per participating script that overrides the
callback and sends them in one call; the engine wires `set_collision_source` to the physics world's
`contact_events()` and the collision system consumes the previous fixed step's contacts.

`register_script_systems(scheduler, runtime, reload)` adds `scripting.frame_begin` (FRAME_BEGIN: reload tick,
flush), `scripting.collision_dispatch` and `scripting.fixed_update` (FIXED_UPDATE), `scripting.update` (UPDATE),
`scripting.late_update` (LATE_UPDATE) and `scripting.frame_end` (FRAME_END: flush); all are `EXCLUSIVE`, so
scripts may change the world directly.

> [!WARNING]
> Scripts run on the main thread inside those systems, and every managed call may call back in: `OnCreate` may
> destroy its own entity, `OnDestroy` may spawn scripted entities. Never hold a `script_set *` or
> `script_entry *` across a managed call; re-fetch through `entry(id)`.

## Entities, names and destruction

`request_entity_destroy(entity)` (what `Entity.Destroy()` does) destroys an entity with live scripts at the next
flush, after `OnDestroy` ran with the entity alive; one without scripts is destroyed now, or through
`deferred_commands()` while the world is locked. `set_entity_name`, `entity_name` and `find_entity_by_name` are
the small name table the core `entity_*` entries use so `Entity.Create(name)` and `Entity.Find(name)` work with
the ECS alone (the scene's `name` component is kept in step by the bindings). A name lives as long as its entity:
removing the entity's scripts keeps it, and every destroy path (native, `Entity.Destroy()`, command buffers, scene
unload) forgets it through an observer of the attached world.

`remove_script(entity, id)` removes one entry and runs its `OnDestroy`; an id that does not belong to that
entity's script set is ignored.

## Fields

A class's fields are described by `script_field_info {hash, name, kind, elementKind, defaultValue, meta}` with
`script_field_kind` (`BOOL`, `INT32`, `INT64`, `FLOAT`, `DOUBLE`, `STRING`, `VEC2`, `VEC3`, `VEC4`, `QUAT`, `COLOR`,
`ENTITY`, `ASSET`, `ENUM`, `LIST`, ...). Fields cross the boundary as the blob of `script_fields.h`:
`field_blob_writer` (`begin`, `write_int32`, `write_float`, `write_string`, `write_entity`, ..., `finish`) and
`field_blob_reader` (`next(field)`, `validate(blob)`) are the primitives, `json_to_blob(class, json, buffer)`
and `blob_to_json(class, blob)` the conversions, `is_value_compatible` the type check `set_field` performs
(`INVALID_ARGUMENT` on a mismatch). `remap_entity_fields_json` rewrites `ENTITY` fields (and lists of them)
through a map, which is how archives and prefabs keep script references pointing at the copied entities.

## The class registry

`script_class_registry` holds `script_class_info {hash, fullName, callbackMask, fields}` for every class the
loaded assembly registered (`has_callback(script_callback::UPDATE)`, `find_field(hash)`). A load stages classes
(`begin_staging`, `stage_class`, `stage_field`) and commits them atomically, so a failed load leaves the running
game untouched; `commit_staging()` without an open staging asserts and keeps the active table, and a class the
managed side registers outside a load is ignored with a WARN. `runtime.classes()` is the committed table;
`script_name_hash(fullName)` is the key.

`initialize(config)` with a `gameAssemblyPath` that does not load returns `ASSEMBLY_LOAD_FAILED` but leaves the
runtime `READY` without an assembly (scenes still load, entries keep their JSON); call `load_game_assembly` again
once the build exists. Only a failure to start the host or to validate the tables leaves it `DISABLED`.

## DOD query scopes

`begin_query_scope(typeIds, out)` locks the world and fills `conjure_query_scope` with one `conjure_chunk_view`
per matching chunk (columns in the order asked, `QUERY_MAX_COMPONENTS` at most), which is what C#'s
`ComponentQuery<T>` iterates as `Span<T>`; `end_query_scope(id)` unlocks and flushes `deferred_commands()`, the
buffer the bindings route structural requests into while the world is locked. A scope is refused with
`NOT_IN_PHASE` outside `run_phase` and collision dispatch (`current_phase()` reports `COUNT` then).

## API summary

| Signature (short) | Purpose |
|---|---|
| `result<component_id> register_script_component(ecs::component_registry &)` | the component type (returns the existing id on re-registration) |
| `script_runtime(unique_ptr<script_host>)`, `result<void> initialize(const script_runtime_config &)`, `void shutdown()` | lifetime |
| `void attach_world(ecs::world *)`, `void attach_registry(ecs::component_registry *)` | binding |
| `result<void> load_game_assembly(const path &)`, `reload_game_assembly(const path &)` | assemblies |
| `void set_mode(script_mode)`, `script_mode mode() const` | play and edit |
| `void run_phase(script_phase, f32 deltaTime)`, `void flush_pending()`, `void dispatch_collision_events(span<const collision_event>)`, `void destroy_all(bool callOnDestroy)` | the frame |
| `script_entry_id add_script(entity, u64 classHash, const json::value &fields)`, `void remove_script(entity, script_entry_id)` | scripts |
| `result<void> set_field(script_entry_id, u64 fieldHash, const json::value &)`, `result<json::document> collect_entry_fields(script_entry_id)` | fields |
| `result<json::document> serialize_entity_scripts(entity)`, `result<void> deserialize_entity_scripts(entity, const json::value &)`, `void remap_entity_fields(entity, const function<entity(entity)> &)` | JSON |
| `vector<script_entry_id> scripts_of(entity) const`, `const script_entry *entry(script_entry_id) const`, `script_entry_id entry_of_slot(i32) const` | lookups |
| `void request_entity_destroy(entity)` | `Entity.Destroy()` |
| `result<u32> begin_query_scope(span<const u32> typeIds, conjure_query_scope &out)`, `void end_query_scope(u32)` | DOD scopes |
| `const script_class_registry &classes() const`, `script_store &store()`, `script_host_status status() const`, `script_runtime_stats stats() const` | introspection |
| `static script_runtime *active()` | the one live runtime |
| `result<void> register_script_systems(ecs::scheduler &, script_runtime &, hot_reload_controller * = nullptr)` | `script_system.h` |
| `u64 script_name_hash(string_view)` | `script_class_registry.h` |

## See also

- 📘 API: [Hosting, the native API and hot reload](hosting-and-hot-reload.md), [Archives and serialisation](../ecs/archives-and-serialization.md), [JSON](../core/json.md)
- 🛠 Handbook: [Scripting module](../../handbook/modules/scripting.md)
