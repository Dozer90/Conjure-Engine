# 14 - Physics module design

Status: **binding** (owner: lead architect). Module `conjure::physics` (wave 3b), static library `conjure_physics`.
Headers in `engine/include/conjure/physics/`, sources in `engine/src/physics/`, tests in `tests/physics/`.
Public link dependencies: `conjure::core`, `conjure::ecs`, `conjure::scene`, `conjure::assets`. Private: Jolt Physics
(`conjure::thirdparty::jolt`, pinned to the 5.3 line in `third_party/CMakeLists.txt`). No Jolt header is reachable
from a public header (CONVENTIONS 2).

This note expands `ARCHITECTURE.md` 4.5 into an implementable design and supersedes it where more specific. It is
reconciled with `09_cross_area_resolutions.md` (R1, R2, R3, R6, R7, R25, R30, R31), `06_engine_loop.md` 2.3 / 3.1 /
3.2 / 8, `03_scripting.md` 3.5, `04_editor.md` 8, `08_testing.md` 5.2 / 7.1 and `10_core.md`. Jolt names are those
of the Jolt 5.3 headers.

---

## 1. Overview and decisions

| Id | Decision | Rationale |
|---|---|---|
| P1 | **Jolt is fully encapsulated in `physics_world`.** One `JPH::PhysicsSystem` per `physics_world`; `physics_world(const physics_settings &, core::jobs::job_system &)` never fails (settings are validated beforehand with `physics_settings::validate()`); `attach(ecs::world &)` installs the ECS observers. | The engine, editor, bindings and tests program against plain structs and `ecs::entity`; Jolt can be upgraded or swapped without touching a public header. |
| P2 | **Jolt jobs run on `core::jobs`.** `jolt_job_system : JPH::JobSystemWithBarrier` with `GetMaxConcurrency() = worker_count() + 1`, a fixed-size job pool, `QueueJob` -> `job_system::schedule`. With 0 workers every Jolt job executes inline inside `QueueJob`, exactly like Jolt's own `JobSystemSingleThreaded`. | One thread pool per process (ARCHITECTURE 3); headless tests and the engine share the same code path (C7). |
| P3 | **Jolt memory goes through `core::memory`.** `JPH::Allocate/Reallocate/Free/AlignedAllocate/AlignedFree` are set to hooks that call `core::memory::tracked_allocate/tracked_free` with a 16-byte size header, so every Jolt allocation bumps `allocation_counters()` and is visible to `conjure::testing::allocation_scope` and `memory_guard`. | The allocation gates of `08_testing.md` 7.1 must see Jolt. `JPH_DISABLE_CUSTOM_ALLOCATOR` is never defined. |
| P4 | **Process-wide Jolt state is reference counted.** `jolt_runtime::acquire()` installs the hooks, `JPH::Trace`, `JPH::AssertFailed`, creates `JPH::Factory::sInstance` and calls `JPH::RegisterTypes()` on the first call; `release()` runs `UnregisterTypes()` and deletes the factory on the last. Guarded by a mutex. | Several `physics_world`s may coexist (tests, editor). `new Factory()` is the one sanctioned `new`: `Factory` carries `JPH_OVERRIDE_NEW_DELETE`, so it still routes through P3. |
| P5 | **Units and axes:** metres, kilograms, seconds, radians in code (inspector fields that are angles end in `Degrees`), right-handed, Y up (`10_core.md` 2.2 and Jolt agree). `math::quat` is constructed `(w, x, y, z)` but stored `x, y, z, w` (core defines `GLM_FORCE_QUAT_DATA_XYZW`, R58), the same storage as `JPH::Quat`; conversions live only in `jolt_conversions.h` and go through named components, never positional constructors. | One place to get conversions wrong. |
| P6 | **The engine owns time.** `physics.step` reads `frame_info.fixedDeltaTime` and calls `PhysicsSystem::Update(fixedDeltaTime, settings.collisionSteps, &tempAllocator, &jobSystem)` once per fixed step. Physics never accumulates time and never interpolates; `scene.propagate_transforms` interpolates `interpolated_transform` (R3, `06_engine_loop.md` 3.2). | Fixed-step semantics stay in one module (D3 of 06). |
| P7 | **Components are plain reflected structs; Jolt ids live only in the world.** `rigid_body`, the five colliders, `physics_material` and `character_controller` carry authored data plus mirrored runtime fields (velocities, ground state). The `entity <-> JPH::BodyID` map is a `body_record` table owned by `physics_world`. | Components serialise, copy into prefabs and archives, and hash (`world_hash`) without Jolt knowledge; a copied entity gets its own body because no handle is copied. |
| P8 | **Bodies are created and destroyed by observers, applied at the next `push_transforms`.** `observe(ADDED)` on `rigid_body` / any collider / `character_controller` enqueues the entity in the world's pending list; `observe(REMOVED)` of the last body-defining component removes and destroys the body at once and synthesises EXIT events. A collider without `rigid_body` is an implicit `STATIC` body. | Components added in one command-buffer flush arrive in arbitrary order with payloads applied after observers fire (`01_ecs.md` 3.4); deferring creation to the step start sees the complete, initialised entity. |
| P9 | **Every body mutation from gameplay is a queued command**, recorded by `physics_world::add_force/add_impulse/set_linear_velocity/teleport/...` into a pre-reserved vector and applied in call order inside `push_transforms`, after pending creations. Reads come from the component mirrors. | Deterministic order independent of which system called; works for bodies that do not exist yet in this step; the step itself never sees half-applied state. |
| P10 | **Layers: up to 32 named physics layers, a symmetric collision matrix, two Jolt broad-phase layers.** `JPH::ObjectLayer = (moving ? 32 : 0) + layerIndex` (64 object layers), broad phase `NON_MOVING` / `MOVING`; the pair table is `JPH::ObjectLayerPairFilterTable(64)`. Names and ignored pairs come from `project.json` `extensions.physics_layers`; a missing section yields one layer `default`. Query masks are `u32` bit sets over layer indices. | Static-static pairs are never tested, moving bodies broad-phase against everything; the data-driven tables Jolt ships are exactly a matrix. 32 layers fit a `u32` mask for scripts. |
| P11 | **Contacts: lock-free per-thread collection, one sorted span per step, exactly-once semantics.** The `JPH::ContactListener` appends raw records to the buffer of `job_system::thread_index()`; `physics.contact_events` merges, sorts by `(bodyIdA, bodyIdB)` (`BodyID::GetIndexAndSequenceNumber()`, A < B as Jolt guarantees), collapses sub-shape manifolds through a persistent pair table and emits `ENTER`/`STAY`/`EXIT` or `TRIGGER_ENTER`/`TRIGGER_EXIT`. The span is cleared at the next `PHYSICS_STEP` (R6). | Jolt calls listeners from many threads with bodies locked; sorting restores a thread-count-independent order; the pair table makes ENTER/EXIT exact even when a body is destroyed between steps. |
| P12 | **Queries return entities and are allocation-free.** `raycast`, `raycast_all`, `overlap_sphere`, `overlap_box`, `shape_cast` run on `JPH::NarrowPhaseQuery` with embedded (stack) shapes and caller-provided output spans; a `query_filter` carries a layer mask, trigger inclusion and an ignored entity. Allowed from any thread while no step is running. | Scripts, the editor and AI commands need cheap queries; Jolt's locking narrow-phase query is thread-safe outside `Update`. |
| P13 | **Character controller = `JPH::CharacterVirtual` with an inner body.** Updated sequentially on the main thread inside `physics.step` before `PhysicsSystem::Update` with `ExtendedUpdate` (stick to floor, walk stairs); the inner body (`mInnerBodyShape`) gives it a `BodyID`, so raycasts, triggers and `body_record` treat it like any other body. | Virtual characters have the best stepping behaviour; the inner body avoids a second entity mapping. |
| P14 | **Mesh and convex colliders reference `conjure.physics.collision_mesh` assets.** Physics cannot depend on `renderer` (ARCHITECTURE 2.2) and `mesh_asset::cpu_geometry()` exists only in Debug/Development (R11), so physics registers its own asset type and importer (positions + indices from `.gltf/.glb/.obj`; listed in `05_assets.md` 8 and `ARCHITECTURE.md` 5, R44). Shapes are built on the asset job (`load_cpu`) and cached per `(asset, version, scale, convex)`. | Distribution builds need collision geometry; one cooked blob per mesh, one Jolt shape per distinct scale. |
| P15 | **Allocation-free steady state.** Job pool, contact buffers, command queue, record table, sort scratch and `JPH::TempAllocatorImpl` are sized at construction (`physics_settings`); overflow degrades with a counted, rate-limited `WARN`, never an allocation inside `step`. `physics.step_10k_sleeping` is the gate. | `08_testing.md` 7.1 budgets (`allocs = 0`). |
| P16 | **Deterministic by construction.** `JPH::PhysicsSettings::mDeterministicSimulation = true`, body creation and commands applied in a fixed order, contacts sorted. Results are identical for 0 and N workers on the same binary; cross-platform bit equality needs `CONJURE_PHYSICS_CROSS_PLATFORM_DETERMINISTIC` (maps to Jolt's `CROSS_PLATFORM_DETERMINISTIC`, OFF by default; `ARCHITECTURE.md` 9, R59). | `tests/physics/test_determinism.cpp` compares 0 vs 4 workers; `08_testing.md` 12 keeps the engine-wide determinism test physics-free until this is proven. |
| P17 | **Debug drawing is a sink interface.** `physics_debug_draw { line, triangle, text }` is implemented by the engine over `renderer` debug draw; `physics_world::debug_draw(sink, flags)` wraps the sink in a `JPH::DebugRendererSimple` (compiled only when `JPH_DEBUG_RENDERER`, i.e. not in Distribution). | Physics stays renderer-agnostic; tests use a counting sink. |
| P18 | **A body command is refused at the call when the kind of body is known and cannot take it** (code sweep F413). `physics_error::WRONG_BODY_KIND` when the entity has a body and the command does not apply: forces, impulses and torques on anything but a `DYNAMIC` body; velocities on a static body; `move_kinematic` on anything but a `KINEMATIC` body without `KINEMATIC_MOVES_WITH_TRANSFORM`; every one of these on a character. `teleport` applies to every kind; `wake` and `sleep` succeed on every kind and do nothing on a static body (never awake: `is_sleeping` is true for it) and on a character (never asleep); `character_move` keeps `NOT_A_CHARACTER`. The kind is read from the entity's components (`character_controller`, else `rigid_body::motionType` and flags, else colliders alone = static), never from the row; a system that sends body commands therefore declares `rigid_body` and `character_controller` as reads (or is `EXCLUSIVE`). An entity still pending at the call has no kind: its command is queued and the push that creates the body drops it, counted in `droppedCommands`, when the new body cannot take it (a command whose entity still has no body at the second push after the call is dropped and counted as before); the same late drop meets a command whose target changed kind between the call and the push. One table (`command_applies`, `physics_world_internal.h`) decides the refusal and the late drop. | A command that returned success and then vanished hid the caller's mistake; `CONJURE_TRY` users and C# (`INVALID_ARGUMENT`, `ArgumentException`) now see it at once. The push applies component changes (a motion type written this frame, a `rigid_body` added to or removed from colliders) before the commands, so the row of the last push would refuse a force sent in the frame that makes a body dynamic. |
| P19 | **Rotations are compared per quaternion component** (code sweep F213). `math::same_rotation(a, b, epsilon)` aligns the sign and then is `nearly_equal`; physics passes it `TELEPORT_EPSILON` (1e-4) for teleport detection and the default 1e-6 for a collider's identity placement. A rotation written into `local_transform` is a teleport above 2e-4 to 4e-4 rad (0.011 to 0.023 degrees, depending on the axis); a lone collider keeps a rotation above 2e-6 to 4e-6 rad (below that it is used as the body's shape without a compound). | The former test, `|dot| >= 1 - epsilon`, was a cosine tolerance: 1e-4 hid turns below 1.6 degrees from the push, which the next pull then overwrote, and 1e-6 dropped collider rotations below 0.16 degrees. The round trip of a pulled rotation (bit-exact for root rows, a few 1e-7 for parented ones) stays far below the new threshold. |
| P20 | **The engine carries a grounded character with its ground; `character_controller::velocity` is the character's own** (code sweep F421, option A). A step that starts with the character `ON_GROUND` refreshes the ground velocity (`CharacterVirtual::UpdateGroundVelocity`, as Jolt's `CharacterVirtualTest` does) and adds its horizontal part to the velocity it gives Jolt; the pull subtracts the same vector before it writes `velocity`, so nothing compounds and the field, `character_move` and the C# `CharacterController.Velocity` mean "without what the ground adds": relative to the ground while grounded. The vector is kept in the row (`carriedVelocity`) while the character is `IN_AIR`, so **a jump off a moving platform keeps the platform's speed** until the character touches ground again: `ON_GROUND` it takes that ground's velocity (zero on static ground), and a step that starts `ON_STEEP_GROUND` or `NOT_SUPPORTED` clears the vector (ground that does not carry the character must not push it along a slope, and where `CancelVelocityTowardsSteepSlopes` cancels the push the pull would still subtract it). A teleport to a new position (the command, or a position written into `local_transform`, which is also how a character parented to a moving entity follows its parent) clears the vector, and the step after it adds no horizontal ground velocity; a rotation alone written into `local_transform`, which is how gameplay turns a character, is applied with `SetRotation` and leaves the carrying alone. Only the horizontal part is carried; the vertical rule (`APPLY_GRAVITY`: follow the ground unless the script jumps) is unchanged except that it reads the refreshed ground velocity, and rotating ground moves the character without turning it. Limits of the vertical rule, which is not part of the carrying: the step after a teleport still reads the ground state and the ground's vertical velocity of the place the character left, so a character teleported off a rising lift keeps the lift's vertical velocity, and one parented to a rising platform under its feet gets that velocity on top of its parent's motion (it hops; a parent that moves sideways or sinks moves it once); a grounded character whose ground sinks faster than 0.1 m/s is by the rule's own test jumping, so it is followed by `STICK_TO_FLOOR` one step behind, not by the rule. The render scale of a character follows the entity's scale at every push that sees its transform change (the capsule itself is never scaled). | A platform slid away under a character that stood on it, and no script could repair that: the velocity of a `KINEMATIC_MOVES_WITH_TRANSFORM` platform is never pulled into a component. Adding the ground velocity without taking it out again would compound, because the pull writes the velocity back and the next step reads it. Keeping the vector in the air is what the jump test settled: dropping it at take-off makes a character fall behind the platform it jumped from (2 m/s for a second is two metres). Skipping the step after a teleport keeps a character from being moved twice when it is parented to the platform under its feet, and keeps one teleported off a platform from drifting with it: the ground state it has until its next update still describes the place it left. |
| P21 | **A static trigger is an active kinematic sensor in Jolt; solid contacts keep their `EXIT` at sleep** (code sweep F408, option C plus the documentation of A). A body with `motion == STATIC` whose colliders are all triggers is created with `EMotionType::Kinematic`, `mIsSensor`, zero velocities, and is added active whatever `START_ASLEEP` says; it stays in the static object layer (`object_layer(layer, STATIC)`) and does not get `mCollideKinematicVsNonDynamic`. Jolt documents that an active kinematic sensor also detects sleeping bodies and never falls asleep (`Body::SetIsSensor`, its test `TestDynamicSleepingVsKinematicSensor`), so a body that falls asleep inside the trigger stays inside, and one that is asleep when the trigger meets it is reported. The static layer keeps it from static bodies and from other static triggers (3.2 never enables a static-static pair). Everything else about it stays static to the engine: `row.motion`, commands (P18), `is_sleeping` (true), `active_body_count()` (not counted), queries with `includeStatic` off, no pull, no `interpolated_transform`. `sync_jolt_motion(row)` is the one place that maps `(motion, isTrigger)` to Jolt's motion type, sensor flag and `mCollideKinematicVsNonDynamic` after creation, so a trigger whose motion type changes meets static geometry as its new kind does (a static trigger turned kinematic enters the static bodies it overlaps). Kinematic and dynamic triggers are otherwise unchanged: awake they are active sensors already. Solid pairs still report `EXIT` when their last awake body falls asleep and `ENTER` at wake. | `TRIGGER_EXIT` for a crate still standing on a pressure plate is a gameplay trap, and the former justification ("Unity behaves the same") was wrong: Unity sends neither `OnTriggerExit` nor `OnCollisionExit` at sleep. Suppressing the exits of solid pairs (option B) needs a sweep at wake for pairs that were not re-added and was left out. Cost: every static trigger is an active body in every step, and every body inside one keeps its body pair and its cached manifold while it sleeps (they count against `maxBodyPairs` and `maxContactConstraints`) and its entry in the pair table (`maxContactEventsPerStep` entries, as for every pair in contact). Jolt also reports such a pair as persisted in every step; the listener does not record persisted contacts of sensor pairs (3.6), which nothing reads, so a full trigger volume does not use up the step's raw-contact budget; `physics.step_10k_sleeping_1k_triggers` (`tests/benchmarks/bench_physics.cpp`, `08_testing.md` 7.1) measures the step. |

