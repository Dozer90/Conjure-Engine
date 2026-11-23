---
kind: header
---

[Engine](../../engine.md) → [ECS](../ecs.md)

# world.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/ecs/world.h` |
| API ↗ | [Archives and serialisation](../../../api/ecs/archives-and-serialization.md), [Command buffers and observers](../../../api/ecs/command-buffers-and-observers.md), [Entities and components](../../../api/ecs/entities-and-components.md), [Entities, components and systems](../../../api/ecs/index.md), [Queries, views and systems](../../../api/ecs/queries-views-and-systems.md) |

<p>The <code><a href="world/world.html">world</a></code>.</p>

## Structs

| File | Description |
|---|---|
| [`world::entity_record`](world/world-entity-record.md) | Where an entity lives. The generation is the current occupant&#x27;s (<code>ALIVE</code>) or the next one to be issued for the index (<code>FREE</code>), which is how flush recognises reserved ids. |
| [`world::flush_entry`](world/world-flush-entry.md) | One entry of the flush plan: everything a command buffer says about one entity, folded into a single archetype move. The plan vector is kept across flushes. |
| [`world::flush_slot`](world/world-flush-slot.md) | One entry of the flush-stamped slot table: the plan index of an entity, valid only while <code>stamp</code> equals the current flush&#x27;s stamp, so the table is never cleared between flushes. |
| [`world_desc`](world/world-desc.md) | Creation parameters of a <code><a href="world/world.html">world</a></code>. |
| [`world_stats`](world/world-stats.md) | Snapshot of a <code><a href="world/world.html">world</a></code>&#x27;s size. |

## Classes

| File | Description |
|---|---|
| [`iteration_scope`](world/iteration-scope.md) | RAII token that marks a <code><a href="world/world.html">world</a></code> locked for iteration: structural calls are refused while any scope is alive. Movable; the moved-from scope releases nothing. Scopes may be taken from any thread (an atomic depth counter). |
| [`world`](world/world.md) |  |

## Enumerations

### `world::entity_state` {#symbol-world-entity-state}

```cpp
enum class entity_state : u8 {
    FREE,
    ALIVE
};
```

## Forward Declarations

### `chunk` {#symbol-chunk}

```cpp
class chunk;
```

### `chunk_pool` {#symbol-chunk-pool}

```cpp
class chunk_pool;
```

### `entity_allocator` {#symbol-entity-allocator}

```cpp
class entity_allocator;
```

### `entity_archive` {#symbol-entity-archive}

```cpp
class entity_archive;
```

### `entity_handle` {#symbol-entity-handle}

```cpp
class entity_handle;
```

### `observer_table` {#symbol-observer-table}

