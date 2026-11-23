---
kind: header
---

[Engine](../../engine.md) → [Physics](../physics.md)

# physics_world.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/physics/physics_world.h` |
| API ↗ | [Physics](../../../api/physics/index.md), [The physics world and queries](../../../api/physics/physics-world-and-queries.md) |

<p>One Jolt world bound to one <code><a href="../ecs/world/world.html">ecs::world</a></code>. Non-copyable and non-movable: the Jolt listeners hold its address. Owner-thread calls (the thread that constructed the world) are attach, detach, the step pipeline and the queued body API; queries run from any thread while no step runs.</p>

## Structs

| File | Description |
|---|---|
| [`physics_stats`](physics-world/physics-stats.md) | Counters for the profiler and tests. |

## Classes

| File | Description |
|---|---|
| [`physics_world`](physics-world/physics-world.md) | One Jolt world bound to one <code><a href="../ecs/world/world.html">ecs::world</a></code>. Non-copyable and non-movable: the Jolt listeners hold its address. Owner-thread calls (the thread that constructed the world) are attach, detach, the step pipeline and the queued body API; queries run from any thread while no step runs. |

## Forward Declarations

### `body_command` {#symbol-body-command}

```cpp
struct body_command;
```

<code>A</code> queued body mutation. Opaque to users.

### `physics_world::state` {#symbol-physics-world-state}

```cpp
struct state;
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
<a id="body_command"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="physics_stats"></a>
<a id="physics_statsactivebodies"></a>
<a id="physics_statsbodies"></a>
<a id="physics_statsbodieswithoutshape"></a>
<a id="physics_statscharacters"></a>
<a id="physics_statscontactevents"></a>
<a id="physics_statsdroppedcommands"></a>
<a id="physics_statsdroppedcontactevents"></a>
<a id="physics_statspullmicroseconds"></a>
<a id="physics_statspushmicroseconds"></a>
<a id="physics_statsshapecacheentries"></a>
<a id="physics_statsstepmicroseconds"></a>
<a id="physics_statsupdateerrors"></a>
<a id="physics_world"></a>
<a id="physics_worldactive_body_count"></a>
<a id="physics_worldadd_angular_impulse"></a>
<a id="physics_worldadd_force"></a>
<a id="physics_worldadd_force_at"></a>
<a id="physics_worldadd_impulse"></a>
<a id="physics_worldadd_impulse_at"></a>
<a id="physics_worldadd_torque"></a>
<a id="physics_worldattach"></a>
<a id="physics_worldbody_count"></a>
<a id="physics_worldbody_pose"></a>
<a id="physics_worldbounds"></a>
<a id="physics_worldcharacter_ground"></a>
<a id="physics_worldcharacter_move"></a>
<a id="physics_worldcollect_contact_events"></a>
<a id="physics_worldcollect_shapes"></a>
<a id="physics_worldcontact_events"></a>
<a id="physics_worlddebug_draw"></a>
<a id="physics_worlddetach"></a>
<a id="physics_worldgravity"></a>
<a id="physics_worldhas_body"></a>
<a id="physics_worldis_attached"></a>
<a id="physics_worldis_sleeping"></a>
<a id="physics_worldlayers"></a>
<a id="physics_worldmove_kinematic"></a>
<a id="physics_worldmstate"></a>
<a id="physics_worldon_asset_reloaded"></a>
<a id="physics_worldoperator"></a>
<a id="physics_worldoverlap_box"></a>
<a id="physics_worldoverlap_sphere"></a>
<a id="physics_worldphysics_world"></a>
<a id="physics_worldphysics_world_1"></a>
<a id="physics_worldpull_transforms"></a>
<a id="physics_worldpush_transforms"></a>
<a id="physics_worldraycast"></a>
<a id="physics_worldraycast_all"></a>
<a id="physics_worldset_angular_velocity"></a>
<a id="physics_worldset_gravity"></a>
<a id="physics_worldset_linear_velocity"></a>
<a id="physics_worldsettings"></a>
<a id="physics_worldshape_cast"></a>
<a id="physics_worldsleep"></a>
<a id="physics_worldstate"></a>
<a id="physics_worldstats"></a>
<a id="physics_worldstep"></a>
<a id="physics_worldteleport"></a>
<a id="physics_worldvalidate"></a>
<a id="physics_worldwake"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