Rejected: a body handle component (copies alias, archives leak ids); synchronous body creation inside observers (P8);
an ECS event queue for contacts (R6); `JPH::Character` for v1 (stepping quality); double precision Jolt; a third
broad-phase layer for sensors (premature).

---

## 2. Data model

Naming per R31: plain structs use camelCase fields, classes `mCamelCase`. Core aliases (`u8`, `u16`, `u32`, `u64`,
`f32`, `usize`) from `conjure/core/types.h`.

### 2.1 Settings (`physics_settings.h`)

```cpp
namespace conjure::physics {
    struct physics_settings {
        math::vec3 gravity = math::vec3(0.0f, -9.81f, 0.0f);
        u32 maxBodies = 65536;                  // JPH::PhysicsSystem::Init inMaxBodies; also sizes body_record table
        u32 numBodyMutexes = 0;                 // 0 = Jolt default
        u32 maxBodyPairs = 65536;
        u32 maxContactConstraints = 16384;
        u32 collisionSteps = 1;                 // PhysicsSystem::Update inCollisionSteps
        usize tempAllocatorBytes = 32 * MIB;    // JPH::TempAllocatorImpl
        u32 maxJobs = 2048;                     // jolt_job_system pool (Jolt's cMaxPhysicsJobs)
        u32 maxBarriers = 8;                    // JPH::JobSystemWithBarrier::Init
        u32 maxContactEventsPerStep = 16384;    // merged events; per-thread raw buffers hold the same count each
        u32 maxQueuedCommands = 8192;           // P9 command queue
        u32 maxCharacters = 256;
        bool deterministic = true;              // PhysicsSettings::mDeterministicSimulation
        bool allowSleeping = true;
        f32 timeBeforeSleepSeconds = 0.5f;
        f32 pointVelocitySleepThreshold = 0.03f;
        u32 numVelocitySteps = 10;
        u32 numPositionSteps = 2;
        f32 speculativeContactDistance = 0.02f;
        bool estimateContactImpulses = true;    // JPH::EstimateCollisionResponse on ENTER events
        physics_layer_table layers = {};        // section 2.2; default: one layer "default"

        [[nodiscard]] core::result<void> validate() const;          // ranges below; INVALID_SETTINGS
        [[nodiscard]] static core::result<physics_settings> from_json(
          const core::json::value &physicsSection,                 // project.json "physics"
          const core::json::value &layersSection                   // "extensions.physics_layers"; may be missing
        );
    };
}
```

`validate()` rejects: `maxBodies == 0` or `> 1 << 23` (Jolt body index limit), `collisionSteps == 0`,
`tempAllocatorBytes < 1 MiB`, `maxJobs < 64`, `maxContactEventsPerStep == 0`, `numVelocitySteps == 0`,
`numPositionSteps == 0`, `timeBeforeSleepSeconds < 0`, and whatever `layers.validate()` rejects. The engine's
`engine::physics_config {enabled, gravity, maxBodies}` (`06_engine_loop.md` 2.3) stays the small
engine-side struct; `physics_subsystem` calls `physics::physics_settings::from_json` on the raw `project.json`
sections so every field above is authorable without the engine knowing it (section 8).

### 2.2 Layers (`physics_layers.h`)

```cpp
inline constexpr u32 MAX_PHYSICS_LAYERS = 32;

struct physics_layer_mask {
    u32 bits = 0xFFFFFFFFu;
    [[nodiscard]] static constexpr physics_layer_mask all() noexcept;
    [[nodiscard]] static constexpr physics_layer_mask none() noexcept;
    [[nodiscard]] static constexpr physics_layer_mask only(u8 layer) noexcept;
    [[nodiscard]] constexpr bool includes(u8 layer) const noexcept;
    [[nodiscard]] constexpr physics_layer_mask with(u8 layer) const noexcept;
    [[nodiscard]] constexpr physics_layer_mask without(u8 layer) const noexcept;
};

class physics_layer_table {        // value type; small_vector<string_id, 8> mNames, std::array<u32, 32> mCollides
    public:
        physics_layer_table();      // one layer "default" that collides with itself
        [[nodiscard]] static core::result<physics_layer_table> from_json(const core::json::value &section);
        [[nodiscard]] core::result<u8> add_layer(core::string_id name);   // LAYER_LIMIT_REACHED, DUPLICATE_LAYER
        void set_collides(u8 a, u8 b, bool collides) noexcept;                     // symmetric
        [[nodiscard]] bool collides(u8 a, u8 b) const noexcept;
        [[nodiscard]] physics_layer_mask collision_mask(u8 layer) const noexcept;
        [[nodiscard]] std::optional<u8> find(core::string_id name) const noexcept;
        [[nodiscard]] core::string_id name(u8 layer) const noexcept;
        [[nodiscard]] u32 count() const noexcept;
        [[nodiscard]] core::result<void> validate() const;                         // count >= 1, matrix symmetric
        [[nodiscard]] core::result<void> to_json(core::json::writer &out) const;
};
```

`project.json` section (snake_case keys; everything collides unless listed in `ignore`):

```json
"extensions": { "physics_layers": {
    "names": ["default", "player", "enemy", "projectile", "trigger"],
    "ignore": [["projectile", "projectile"], ["player", "player"], ["trigger", "trigger"]]
} }
```

`from_json` errors: `names` missing or empty -> `LAYER_CONFIG_INVALID`; more than 32 -> `LAYER_LIMIT_REACHED`;
duplicate -> `DUPLICATE_LAYER`; unknown name in `ignore` -> `UNKNOWN_LAYER`. Layer indices are positions in `names`
(renaming keeps `rigid_body.layer` valid, reordering is a content change). The table is immutable once a
`physics_world` is constructed from it.

### 2.3 Components (`components.h`, `character_controller.h`)

All components are registered by `physics::register_components` with
`CONJURE_COMPONENT(type, "conjure.physics.<struct name>")`, without exception (`physics_material` registers as
`conjure.physics.physics_material`; its earlier key `conjure.physics.material` was renamed before any content used
it, sweep F629) and described with `CONJURE_REFLECT_STRUCT` (`10_core.md` 2.11). Fields marked `[runtime]` carry
`.readOnly = true` in their `field_attributes` and are serialised (so a saved scene reopens with the same velocities)
but never authored; `[transient]` fields are `CONJURE_FIELD_HIDDEN`.

```cpp
enum class motion_type : u8 { STATIC, KINEMATIC, DYNAMIC };

enum class rigid_body_flags : u16 {
    NONE = 0,
    CONTINUOUS_COLLISION = 1 << 0,            // EMotionQuality::LinearCast
    KINEMATIC_MOVES_WITH_TRANSFORM = 1 << 1,  // kinematic follows local_transform (MoveKinematic); else velocity driven
    ALLOW_SLEEPING = 1 << 2,
    START_ASLEEP = 1 << 3,                    // EActivation::DontActivate at creation
    LOCK_ROTATION = 1 << 4,                   // EAllowedDOFs: translation only
    LOCK_ROTATION_X = 1 << 5, LOCK_ROTATION_Y = 1 << 6, LOCK_ROTATION_Z = 1 << 7,
    LOCK_TRANSLATION_X = 1 << 8, LOCK_TRANSLATION_Y = 1 << 9, LOCK_TRANSLATION_Z = 1 << 10,
    USE_GYROSCOPIC_FORCE = 1 << 11
};
CONJURE_ENUM_FLAGS(rigid_body_flags);

struct rigid_body {                                   // "conjure.physics.rigid_body", 52 bytes (4-aligned); mirrored field-for-field by C# RigidBodyData (15_bindings.md 2.6, R48)
    motion_type motionType = motion_type::DYNAMIC;
    u8 layer = 0;                                     // index into physics_layer_table (inspector shows names)
    rigid_body_flags flags = rigid_body_flags::ALLOW_SLEEPING;
    f32 mass = 1.0f;                                  // kg; 0 = derive from shape volume (CalculateMassAndInertia)
    f32 linearDamping = 0.05f;                        // 1/s
    f32 angularDamping = 0.05f;
    f32 gravityFactor = 1.0f;
    f32 maxLinearVelocity = 500.0f;                   // m/s
    math::vec3 linearVelocity = math::vec3(0.0f);     // [runtime] pulled each step; a write before PHYSICS_STEP sets it
    math::vec3 angularVelocity = math::vec3(0.0f);    // [runtime] radians/s
    bool isSleeping = false;                          // [runtime]
};

// Colliders: "conjure.physics.<name>". offset/rotation place the shape in entity space (one declaration per line in
// the real header; condensed here). ZERO = math::vec3(0.0f), IDENTITY = math::quat(1.0f, 0.0f, 0.0f, 0.0f).
struct box_collider {
    math::vec3 halfExtents = math::vec3(0.5f); math::vec3 offset = ZERO; math::quat rotation = IDENTITY;
    bool isTrigger = false;
};
struct sphere_collider { f32 radius = 0.5f; math::vec3 offset = ZERO; bool isTrigger = false; };
struct capsule_collider {                             // Y axis; total height 2 * (halfHeight + radius)
    f32 radius = 0.5f; f32 halfHeight = 0.5f; math::vec3 offset = ZERO; math::quat rotation = IDENTITY;
    bool isTrigger = false;
};
struct convex_collider {
    assets::asset_ref<collision_mesh_asset> mesh = {}; math::vec3 offset = ZERO; math::quat rotation = IDENTITY;
    f32 convexRadius = 0.05f; bool isTrigger = false;                 // ConvexHullShapeSettings inMaxConvexRadius
};
struct mesh_collider { assets::asset_ref<collision_mesh_asset> mesh = {}; bool isTrigger = false; };   // not DYNAMIC
struct physics_material { f32 friction = 0.5f; f32 restitution = 0.0f; };   // optional; combined sqrt(f1 f2) / max
```