```cpp
class observer_table;
```

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="chunk"></a>
<a id="chunk_pool"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="entity_allocator"></a>
<a id="entity_archive"></a>
<a id="entity_handle"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="iteration_scope"></a>
<a id="iteration_scopeis_active"></a>
<a id="iteration_scopeiteration_scope"></a>
<a id="iteration_scopeiteration_scope_1"></a>
<a id="iteration_scopemworld"></a>
<a id="iteration_scopeoperator"></a>
<a id="iteration_scoperelease"></a>
<a id="observer_table"></a>
<a id="other-declarations"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<a id="world"></a>
<a id="world_desc"></a>
<a id="world_descchunksperslab"></a>
<a id="world_descinitialentitycapacity"></a>
<a id="world_descmaxchunkslabs"></a>
<a id="world_descname"></a>
<a id="world_stats"></a>
<a id="world_statsaliveentities"></a>
<a id="world_statsarchetypes"></a>
<a id="world_statschunkbytes"></a>
<a id="world_statschunks"></a>
<a id="world_statsversion"></a>
<a id="worldadd"></a>
<a id="worldadd_raw"></a>
<a id="worldadd_storage"></a>
<a id="worldarchetype_for"></a>
<a id="worldarchetype_with"></a>
<a id="worldarchetypes"></a>
<a id="worldbegin_structural"></a>
<a id="worldbump_version"></a>
<a id="worldcheck_structural"></a>
<a id="worldchunk_view"></a>
<a id="worldclear"></a>
<a id="worldcommand_buffer"></a>
<a id="worldcomponent_registry"></a>
<a id="worldconstruct_column_default"></a>
<a id="worldcreate_entities"></a>
<a id="worldcreate_entity"></a>
<a id="worldcreate_query"></a>
<a id="worlddestroy_alive"></a>
<a id="worlddestroy_entity"></a>
<a id="worlddestruct_column"></a>
<a id="worlddiscard_placed"></a>
<a id="worlddrain_deferred"></a>
<a id="worldend_structural"></a>
<a id="worldensure_records"></a>
<a id="worldentity_archive"></a>
<a id="worldentity_count"></a>
<a id="worldentity_record"></a>
<a id="worldentity_recordchunkindex"></a>
<a id="worldentity_recordgeneration"></a>
<a id="worldentity_recordowner"></a>
<a id="worldentity_recordrow"></a>
<a id="worldentity_recordstate"></a>
<a id="worldentity_state"></a>
<a id="worldentity_statealive"></a>
<a id="worldentity_statefree"></a>
<a id="worldfind_alive"></a>
<a id="worldfinish_add"></a>
<a id="worldfire_added"></a>
<a id="worldfire_destroyed"></a>
<a id="worldfire_removed"></a>
<a id="worldflush"></a>
<a id="worldflush_entry"></a>
<a id="worldflush_entryaddmask"></a>
<a id="worldflush_entryalive"></a>
<a id="worldflush_entrycommands"></a>
<a id="worldflush_entrydestroy"></a>
<a id="worldflush_entryid"></a>
<a id="worldflush_entrypayloads"></a>
<a id="worldflush_entryremovemask"></a>
<a id="worldflush_inline_payloads"></a>
<a id="worldflush_internal"></a>
<a id="worldflush_slot"></a>
<a id="worldflush_slotplan"></a>
<a id="worldflush_slotstamp"></a>
<a id="worldget"></a>
<a id="worldget_raw"></a>
<a id="worldhandle"></a>
<a id="worldhas"></a>
<a id="worldhas_raw"></a>
<a id="worldinstance_count"></a>
<a id="worldinstantiate"></a>
<a id="worldinstantiate_batch"></a>
<a id="worldinstantiate_now"></a>
<a id="worldis_alive"></a>
<a id="worldis_locked"></a>
<a id="worldis_reserved"></a>
<a id="worlditeration_scope"></a>
<a id="worldlock_iteration"></a>
<a id="worldmalivecount"></a>
<a id="worldmallocator"></a>
<a id="worldmarchetypebymask"></a>
<a id="worldmarchetypepointers"></a>
<a id="worldmarchetypes"></a>
<a id="worldmask_of"></a>
<a id="worldmchunkpool"></a>
<a id="worldmdeferred"></a>
<a id="worldmdeferredflushing"></a>
<a id="worldmemptyarchetype"></a>
<a id="worldmflushplan"></a>
<a id="worldmflushslots"></a>
<a id="worldmflushstamp"></a>
<a id="worldminobserver"></a>
<a id="worldmlockdepth"></a>
<a id="worldmname"></a>
<a id="worldmobservers"></a>
<a id="worldmove_entity"></a>
<a id="worldmownerthread"></a>
<a id="worldmplacedscratch"></a>
<a id="worldmrecords"></a>
<a id="worldmregistry"></a>
<a id="worldmstructuraldepth"></a>
<a id="worldmversion"></a>
<a id="worldname"></a>
<a id="worldobserve"></a>
<a id="worldobserve_destroyed"></a>
<a id="worldoperator"></a>
<a id="worldowner_thread"></a>
<a id="worldplace_rows"></a>
<a id="worldquery"></a>
<a id="worldregistry"></a>
<a id="worldrelayout_archetypes"></a>
<a id="worldrelease_reserved"></a>
<a id="worldremove"></a>
<a id="worldremove_from_all"></a>
<a id="worldremove_raw"></a>
<a id="worldremove_row"></a>
<a id="worldscheduler"></a>
<a id="worldset"></a>
<a id="worldset_owner_thread"></a>
<a id="worldstats"></a>
<a id="worldunobserve"></a>
<a id="worldvalidate"></a>
<a id="worldversion"></a>
<a id="worldview"></a>
<a id="worldworld"></a>
<a id="worldworld_1"></a>
<!-- doc-legacy-anchors:end -->