Rules enforced at body (re)build (3.3): several colliders form a compound shape; all must agree on `isTrigger`
(`MIXED_TRIGGER_FLAGS`, WARN, body built non-trigger); `mesh_collider` on a `DYNAMIC` body is
`UNSUPPORTED_SHAPE_FOR_MOTION` (WARN, collider ignored). A trigger body is a Jolt sensor (`mIsSensor = true`): no
response, `TRIGGER_*` events only. A static trigger is a kinematic sensor to Jolt, kept active in the static object
layer (P21): it detects dynamic and kinematic bodies and characters, awake or asleep, and neither static geometry nor
another static trigger. A kinematic trigger also detects static geometry (`mCollideKinematicVsNonDynamic`). A
kinematic or dynamic trigger detects sleeping bodies while it is active itself, which it stays once it is (a sensor
never falls asleep on its own); created with `START_ASLEEP` or put to sleep it detects active bodies only.

```cpp
enum class ground_state : u8 { ON_GROUND, ON_STEEP_GROUND, NOT_SUPPORTED, IN_AIR };   // CharacterBase::EGroundState

enum class character_flags : u8 {
    NONE = 0,
    STICK_TO_FLOOR = 1 << 0,     // ExtendedUpdateSettings::mStickToFloorStepDown
    WALK_STAIRS = 1 << 1,        // ExtendedUpdateSettings::mWalkStairsStepUp = (0, stepHeight, 0)
    APPLY_GRAVITY = 1 << 2       // integrate settings.gravity into velocity while not ON_GROUND
};
CONJURE_ENUM_FLAGS(character_flags);

struct character_controller {                         // "conjure.physics.character_controller"
    f32 radius = 0.3f;
    f32 height = 1.8f;                                // total capsule height, feet at the entity origin
    f32 maxSlopeAngleDegrees = 45.0f;
    f32 stepHeight = 0.4f;
    f32 mass = 70.0f;                                 // CharacterVirtualSettings::mMass (push on dynamic bodies)
    f32 maxStrength = 100.0f;
    f32 padding = 0.02f;                              // mCharacterPadding
    u8 layer = 0;
    character_flags flags = character_flags::STICK_TO_FLOOR | character_flags::WALK_STAIRS
      | character_flags::APPLY_GRAVITY;
    math::vec3 velocity = math::vec3(0.0f);           // input: desired velocity for the step; output: achieved velocity;
                                                      // the character's own, without what its ground adds (P20)
    ground_state ground = ground_state::IN_AIR;       // [runtime]
    math::vec3 groundNormal = math::vec3(0.0f, 1.0f, 0.0f);  // [runtime]
    ecs::entity groundEntity = ecs::NULL_ENTITY;      // [runtime]
};
```

A `character_controller` excludes `rigid_body` (both present -> `CONFLICTING_BODY_COMPONENTS`, WARN, the character
wins). Scripts drive it by writing `velocity` (horizontal from input, vertical kept from the previous step unless
jumping) in `FIXED_UPDATE`; `physics_world::character_move(entity, velocity)` is the queued equivalent for bindings.
Either way the velocity is the character's own (P20): the step adds the horizontal velocity of the ground that carries
the character and the pull takes it out again, so on a moving platform zero means standing still on the platform.

### 2.4 Contact events (`contact_events.h`)

```cpp
enum class contact_kind : u8 { ENTER, STAY, EXIT, TRIGGER_ENTER, TRIGGER_EXIT };

struct contact_event {                                // 48 bytes, trivially copyable
    ecs::entity a = {};                               // body with the lower Jolt BodyID
    ecs::entity b = {};
    math::vec3 point = math::vec3(0.0f);              // world space, first manifold point (zero for EXIT)
    math::vec3 normal = math::vec3(0.0f);             // world space, points from a towards b (zero for EXIT)
    f32 impulse = 0.0f;                               // kg*m/s, estimated on ENTER when estimateContactImpulses; else 0
    contact_kind kind = contact_kind::ENTER;
    u8 contactCount = 0;                              // manifold points summed over sub-shapes
    u8 layerA = 0;
    u8 layerB = 0;
};
```

Ordering of `physics_world::contact_events()`: ascending `(bodyIdA, bodyIdB)` with `bodyId = BodyID::GetIndexAndSequenceNumber()`,
then `kind` in enum order; per pair per step at most one of `ENTER`/`STAY` (or `TRIGGER_ENTER`) and at most one
`EXIT` (or `TRIGGER_EXIT`), the exit after the enter. Consumers reporting to `b` flip `normal` (`03_scripting.md` 3.5).

### 2.5 Queries (`queries.h`)

```cpp
struct query_filter {
    physics_layer_mask layers = physics_layer_mask::all();
    bool includeTriggers = false;
    bool includeStatic = true;
    ecs::entity ignore = ecs::NULL_ENTITY;            // typically the caster
};
struct raycast_hit {                 // all fields value-initialised in the header
    ecs::entity entity; math::vec3 point; math::vec3 normal; f32 distance; f32 fraction; u8 layer; bool isTrigger;
};
struct shape_cast_hit {
    ecs::entity entity; math::vec3 point; math::vec3 normal; f32 distance; f32 fraction; f32 penetrationDepth; u8 layer;
};

struct sphere_shape_desc { f32 radius = 0.5f; };
struct box_shape_desc { math::vec3 halfExtents = math::vec3(0.5f); };
struct capsule_shape_desc { f32 radius = 0.5f; f32 halfHeight = 0.5f; };
using cast_shape_desc = std::variant<sphere_shape_desc, box_shape_desc, capsule_shape_desc>;
```

### 2.6 Internal records (`engine/src/physics/body_record.h`, private)

```cpp
enum class record_kind : u8 { BODY, CHARACTER };
enum class record_dirty_flags : u8 { NONE = 0, SHAPE = 1 << 0, PROPERTIES = 1 << 1, LAYER = 1 << 2, MOTION = 1 << 3 };

struct body_record {                 // indexed by JPH::BodyID::GetIndex(); grown in 1024-row blocks up to maxBodies
    ecs::entity entity = {};
    JPH::BodyID bodyId = {};         // invalid when the row is free
    record_kind kind = record_kind::BODY;
    motion_type motion = motion_type::STATIC;
    u8 layer = 0;
    bool isTrigger = false;
    bool hasShape = false;
    std::atomic<bool> awake = false;             // written by the BodyActivationListener (Jolt threads), read by pull
    record_dirty_flags dirty = record_dirty_flags::NONE;
    u64 shapeKey = 0;                            // shape_cache key (0 = per-entity compound, not shared)
    rigid_body mirror = {};                      // last pushed/pulled component value (field diffing)
    math::transform lastPulled = {};             // world pose written by the last pull (teleport detection)
    math::vec3 lastScale = math::vec3(1.0f);     // world scale used to build the shape (characters: the render scale)
    bool staticSensor = false;                   // a static trigger: an active kinematic sensor in Jolt (P21)
    bool skipGroundCarry = false;                // CHARACTER rows: moved by a teleport since the last step (P20)
    math::vec3 carriedVelocity = math::vec3(0.0f);   // CHARACTER rows: the ground velocity step adds and pull removes (P20)
    JPH::Ref<JPH::CharacterVirtual> character;   // CHARACTER rows only
};
```

Other private state of `physics_world`: `std::vector<body_record> mRecords`; `core::flat_hash_map<ecs::entity, u32> mRowOfEntity`;
`std::vector<ecs::entity> mPendingCreate`, `mPendingRebuild`; `std::vector<body_command> mCommands` (P9); `shape_cache mShapes`
(`flat_hash_map<u64, JPH::RefConst<JPH::Shape>>`); `contact_collector mContacts` (3.6, per-thread buffers + merged `mRawMerged`);
`flat_hash_map<u64, contact_pair_state> mActivePairs` (`{u16 subShapePairs; bool trigger;}`); `std::vector<contact_event> mEvents`; radix-sort scratch; the Jolt
objects (`PhysicsSystem`, `TempAllocatorImpl`, `jolt_job_system`, `BroadPhaseLayerInterfaceTable(64, 2)`,
`ObjectVsBroadPhaseLayerFilterTable`, `ObjectLayerPairFilterTable(64)`, contact and activation listeners); `ecs::world *mWorld`
+ observer ids; `physics_stats mStats`; `std::atomic<bool> mStepping`; `std::thread::id mOwnerThread`. Every container is
reserved from `physics_settings` at construction (P15).

```cpp
struct physics_stats {
    u32 bodies = 0; u32 activeBodies = 0; u32 characters = 0; u32 bodiesWithoutShape = 0;
    u32 contactEvents = 0; u32 droppedContactEvents = 0; u32 droppedCommands = 0;
    u32 shapeCacheEntries = 0; u32 updateErrors = 0;                 // EPhysicsUpdateError != None count
    u64 stepMicroseconds = 0; u64 pushMicroseconds = 0; u64 pullMicroseconds = 0;
};
```

### 2.7 Collision mesh asset (`collision_mesh_asset.h`)

`conjure.physics.collision_mesh`: cooked payload `{ u32 vertexCount; u32 indexCount; f32 position[3][vertexCount]; u32 index[indexCount]; }`
(little-endian, 16-byte aligned sections). `class collision_mesh_asset` exposes `positions() -> std::span<const math::vec3>`,
`indices() -> std::span<const u32>`, `bounds() -> math::aabb`, `triangle_count()` and `[[nodiscard]] static core::result<collision_mesh_asset>
from_geometry(std::span<const math::vec3>, std::span<const u32>)` (validates indices, drops degenerate triangles; tests
and procedural terrain). `collision_mesh_importer` (`.gltf`, `.glb` via cgltf, `.obj` via a small reader) bakes node
transforms, merges primitives and welds vertices (`weldDistance`, default 1e-4 m), deterministically. It claims only
`.obj` by extension: a `.gltf` or `.glb` is a render model by extension (`conjure.renderer.gltf`) and becomes
collision geometry when its `.meta` names this importer (`asset_database::set_importer`, the editor's `asset.import` /
`asset.reimport` with `importer`). Both importers once claimed the glTF extensions and the one registered first,
this one, took every model (code sweep F137); `importer_registry` now warns when an extension is claimed twice. The Jolt shape
is built at load time on the asset job, not cooked: `JPH::MeshShape` binary state is version-bound and a 100k triangle
build is ~20 ms, inside the async budget of `05_assets.md`.

---

## 3. Algorithms and flow

### 3.1 Process and world lifetime

```
jolt_runtime::acquire()                     first caller only, under a mutex
  JPH::Allocate / Reallocate / Free   -> tracked_allocate(size + 16, 16) with the size in a 16-byte header;
                                         tracked_free(block - 16, header.size); reallocate = allocate + memcpy + free
  JPH::AlignedAllocate / AlignedFree  -> tracked_allocate(size + alignment, alignment), header at block - 16
  JPH::Trace = CONJURE_LOG_DEBUG(LOG_PHYSICS, ...)
  JPH::AssertFailed = log ERROR + CONJURE_ASSERT(false)                             [JPH_ENABLE_ASSERTS builds]
  JPH::Factory::sInstance = new JPH::Factory(); JPH::RegisterTypes();
physics_world::physics_world(settings, jobs)
  jobSystem.Init(settings.maxJobs, settings.maxBarriers, jobs)
  tempAllocator = make_unique<JPH::TempAllocatorImpl>(settings.tempAllocatorBytes)
  build layer tables from settings.layers (3.2)
  physicsSystem->Init(maxBodies, numBodyMutexes, maxBodyPairs, maxContactConstraints,
                      broadPhaseLayers, objectVsBroadPhase, pairFilter)
  physicsSystem->SetGravity; SetPhysicsSettings (deterministic, sleeping, steps, speculative distance)
  SetContactListener(&mContactListener); SetBodyActivationListener(&mActivationListener)
  reserve every container of 2.6 (P15)
physics_world::attach(ecs::world &)   observers: rigid_body / colliders / character_controller ADDED + REMOVED
physics_world::detach()               unobserve; destroy every body (scene unload reaches this through REMOVED anyway)
~physics_world                        detach if attached; Jolt system, allocator, job system; jolt_runtime::release()
```

`UnregisterTypes()` and the factory delete on the last release keep `memory_guard` clean across tests.

### 3.2 Layers to Jolt tables

`object_layer(layer, motion) = (motion == STATIC ? 0 : 32) + layer`. `BroadPhaseLayerInterfaceTable(64, 2)` maps
`0..31 -> NON_MOVING(0)`, `32..63 -> MOVING(1)`. `ObjectLayerPairFilterTable(64)`: for every `(a, b)` with
`layers.collides(a, b)` enable `(a+32, b)`, `(a, b+32)`, `(a+32, b+32)`; static-static stays disabled.
`ObjectVsBroadPhaseLayerFilterTable(broadPhaseInterface, 2, pairFilter, 64)` derives the rest. `layer >= layers.count()`
-> layer 0 + one WARN per entity (`UNKNOWN_LAYER`). Query masks become `layer_mask_object_filter : JPH::ObjectLayerFilter`
(`ShouldCollide = mask.includes(objectLayer % 32) && (includeStatic || objectLayer >= 32)`) plus a
`JPH::BroadPhaseLayerFilter` dropping `NON_MOVING` when `!includeStatic`. A static trigger takes
`object_layer(layer, STATIC)` like every static body although Jolt holds it as a kinematic body (P21): the disabled
static-static pairs are what keeps it from static geometry and from other static triggers, and `includeStatic` treats it
as static.

### 3.3 Body creation, shapes and destruction

Observer callbacks (owner thread, inside the structural call; they touch physics state only, never the ECS, except the
deferred add / remove of `scene::interpolated_transform` described in step 6 and `destroy_body`, R47):

- `ADDED` of `rigid_body`, any collider or `character_controller`: `mark_pending_create(entity)`, or
  `mark_rebuild(entity, SHAPE | PROPERTIES)` when a row already exists.
- `REMOVED` of a collider: `mark_rebuild(SHAPE)`, or `destroy_body(row)` when no body-defining component remains
  (`world.mask_of(entity)` minus the removed id). `REMOVED` of `rigid_body`: colliders remain -> rebuild as implicit
  STATIC, else destroy. `REMOVED` of `character_controller`: destroy the character and its inner body. Entity
  destruction fires `REMOVED` per column (`01_ecs.md` 3.1), but `world.mask_of(entity)` still holds every column
  while those observers run, so a body with more than one body-defining component would survive them;
  `observe_destroyed` (`on_entity_destroyed`) therefore destroys the body and the character of a destroyed entity.

`destroy_body(row)`: synthesise `EXIT`/`TRIGGER_EXIT` for every entry of `mActivePairs` involving `bodyId` into the
current step's event list (they are reported by the next `physics.contact_events`, so scripts see the exit one step
later, consistent with R6; each names the body's entity), erase those pairs, `BodyInterface::RemoveBody` +
`DestroyBody` (characters: inner body through the character, then `character = nullptr`), free the row, erase
`mRowOfEntity`; when the entity is still alive, `deferred.remove<scene::interpolated_transform>(entity)` so the
render pose stops following a dead body (R47). Late `OnContactRemoved` callbacks for unknown pairs are ignored.
When the body goes because its entity was destroyed, that entity is no longer alive when the exit is reported, so
consumers check `is_alive` before touching a participant (sweep F410). A body destroyed while its entity lives on
(its body components removed, a character swap, a rebuild that replaces it) names that live entity.

`flush_pending_creates()` (start of `push_transforms`, main thread, world unlocked or locked read-only):

1. For each pending entity still alive: read its components; decide `kind` (CHARACTER if `character_controller`),
   `motion` (`rigid_body.motionType`, or STATIC when absent), `layer`, `isTrigger`.
2. `build_shape(entity) -> result<shape_build>` (below). No colliders yet -> keep pending (counted in
   `bodiesWithoutShape`), re-examined next step; a pending entity with no body component at all is dropped.
3. World pose: `compose_world_pose(entity)` walks `scene::parent` + `scene::local_transform` to the root (O(depth), no
   allocation); position/rotation feed `BodyCreationSettings`, scale feeds the shape (`JPH::ScaledShapeSettings` when
   `scale != 1`; `Shape::IsValidScale` failures -> `INVALID_SHAPE_SCALE`, WARN, built unscaled).
4. `BodyCreationSettings s(shape, position, rotation, EMotionType, object_layer(layer, motion))` (`EMotionType` is the
   motion type, except `Kinematic` for a static trigger, whose velocities are created zero, P21) with
   `mUserData = entity.to_u64()`, damping, `mGravityFactor`, `mMaxLinearVelocity`, `mMotionQuality = LinearCast` when
   `CONTINUOUS_COLLISION`, `mAllowSleeping`, `mIsSensor`, `mFriction/mRestitution` from `physics_material` (0.5 / 0.0),
   `mAllowedDOFs` from the lock flags, `mApplyGyroscopicForce`, initial velocities from the component (a saved scene
   resumes), `mOverrideMassProperties = CalculateInertia` + `mMassPropertiesOverride.mMass = mass` when `mass > 0`
   (else `CalculateMassAndInertia`), `mCollideKinematicVsNonDynamic = isTrigger && motion == KINEMATIC`.
5. `CreateBody` per entity, then one `AddBodiesPrepare/AddBodiesFinalize` for the batch (`DontActivate` for STATIC
   and `START_ASLEEP`, else `Activate`; a static trigger is always added with `Activate`, P21). A null `CreateBody` ->
   `BODY_LIMIT_REACHED`, ERROR once per step, entity stays pending.
6. Rows: `mRecords[bodyId.GetIndex()] = {...}`, `mirror = rigid_body`, `lastPulled = pose`, `awake` from
   activation. `scene::interpolated_transform` is defined and registered by `scene` (`13_scene.md` 2.1) but added
   only by physics: the `ADDED` observer of a `DYNAMIC`/`KINEMATIC` `rigid_body` or of a `character_controller`
   enqueues `deferred.add<scene::interpolated_transform>(entity)`, and the first `pull` writes
   `previous = current = pose`, so nothing interpolates from the origin; a `rigid_body` that becomes `STATIC` or a
   destroyed body on a living entity removes it again (R47; `06_engine_loop.md` 3.2 describes the same rule).
7. If more than `max(64, bodies / 20)` bodies were added since the last call, `PhysicsSystem::OptimizeBroadPhase()`
   (scene load path; cheap otherwise).

`build_shape(entity)`: collect colliders in a fixed order (box, sphere, capsule, convex, mesh; one of each type per
entity, as the ECS stores one component per type). A single collider with zero offset and identity rotation (`same_rotation` at the default 1e-6, P19) is the
leaf itself; otherwise `JPH::StaticCompoundShapeSettings` + `AddShape(offset, rotation, leaf)` (a collider change
rebuilds). Leaves: `BoxShapeSettings(halfExtents, min(convexRadius, minHalfExtent * 0.5))`, `SphereShapeSettings(radius)`,
`CapsuleShapeSettings(halfHeight, radius)`, `ConvexHullShapeSettings(points, count, convexRadius)`,
`MeshShapeSettings(vertexList, triangleList)`. A `ShapeSettings::Create()` error -> `SHAPE_CREATE_FAILED` with Jolt's
message; the body is built without that collider. Leaves are cached in `shape_cache` by `fnv1a64(kind, quantised dims,
asset uuid + version, scale)`; compounds are per entity; `collect_shapes()` drops entries with Jolt refcount 1.
Unloaded `asset_ref`s keep the body pending (`bodiesWithoutShape++`); `on_asset_reloaded` marks dependants `SHAPE` dirty.

Rebuild (`SHAPE` dirty): `BodyInterface::SetShape(bodyId, shape, updateMassProperties = mass == 0, Activate)`.
`PROPERTIES` dirty: field diff against `mirror` -> `SetFriction`, `SetRestitution`, `SetGravityFactor`, damping via
`BodyLockWrite` + `MotionProperties::SetLinearDamping/SetAngularDamping`, `SetMotionQuality`. `MOTION` dirty ->
`SetMotionType(bodyId, type, Activate)` + `SetObjectLayer`. `LAYER` dirty -> `SetObjectLayer`.

Both the `MOTION` path and a `SHAPE` rebuild end in `sync_jolt_motion(row)` (P21), which sets the sensor flag,
`mCollideKinematicVsNonDynamic` (kinematic triggers only) and the Jolt motion type from `(row.motion, row.isTrigger)`: a
body that becomes static is put to sleep, one that starts to move is woken, a static trigger is made `Kinematic`,
activated and stopped (`SetLinearAndAngularVelocity(0, 0)`: the body may have been moving, and a static body keeps the
velocities it was created with), and a body that keeps its Jolt motion type keeps its sleep state. An implicit static
body that was no trigger has no motion properties (`mAllowDynamicOrKinematic = false`) and cannot become kinematic:
when a collider change turns it into a trigger the rebuild destroys it (exits synthesised) and queues it for creation,
which the flush of the same push performs.

### 3.4 `physics.push_transforms` (PHYSICS_STEP, `MAIN_THREAD`, `before("physics.step")`)

```
clear mEvents (R6: the previous step's span dies here); mStats.contactEvents = 0
flush_pending_creates(); apply dirty rebuilds (3.3)
for each chunk of view<const rigid_body, const scene::local_transform, maybe<const scene::parent>>
        .changed<rigid_body>(sinceVersion)
  or .changed<scene::local_transform>(sinceVersion):                 // chunk-granular, so compare per row
    row = mRowOfEntity[entity]; skip if none
    if rigid_body differs from row.mirror:
        velocities differ -> SetLinearVelocity / SetAngularVelocity (the user wrote them)
        motion / layer / flags / mass / damping differ -> dirty bits, applied now
        row.mirror = rigid_body
    pose = compose_world_pose(entity)
    if row.motion == KINEMATIC && KINEMATIC_MOVES_WITH_TRANSFORM:
        BodyInterface::MoveKinematic(bodyId, pos, rot, fixedDeltaTime)
    else if !nearly_equal(pose.position, lastPulled.position, TELEPORT_EPSILON)
         || !same_rotation(pose.rotation, lastPulled.rotation, TELEPORT_EPSILON):   // per component, P19
        teleport: SetPositionAndRotation(bodyId, pos, rot, Activate); lastPulled = pose
        interpolated.previous = interpolated.current = pose
    if pose.scale != row.lastScale: rebuild the shape at the new scale (allocates; editor-time operation)
characters (changed character_controller or local_transform; parented ones every push): authored fields differ ->
    rebuild in place; position differs from lastPulled -> teleport: SetPosition / SetRotation, carriedVelocity = 0,
    skipGroundCarry = true (P20); only the rotation differs -> SetRotation, the carrying stays;
    row.lastScale = pose.scale (no rebuild: the capsule is not scaled, the pull writes the scale into the
    render pose)
for each queued body_command in order (P9): AddForce / AddImpulse / AddTorque / SetLinearVelocity / ActivateBody /
    DeactivateBody / SetPositionAndRotation (+ interpolated reset) / MoveKinematic(pose, fixedDeltaTime) (R49) /
    PhysicsSystem::SetGravity (R49) / character desired velocity; a command on a row
    that does not exist is kept one step while the entity is pending, else dropped (droppedCommands++);
    a command the row's kind cannot take (command_applies, P18) is dropped (droppedCommands++): the call
    refused it when the body existed, so this is one queued for a pending entity or one whose target
    changed kind since the call
mCommands.clear()
```

The call side of P18 (`physics_world::state::queue`): `WRONG_THREAD`, then `INVALID_ARGUMENT` for a non-finite
value, then `NO_BODY` when there is neither a row nor a pending entry, then, when a row exists,
`command_applies(command, target_of_components(entity))` or `WRONG_BODY_KIND` (`NOT_A_CHARACTER` for
`character_move` in both places), then `COMMAND_QUEUE_FULL`.

| Command | static, colliders alone | kinematic | kinematic, moves with transform | dynamic | character |
|---|---|---|---|---|---|
| `add_force(_at)`, `add_impulse(_at)`, `add_torque`, `add_angular_impulse` | no | no | no | yes | no |
| `set_linear_velocity`, `set_angular_velocity` | no | yes | yes (the next push re-poses the body from its transform) | yes | no |
| `move_kinematic` | no | yes | no | no | no |
| `teleport` | yes | yes | yes | yes | yes |
| `wake`, `sleep` | accepted, no-op | yes | yes | yes | accepted, no-op |
| `character_move` | no | no | no | no | yes |

`compose_world_pose` reads other entities' `local_transform` (ancestors). Push is `MAIN_THREAD` in a stage whose
other systems may not write `local_transform` (scheduler conflict rules), so the reads are race-free. Change
detection is chunk-granular (`01_ecs.md` D5); the field diff makes it exact per row.

### 3.5 `physics.step` (PHYSICS_STEP, `MAIN_THREAD`, `after("physics.push_transforms")`)

```
CONJURE_ASSERT(core::jobs::is_main_thread()); mStepping = true
dt = context.frame().fixedDeltaTime
for each CHARACTER row (table order = creation order, deterministic):
    settings = ExtendedUpdateSettings{ stickToFloor ? (0, -stepHeight, 0) : 0, walkStairs ? (0, stepHeight, 0) : 0 }
    velocity = queued character_move, else component.velocity          // the character's own (P20)
    grounded = character->GetGroundState() == OnGround                 // as the last update left it
    if row.skipGroundCarry: row.skipGroundCarry = false                // teleported: carriedVelocity is zero
    else if grounded: character->UpdateGroundVelocity()
                      row.carriedVelocity = horizontal part of character->GetGroundVelocity()
    else if ground state is OnSteepGround or NotSupported: row.carriedVelocity = 0   // ground that does not carry
    if APPLY_GRAVITY: grounded ? (vertical follows the ground's unless 0.1 m/s faster upwards: a jump)
                               : velocity += gravity * dt              // 'grounded' is stale after a teleport
    velocity += row.carriedVelocity                                    // kept while InAir: a jump keeps it
    velocity = character->CancelVelocityTowardsSteepSlopes(velocity); character->SetLinearVelocity(velocity)
    character->ExtendedUpdate(dt, gravity, settings, broadPhaseFilter(layer), objectFilter(layer),
                              ignoreInnerBody, {}, *tempAllocator)
error = physicsSystem->Update(dt, settings.collisionSteps, tempAllocator.get(), &mJobSystem)
if error != EPhysicsUpdateError::None:
    mStats.updateErrors++; rate-limited WARN naming ManifoldCacheFull / BodyPairCacheFull / ContactConstraintsFull
mStepping = false
```

Characters update before the body step so their pushes on dynamic bodies resolve in the same step (Jolt's sample
ordering); updates are sequential in v1. `physics_world::step(f32)` is the entry used by the system and by tests.

### 3.6 Contact collection and `physics.contact_events` (FIXED_END, `MAIN_THREAD`)

`jolt_contact_listener` (`contact_collector.h`): `OnContactValidate` returns `AcceptAllContactsForThisBodyPair`;
`OnContactAdded/Persisted(body1, body2, manifold, settings)` append
`raw_contact { u32 bodyA, bodyB; u8 subShapeKind (ADDED/PERSISTED); vec3 point = manifold.GetWorldSpaceContactPointOn1(0);
vec3 normal = manifold.mWorldSpaceNormal; u8 pointCount; f32 impulse; }` to `mBuffers[job_system::thread_index()]`
(`std::vector<raw_contact>` reserved to `maxContactEventsPerStep`, `push_back` only while `size() < capacity()`,
else `droppedContactEvents++`); `impulse` is computed in `OnContactAdded` only, when `estimateContactImpulses`,
with `JPH::EstimateCollisionResponse(body1, body2, manifold, result, settings.mCombinedFriction,
settings.mCombinedRestitution)` summing `result.mImpulses[i].mContactImpulse`. `OnContactPersisted` records nothing
when either body is a sensor: a trigger pair has no `STAY`, and a body inside a static trigger is reported as persisted
in every step, asleep too (P21), which would spend one record of the budget per body and step. `OnContactRemoved(subShapePair)` appends
`{ pair.GetBody1ID(), pair.GetBody2ID(), REMOVED }`. Bodies are never dereferenced in `OnContactRemoved` (Jolt forbids it).
`jolt_activation_listener::OnBodyActivated/Deactivated(bodyId, userData)` stores `mRecords[index].awake` (relaxed).

`collect_contact_events()` (main thread, after the step):

1. Concatenate the per-thread buffers into `mRawMerged`; keys `(u64(bodyA) << 32) | bodyB`, values = raw indices;
   `core::radix_sort_u64(keys, values, scratchKeys, scratchValues)` (stable, caller scratch, allocation-free).
2. Walk runs of equal key; per run update `mActivePairs[key]`: `ADDED` -> `subShapePairs++`, `REMOVED` ->
   `subShapePairs--`, `PERSISTED` -> nothing. Emit:
   - pair absent before the run and `subShapePairs > 0` after it -> `ENTER` (`TRIGGER_ENTER` if either row
     `isTrigger`), `point/normal/impulse/contactCount` from the first `ADDED` record (impulses summed, points counted);
   - pair present before and still > 0 -> `STAY` (bodies only; triggers emit nothing while overlapping);
   - `subShapePairs == 0` after the run -> `EXIT`/`TRIGGER_EXIT` (after the ENTER if both happened), pair erased.
   `entity` and `layer` fields come from the rows (destroyed bodies already got their synthesised exits in 3.3; unknown
   indices are skipped).
3. Clear the per-thread buffers. The result is sorted by construction (key order, then kind order).

Sleeping (P21): Jolt keeps a contact only while one body of the pair is active, so it fires `OnContactRemoved` when the
last active one falls asleep and `OnContactAdded` when one wakes. A resting stack of solid bodies therefore reports
`EXIT` once at sleep and `ENTER` once at wake although nothing parted (Unity does not: it sends no `OnCollisionExit` at
sleep; here the behaviour is kept and documented, and a body whose contacts must stay reported clears
`ALLOW_SLEEPING`); counting tests disable `ALLOW_SLEEPING`. Triggers differ. A static trigger is an active kinematic
sensor, so its pair with a body that falls asleep keeps being found every step: no `TRIGGER_EXIT` at sleep, no second
`TRIGGER_ENTER` at wake, and a body that is asleep when the trigger is created or teleported over it gets its
`TRIGGER_ENTER`. A kinematic or dynamic trigger behaves the same while it is active; asleep (`START_ASLEEP`, `sleep`) it
pairs with active bodies only.

### 3.7 `physics.pull_transforms` (FIXED_END, PARALLEL, `before("physics.contact_events")`)

Declared access: writes `scene::local_transform`, `scene::interpolated_transform`, `rigid_body`, `character_controller`;
reads `scene::parent`. The view is `view<rigid_body, scene::interpolated_transform, scene::local_transform, maybe<const scene::parent>>`
plus a second pass for characters.

```
parallel over chunks (context.parallel_for_chunks):
    for each row: record = row of entity (mRowOfEntity lookup, read-only after push)
        if record.motion == STATIC or (KINEMATIC with KINEMATIC_MOVES_WITH_TRANSFORM) : skip
        if !record.awake and record.mirror.isSleeping: continue   // already written as sleeping: one branch per body
        BodyInterfaceNoLock::GetPositionAndRotation(bodyId, position, rotation)   // no step runs; NoLock is safe
        interpolated.previous = interpolated.current; interpolated.current = {position, rotation, current.scale}
        record.lastPulled = interpolated.current
        rigid_body.linearVelocity / angularVelocity = Get*Velocity; isSleeping = !awake; record.mirror = rigid_body
        if parent is null: local_transform.position/rotation = current (scale untouched)
        else: push the row index into the per-grain "parented" list (small_vector<u32, 16>, arena overflow)
serial tail on the calling thread: for each parented row, parentWorld = compose_world_pose(parent);
    local = parentWorld.inverse() * current (scale kept)
characters (serial, calling thread): position = character->GetPosition(); ground = GetGroundState(); groundNormal;
    groundEntity from the ground BodyID's row; velocity = GetLinearVelocity() - row.carriedVelocity (P20: Jolt keeps the
    velocity the step set, so this is the character's own again); same interpolated/local write as a root, with
    scale = row.lastScale
```

The serial tail exists because a parent may itself be a dynamic body whose `local_transform` another grain is writing;
roots (the overwhelming case) stay parallel. Writing `local_transform` stamps the chunk, so `scene.propagate_transforms`
recomputes `world_transform` and the interpolated `render_world_transform` (`06_engine_loop.md` 3.2); push (3.4) does
not mistake that write for a teleport because it compares against `lastPulled`.

### 3.8 Queries

All `const`, callable from any thread while `!mStepping` (`STEP_IN_PROGRESS` assert + empty result otherwise).

- `raycast`: `RRayCast { origin, direction * maxDistance }`, `NarrowPhaseQuery::CastRay(ray, hit, broadPhaseFilter,
  objectFilter, ignoreBodyFilter)` (closest); normal via `BodyLockRead` + `Body::GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, point)`.
- `raycast_all`: `CastRay(ray, RayCastSettings{}, collector, ...)` with a fixed-capacity `span_collector<CastRayCollector>`
  writing into `out` (`ForceEarlyOut` when full), insertion-sorted by fraction in place; returns the count.
- `overlap_sphere/overlap_box`: embedded `JPH::SphereShape` / `JPH::BoxShape` (`SetEmbedded()`, stack lifetime),
  `CollideShape(&shape, Vec3::sOne(), centerOfMassTransform, CollideShapeSettings{}, center, collector, ...)`; one
  entity per body (the collector deduplicates `mBodyID2`).
- `shape_cast`: `RShapeCast::sFromWorldTransform(&embedded, Vec3::sOne(), start.to_matrix(), direction)`,
  `CastShape(cast, ShapeCastSettings{}, start.position, closestHitCollector, ...)`; `distance = fraction * length(direction)`.
- Filters: `layer_mask_object_filter`, `broad_phase_mask_filter` (3.2), `JPH::IgnoreSingleBodyFilter(row.bodyId)` for
  `filter.ignore`; sensors skipped by the collector unless `includeTriggers`. Entities come from `Body::GetUserData()`.

### 3.9 Sleeping and activation

`is_sleeping(entity)` reads the row's `awake` atomic and is true for every static row; `wake`/`sleep` queue
`ACTIVATE`/`DEACTIVATE` commands, which do nothing on a static row;
`active_body_count()` = `PhysicsSystem::GetNumActiveBodies(EBodyType::RigidBody)` less the static triggers, which Jolt
keeps active (P21) and the engine counts as static (the world keeps their number). Thresholds come from
`physics_settings`; `ALLOW_SLEEPING` off per body disables sleep for that body only.

### 3.10 Debug draw

`physics_world::debug_draw(sink, flags)` wraps the sink in a `debug_renderer_adapter : JPH::DebugRendererSimple`
(`DrawLine`, `DrawTriangle`, `DrawText3D` forward; colours `0xAABBGGRR`), fills `JPH::BodyManager::DrawSettings` from the
flags (`mDrawShape`, `mDrawShapeWireframe`, `mDrawBoundingBox`, `mDrawCenterOfMassTransform`, `mDrawVelocity`,
`mDrawSleepStats`, `mDrawShapeColor = MotionTypeColor`) and calls `PhysicsSystem::DrawBodies(settings, &adapter)`;
characters draw through `Shape::Draw`. An empty function without `JPH_DEBUG_RENDERER` (Distribution). The engine's
`physics_subsystem` calls it in EXTRACT when the editor toggles the overlay.

---

## 4. Public API

| Header | Main types |
|---|---|
| `physics.h` | umbrella include |
| `log_category.h` | `CONJURE_DECLARE_LOG_CATEGORY(LOG_PHYSICS)` |
| `physics_error.h` | `physics_error`, `physics_error_name`, `CONJURE_DECLARE_ERROR_CATEGORY` |
| `physics_settings.h` | `physics_settings` |
| `physics_layers.h` | `MAX_PHYSICS_LAYERS`, `physics_layer_mask`, `physics_layer_table` |
| `components.h` | `motion_type`, `rigid_body_flags`, `rigid_body`, `box_collider`, `sphere_collider`, `capsule_collider`, `convex_collider`, `mesh_collider`, `physics_material` |
| `character_controller.h` | `ground_state`, `character_flags`, `character_controller` |
| `contact_events.h` | `contact_kind`, `contact_event` |
| `queries.h` | `query_filter`, `raycast_hit`, `shape_cast_hit`, `*_shape_desc`, `cast_shape_desc` |
| `collision_mesh_asset.h` | `collision_mesh_asset`, `COLLISION_MESH_ASSET_TYPE` (`"conjure.physics.collision_mesh"`) |
| `physics_debug_draw.h` | `physics_debug_draw`, `physics_debug_draw_flags` |
| `physics_world.h` | `physics_world`, `physics_stats`, `body_command` (opaque to users) |
| `register.h` | `register_components`, `register_systems`, `register_asset_types`, system name constants |

Private files (`engine/src/physics/`): `jolt_runtime`, `jolt_memory`, `jolt_job_system`, `jolt_conversions.h`, `jolt_layers`,
`body_record.h`, `shape_builder`, `shape_cache`, `contact_collector`, `physics_world.cpp`, `physics_world_bodies.cpp` (3.3),
`physics_world_sync.cpp` (3.4, 3.7), `physics_world_queries.cpp`, `physics_world_debug.cpp`, `physics_systems.cpp`,
`collision_mesh_asset.cpp`, `collision_mesh_importer`, `log_category.cpp`, `README.md`. Jolt headers are included only there.

### 4.1 `physics_error.h`

`enum class physics_error : u16` with values (one per line in the header) `NONE = 0`, `INVALID_SETTINGS`,
`LAYER_CONFIG_INVALID`, `LAYER_LIMIT_REACHED`, `DUPLICATE_LAYER`, `UNKNOWN_LAYER`, `BODY_LIMIT_REACHED`, `NO_BODY`,
`NOT_A_CHARACTER`, `NOT_ATTACHED`, `ALREADY_ATTACHED`, `SHAPE_CREATE_FAILED`, `INVALID_SHAPE_SCALE`,
`UNSUPPORTED_SHAPE_FOR_MOTION`, `MIXED_TRIGGER_FLAGS`, `CONFLICTING_BODY_COMPONENTS`, `GEOMETRY_INVALID`,
`COLLISION_MESH_PARSE_FAILED`, `STEP_IN_PROGRESS`, `WRONG_THREAD`, `COMMAND_QUEUE_FULL`, `WRONG_BODY_KIND` (P18,
appended so the earlier values did not move); plus
`[[nodiscard]] std::string_view physics_error_name(physics_error code) noexcept;` and
`CONJURE_DECLARE_ERROR_CATEGORY(conjure::physics::physics_error, "physics", conjure::physics::physics_error_name);`.
Errors are built with `core::make_error(physics_error::X, "...")` (no module-local `make_error`, per `error.h`).

### 4.2 `physics_world.h`

```cpp
class physics_world {                // non-copyable, non-movable (Jolt listeners hold `this`)
    public:
        physics_world(const physics_settings &settings, core::jobs::job_system &jobs);
        ~physics_world();

    public:
        // ECS binding (owner thread of the ecs::world).
        [[nodiscard]] core::result<void> attach(ecs::world &target);
        void detach();
        [[nodiscard]] bool is_attached() const noexcept;

        // Step pipeline; the systems of register_systems call exactly these, tests may call them directly.
        void push_transforms(f32 fixedDeltaTime, ecs::command_buffer *deferred = nullptr);   // 3.4 (pending creation; the system passes its buffer, R47)
        void step(f32 fixedDeltaTime);                                              // 3.5
        void pull_transforms(ecs::system_context *context = nullptr);               // 3.7 (nullptr => inline loop)
        void collect_contact_events();                                              // 3.6
        [[nodiscard]] std::span<const contact_event> contact_events() const noexcept;   // until the next push (R6)

        // Bodies (queued, applied at the next push_transforms, 3.4). Main thread. WRONG_BODY_KIND at the call when
        // the entity's kind of body cannot take the command (P18, the table of 3.4).
        [[nodiscard]] bool has_body(ecs::entity target) const noexcept;
        [[nodiscard]] core::result<void> add_force(ecs::entity target, const math::vec3 &force);
        [[nodiscard]] core::result<void> add_force_at(
          ecs::entity target,
          const math::vec3 &force,
          const math::vec3 &point
        );
        [[nodiscard]] core::result<void> add_impulse(ecs::entity target, const math::vec3 &impulse);
        [[nodiscard]] core::result<void> add_impulse_at(
          ecs::entity target,
          const math::vec3 &impulse,
          const math::vec3 &point
        );
        [[nodiscard]] core::result<void> add_torque(ecs::entity target, const math::vec3 &torque);
        [[nodiscard]] core::result<void> add_angular_impulse(ecs::entity target, const math::vec3 &impulse);
        [[nodiscard]] core::result<void> set_linear_velocity(ecs::entity target, const math::vec3 &velocity);
        [[nodiscard]] core::result<void> set_angular_velocity(ecs::entity target, const math::vec3 &velocity);
        [[nodiscard]] core::result<void> teleport(ecs::entity target, const math::transform &pose);   // scale ignored; resets velocities and interpolated_transform
        [[nodiscard]] core::result<void> move_kinematic(ecs::entity target, const math::transform &pose);   // KINEMATIC without KINEMATIC_MOVES_WITH_TRANSFORM only: MoveKinematic over the next fixed step (physics_move_kinematic, R49)
        [[nodiscard]] core::result<void> set_gravity(const math::vec3 &gravity);   // queued: PhysicsSystem::SetGravity at the next push (physics_set_gravity, R49)
        [[nodiscard]] math::vec3 gravity() const noexcept;                          // value after the last applied set_gravity (or settings().gravity)
        [[nodiscard]] core::result<void> wake(ecs::entity target);                  // no-op on a static body and on a character
        [[nodiscard]] core::result<void> sleep(ecs::entity target);                 // no-op on a static body and on a character
        [[nodiscard]] bool is_sleeping(ecs::entity target) const noexcept;          // false when no body
        [[nodiscard]] core::result<void> character_move(ecs::entity target, const math::vec3 &velocity);   // NOT_A_CHARACTER
        [[nodiscard]] std::optional<ground_state> character_ground(ecs::entity target) const noexcept;

        // Queries (3.8). Any thread while no step runs.
        [[nodiscard]] std::optional<raycast_hit> raycast(
          const math::ray &ray,
          f32 maxDistance,
          const query_filter &filter = {}
        ) const;
        [[nodiscard]] u32 raycast_all(
          const math::ray &ray,
          f32 maxDistance,
          std::span<raycast_hit> out,
          const query_filter &filter = {}
        ) const;
        [[nodiscard]] u32 overlap_sphere(
          const math::vec3 &center,
          f32 radius,
          std::span<ecs::entity> out,
          const query_filter &filter = {}
        ) const;
        [[nodiscard]] u32 overlap_box(
          const math::vec3 &center,
          const math::vec3 &halfExtents,
          const math::quat &rotation,
          std::span<ecs::entity> out,
          const query_filter &filter = {}
        ) const;
        [[nodiscard]] std::optional<shape_cast_hit> shape_cast(
          const cast_shape_desc &shape,
          const math::transform &start,
          const math::vec3 &direction,                                            // includes length
          const query_filter &filter = {}
        ) const;

        // Assets, shapes, introspection.
        void on_asset_reloaded(assets::asset_id id);                                // dependants become SHAPE dirty
        void collect_shapes();                                                      // drop unreferenced cached shapes
        [[nodiscard]] const physics_settings &settings() const noexcept;
        [[nodiscard]] const physics_layer_table &layers() const noexcept;
        [[nodiscard]] physics_stats stats() const noexcept;
        [[nodiscard]] u32 body_count() const noexcept;
        [[nodiscard]] u32 active_body_count() const noexcept;
        [[nodiscard]] std::optional<math::transform> body_pose(ecs::entity target) const noexcept;   // Jolt truth
        [[nodiscard]] math::aabb bounds() const noexcept;                           // PhysicsSystem::GetBounds
        void debug_draw(physics_debug_draw &sink, physics_debug_draw_flags flags) const;   // 3.10
        [[nodiscard]] core::result<void> validate() const;                          // rows <-> entities <-> Jolt agree
};
```

### 4.3 `register.h`

```cpp
inline constexpr core::string_id SYSTEM_PUSH_TRANSFORMS{"physics.push_transforms"};
inline constexpr core::string_id SYSTEM_STEP{"physics.step"};
inline constexpr core::string_id SYSTEM_PULL_TRANSFORMS{"physics.pull_transforms"};
inline constexpr core::string_id SYSTEM_CONTACT_EVENTS{"physics.contact_events"};

[[nodiscard]] core::result<void> register_components(ecs::component_registry &registry);   // eight types; idempotent
[[nodiscard]] core::result<void> register_systems(ecs::scheduler &scheduler, physics_world &world);   // NOT_ATTACHED
[[nodiscard]] core::result<void> register_asset_types(
  assets::asset_type_registry &types,
  assets::importer_registry &importers
);
```

`register_systems` adds `physics.push_transforms` (PHYSICS_STEP, `MAIN_THREAD`, reads `rigid_body`, colliders,
`physics_material`, `character_controller`, `scene::local_transform`, `scene::parent`; writes `scene::interpolated_transform`;
`before = {SYSTEM_STEP}`), `physics.step` (PHYSICS_STEP, `MAIN_THREAD`, no component access), `physics.pull_transforms`
(FIXED_END, PARALLEL, access of 3.7, `before = {SYSTEM_CONTACT_EVENTS}`) and `physics.contact_events` (FIXED_END,
`MAIN_THREAD`, no component access). None is `RUNS_WHILE_PAUSED`: the fixed loop does not run while PAUSED (R13).

### 4.4 `physics_debug_draw.h`

```cpp
enum class physics_debug_draw_flags : u16 {
    NONE = 0, SHAPES = 1 << 0, WIREFRAME = 1 << 1, BOUNDING_BOXES = 1 << 2, CENTER_OF_MASS = 1 << 3,
    VELOCITIES = 1 << 4, SLEEP_STATE = 1 << 5, CHARACTERS = 1 << 6, CONTACTS = 1 << 7
};
CONJURE_ENUM_FLAGS(physics_debug_draw_flags);

class physics_debug_draw {           // implemented by the engine over renderer debug draw; by tests with counters
    public:
        virtual ~physics_debug_draw();
    public:
        virtual void line(const math::vec3 &from, const math::vec3 &to, u32 rgba) = 0;
        virtual void triangle(const math::vec3 &a, const math::vec3 &b, const math::vec3 &c, u32 rgba) = 0;
        virtual void text(const math::vec3 &position, std::string_view text, u32 rgba, f32 height);   // default: no-op
};
```

---

## 5. Threading and ownership

1. **Owner thread.** `physics_world` captures the constructing thread (the ECS world's owner, the main thread in the
   engine); `attach`, `push_transforms`, `step`, `collect_contact_events`, the queued body API and `detach` are
   owner-thread only (`CONJURE_ASSERT` + `WRONG_THREAD` where a result is returned).
2. **During `step`** Jolt owns every body: `jolt_job_system` fans jobs out to `core::jobs` workers and the calling
   thread executes jobs inside `JobSystemWithBarrier::WaitForJobs`; listeners run on those threads and write only their
   per-thread buffer or a row's atomic. No ECS access and no query happens inside `step` (`STEP_IN_PROGRESS`).
3. **`pull_transforms`** is PARALLEL: workers read Jolt through `BodyInterfaceNoLock` (safe: no step) and write only
   their chunk; parented rows and characters finish on the calling thread. **Queries** are `const` and thread-safe
   outside a step (`NarrowPhaseQuery` takes body read locks; results go into caller memory).
4. **Ownership.** `physics_world` owns the Jolt system, temp allocator, job adapter, layer tables, listeners, records,
   shape cache (Jolt `Ref`s), command queue and event buffers; `jolt_runtime` owns process state by reference count;
   `collision_mesh_asset` payloads belong to `assets::asset_manager` slots (physics holds `asset_ref`s and
   `RefConst<Shape>`s, never raw payload pointers). The non-owning `ecs::world *` is valid between `attach` and `detach`
   (startup 9 < 12, shutdown reverse, `06_engine_loop.md` 3.4/3.5). No raw owning pointers; the only `new` is Jolt's factory (P4).
5. **Shared state:** per-thread contact buffers (Jolt threads during `step`, main thread after); `body_record::awake` and
   `mStepping` (atomics); everything else owner-thread or read-only during the PARALLEL pull.

---

## 6. Failure handling

| Situation | Behaviour |
|---|---|
| `physics_settings::validate` fails | `INVALID_SETTINGS` naming the field; the engine maps it to `PHYSICS_INIT_FAILED` |
| `extensions.physics_layers` malformed | `LAYER_CONFIG_INVALID` / `LAYER_LIMIT_REACHED` / `DUPLICATE_LAYER` / `UNKNOWN_LAYER` from `from_json`; the subsystem fails startup (content error, loud) |
| `rigid_body.layer >= layers.count()` | body on layer 0, one WARN per entity, `UNKNOWN_LAYER` in `validate()` |
| `CreateBody` returns null (limit) | `BODY_LIMIT_REACHED` logged ERROR once per step; entity stays pending; `stats.bodiesWithoutShape` |
| Collider shape fails (`ShapeSettings::Create` error, bad scale) | `SHAPE_CREATE_FAILED` / `INVALID_SHAPE_SCALE` WARN with Jolt's text; body built without that collider (or unscaled) |
| `mesh_collider` on a DYNAMIC body | `UNSUPPORTED_SHAPE_FOR_MOTION` WARN; collider ignored |
| Trigger and non-trigger colliders on one body | `MIXED_TRIGGER_FLAGS` WARN; body is non-trigger |
| `rigid_body` and `character_controller` on one entity | `CONFLICTING_BODY_COMPONENTS` WARN; character wins |
| Body API on entity without body | `NO_BODY` (if the entity is pending, the command is kept one step and `NO_BODY` is not returned) |
| Body API on a body of a kind the command cannot act on (P18) | `WRONG_BODY_KIND` at the call, nothing queued; `character_move` on a body that is no character: `NOT_A_CHARACTER`. For an entity that was pending at the call, or whose kind changed before the push: dropped at the push, `stats.droppedCommands++` |
| Command queue full | `COMMAND_QUEUE_FULL`, `stats.droppedCommands++`, rate-limited WARN |
| Contact buffer full | records dropped, `stats.droppedContactEvents++`, rate-limited WARN suggesting `maxContactEventsPerStep` |
| `PhysicsSystem::Update` returns an error flag | `stats.updateErrors++`, rate-limited WARN with the flag name and the setting to raise; simulation continues |
| Query during a step / owner-thread API off thread | `CONJURE_ASSERT` + empty result (`STEP_IN_PROGRESS`) / `CONJURE_ASSERT` + `WRONG_THREAD` |
| `register_systems` before `attach`; `attach` twice | `NOT_ATTACHED`; `ALREADY_ATTACHED` |
| Collision mesh with bad indices / zero triangles | `GEOMETRY_INVALID` from the importer or `from_geometry`; asset enters the FAILED state of `05_assets.md`, dependent bodies stay pending with WARN |
| glTF/OBJ parse failure | `COLLISION_MESH_PARSE_FAILED` with file and cgltf/OBJ line |
| Jolt assert (`JPH_ENABLE_ASSERTS` builds) | logged ERROR with expression/file/line, `CONJURE_ASSERT(false)`; Distribution compiles Jolt without asserts |
| Entity destroyed while pending; `detach` with live bodies | dropped at the next flush; all bodies destroyed, synthesised exits discarded |

Everything asserted also returns an error or degrades, so Distribution behaviour is defined. `physics_world::validate()`
checks: every row's `entity` is alive and `mRowOfEntity[entity]` points back, `Body::GetUserData() == entity.to_u64()`
for every body, `body_count()` equals the Jolt body count, no pair in `mActivePairs` references a free row, every
cached shape has `GetRefCount() >= 1`.

---

## 7. Headless and determinism

Jolt has no device dependency; the module runs identically in the editor, the runtime, `--headless` and tests - no
null backend exists or is needed. What matters for tests:

- 0 workers make every Jolt job run inline inside `QueueJob` (P2); `tests/physics/physics_test_fixture.h` offers
  `for_each_job_mode(callable)` running a body with 0 and 4 workers, mirroring `01_ecs.md` 7.
- With `deterministic = true` the same `push/step/pull` sequence on the same binary yields bit-identical poses for any
  worker count (Jolt guarantee, P16) and bit-identical `contact_events()` (sorted). Cross-platform bit equality needs
  `CONJURE_PHYSICS_CROSS_PLATFORM_DETERMINISTIC=ON` plus `-ffp-contract=off` / `/fp:precise`; CI does not enable it.
- Tests drive `physics_world` directly at `1/60` or through `scheduler::run_phase(PHYSICS_STEP / FIXED_END, frame_info)`;
  the `headless_engine` fixture (`08_testing.md` 4.4) is used only by `tests/integration`.
- Debug draw works headless (counting sink); `JPH_DEBUG_RENDERER` is defined in Debug and Development only.

---

## 8. Integration points

**Needs from `core` (wave 1, `10_core.md`):** `jobs::job_system` (`schedule(std::function<void()>, priority)`, `worker_count()`,
static `thread_index()`, `is_main_thread()`) - the `std::function` built in `QueueJob` captures one pointer and fits the
small buffer of MSVC, libstdc++ and libc++; should `bench_physics` ever show an allocation there, core adds
`schedule(void (*fn)(void *), void *context, priority)`; `memory::tracked_allocate/tracked_free/allocation_counters()`;
`flat_hash_map`, `small_vector`, `radix_sort_u64` (caller scratch); `string_id`; `math` (`transform::inverse/operator*/to_matrix`,
`ray`, `aabb`, `nearly_equal`); `json::value/writer`; `result/error` + `CONJURE_DECLARE_ERROR_CATEGORY`; log macros;
`reflect` (`CONJURE_REFLECT_STRUCT/ENUM`, `field_attributes.readOnly/hidden`).

**Needs from `ecs` (wave 2, `01_ecs.md`):** `CONJURE_COMPONENT`, `component_registry::register_component<T>`,
`world::observe(id, ADDED/REMOVED, fn)` with the deferred `command_buffer` (used once, to add `interpolated_transform`),
`world::mask_of`, `is_alive`, `get<T>`, `view<...>::changed<T>()`, `system_context::parallel_for_chunks`, `frame()`,
`since_version()`, `scheduler::add_system` with `system_flags::MAIN_THREAD`, `after`/`before` labels (R2),
`entity::to_u64/from_u64`, `type_id_of<entity>()` for `character_controller.groundEntity` (ENTITY field kind).

**Needs from `scene` (wave 3a, `13_scene.md`):** components `parent { entity value; u32 siblingIndex }`, `local_transform`,
`interpolated_transform { transform previous, current }` registered by `scene::register_components` before
`physics::register_components` (startup 10 < 12); physics alone adds and removes `interpolated_transform` (3.3, R47); `scene.propagate_transforms` interpolating with `time.interpolatePhysics`
(R3); `scene_manager::on_before_unload` so the engine can call `physics_world::collect_shapes()` after the unload flush.
Physics never calls scene functions; it reads `parent`/`local_transform` columns directly.

**Needs from `assets` (wave 2, `05_assets.md`):** `asset_ref<T>` with reflection kind `ASSET_REF` and the scene bind
pass (R25); `asset_type_registry` / `importer_registry` registration for `conjure.physics.collision_mesh` with a CPU-only
loader (`load_cpu` builds the Jolt shape on the job, `finalise` publishes); asset events `LOADED/RELOADED/UNLOADED`
(the engine forwards `RELOADED` to `on_asset_reloaded`); `.meta` importer selection so a `.glb` that is also a render
model can be imported a second time as collision geometry (documented in `05_assets.md` 8 as an addition: "physics
registers `conjure.physics.collision_mesh` + `collision_mesh_importer`"; ARCHITECTURE 5 "who registers what" gains the
physics row).

**Offers to `engine` (wave 4, `06_engine_loop.md` 3.4 step 12, 8):** `physics_settings::from_json(project.physics,
project.extensions["physics_layers"])` + `validate()` (-> `PHYSICS_INIT_FAILED`), `physics_world(settings, jobs)`,
`attach(world)`, `register_components`, `register_asset_types`, `register_systems(scheduler, world)` at step 18;
`stats()` for `frame_stats`; `debug_draw` for EXTRACT; `collect_shapes()` on scene unload; `on_asset_reloaded`.
`engine_context::physics` (`physics::physics_world *`, already in `06_engine_loop.md` 2.5) is null when
`CONJURE_WITH_PHYSICS=OFF` or `physics.enabled == false` (R51, R53).

**Offers to `scripting` (wave 3b, `03_scripting.md` 3.5):** `contact_events()` span with `contact_event {a, b, point,
normal, impulse, kind}` sorted by body-id pair, filled in FIXED_END and valid until the next PHYSICS_STEP (R6);
`scripting.collision_dispatch` runs first in FIXED_UPDATE and reads it.

**Offers to `bindings` (wave 4, `15_bindings.md` 2.3, R49):** the `physics_*` native entries (named by 15) map onto
4.2: `physics_raycast(_all)`, `physics_shape_cast`, `physics_overlap_sphere/box` -> the queries; `physics_get/set_linear_velocity`,
`physics_get/set_angular_velocity` -> the component mirrors / `set_*_velocity`; `physics_add_force(_at)` and
`physics_add_torque` with `conjure_force_mode` -> `add_force(_at)` (FORCE), `add_impulse(_at)` (IMPULSE),
`add_impulse(mass * v)` (VELOCITY_CHANGE), `add_force(mass * a)` (ACCELERATION; mass from the mirror, derived mass
read through `body_pose`'s row), `add_torque` / `add_angular_impulse`; `physics_move_kinematic` -> `move_kinematic`;
`physics_teleport` -> `teleport`; `physics_is_sleeping/wake` -> `is_sleeping/wake`; `physics_get/set_gravity` ->
`gravity()/set_gravity`; `physics_layer_index/layer_name/layers_collide` -> `layers().find/name/collides`;
`physics_character_move` -> `character_move`, `physics_character_is_grounded` -> `character_ground() == ON_GROUND`,
`physics_character_velocity` -> `character_controller::velocity`. `rigid_body`, the five colliders,
`physics_material` and `character_controller` get `[NativeComponent]` mirrors with exactly the field lists of 2.3
(R48), verified by `layout_hash()`; `physics_layer_mask` is a `uint` in C#.

**Offers to `editor` (wave 4, `04_editor.md` 8):** `raycast`/`overlap_sphere` for the `physics.raycast` /
`physics.overlap` commands and gizmo surface snapping; `physics_layer_table::name/find` for the inspector's layer
drop-down on `u8 layer` fields; `debug_draw` flags behind the viewport "Physics" overlay; `validate()` as a debug
action; `stats()` in the profiler. Picking stays CPU-side (R11). **Modules:** plugins reach physics through the
`physics_world *` in `engine_context`; nothing module-specific exists.

**Build:** `conjure_add_module(physics SOURCES ... PUBLIC_HEADERS ... PUBLIC_DEPS conjure::core conjure::ecs conjure::scene
conjure::assets PRIVATE_DEPS conjure::thirdparty::jolt conjure::thirdparty::cgltf)`. `third_party/CMakeLists.txt` pins Jolt
to a 5.3.x tag with `USE_SSE4_1=ON USE_SSE4_2=ON USE_AVX=OFF USE_AVX2=OFF USE_LZCNT=OFF USE_TZCNT=OFF USE_F16C=OFF USE_FMADD=OFF`
(baseline x64; Jolt's instruction-set defines must match the including translation units and are propagated on the
interface target), `DOUBLE_PRECISION=OFF`, `OBJECT_LAYER_BITS=16`, `DEBUG_RENDERER_IN_DEBUG_AND_RELEASE=ON` (off in
Distribution), `USE_ASSERTS` in Debug/Development, `CROSS_PLATFORM_DETERMINISTIC=${CONJURE_PHYSICS_CROSS_PLATFORM_DETERMINISTIC}`,
`INTERPROCEDURAL_OPTIMIZATION=OFF`. `tests/sanitizers/tsan.supp` keeps its Jolt entries (`08_testing.md` 7.3).

---

## 9. Implementation plan

1. **Jolt bring-up**: `Dependencies.cmake` target with the option set of section 8; `log_category`, `physics_error`,
   `jolt_runtime` + `jolt_memory` (size-header hooks); `conjure_header_check(physics)`; `test_physics_error`.
2. **Job adapter**: `jolt_job_system : JobSystemWithBarrier` over a `JPH::FixedSizeFreeList<job>` pool; Jolt-free unit
   test queues 10k dependent jobs through a barrier with 0 and 4 workers.
3. **Settings and layers**: `physics_settings`, `physics_layer_table`, `jolt_layers`; `test_physics_settings`, `test_physics_layers`.
4. **World core**: constructor/destructor, empty `step()`, `stats`, `validate`, `jolt_conversions`; `test_physics_world`.
5. **Components**: `components.h`, `character_controller.h`, reflection, `register_components`; `test_components`.
6. **Bodies**: observers, pending creation, `shape_builder`, compounds, `shape_cache`, destruction, implicit static
   bodies, dirty rebuilds, command queue; `test_rigid_body`, `test_colliders`, `test_layers`.
7. **Sync**: `push_transforms` and `pull_transforms` as in 3.4 / 3.7; `test_component_sync`, `test_fixed_step`.
8. **Contacts**: `contact_collector`, pair table, sort, synthesised exits, impulses, triggers; `test_contact_events`.
9. **Queries**: 3.8 with filters and embedded shapes; `test_queries`.
10. **Character controller**: `CharacterVirtual` + inner body, `ExtendedUpdate`, ground mirror; `test_character_controller`.
11. **Collision mesh asset**: asset, importer (cgltf, OBJ), job-side shape build, `on_asset_reloaded`;
    `test_collision_mesh_asset` with `tests/assets/physics/ramp.glb` and `hull.obj`.
12. **Systems, debug draw, benchmarks, README**: `register_systems`, `physics_debug_draw`, `bench_physics.cpp`,
    `test_determinism`, `test_physics_stack`, `engine/src/physics/README.md`.

Steps 1-5 are sequential; 6-10 proceed in parallel once 5 lands; 11 depends on 6; 12 last. Definition of done:
`08_testing.md` 7.4 plus the wave-3 gates of `ARCHITECTURE.md` 11.

---

## 10. Tests (`tests/physics/`, target `conjure_physics_tests`, all headless)

Files mirror the public headers one-to-one (`08_testing.md` 5.1, R16) plus the topic files of `08_testing.md` 5.2.
`TEST_SUITE("physics/<file>")`; world-level tests run under `for_each_job_mode`. Fixture `physics_test_fixture.h`:
registry with scene + physics components, `ecs::world`, `physics_world` from `test_settings()` (small limits so overflow
paths are testable), `spawn_box(world, position, motion, halfExtents)`, `spawn_ground()`, `run_steps(n, dt = 1/60)`
(push/step/pull/collect in order), `settled(entity)`.

| File | Cases (minimum) |
|---|---|
| `test_physics_error.cpp` | every enumerator has a name; `make_error(physics_error::NO_BODY).is(physics_error::NO_BODY)`; category name `"physics"`; `WRONG_BODY_KIND` is the last code and has its name (P18) |
| `test_physics_settings.cpp` | defaults validate; each rejected field produces `INVALID_SETTINGS` naming it; `from_json` of the `06_engine_loop.md` 2.10 sample; unknown keys ignored with a WARN captured by `log_capture` |
| `test_physics_layers.cpp` | default table has `default` colliding with itself; `from_json` sample of 2.2; symmetric `set_collides`; 33 names -> `LAYER_LIMIT_REACHED`; duplicate -> `DUPLICATE_LAYER`; unknown in `ignore` -> `UNKNOWN_LAYER`; `collision_mask` bits; `to_json` round trip canonical-equal |
| `test_components.cpp` | registration of all eight types succeeds and is idempotent; reflection fields/offsets match `offsetof`; `rigid_body` is 52 bytes and trivially copyable (R48); flags enum ops; `component_to_json/from_json` round trip including `asset_ref` uuid strings; `world_hash` changes when `mass` changes |
| `test_character_controller.cpp` | capsule on a 30 degree slope with `velocity = (2, 0, 0)` for 120 steps: moves up, `ground == ON_GROUND`, `groundEntity` is the slope; 60 degree slope with `maxSlopeAngleDegrees = 45`: `ON_STEEP_GROUND` and no upward progress; walks into a 2 m wall: x stops within `padding + 1 cm` of the wall face; step onto a 0.3 m stair with `stepHeight 0.4` succeeds, 0.5 m stair blocks; `APPLY_GRAVITY` falls off a ledge and lands (`IN_AIR` then `ON_GROUND`); raycast hits the character's inner body; `character_move` on a non-character -> `NOT_A_CHARACTER`; a body command on a character -> `WRONG_BODY_KIND`, `teleport` moves it, `wake` / `sleep` succeed and drop nothing (P18); P20: a runtime scale change (own or the parent's) reaches the interpolated pose; a character standing on a platform moving at 2 m/s (driven through `local_transform` or by a velocity) keeps its place on it within 1 cm over 60 steps and reads `velocity == 0`; a velocity written once or sent with `character_move` every step stays what it was while the platform moves; a jump keeps the platform's speed in the air and lands where it started, or ahead when the platform stops under it; a teleport (command or `local_transform`) off the platform falls straight down; a character parented to the platform moves with it once when the platform moves sideways or sinks, and hops when it rises (the pinned limit of the vertical rule); a turning platform takes the character round; a character rebuilt while riding stays carried; a character turned by a degree in every step through `local_transform` stays carried and its rotation reaches the body; lifts: rising, rising and moving sideways, and sinking slower than 0.1 m/s are ridden with `velocity.y` reading the lift's, a lift sinking at 1 m/s is followed by `STICK_TO_FLOOR` one step behind with `velocity.y == 0`; a jump off a moving platform onto a 60 degree static slope neither slides along the slope nor reads a horizontal velocity; a heavy dynamic slab sliding on a frictionless floor carries the character; a character without `APPLY_GRAVITY` is carried while its own velocity presses it onto the platform; a teleport off a rising platform takes none of its sideways speed and all of its vertical velocity for one step, then falls |
| `test_contact_events.cpp` | box dropped on ground (`ALLOW_SLEEPING` off): exactly one `ENTER`, then one `STAY` per step, one `EXIT` after an impulse lifts it; events sorted by body-id pair with 50 boxes landing in one step (checked against a sorted copy and uniqueness per `(a, b, kind)`); trigger box overlapped by a falling sphere: one `TRIGGER_ENTER`, zero while inside, one `TRIGGER_EXIT`; destroying an entity in contact produces `EXIT` next step naming the destroyed entity (no longer alive; consumers check `is_alive`, sweep F410), and no later event references it; `impulse > 0` on ENTER with estimation on and `== 0` with it off; `contact_events()` is empty after the next `push_transforms`; buffer overflow with `maxContactEventsPerStep = 4` drops and counts; sleeping stack reports EXIT once at sleep; P21: a body that falls asleep inside a static trigger (with a `rigid_body` or colliders alone) gets no `TRIGGER_EXIT`, none and no second enter at wake, one exit when it is teleported out; a static trigger reports a body created asleep inside it and one it is moved or teleported over, without waking it; static triggers report neither static geometry nor each other, read `is_sleeping` and leave `active_body_count()` at 0; a static solid that turns into a trigger (converted in place with a `rigid_body`, replaced when made of colliders alone) and back; a static trigger turned kinematic and back keeps the sleeping body; a static trigger does not move by velocities authored or left over; a character walking through a static trigger enters and leaves once; a static trigger with `START_ASLEEP` keeps the body that falls asleep in it; six bodies asleep inside a static trigger of two colliders (twelve records in the step they enter, the whole budget) drop nothing afterwards and a solid pair elsewhere still gets its `ENTER`; a dynamic trigger turned kinematic keeps the static wall it is in, a static one turned kinematic enters it and leaves it when it turns static again |
| `test_queries.cpp` | raycast down onto ground hits the ground entity at `distance == 10 +- 1e-3` with normal `(0, 1, 0)`; layer mask excluding the ground returns `nullopt`; `ignore` skips the caster; `raycast_all` through three stacked plates returns three hits sorted by distance into a span of 8 and truncates into a span of 2; `overlap_sphere` finds exactly the bodies inside; `overlap_box` rotated 45 degrees; `shape_cast` sphere stops at the wall with `fraction` within 1e-3 of analytic; triggers excluded unless `includeTriggers`, and a static trigger also with `includeStatic` off (P21); queries during a step assert (`expect_assert`) |
| `test_collision_mesh_asset.cpp` | `from_geometry` rejects out-of-range indices and zero triangles (`GEOMETRY_INVALID`); `ramp.glb` imports to the expected vertex/index counts with node transforms baked, byte-identical across two imports; `hull.obj` convex hull wraps all points; a sphere dropped on the ramp mesh collider slides and lands at the analytic x; convex collider stacks on a box; a static trigger made of a mesh (alone, or next to a static `rigid_body` with mass 0) reports the ball that falls through it without a Jolt assert (P21: Jolt holds it as a kinematic body, which needs a mass) |
| `test_physics_debug_draw.cpp` | counting sink receives > 0 lines/triangles for `SHAPES | BOUNDING_BOXES` with two bodies and 0 with `NONE`; Distribution-style build (`JPH_DEBUG_RENDERER` undefined) compiles the call to a no-op (guarded `#if`) |
| `test_physics_world.cpp` | construct/destroy 10 worlds, `memory_guard` clean; Jolt allocations visible: `allocation_scope` around spawning 100 bodies reports `allocation_count() > 0`; 600 empty steps allocation-free; `attach` twice -> `ALREADY_ATTACHED`; `register_systems` before attach -> `NOT_ATTACHED`; `validate()` passes after random create/destroy; `body_pose` equals Jolt truth; `stats()` counts; off-thread `add_force` and `character_move` assert + `WRONG_THREAD`; P18: every command against a body of every kind (the table of 3.4) is queued or `WRONG_BODY_KIND` at the call with nothing dropped later, the same commands on a pending entity are all accepted and the inapplicable ones are dropped and counted by the push (`character_move` is part of both tables, refused with `NOT_A_CHARACTER`), a command the body cannot take is refused as such while the queue is full and is not counted as a drop, and the kind follows the components (a static body switched to `DYNAMIC`, or colliders given a `rigid_body`, take an impulse in the same frame) |
| `test_register.cpp` | systems registered with the right phases/flags/labels (inspect `scheduler::stages`); `scheduler::finalise()` succeeds together with scene systems; running `PHYSICS_STEP` + `FIXED_END` via `run_phase` moves a falling body |
| `test_rigid_body.cpp` | free fall: `y(60 steps) == 10 - 0.5 g t^2` within 1 cm (semi-implicit Euler bound documented in the test); kinematic push: a `KINEMATIC_MOVES_WITH_TRANSFORM` plate driven by `local_transform` carries a dynamic box with it; velocity written into the component before a step becomes the body velocity; `teleport` resets `interpolated_transform.previous`; mass 0 derives from volume (`body_pose` unaffected, `validate` ok); `set_linear_velocity` on a pending body applies after creation; `START_ASLEEP` and `wake`; `LOCK_ROTATION` keeps rotation identity after an off-centre impulse; `move_kinematic` reaches the pose in one step on a kinematic body and is `WRONG_BODY_KIND` on a dynamic one and on one that follows its transform (P18) |
| `test_colliders.cpp` | each primitive rests on the ground within 1 cm of its analytic height after 120 steps; offset + rotation compound (box + sphere) centre of mass; mixed trigger flags -> WARN, non-trigger body; `mesh_collider` on a dynamic body -> WARN; collider removed -> shape rebuilt (overlap no longer hits); last collider removed from an implicit static body destroys the body; non-uniform scale on a sphere -> `INVALID_SHAPE_SCALE` WARN; the shape cache shares one Jolt shape across 100 identical boxes (`stats().shapeCacheEntries == 1`); a lone collider turned by a tenth of a degree keeps its rotation (P19) |
| `test_layers.cpp` (topic) | `ignore [["a", "a"]]`: two layer-`a` boxes pass through each other, `a` vs `default` collide; `rigid_body.layer` change at runtime re-filters next step; unknown layer index -> WARN + layer 0 |
| `test_fixed_step.cpp` (topic) | 600 steps at 1/60 equal 10 s of fall; `interpolated_transform` after a step: `transform::lerp(previous, current, 0.5)` is the midpoint of the two pulled poses; `collisionSteps = 2` matches `collisionSteps = 1` within 1 mm for free fall |
| `test_component_sync.cpp` (topic) | parented dynamic body under a rotated parent writes a `local_transform` whose composed world pose equals `body_pose` within 1e-4; sleeping bodies are not written (chunk versions unchanged); teleport via `local_transform` write detected while the pull's own write is not; a half degree turn written into `local_transform` reaches the body (root, static, parented, through the parent, character; P19); scale change rebuilds the shape; 10k sleeping bodies: `pull_transforms` allocation-free and < 0.3 ms in Development (`[bench]` tag, informative) |
| `test_physics_stack.cpp` (topic) | 10 boxes stacked over a plane, 180 steps: top box y within 1 cm of `10 * 1 m`; `active_body_count() == 0` at the end; `physics.step_10k_sleeping`-style check: 2,000 settled boxes, one `step()` under `CONJURE_CHECK_NO_ALLOCATIONS`; the same gate over 100 static triggers holding 400 sleeping boxes, each entered once and never left (P21) |
| `test_determinism.cpp` (topic) | 200 boxes dropped for 300 steps with 0 and 4 workers: every `body_pose` bit-identical and `contact_events()` identical per step |

Benchmarks (`tests/benchmarks/bench_physics.cpp`, not in ctest, thresholds of `08_testing.md` 7.1):
`physics.step_10k_active` (8.0 ms, 0 allocations), `physics.step_10k_sleeping` (0.8 ms, 0 allocations),
`physics.step_10k_sleeping_1k_triggers` (the sleeping field under 1,000 static triggers, P21: 1.5 ms, 0 allocations),
`physics.sync_10k` (0.3 ms, 0 allocations).
The `feature_test` entities of `08_testing.md` 7.2 (`Ground`, `Stack_0..9`, `BouncySphere`, `Capsule`, `ConvexProp`,
`Terrain`, `ForceApplier`) exercise every component type and the force/impulse/velocity bindings.

---

## 11. Out of scope / future

- Constraints and joints (`JPH::Constraint` family), vehicles, soft bodies, buoyancy.
- 2D physics (v1 uses 3D bodies with `LOCK_TRANSLATION_Z | LOCK_ROTATION_X | LOCK_ROTATION_Y`).
- Mutable compound shapes, per-collider material/layer, height-field colliders (`JPH::HeightFieldShape`).
- Parallel `CharacterVirtual` updates and character-vs-character collision (`CharacterVsCharacterCollisionSimple`).
- Cooking Jolt shape binary state into the collision mesh blob; origin shifting / double precision.
- `JPH::StateRecorder` rewind snapshots; contact modification callbacks exposed to scripts (`ContactSettings`).
- Cross-platform bit-exact determinism gated in CI.
