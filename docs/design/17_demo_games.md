# 17 - Demo games (wave 6)

Status: **binding** (owner: lead architect). Added 2026-11-10 from the customer's request: concrete demo games for 2D
and 3D that double as the user's introduction to the engine. Designs are generic and not derived from any copyrighted
title; the customer supplies art, audio and model assets they hold the rights to.

Revised 2026-11-12 at the owner's request so the demos showcase the engine's data-oriented design: **Barrage** (a
bullet hell written entirely in C# over query spans) and **Colony** (an ant colony simulation with a native game
module, the flagship) replace the two earlier small 2D demos. Grid Sweeper, Rolling Course and Corridor Strike are
unchanged. Sections 2.3 and 2.4 carry the two new designs. Every engine capability they name was checked against the
headers; what is missing is a **gap** with its way around (2.3.11, 2.4.12), and the engine work owed is recorded in
`09_cross_area_resolutions.md` section 3 ("Demo lineup revision").

## 1. Goals

| Id | Decision |
|---|---|
| G1 | Five demos under `samples/games/`: three 2D (Grid Sweeper: small, one mechanic; Barrage: small, data-oriented C#; Colony: the flagship, C# plus one native plugin module) and two 3D (larger, several systems each). Each is a complete Conjure project that opens and runs in place: `project.json`, scenes, prefabs where the demo uses them, C# scripts (and for Colony one native plugin module), assets, README, and a CTest entry that boots it headless for N frames with `Conjure.Diagnostics.Check` assertions. |
| G2 | Every demo is also a **wiki tutorial** (📘 `docs/api/tutorials/<demo>.md`): the order in which it was built, the engine features it shows, and links to the API pages used. Grid Sweeper and Barrage are the "first hour" path; Colony and the 3D demos are the "first weekend" path. |
| G3 | Demos are built through the public surface only: the editor protocol, the C# scripting API, project files and, where a demo's row says so, a native module inside the demo's own project, built with `conjure_add_game_module` (`cmake/ConjureModule.cmake`) against the public headers under `engine/include/conjure` and registered through the module system (`modules::module_registrar`). Demo code, its tests and its benchmark executable live entirely under `samples/games/<demo>` and never include `engine/src`. No file under `engine/`, `managed/Conjure.Managed`, `editor/`, `runtime/` or `tests/` may be added or changed for a demo; a missing capability is an engine bug (fixed as an engine change of its own, with its own tests, section 4.1) or a scoped follow-up, recorded in the demo README together with the way around it. |
| G4 | Assets: the customer supplies CC0 packs under `samples/assets/` (ignored by git; layout, licences and the demo-to-kit mapping are in `samples/assets/README.md`): the Kenney all-in-one bundle (GLB models, PNG spritesheets with XML atlases, OGG audio) and Poly Haven 4K `.hdr` skies. Demos reference them through stable asset ids and ship with generated placeholders (primitive meshes, flat-colour sprites, synthesised tones) behind the same ids, so a clone without the packs still runs every demo and its tests; the README of each demo lists the slots. The CTest runs use placeholders only, so they never depend on the packs. Pack art never overwrites a tracked placeholder: `samples/tools/use_packs.py <demo>` (wave 6, project tooling, not engine code) copies the demo project to the git-ignored `samples/games_packed/<demo>/` and replaces each slot's source file there, keeping the `.meta` id; tests always run the tracked project. |
| G5 | Each demo has a "known good" headless run recorded as a `world_hash` after N fixed steps (determinism) and a screenshot reference for the golden image job where applicable. The recorded value lives in a data file next to the test (`samples/games/<demo>/tests/recorded_hashes.json`), not in prose. It is valid for one build feature set and one registration path only (the default preset: `ecs::world_hash` combines components in id order and ids follow registration order), is re-recorded when component registration changes (`CONJURE_RECORD_HASHES=1` rewrites the file and reports the case as failed, so a re-record never passes silently), and is never shared between a statically registered build and a plugin build. Tests assert equality between two runs in one process and between worker counts first, the recorded value second. |
| G6 | Scope is bounded: a demo is finished when its feature list below is met, its checks pass, and the tutorial reads cleanly. Polish beyond that is explicitly out of scope. |
| G7 | Data-oriented showcase: Barrage and Colony exist to prove the engine's data-oriented design with numbers, and the numbers are enforced. **Barrage**: 4,096 bullets alive with about 2,000 spawned and destroyed per second, all from C#; whole frame p50 2.0 ms / p99 4.0 ms, `FIXED_UPDATE` 0.5 / 1.0 ms. **Colony**: 5,000 ants at 60 FPS; whole frame p50 4.0 ms / p99 8.0 ms (of the 16.67 ms frame), `FIXED_UPDATE` 1.0 / 2.0 ms. Both: zero native allocations per steady-state frame; Barrage also zero managed bytes per frame on the main thread. The budgets are gated by benchmark scenarios in the style of `tests/benchmarks` (section 4.3): Development configuration, `desktop` machine class on an idle reference machine, `ci` class at three times the desktop times in the nightly job, reported only in Debug. Simulation results are identical for every worker count (G5). |

## 2. The demos

### 2.1 2D

| Demo | Mechanics | Engine features shown |
|---|---|---|
| **Grid Sweeper** (`samples/games/grid_sweeper`, minesweeper-style) | grid of hidden cells, numbers, flags, win/lose, timer, difficulty presets | protocol-built scene (this replaces the earlier PacMan-style walkthrough as the AI-protocol end-to-end test: an agent authors the whole project from a command file), prefab instancing per cell, sprite atlas, orthographic camera, mouse picking in 2D, UI text via sprites/fonts, game state in C#, scene reload |
| **Barrage** (`samples/games/barrage`, bullet hell; design in 2.3) | move, focus and fire a ship; one boss with three bullet-pattern phases (rings, spirals, weave) driven by a fixed-step state machine; up to 4,096 bullets alive with about 2,000 spawned and destroyed per second; 3 lives, win/lose, restart | gameplay written entirely in C# as data-oriented systems: `ComponentQuery` scopes with `Span<T>` writes into engine memory, `[EcsComponent]` structs and a `[NativeComponent]` sprite mirror, bullets as script-less ECS entities, spawn and destroy churn through the deferred command buffer flushed at scope end, circle collision in the iteration loop instead of physics, thousands of sprites in one batcher run, parallel transform propagation, seeded fixed-step determinism (`world_hash`), frame budget and zero native and managed steady-state allocations enforced by a benchmark scenario |
| **Colony** (`samples/games/colony`, ant colony simulation, the flagship; design in 2.4) | start with a single queen on one 128 x 96 cross-section map; eggs become larvae (grown by nurse care) and then adults; foragers lay and climb food and home pheromone trails (flat float grids, no pathfinding) to seeded food piles; diggers excavate player-painted dig zones to raise nest capacity; soldiers repel waves of one raider type that follows the colony's own home trail; the player has two inputs only: four role-ratio sliders and painting dig zones; one food type, one threat type; target 5,000 ants at 60 FPS; restart when the queen dies | native game module as a project plugin (`module.json`, `conjure_add_game_module`, components and systems registered through `module_registrar` with declared access), role tags as archetypes with one chunk-parallel system per role, role changes, hatching and deaths as structural moves through the command buffer, pheromones as module-owned flat grids climbed by gradient, a per-row intent column merged serially with integer deposits so `world_hash` is identical for any worker count, grid checksum folded into a hashed component, C# colony director that reads and writes singleton components through `[NativeComponent]` mirrors, about 8,000 sprites through the batcher fed by direct `render_world_transform` writes, a frame-budget and zero-steady-state-allocation gate in the style of `tests/benchmarks` |

### 2.2 3D

| Demo | Mechanics | Engine features shown |
|---|---|---|
| **Rolling Course** (`samples/games/rolling_course`, marble-run style) | control a rolling ball by applying forces relative to the camera, ramps/bumpers/boosters/moving platforms, gems, checkpoints, par times, 5 courses, respawn on fall | dynamic rigid bodies with friction and restitution materials, contact events, kinematic movers, trigger volumes, third-person orbit camera with collision, PBR materials and IBL skies, cascaded shadows, SSAO, bloom on emissive gems, audio spatialisation (ball rolling loop pitched by speed), timer and best-times persistence |
| **Corridor Strike** (`samples/games/corridor_strike`, simple first-person shooter) | first-person movement, hitscan and projectile weapons, pickups (health/ammo/keys), doors and switches, 3 enemy types with simple state-machine AI (patrol/chase/attack), 2 levels, HUD | character controller, raycasts and shape casts, physics layers (player/enemy/projectile/world), prefab-heavy level assembly, navigation by waypoints, animation-less enemies using billboards or static meshes (assets decide), spot/point shadow lights, decals via sprites, muzzle flash lights, 3D audio, scene transitions, exported standalone build |

### 2.3 Barrage design

Purpose: prove that C# scripting scales when it is written data-oriented. One `ScriptBehaviour` (`BarrageDirector`)
runs every gameplay "system" as a static method over `ComponentQuery` spans
(`managed/Conjure.Managed/Api/Dod/ComponentQuery.cs`, `ChunkView.cs`; `03_scripting.md` 3.9); bullets are plain ECS
entities with no script on them. No physics, no native module, no per-bullet native call in the simulation loop.

#### 2.3.1 Mechanics

- Vertical playfield 24 x 32 world units, orthographic camera (`renderer::camera_component`,
  `engine/include/conjure/renderer/components.h`). The boss sits at (0, 12), the player starts at (0, -12).
- Player: 8-way move at 9 units/s (WASD or arrows, `Input.IsKeyDown`), 4 units/s while Left Shift is held (focus),
  hold Space to fire a 3-shot fan upward every 5 steps (12 volleys per second; shot speed 30 units/s, damage 1,
  lifetime 60 steps, so at most 36 shots are alive; the side shots are 4 direction-table steps, 1.4 degrees, off
  vertical, 0.59 units at the boss, so all three hit a boss of radius 2 while the player is within 1.4 units of the
  centre line). Hit radius 0.15; 3 lives; a hit clears enemy bullets within 3 units and grants 90 steps of
  invulnerability. `R` reloads the scene (`Scene.Load`; replace mode ends in `world.clear()`, so no bullet survives a
  reload).
- One boss, three phases of 240 health each, plus intro, transition and defeated states (2.3.3). Enemy bullets fly 12
  to 16 units/s and live 120 steps (2 s): 24 to 32 units, past the player's start line 24 units below the boss.
- Bullets are round orbs that never rotate (identity rotation, unit scale; size and tint live in the sprite), so the
  world matrix is an exact copy of the position and the recorded hash cannot depend on floating-point contraction.
- HUD without text: 3 life pips, a boss health bar (one sprite whose `Size.X` is written when health changes), 3 phase
  pips.
- Win is the Defeated state, Lose is zero lives. Both are states in which the simulation keeps stepping: in Lose the
  player sprite is hidden, takes no hits and fires nothing, and the boss carries on by its step caps. A headless run
  is therefore defined whatever the input does.

#### 2.3.2 Data model

Game components are `[EcsComponent]` unmanaged structs in the game assembly
(`managed/Conjure.Managed/Api/Attributes.cs`; registered in `FullName` order by `ScriptCatalog`),
`StructLayout(LayoutKind.Sequential)`.

| Component | Fields | Notes |
|---|---|---|
| `EnemyBullet` | `Vector2 Velocity; float Radius; int StepsLeft` (16 bytes) | one component for all enemy bullets |
| `PlayerShot` | `Vector2 Velocity; int StepsLeft; int Damage` (16 bytes) | its own component, so each loop is branch free and no `without` filter is needed (gap 6) |
| `Player` | `int Lives; int InvulnerableSteps; int FireCooldown; float HitRadius` | one entity, authored in the scene |
| `Boss` | `int Phase; int State; int StateSteps; int Health; int PatternCursor; uint Rng; int BulletsAlive; int BulletsDropped` | one entity; the state machine, the PRNG and the cap counters are component data, so they are in `world_hash` |
| `LocalTransform` (shipped mirror of `conjure.scene.local_transform`) | position written through spans | the scene's observer adds `world_transform`, `render_world_transform`, `transform_state` and the `transform_dirty` tag (`engine/include/conjure/scene/components.h`) |
| `Sprite` (declared in the game assembly, `[NativeComponent("conjure.renderer.sprite")]`) | `AssetRef<TextureAsset> Texture; Vector4 UvRect; Vector4 Colour; Vector2 Size; SpriteSpace Space (byte enum); short Layer; short Order` | mirrors `renderer::sprite_component` (72 bytes); gap 3 |

- Enemy bullets occupy two archetypes, with and without `conjure.scene.transform_dirty` (the tag is added on spawn and
  removed by the propagation pass); 272 bytes per row (entity 8, `EnemyBullet` 16, `LocalTransform` 40, `Sprite` 72,
  two 64-byte matrices, `transform_state` 8), 1.1 MB at 4,096 bullets.
- Bullets carry no `guid`, `name`, `parent` or script: `Entity.Create("")` is a bare `world::create_entity()`
  (`engine/src/scripting/native_api_core.cpp`; the wiki says otherwise, gap 4). A scene entity would get a random
  guid, which is hashed (gap 5).
- Bullets keep `LocalTransform` on purpose: the demo shows a script moving `LocalTransform` through spans and the
  engine doing the rest (change stamps from `query_begin`, parallel `scene.propagate_transforms`, extraction). The
  price is in the budget (2.3.8).
- Managed state outside the ECS is allocated once in `OnCreate` and never resized: `SpawnRequest[] _spawns` (capacity
  512), `Vector2[] _directions` (1,024 unit vectors), every pattern table (touched in `OnCreate` so first use
  allocates nothing later). Nothing gameplay-relevant lives only in managed memory.

#### 2.3.3 Boss state machine and the numbers it produces

`Boss.State`: Intro, Fight, Transition, Defeated; `Boss.Phase` 1 to 3; all durations are fixed-step counts. Alive at
steady state = spawn rate x lifetime (2 s).

| State | Exit | Emission | Spawns per second | Alive, steady |
|---|---|---|---|---|
| Intro | 60 steps | none; boss slides in, invulnerable | 0 | 0 |
| Fight 1 "Rings" | health 240 to 0, or 420 steps | a ring of 16 every 2 steps, start index +5 direction steps per ring; an aimed 5-fan every 90 steps | 480 + 3 = 483 | about 970 |
| Transition | 45 steps | every enemy bullet destroyed in one flush (the burst); invulnerable; `Phase += 1`; health refilled | 0 | 0 |
| Fight 2 "Spirals" | health, or 900 steps | 4 spiral arms x 5 speeds every step (20 per step), arm index +13 direction steps per step | 1,200 | 2,400 |
| Fight 3 "Weave" | health, or 1,500 steps | two counter-rotating 8-arm spirals (16 per step), a ring of 32 every 2 steps (16 per step on average), 4 seeded scatter bullets per step: 36 per step requested | 2,160 requested; the cap admits 4,096 / 120 = 34 per step, about 2,050 | 4,060 to 4,096 (4,320 requested, so the cap binds) |
| Defeated | terminal | bullets cleared, win flag | 0 | 0 |

- The cap of 4,096 is enforced by the emitter: requests beyond `4096 - Boss.BulletsAlive` are dropped in emission
  order and counted in `Boss.BulletsDropped`, so the rule is deterministic and testable.
- Timeline by hand: the intro ends at step 60; with every shot hitting (3 damage per 5 steps) phase 1 takes 240 / 3 x
  5 = 400 steps, otherwise its cap of 420 ends it, so phase 1 ends between step 460 and step 480 and phase 2 starts
  between step 505 and step 525 whatever the player does. The 600-frame run therefore contains about 970 bullets, one
  transition burst and the start of phase 2. A full game lasts at most 60 + 420 + 45 + 900 + 45 + 1,500 = 2,970 steps
  (about 50 s).
- Transitions are a `switch` over two ints; pattern functions are static methods that only append to `_spawns`.

#### 2.3.4 Systems and their order (one fixed step)

Everything runs in `BarrageDirector.OnFixedUpdate` (phase `FIXED_UPDATE`); `OnUpdate` does nothing gameplay related.

1. `ReadInput`: `Input.IsKeyDown` (frame block, no native call); the player's `LocalTransform` and `Player` through
   `GetComponent` / `SetComponent`.
2. `TickBoss`: integer state machine on a local copy of `Boss`; appends `SpawnRequest`s (position, velocity, radius,
   lifetime, tint, size), applying the cap.
3. `SimulateEnemyBullets`: `using ComponentQuery q = ComponentQuery.Begin<LocalTransform, EnemyBullet>()`. Per chunk
   `Span<LocalTransform> t = chunk.Get<LocalTransform>(0)`, `Span<EnemyBullet> b = chunk.Get<EnemyBullet>(1)`,
   `ReadOnlySpan<Entity> e = chunk.Entities`. Per row: integrate the position, decrement `StepsLeft`, one
   squared-distance test against the player position held in locals. A row that expired, left the field or hit the
   player calls `e[i].Destroy()`: the scope holds the world locked, the bullet has no script, so
   `script_runtime::request_entity_destroy` (`engine/include/conjure/scripting/script_runtime.h`) records
   `command_buffer::destroy_entity` in the runtime's deferred buffer. Still inside the scope, `ApplySpawns` then walks
   `_spawns` in emission order: `Entity.Create("")` reserves an id in the same deferred buffer, and three
   `AddComponent` calls (`LocalTransform`, `Sprite`, `EnemyBullet`) record `add_raw` against it. `Dispose()`
   (`query_end`) flushes once: the step's destroys and creates, one archetype move per new bullet for its three adds.
   This is the deferred command path for both halves of the churn and needs prerequisite P2 (gap 1).
4. `SimulatePlayerShots`: `ComponentQuery.Begin<LocalTransform, PlayerShot>()`, same shape against the boss circle;
   damage accumulates in a local; the volley's new shots are created in this scope.
5. `ApplyHits`: write `Player` and `Boss` back. On entering a transition `ClearBullets` opens `Begin<EnemyBullet>()`
   and destroys every row deferred (the burst).
6. `UpdateHud`: a handful of `SetComponent` calls, only when a value changed.

- Through spans: bullet positions and lifetimes (two contiguous columns, no native call per row). Through native
  calls: 1 per destroyed bullet, 5 per spawned bullet (`Create`, three `AddComponent`, and the `AssetRefBind` that
  `Entity.AddComponent` issues for the sprite's `AssetRef`).
- A new bullet is moved three times before it settles: the flush move for its three adds, a second for the observer's
  companions (`engine/src/scene/scene_observers.cpp`), a third when the propagation pass removes `transform_dirty`.
  New bullets are first simulated in the next step.
- Transforms: `query_begin` stamps every requested column as written, so `scene.propagate_transforms` recomputes the
  bullet chunks in the `TRANSFORM` phase with its parallel chunk jobs (it fans out above 512 dirty rows); the script
  never marks anything dirty.
- Rendering: the engine's extract pass pushes one `renderer::sprite` per bullet and `sprite_batcher::build`
  (`engine/include/conjure/renderer/sprite_batch.h`) sorts and batches them. Every bullet uses the one bullet texture,
  tinted and sized per pattern, on one layer, so bullets form one run; about 4,200 sprites against
  `MAX_SPRITES_PER_FRAME = 65536` (`renderer/limits.h`).
- If P2 has not landed when Barrage is built, `ApplySpawns` runs right after the scope closes (immediate create and
  adds: five row moves through four archetypes per bullet). That is a deviation from the owner's scope and is flagged
  in the README until P2 lands.

#### 2.3.5 Collision without a physics engine

Bullet against player is one circle test per bullet inside the loop that already touches the bullet; player shots
against the boss are at most 36 tests. About 4,200 one-to-many tests per step need no broad phase. With Jolt each
bullet would be a sensor body: 2,000 body creations and removals per second, `physics.step_10k_active` at 3.9 ms per
10,000 bodies (about 1.6 ms for 4,096) and trigger callbacks one fixed step late. The project disables physics
(`project.json`; `headless_engine_options::enablePhysics = false` in tests).

#### 2.3.6 Determinism

- Fixed step only: `Step = 1f / 60f` is a literal, `Time.DeltaTime` is not used for gameplay, all timing is integer
  step counters stored in components.
- Directions come from `_directions[1024]`, built once by repeated rotation with literal constants using only `+ - *`
  and a renormalising `MathF.Sqrt` (no `MathF.Sin` / `Cos`, whose results differ between C runtimes); patterns index
  it with integers. Aimed shots use one `MathF.Sqrt` and one division.
- Randomness is a xorshift32 in `Boss.Rng`, seeded from the director's serialised `Seed` (default `0x5EED`, never
  zero); only the phase 3 scatter uses it. No `System.Random`, no clock.
- Spawn order is emission order on the main thread, so entity ids are deterministic; destroy order is chunk order. No
  run-time entity gets a guid.
- The hash also covers `scene::transform_state::lastUpdateRun` (transient but hashed), so it depends on the exact
  number of `TRANSFORM` runs; headless runs step exactly one fixed step per frame.

#### 2.3.7 What runs where

| Part | Where |
|---|---|
| All gameplay (input, boss, patterns, bullet integration, collision, spawn and destroy, HUD) | C#: `BarrageDirector : ScriptBehaviour`, static helpers `Patterns`, `BulletSim`, `DirectionTable`, the component structs |
| Transform propagation, sprite extraction, batching, drawing | existing engine systems, untouched |
| Native module | none |
| Project data | `project.json` (physics off, `maxFixedStepsPerFrame` 2), `scenes/barrage.scene.json`, `scenes/soak.scene.json`, `scenes/soak_burst.scene.json`, built by the protocol command file `commands/build_barrage.json` |

The soak scenes set the director's serialised `Soak` flag: the boss is pinned in phase 3 with no health loss and no
step cap, and the player takes no hits. `soak_burst` also sets `BurstPeriod = 200` (a `ClearBullets` every 200 steps);
the population refills to the cap 114 steps after each burst.

#### 2.3.8 Numbers and budgets

Reference: the benchmark machine and figures of `tests/benchmarks/README.md`, Development, soak scene (about 4,096
bullets, about 34 destroys and 34 spawns per step), null render device. No existing row measures C# structural calls,
the scene observer or the sprite path, so every estimate below is replaced by a measurement in stage B0 and the gates
are confirmed then.

| Item | Arithmetic | Estimate | Gate (desktop p50 / p99) |
|---|---|---|---|
| span loop | 4,096 rows x 3 to 5 ns (two integrations, a decrement, bounds and distance tests; `scripting.update_10k_dod` at 0.5 ns per row is a floor for a simpler body) | 0.012 to 0.020 ms | |
| churn | 34 x 1 + 34 x 5 = 204 bridge calls x 12 ns floor (`scripting.native_call_100k`) = 0.002 ms; 34 destroys + 34 x 3 moves = 136 operations on 272-byte rows at 120 to 400 ns (`ecs.spawn_destroy_10k`: 0.243 ms per 2,000 operations on 24-byte rows) = 0.016 to 0.054 ms | 0.02 to 0.06 ms | |
| `FIXED_UPDATE` total | loop + churn + boss, input, HUD | 0.08 to 0.2 ms | 0.5 / 1.0 ms |
| transform propagation | 4,200 rows x 17 ns median, 48 ns p99 (`scene.transform_propagation_10k`) | 0.07 / 0.20 ms | |
| sprite path | 4,200 sprites x 100 to 180 ns (extraction takes a mutex and a hash lookup per sprite in `find_texture`, then sort, four vertices and upload) | 0.42 to 0.76 ms | |
| whole frame (`step_frame`) | sum plus frame overhead | 0.8 to 1.3 ms | 2.0 / 4.0 ms |
| burst frame | 4,096 destroys x 120 to 240 ns + 4,096 bridge calls | 0.5 to 1.0 ms extra | inside the p99 gate |
| native allocations per frame | | 0 | 0 |
| managed bytes per frame, main thread | | 0 | 0 |

- `ci` class thresholds are three times the desktop times; allocation gates are the same.
- The budgets are for one fixed step per frame (the headless case). `project.json` sets `maxFixedStepsPerFrame` to 2
  (engine default 8, `engine/include/conjure/engine/engine_config.h`) so a hitch in windowed play cannot multiply the
  step cost.
- Managed rules: no LINQ, lambdas, `foreach` over interfaces, string building or boxing in step code; arrays allocated
  in `OnCreate`; `Entity.Create("")` with the empty name only (gap 8); `Diagnostics.Check` once at a report step,
  never per frame.

#### 2.3.9 Tests

All under `samples/games/barrage/tests/`; hashes follow G5.

| CTest name | Label | Checks |
|---|---|---|
| `samples.games.barrage` | `slow`, `dotnet` | `conjure::testing::headless_engine` (scripting on, physics off, the project directory and its built assembly); `input_script` holds Space from frame 0 and taps left and right for one frame every 30 frames; 600 frames of `barrage.scene.json`. Checks `barrage.mirror_sprite_resolved`, `barrage.phase_reached_2`, `barrage.bullets_spawned`, `barrage.bullets_destroyed`, `barrage.burst_flushed`; more than 512 bullets alive at frame 450 (so propagation is known to fan out); `world_hash` equal with `jobThreadCount` 0 and 8, then equal to the recorded value. Deterministic facts only, no timing and no managed allocation gate |
| `samples.games.barrage.soak` | `slow`, `dotnet` | 600 frames of `soak_burst.scene.json` with 0 and 8 workers: at least 4,060 bullets alive and `BulletsDropped > 0` at frame 580 (`barrage.cap_respected`), scatter active, two bursts behind it; `world_hash` at frame 580 equal between worker counts, then to the recorded value |
| `benchmark.barrage.soak_4k`, `benchmark.barrage.burst_4k` | `benchmark`, `scripting`, `dotnet` | section 4.3: `barrage_benchmarks`, whole frame and `FIXED_UPDATE` (`add_phase`) gates of 2.3.8, 0 native allocations; the director samples `GC.GetAllocatedBytesForCurrentThread()` at the start of each fixed step from step 620 to step 820 (the 200 measured frames that follow 600 pre-warm and 20 warm-up frames) and reports `barrage.managed_bytes_per_frame_zero` once at step 820, in one extra frame the scenario steps after `measure` returns, because the report itself allocates natively (`check_registry::report`, `engine/src/engine/check_registry.cpp`) and must stay outside the measured window; the scenario fails when that check failed or is missing, when fewer than 4,060 bullets are alive outside the 114 steps after a burst, or when a step failed. `soak_4k` gates p50 and p99 on `soak.scene.json`; `burst_4k` gates p99 and allocations on `soak_burst.scene.json` (any 200 measured frames hold exactly one burst) |
| `samples.games.barrage.export` | `process`, nightly | export and run the exported folder for 120 frames |

#### 2.3.10 Staging, assets and cut list

| Stage | Adds |
|---|---|
| B0 spike | the `Sprite` mirror resolves; deferred spawn inside a scope (P2); spawn, destroy and sprite costs measured and the gates of 2.3.8 confirmed |
| B1 game | player, boss, three phases, HUD, `samples.games.barrage` |
| B2 scale | soak scenes, `barrage_benchmarks`, `samples.games.barrage.soak` |
| B3 presentation | pack art slots, tutorial page; optional: three audio one-shots (`Audio.PlayOneShot`, rate limited, none in the soak scenes) |

| Slot (stable asset id in `.meta`) | Placeholder committed with the demo | Kenney file when the pack is present |
|---|---|---|
| `barrage/bullet` | generated white disc, tinted and sized by `Sprite` | `2D assets/Particle Pack` |
| `barrage/ship`, `barrage/boss` | flat rectangles | `2D assets/Simple Space` |
| `barrage/pip`, `barrage/background` | flat squares | `2D assets/Simple Space` |
| `barrage/sfx_shot`, `sfx_hit`, `sfx_phase` (optional) | synthesised tones | `Audio/Digital Audio` |

Separate textures per slot, no packed atlas: two or three batcher runs cost nothing at 4,200 sprites and no atlas
packer is needed. The scene references every texture from an authored sprite so it stays loaded.

Cut: stages and regular enemies (boss only), bombs, power-ups, grazing, scoring beyond a hit counter, text HUD, menus,
pause, gamepad, difficulty levels, bullet rotation or animation, particles, save data, physics, a native module,
per-bullet scripts, object pooling (the point is real create and destroy churn).

#### 2.3.11 Gaps and how each is handled

| # | Gap (checked against the code) | Way around in the demo | Engine follow-up |
|---|---|---|---|
| 1 | `entry_component_add` (`engine/src/scripting/native_api_core.cpp`) rejects an id that `Entity.Create` only reserved inside a scope (`is_alive` is tested before `is_locked`), so C# cannot spawn with components through the deferred buffer | immediate spawn after the scope, flagged as a deviation | **P2**: accept `add_raw` for ids reserved by the runtime's deferred buffer; later a batch spawn entry |
| 2 | no multi-component create from C#: one create plus three adds per bullet | accepted at 34 spawns per step | the batch spawn entry of gap 1 |
| 3 | no shipped C# mirror of `conjure.renderer.sprite` or the camera, no `SpriteSpace` enum | the game assembly declares its own mirror; `barrage.mirror_sprite_resolved` is the first self-check | ship `Sprite` and `Camera` mirrors in `Conjure.Managed` |
| 4 | `docs/api/bindings/csharp-api.md` says `Entity.Create` makes a scene entity with a transform and a guid; the entry creates a bare entity | the design relies on the code | correct the page (or the entry, then gap 5 applies to every bullet) |
| 5 | run-time scene entities get `core::uuid::generate()` guids, and `world_hash` covers the guid bytes | bare entities only; no `Entity.CreateMany` or `Prefab.InstantiateMany` in a hashed run | seedable guids in headless mode, or a guid setter in the bindings |
| 6 | `ComponentQuery` has no `without` or `changed` filter from C# | enemy bullets and player shots are different components | expose the filters |
| 7 | no managed allocation figure in the frame statistics; `allocation_scope` sees only native `operator new` | the director samples `GC.GetAllocatedBytesForCurrentThread()` and reports through `Diagnostics.Check` | a managed-bytes field in `FrameStats` |
| 8 | `Entity.Create` encodes the name with `Encoding.UTF8.GetBytes`; allocation free only for the empty string | bullets use `""`; the managed gate is the regression test | a stack-allocated encode path |
| 9 | no benchmark row contains a sprite; extraction locks a mutex and hashes a uuid per sprite (`gpu_resource_cache::find_texture`) | gates confirmed in B0 from a measurement | **P3**: `renderer.sprites_10k`; a last-asset memo in the extract loop |

### 2.4 Colony design

Flagship 2D demo. An ant colony grows from a single queen; the budget scene holds 5,000 ants at 60 FPS. Per-ant
simulation lives in a native game module (a C++ plugin with chunk-parallel systems); the colony director is one C#
script that reads and writes singleton rows. Gradient-climbing ants on pheromone grids are textbook ant colony
optimisation and the cross-section view is a generic science-toy presentation; nothing derives from an existing title.

#### 2.4.1 Staging

Colony is two implementer tasks with a review between them (section 3). Each stage ends with a green tree.

| Stage | Task | Adds | Why here |
|---|---|---|---|
| S0 technical slice | 1 | the spike (real plugin, real CLR and one mirror read in one shared-engine test process, gap 9); plugin, grids, `setup`, foragers only, FOOD and HOME trails, `apply_intents`, `fields`, `present`; static registration for tests; determinism, allocation and budget tests at 5,000 ants | retires the real risks (shared-grid determinism, frame budget, sprite path cost, plugin plus CLR) before any game rules exist; reviewed on numbers alone |
| S1 roles and digging | 2 | four role tags, `assign_roles`, digger, DIG scent, paint, capacity from dug cells, C# director, mirrors, HUD bars and sliders | the player trades food income against room to grow |
| S2 life cycle | 2 | brood (egg, larva, hatch as a structural move), nurse care, ageing and death | makes the nurse ratio matter; steady spawn and destroy churn |
| S3 threat | 2 | raiders, soldiers, threat block index, wave timer, queen health, restart | adds loss and the fourth ratio |
| S4 presentation | 2 | pack art slots, tutorial polish | safe to drop |

All four roles are mandatory for done: the demo is not shipped with sliders for roles that do nothing.

#### 2.4.2 Mechanics

- **Map**: 128 x 96 cells (12,288), 1 cell = 1 world unit, `cell = y * 128 + x`. The outer ring is SOLID, so 3-tap
  sampling needs no bounds checks. Rows 1 to 31 are open surface, rows 32 to 94 soil. Pre-dug: one shaft to a 5 x 3
  queen chamber. Generated from `config.seed`. Fixed camera, 7.5 pixels per cell in a 1280 x 720 window that is not
  resizable, leaving a 320 pixel HUD panel.
- **Start**: one queen (static), 60 food in the store (10 ants at 6 food each), no workers. The queen tends the first
  brood herself (constant care 2 on chamber cells).
- **Steering kernel (all movers)**: sample one field at three cells ahead (left, centre, right along
  `DIRECTION[heading - 2, heading, heading + 2]`, 2 cells out), turn toward the largest with a small turn from the row
  RNG, step if the target cell is open, otherwise turn. No pathfinding anywhere.
- **Foragers**: SEEK climbs FOOD (random walk below a threshold) and deposits HOME with strength falling with
  `sinceAnchor`; on a food cell it requests TAKE_FOOD; RETURN climbs HOME and deposits FOOD; within 2 cells of the
  store it requests STORE_FOOD. A give-up cooldown flips SEEK to RETURN. An exhausted food cluster is replaced by a
  new seeded cluster farther out.
- **Diggers**: the player paints marks on soil. Marks touching an OPEN cell are the frontier and emit DIG scent;
  diggers climb it and request DIG on an adjacent frontier cell. At `digWork >= 12` the cell opens, its mark tile is
  destroyed and a dug tile is created. Capacity = 30 + 4 x dug cells.
- **Life cycle**: C# raises `orders.eggsRequested`; native lays (2 food each, at most 8 per step, blocked at capacity)
  on a seeded open cell within radius 6 of the queen. Egg: 240 steps, then `stage = LARVA`. Larva: growth += min(care
  at its cell, 4) per step, 1 food per 150 points, 600 points to hatch (150 steps at full care, 300 under the queen
  alone, so the first ant hatches near step 550). Hatch is one structural move on the same entity: remove
  `colony.brood`, add `ant_motion`, `ant_memory`, `ant_intent` and the role tag with the largest deficit. Adults die
  at age 25,000 steps (about 7 minutes).
- **Nurses**: climb HOME (the queen cell is a constant HOME source and HOME diffuses through open cells, so a gradient
  to the chamber exists in every connected tunnel) and request CARE on their own cell each step.
- **Threat (raiders, one type)**: at most 32 alive, spawned at a surface edge when C# raises `orders.wavesRequested`;
  they climb HOME into the nest at half ant speed, eat from the store, damage the queen when the store is empty and
  mark their cell dangerous. An ant on a danger cell dies with probability 1/8 per step (soldiers 1/32) from its own
  RNG. Soldiers hold a HOME isoline around the entrance; with a raider in the 3 x 3 neighbourhood of the 8 x 8 threat
  block index they steer at it and request ATTACK. A raider dies at 0 health. Queen health 0 ends the run; C# bumps
  `orders.restartSerial`.
- **Roles**: four tag components. Every step `assign_roles` moves at most 16 non-carrying ants (the first in chunk
  order) from the most over-staffed to the most under-staffed role: `remove<from>`, `add<to>`,
  `set<sprite_component>` (tint), one archetype move per ant, entity id unchanged. That is 960 ants per second, so a
  1,500-ant swing recolours in under two seconds, with no periodic spike.
- **Player input (exactly two)**: four role sliders that always sum to 100 percent, and painting (left mouse) or
  erasing (right mouse) dig marks with a 3 x 3 brush.
- **Readability**: role is colour (forager amber, nurse white, digger brown, soldier red), so the colony reads as
  coloured streams. Each role has one visible bottleneck and one HUD bar: forager, the food bar; nurse, the
  brood-waiting bar; digger, the capacity and dig-queue bars; soldier, the queen health bar.

#### 2.4.3 Data model

**Grids** are module-owned memory, not entities: `MAX_COMPONENT_SIZE` is 4,096 bytes
(`engine/include/conjure/ecs/component.h`) and the world has no resource storage (gap 1). One 64-byte aligned block,
`colony_state`, about 400 KB.

| Plane | Type | Meaning |
|---|---|---|
| `field[FOOD]`, `field[HOME]`, `field[DIG]` | `f32` x 12,288, two buffers each | pheromones (front, back for diffusion) |
| `deposit[FOOD]`, `deposit[HOME]` | `u16` x 12,288 | integer deposits of the current step |
| `terrain` | `u8` | bits OPEN, MARK, FOOD, STORE, DANGER, SOLID |
| `foodAmount`, `digWork`, `care` | `u8` each | food left, dig work done, care this step |
| `threatBlock` | `u8` x 192 (16 x 12 blocks of 8 x 8 cells) | raider slot per block, rebuilt each step |
| `raiderDamage` | `u16` x 33 | damage per raider slot (1 to 32; 0 is no raider) this step; zeroed when applied and when a slot is assigned |
| `spawnRng`, `spawnSerial` | `u32` | serial RNG for map, brood cells, raider entry, new-ant seeds |

- Ownership: `system_extension_desc::run` is `void (*)(system_context &)` with no user pointer
  (`engine/include/conjure/ecs/component_type_desc.h`), so `colony_state` is reached through one module-level static
  pointer: one colony per process. `colony::acquire_state()` and `colony::release_state()` are the only allocation and
  free; the plugin calls them from `module_lifecycle::onActivated` / `onDeactivating`, the static test builds from
  `register_static()` / `shutdown_static()` (called before the fixture dies, so its `memory_guard` balances).
  `colony.setup` zero-resets every plane before rebuilding, so a second engine in the same process starts from
  identical bytes.
- Float rules: `t = field + f32(deposit) * UNIT` with `UNIT` a power of two (the product is exact, so contraction
  cannot change the result), then `field = min(t, CAP) * DECAY`; diffusion is `(n + s + e + w + c) * 0.2f` over open
  cells only, front to back, one field per step in rotation, values below 1e-12 flushed to zero. No libm. The module
  target sets `-ffp-contract=off` (GCC, Clang) and `/fp:precise` with `#pragma fp_contract(off)` in the grid source
  (MSVC), since the repository has no contraction policy.

**Components** are registered by the plugin: trivially copyable, `CONJURE_COMPONENT` + `CONJURE_REFLECT_STRUCT`,
exported with `ecs::describe_component<T>()`, explicit `pad*` fields, `u32` only where C# mirrors them.

| Name | Bytes | Fields | Written by |
|---|---|---|---|
| `colony.ant_motion` | 16 | `i32 x; i32 y` (24.8 fixed point); `u32 rng; u8 heading` (0..31); `u8 state` (SEEK, RETURN, WORK, DEAD); `u8 cargo; u8 hp` | steer systems (own row), `apply_intents` (grants) |
| `colony.ant_memory` | 8 | `u16 sinceAnchor; u16 age; u16 cooldown; u16 pad0` | steer systems |
| `colony.ant_intent` | 8 | `u32 cell` (action target); `u16 deposit` (amount at the ant's own cell); `u8 channel` (NONE, FOOD, HOME); `u8 action` (NONE, TAKE_FOOD, STORE_FOOD, DIG, CARE, ATTACK) | steer systems; cleared by `apply_intents` |
| `colony.forager`, `.nurse`, `.digger`, `.soldier` | 0 | tags | `assign_roles`, `brood`, through the command buffer |
| `colony.brood` | 8 | `u32 cell; u16 growth; u8 stage; u8 pad0` | `apply_orders` (lay), `brood` |
| `colony.queen` | 8 | `u32 cell; u16 hp; u16 pad0` | `setup`, `steer_threats` |
| `colony.threat` | 16 | `i32 x; i32 y; u32 rng; i16 hp; u8 heading; u8 slot` | `steer_threats` |
| `colony.tile` | 8 | `u32 cell; u32 kind` (DUG, MARK, FOOD) | creation only |
| `colony.hud_element` | 8 | `u32 kind; u32 slot` | scene file |
| `colony.sprite_proto` | 4 | `u32 kind` | scene file; on `scene::disabled` prototype entities whose `sprite_component` the module copies when it spawns |
| `colony.config` (singleton) | 64 | `u32 seed, startAdults, startFood, startBrood, ageSpread, preDugCells, startRaiders, populationCap, cellPixelsQ8, mapPixelWidth, trackX, trackY, trackWidth, trackHeight, trackPitch, pad0` | scene file; the one place for scenario seeding and for the map and slider geometry both languages use |
| `colony.orders` (singleton) | 48 | `u32 foragerPermille, nursePermille, diggerPermille, soldierPermille, eggsRequested, wavesRequested, raidersPerWave, paintMode, paintFromCell, paintToCell, paintRadius, restartSerial` | **C# only**; request counters are monotonic |
| `colony.stats` (singleton) | 96 | `u32 step, builtSerial, foragers, nurses, diggers, soldiers, eggs, larvae, raiders, foodStored, foodDelivered, dugCells, capacity, digQueue, broodWaiting, antsLost, eggsLaid, hatched, roleMoves, wavesSpawned, raidersKilled, queenHp, steerGrains, pad0` | **native only**; `steerGrains` packs each steer system's `last_grain_count()`, one byte per role |
| `colony.grid` (singleton) | 16 | `u32 checksumLow, checksumHigh, checksumStep, pad0` | the scheduler-visible stand-in for the grids: every system that reads a plane declares it read, every system that writes any plane declares it written; its checksum puts the grids into `world_hash` |

- `config`, `orders`, `stats`, `grid` and the `ColonyDirector` script sit on one scene entity named `colony`.
- Every drawn entity carries `renderer::sprite_component` and `scene::render_world_transform`, which is what the
  engine's sprite extraction views (`view<const sprite_component, const render_world_transform>` without
  `scene::disabled` / `scene::inactive`). Ants, brood, raiders and tiles are raw ECS entities: no
  `scene::local_transform` (so `scene.propagate_transforms` never visits them, saving 0.09 to 0.26 ms per frame), no
  hierarchy, no guid (a random guid would break the recorded hash). `colony.present` writes the matrix directly.
- Two rules the tutorial states: role is a tag because it changes rarely and selects a system; cargo, state, hp and
  brood stage are bytes because they flip often or share one system.
- New rows are seeded with a splitmix of `spawnSerial` forced odd, so a xorshift32 state is never zero.

| Archetype | Set | Rows at target | Row bytes (approx) | Rows per 16 KiB chunk |
|---|---|---|---|---|
| Forager / Nurse / Digger / Soldier | ant_motion, ant_memory, ant_intent, role tag, render_world_transform, sprite | 2,500 / 1,000 / 750 / 750 | 176 | 93 |
| Brood | brood, render_world_transform, sprite | up to 600 | 152 | 107 |
| Raider | threat, render_world_transform, sprite | up to 32 | 160 | 102 |
| Tile | tile, render_world_transform, sprite | up to about 2,400 (1,300 dug, 1,024 marks, 64 food) | 152 | 107 |

5,000 ants are 27 + 11 + 9 + 9 = 56 chunks; the steering hot set is 32 bytes per row, 160 KB.

#### 2.4.4 The deterministic parallel deposit

Requirement: chunk jobs deposit into shared grids and the result must be bit-identical for any worker count, because
G5 records `ecs::world_hash` (`engine/include/conjure/ecs/world_hash.h`).

1. **Parallel pass** (`colony.steer_*`, `view::parallel_for_each_chunk`, `engine/include/conjure/ecs/view.h`): the
   grids are read-only for the whole stage (`colony.grid` declared read). A row's output (position, heading, state,
   RNG, age, one deposit and one action in its own `ant_intent`) is a pure function of its own row and the grid
   snapshot. No row reads another row, and no grain touches `colony.stats`: the system writes its `steerGrains`
   byte on its own thread after the join. The passes record no structural commands and never call
   `create_entity()`: ids reserved inside grain buffers come in blocks of 64 whose refill order depends on thread
   timing (destroys recorded from grains would be deterministic, because grain buffers are applied in grain order,
   but Colony does not need them).
2. **Merge pass** (`colony.apply_intents`, one `MAIN_THREAD` system, `colony.grid` declared written): the four role
   views in fixed order, chunks in chunk order, rows in row order. Deposits: `deposit[channel][ownCell] += amount`,
   saturating `u16`. Contended actions (the last food unit of a cell, the last dig tick) are granted first come in
   that walk and the grant is written back into the row (`cargo`, `state`). CARE and ATTACK add into `care[cell]` and
   `raiderDamage[slot]`. Ants with `state == DEAD` are destroyed and tiles are created and destroyed, all through
   `context.commands()` on this one thread.
3. **Fold pass** (`colony.fields`, next step, serial): the fixed float expression of 2.4.3 per cell, then decay,
   diffusion of one field, threat block rebuild.

Why the bytes match for 0, 2 and 16 workers: (a) parallel outputs do not depend on scheduling; (b) the merge is one
thread walking chunk order, and chunk contents change only through flushes of serial systems in a fixed `after` order;
(c) deposits are integers, so their per-cell saturating sum is exact and order-free, and floats are touched once per
cell per step in one fixed expression; (d) entity ids are created only by serial systems, mutually serialised by their
write on `colony.stats`; (e) randomness is per-row xorshift32, never `core::random` or thread state; (f)
`colony.present` builds the hashed `render_world_transform` from integers and a literal 32-entry float table with
single operations; (g) the grid checksum (word-wise 64-bit hash of the front buffers and the integer planes, about
31,000 words; `colony.publish_stats` increments `stats.step` first and computes the checksum when the new value is a
multiple of 60, so a hash taken after 60, 120, ... 600 steps covers the grids of that very step) is a hashed
component.

The owner's stated preference was per-worker deposit buffers merged after the pass. Those are buildable today:
`core::jobs::job_system::thread_index()` (`engine/include/conjure/core/jobs/job_system.h`) is 0 on the main thread and
1..N on workers, and saturating integer sums make the merge order-free. The intent column was chosen instead because
contended grants (food, dig) need a serial arbiter that writes back into the ant's row in any case, so the column
exists either way and deposits ride in it for free (0.04 ms at 5,000 rows); per-worker grids would be a second
mechanism costing (workers + 1) x 49 KB to clear and sum each step. Switching later is confined to the steer kernels
and `apply_intents`. This is item 1 of 2.4.13.

#### 2.4.5 Native game module

```
samples/games/colony/
  project.json            window 1280 x 720 not resizable, fixed step 1/60, maxFixedStepsPerFrame 2, physics off,
                          modules search path "modules", required ["conjure.samples.colony_sim"], game assembly
  module.json             kind "game", no binary, managed ["managed/Colony.dll"],
                          dependencies [{ "name": "conjure.samples.colony_sim", "version": "^1.0.0" }]
  modules/colony_sim/
    module.json           { "schema": 1, "name": "conjure.samples.colony_sim", "version": "1.0.0", "kind": "plugin",
                            "engine": "^0.1.0", "requires": ["renderer"], "binary": "colony_sim" }
    CMakeLists.txt        conjure_add_game_module(conjure_sample_colony_sim MANIFEST module.json
                              SOURCES ${COLONY_SIM_SOURCES} source/colony_module.cpp)
    source/colony_components.h     the structs, CONJURE_COMPONENT / CONJURE_REFLECT_STRUCT, constants in one place
    source/colony_grid.{h,cpp}     colony_state and pure functions (no ECS include): steer, fold, diffuse, brush,
                                   checksum
    source/colony_systems.{h,cpp}  the run functions
    source/colony_register.{h,cpp} constexpr string_id access arrays, the system_extension_desc table,
                                   colony_entry(), acquire_state / release_state, register_static / shutdown_static
    source/colony_module.cpp       only CONJURE_DECLARE_MODULE(colony_entry)
  managed/                Colony.csproj: ColonyDirector.cs, ColonyInput.cs, Mirrors.cs
  assets/                 scenes/colony.scene.json, colony_headstart.scene.json, colony_5k.scene.json,
                          colony_20k.scene.json, placeholders
  tests/   README.md
```

- **Build**: `conjure_add_game_module` makes a SHARED library linking `conjure::shared` and, with no `OUTPUT_ROOT`,
  writes the binary to `modules/colony_sim/bin/<platform>-<arch>-<config>/` inside the project, which is where the
  loader derives it from the manifest's directory (`engine/include/conjure/modules/module_manifest.h`); `**/bin/` is
  already ignored by git. The project therefore opens in place in the editor and the runtime (G1). The managed half
  uses `conjure_add_managed_project` (`cmake/ConjureManaged.cmake`).
- **Load**: `engine::project::module_manifests()` (`engine/include/conjure/engine/project.h`) returns the root
  manifest and every `<searchPath>/*/module.json`; the modules subsystem resolves the plugin before the game module,
  checks the ABI and runs the entry; the engine's ECS consumer turns the records into component registrations and
  `scheduler::add_system` calls. A missing or stale binary fails start-up because the name is in `modules.required`.
- **Entry**: `registrar.register_extension(ecs::describe_component<T>())` for the 17 components, one
  `ecs::system_extension_desc` per system with `reads` / `writes` / `after` as namespace-scope `constexpr
  core::string_id` arrays, and `set_lifecycle({on_activated, on_deactivating})` for the grid block.
- Typed access to `renderer::sprite_component` needs its name binding, which today lives in the private
  `engine/src/engine/subsystems/renderer_components.h` (gap 3, prerequisite P4).

Systems (`FIXED_UPDATE` unless noted; order pinned by `after` lists in the listed order):

| # | System | Flags | Reads | Writes | Does |
|---|---|---|---|---|---|
| 1 | `colony.setup` | EXCLUSIVE | | | runs only when `stats.builtSerial != orders.restartSerial + 1` (true on a fresh scene, where both are 0) and then sets `builtSerial = restartSerial + 1`; destroys every entity the module spawned, zero-resets and rebuilds the grids from `config.seed`, spawns the queen, food tiles and the `config.start*` population with immediate world calls. Keyed on world data, so a scene reload or editor play/stop rebuilds without relying on module statics |
| 2 | `colony.apply_orders` | MAIN_THREAD | orders, config, sprite_proto, sprite | grid, stats, brood | rasterises the paint segment into marks (mark tiles through commands, cap 1,024), lays eggs while `eggsLaid < eggsRequested`, spawns raiders while `wavesSpawned < wavesRequested`; enforces `config.populationCap` |
| 3 | `colony.assign_roles` | MAIN_THREAD | orders, ant_motion, 4 tags, sprite_proto | stats, sprite | every step, counts from `query::entity_count()`, at most 16 moves |
| 4 | `colony.fields` | (job) | threat | grid | fold, decay, diffuse, threat block rebuild |
| 5 to 8 | `colony.steer_foragers` / `_nurses` / `_diggers` / `_soldiers` | parallel chunks | grid, role tag | ant_motion, ant_memory, ant_intent, stats | one `view<ant_motion, ant_memory, ant_intent>().with<role>()` each |
| 9 | `colony.steer_threats` | (job) | | grid, threat, queen, stats | applies `raiderDamage`, records raider deaths, steers the rest on HOME, eats store or damages the queen, rewrites danger bits |
| 10 | `colony.apply_intents` | MAIN_THREAD | 4 tags, sprite_proto, sprite | grid, ant_motion, ant_intent, stats | the merge of 2.4.4 |
| 11 | `colony.brood` | MAIN_THREAD | orders | grid, brood, stats | timers, care-limited growth (consumes `care`), hatch |
| 12 | `colony.publish_stats` | (job) | orders | stats, grid | counts, capacity, checksum when `step % 60 == 0` |
| 13 | `colony.present` (LATE_UPDATE) | parallel chunks | ant_motion, brood, threat | render_world_transform | matrix from the fixed-point position and the 32-entry literal cos/sin table |
| 14 | `colony.hud` (LATE_UPDATE) | (job) | orders, stats, config, hud_element | sprite, render_world_transform | sizes bars, positions slider knobs (SCREEN space, pixel matrix) |

- The scheduler packs stages by component mask, not archetype, so systems 5 to 8 (all write `ant_motion`) are four
  consecutive stages: one role, one archetype, one system, as the owner decided. Systems 2, 3, 10 and 11 create
  entities or move rows and are serialised by their write on `colony.stats`. S0 registers 1, 4, 5, 10, 12, 13.
- **Payload rule**: `world::FLUSH_INLINE_PAYLOADS` is 4 (`engine/include/conjure/ecs/world.h`); a fifth
  payload-carrying command on one entity in one flush allocates. Steady-state commands therefore carry at most 4
  payloads per entity per flush: a hatch has 2 (`ant_motion`, the sprite tint; `ant_memory`, `ant_intent` and the tag
  are added without a payload and take their registered defaults), an egg or a raider 2, a tile 3, a role move 1. The
  5-payload adult spawn exists only in `colony.setup`, which is EXCLUSIVE and uses immediate world calls outside every
  measured window (gap 5).
- Run functions hold no local standard containers (MSVC Debug iterators allocate for them), so the zero-allocation
  assertion also holds in the Debug nightly run.

#### 2.4.6 What runs where

**C++ plugin**: every per-ant, per-brood, per-raider and per-cell operation, all entity creation and role moves, grid
ownership, HUD sprite layout. **C#**: policy at slow cadence, and input. **Engine**: untouched.

`ColonyDirector : ScriptBehaviour` sits on the `colony` entity; `Mirrors.cs` declares `[NativeComponent]` structs for
`colony.orders`, `colony.stats` and `colony.config` (sequential layout, `uint` fields). `colony.mirrors_resolve` is
the first self-check, because mirrors of plugin-registered components have no precedent (gap 9).

- `OnStart`: one `GetComponent<Config>()` for the geometry.
- `OnFixedUpdate`: every 6th step one `GetComponent<Stats>()`; one `SetComponent(in orders)` when anything changed.
  - Egg laying: request an egg when `FoodStored > 2 * (Eggs + Larvae) + reserve`, population plus brood is below
    `Capacity`, and `BroodWaiting <= max(Nurses / 10, 2)` (the floor of 2 is the queen's own care, so the rule can
    fire with zero nurses).
  - Role ratios: the four slider values normalised to permille; while `Raiders > 0` soldiers get at least 15 percent.
  - Threat waves: first wave at step 3,600, then every 2,400 steps (`Time.FixedStepIndex`), `RaidersPerWave = min(1 +
    population / 320, 16)`.
  - Restart: when `QueenHp == 0`, bump `RestartSerial`.
- Input, also in `OnFixedUpdate` so one native step consumes one write: `Input.MousePosition` and
  `Input.IsMouseButtonDown` are mapped with the `Config` geometry (cell = pixel / 7.5 inside the 960 pixel map area;
  slider tracks are rectangles in the panel) to `PaintMode`, `PaintFromCell`, `PaintToCell`, `PaintRadius` or a slider
  value (gap 4).
- `Diagnostics.Check` records the self-checks; no numbers are drawn and there is no debug key (the player has exactly
  two inputs). No `System.Random`, no wall clock, no steady-state `new`.
- Requests are monotonic counters and paint is level state, so the result does not depend on whether
  `scripting.fixed_update` runs before or after the plugin's systems; the order is the registration order, is stable,
  and is part of the recorded hash. No cross-module `after` edge is declared.

The boundary rule the tutorial states: the hot loop stays on one side; the other side reads one row and writes one
row. A per-ant `GetComponent` + `SetComponent` from C# would cost 5,000 x 2 x 12 ns = 0.12 ms before doing any work
(`scripting.native_call_100k`), shown as the anti-example.

#### 2.4.7 Numbers and budget

Budget scene `colony_5k.scene.json`: 5,000 adults (2,500 / 1,000 / 750 / 750), 500 brood, 16 raiders, 1,300 pre-dug
cells, one fixed step per frame. Yardsticks from `tests/benchmarks/README.md` (reference machine, Development):
`ecs.iterate_100k` 0.049 ms; `ecs.spawn_destroy_10k` 0.243 ms per 2,000 operations (about 120 ns each, 0 allocations);
`scripting.native_call_100k` 12 ns per call; `engine.full_frame_10k_transform_render` 0.470 ms. Per-row figures are
estimates until `colony.sim_step_5k` replaces them.

| Work | Rows | ns per row | ms |
|---|---|---|---|
| four steer systems (3 grid loads in L2, table, RNG, wall test, intent) | 5,000 | 40 | 0.200 |
| `apply_intents` (sequential read, scattered integer writes) | 5,000 | 8 | 0.040 |
| `fields` (fold 2 planes, decay 3, diffuse 1: 6 x 12,288) | about 74,000 cell visits | about 0.8 (0.5 fold and decay, 2.5 diffuse) | 0.060 |
| `assign_roles` (16 moves x 3 commands, every step) | 16 | 360 | 0.006 |
| `apply_orders`, `brood`, `steer_threats`, churn (about 12 births and deaths per second) | under 700 | | 0.015 |
| `publish_stats`; grid checksum every 60th step (31,000 words) | | | 0.003; peak 0.03 |
| `present` | 5,500 | 10 | 0.055 |
| `hud`, C# director | | | 0.006 |
| **Colony systems, inline** | | | **about 0.39, peak 0.42** |
| sprite path: extraction (mutex and hash lookup per sprite in `find_texture`), batcher build, upload | about 6,900 in the budget scene, 8,000 worst case | 100 to 180 | 0.7 to 1.4 |
| **Whole headless frame, estimate** | | | **about 1.5 to 2.5** |

- Gate (G7): whole frame p50 4.0 / p99 8.0 ms on `desktop`, 12.0 / 24.0 on `ci`; `FIXED_UPDATE` 1.0 / 2.0 ms (3.0 /
  6.0 ci); 0 allocations. The 60 FPS claim is the desktop p99 of 8.0 ms against 16.67 ms, for one fixed step per
  frame. Headroom to the frame gate is 1.6 to 2.7 times; if the measured sprite path exceeds it, the first lever is
  tile count (coarser dug tiles), not an engine change.
- No benchmark row contains a sprite (the renderer rows are meshes), so the sprite line is the least certain;
  prerequisite P3 measures it and the frame gate is confirmed in S0.
- Grain arithmetic (`engine/src/ecs/system.cpp`): a grain is `max(1, minEntitiesPerJob / capacity)` = 1,024 / 93 = 11
  chunks, and a pass fans out only with more than one grain. Foragers: 2,500 rows = 27 chunks = 3 grains. Nurses:
  1,000 rows = 11 chunks = 1 grain. Diggers and soldiers: 750 rows = 9 chunks = 1 grain each. At 5,000 ants only the
  forager pass uses workers (saving about 0.065 ms against 0.03 to 0.05 ms of dispatch), so the budget above is the
  inline case and no speed-up from workers is claimed or asserted. `colony_20k.scene.json` (20,000 ants, 10,000 /
  4,000 / 3,000 / 3,000 = 10 + 4 + 3 + 3 = 20 grains, about 23,000 sprites against `MAX_SPRITES_PER_FRAME = 65536`) is
  the ungated stress scene where the tutorial compares worker counts (gap 6).
- Population: `config.populationCap` is 8,192 in the game scenes; the module's hard limit is 32,768.
- Zero steady-state allocations: grids allocated once; no container growth in any run function; births, deaths, role
  moves and tiles ride command buffers and parked chunks under the payload rule; the budget scene holds births equal
  to deaths and its warm-up touches every role, brood, raider and tile archetype.
- `world_hash` over about 7,000 sprite rows hashes roughly 1.5 MB (`sprite_component` is trivially copyable and hashed
  by its reflected field bytes), about 2 ms and allocation free; it is taken at check points only, never inside a
  measured or allocation-gated window.

#### 2.4.8 Scenes

| Scene | Seeding (`colony.config`) | Used by |
|---|---|---|
| `colony.scene.json` | one queen, 60 food | the game; `samples.games.colony` (boot and input) |
| `colony_headstart.scene.json` | `startAdults` 200 | `samples.games.colony.headstart` (the gameplay hash) |
| `colony_5k.scene.json` | `startAdults` 5,000, `startBrood` 500 with staggered stage and growth, `ageSpread` 25,000 (start ages uniform over the lifetime: 5,000 / 25,000 = 0.2 deaths per step), `preDugCells` 1,300 (capacity 5,230), `startRaiders` 16 | determinism, allocations, benchmarks |
| `colony_20k.scene.json` | 20,000 adults, cap 32,768 | tutorial experiment, ungated |

#### 2.4.9 Tests

All under `samples/games/colony/tests/`. `COLONY_SIM_SOURCES` is compiled a second time into the test and benchmark
executables against the static engine and registered through `register_static()` from
`headless_engine_options::registerSystems` (the `tests/integration/test_determinism.cpp` pattern), because an
executable that links the shared engine cannot count allocations (gap 8). Static and plugin builds have separate
recorded hashes (G5).

| CTest name | Label | Checks |
|---|---|---|
| `colony.grid - *` | unit | pure functions: 3-tap steer picks the largest and never enters SOLID; fold, decay, diffusion values and walls; deposit saturation; brush rasterisation; the map is a pure function of the seed; the checksum changes on one flipped bit |
| `colony.systems - *` | unit | bare `ecs::world` + `ecs::scheduler`, null jobs: food reaches the store from a corridor; a frontier mark is dug and an enclosed mark is not; ratios converge with at most 16 moves per step and unchanged entity ids; a larva hatches only with care and food; capacity blocks laying; age death; a raider reaches the queen without soldiers and dies with them; the egg rule fires with zero nurses |
| `samples.games.colony.determinism` | slow, TIMEOUT 600 | static build, `colony_5k`, scripted `orders` writes (ratios to 25 / 25 / 25 / 25, so each role is 1,250 rows = 14 chunks = 2 grains; eggs and one wave requested), 300 steps at 0, 2 and 8 job threads: identical `world_hash` and grid checksum at every 60th step; every byte of `stats.steerGrains` above 1; `hatched`, `antsLost`, `roleMoves` and `raidersKilled` all non-zero |
| `samples.games.colony.allocations` | slow, TIMEOUT 600 | static build, `colony_5k`, 600 warm-up frames, then 300 `step_frames(1)` calls inline under `allocation_scope`: 0 allocations, with at least 40 hatches, 40 deaths (expected 60 each), 16 role moves and one raider wave inside the window; no timing |
| `samples.games.colony.fast_forward` | slow, nightly | static build without a renderer, one queen, 40,000 fixed steps with fixed ratios and a C++ copy of the egg rule: first hatch before step 600, no 6,000-step window without a hatch while food and capacity allow, population milestones recorded when S2 lands. The proof that the colony grows from one queen, and the tuning tool for the trail constants |
| `benchmark.colony.sim_step_5k`, `benchmark.colony.full_frame_5k` | benchmark | section 4.3: `colony_benchmarks`; one `run_phase(FIXED_UPDATE)` and one full headless frame against the gates of 2.4.7, reported for `--threads 0` and `--threads auto` side by side |
| `samples.games.colony.module` | process | `LINK_ENGINE_SHARED`: boots the project through `headless_engine` with `projectDirectory`; the plugin is LOADED, the 17 components resolve, 60 frames advance `stats.step`; depends on the plugin target so a stale binary cannot pass |
| `samples.games.colony` | slow, dotnet | real CLR and real plugin (the S0 spike), 600 frames of `colony.scene.json` with `input_script` (drag-paint a zone at frames 30 to 60, drag the digger slider at frame 90). A boot and input test: `colony.mirrors_resolve`, `colony.eggs_laid`, `colony.first_hatch`, `colony.marks_painted`, `colony.orders_applied`; recorded `world_hash` at frame 600 |
| `samples.games.colony.headstart` | slow, dotnet | 600 frames of `colony_headstart.scene.json`: `foodDelivered > 0`, `dugCells` grew, role counts within 10 percent of the ratios; `world_hash` equal with 0 and 4 workers, then equal to the recorded value. This is the G5 hash that covers gameplay |
| `samples.games.colony.export` | process, nightly | export and run 120 frames; the folder contains `modules/<name>/{module.json, bin/<triplet>/}` and `managed/Colony.dll` |

If the S0 spike shows that the real plugin and the CLR cannot share one shared-engine test process,
`samples.games.colony` and `.headstart` run on the static engine with `register_static()` plus the CLR, and the real
binary is covered by `.module` and `.export` only.

#### 2.4.10 Tutorial page and asset slots

`docs/api/tutorials/colony.md`, seven steps in build order, each ending with an experiment and a number that changes:
(1) count before you code: 5,000 ants, 16.67 ms, the budget table; (2) an ant is a row: 32 hot bytes, experiment: add
a 256-byte pad and rerun `colony.sim_step_5k`; (3) a field is an array: 12,288 floats, not 12,288 entities,
experiment: double the map and watch the fold cost follow cells, not ants; (4) a role is a table: tags, archetypes,
moving a row through the command buffer, experiment: drag a slider and watch the per-role counts; (5) going wide
without races: read old, write your own row, merge, experiment on `colony_20k`: compare the printed `world_hash` and
the step time with 0 and 16 job threads, then make a steer system write a grid directly and watch the hashes diverge;
(6) two languages, one boundary: the director reads one row and writes one row, and the per-ant C# anti-example with
its 0.12 ms price; (7) prove it: the benchmark rows and the allocation gate. It links the modules, queries, command
buffer, jobs and C# API pages, `docs/handbook/data-oriented-design.md`, `tests/benchmarks/README.md` and the Barrage
tutorial for C# query scopes.

Placeholders are committed and used by every test (G4): one white texture tinted through `sprite_component::colour`,
so the batcher emits one or two runs. Layers: 0 background, 1 dug tiles, 2 marks and food, 3 brood, 4 ants, 5 raiders;
SCREEN space for the HUD. Soil is two background sprites plus one tile entity per dug, marked or food cell.

| Slot (stable asset id name) | Placeholder | With the packs |
|---|---|---|
| `colony/sprites/ant`, `queen`, `raider` | white 8 x 8 quad, tinted per role / gold / red | `2D assets/Micro Roguelike`, fallback `2D assets/Shape Characters` |
| `colony/sprites/egg`, `larva`, `food` | small tinted quads | `2D assets/Particle Pack`, `2D assets/Micro Roguelike` |
| `colony/sprites/soil`, `surface`, `tunnel`, `mark` | flat brown / green / dark brown / translucent yellow | `2D assets/Pattern Pack` |
| `colony/ui/track`, `knob`, `bar` | flat rectangles | `UI assets/UI Pack` |

#### 2.4.11 Cut list

Owner decision: genetics, rival colonies, per-ant inspection, tech trees.

| Also cut | Saves |
|---|---|
| pheromone overlay (its system and 768 block tiles), audio, the per-slider "actual share" bars, the wave countdown bar, the C# `ComponentQuery` tutorial extra | scope; Barrage covers C# query scopes |
| zoom and pan, resizable window, fullscreen | a camera mirror and a screen-size API that C# does not have |
| nurses carrying food to individual larvae | a carry state machine and any ant-to-larva lookup |
| hunger and starvation | a second death path and a balance problem |
| rock and soil types, several maps, soil disposal, gravity | terrain stays one byte of flags |
| per-cell soil tile entities | thousands of static sprites per frame |
| raider variety, ant-to-ant collision or avoidance | per-pair work |
| carrying sprite frame, sprite animation, text, digits, milestone banners, minimap | atlas and text work; role tint and bars carry the information |
| parallel field update, per-worker deposit grids | not needed at 12,288 cells and 5,000 ants |
| save and load, pause menu, speed control, gamepad, win screen | VFS and UI work off the critical path |
| module hot reload of live simulation state | grids are rebuilt from the seed, never snapshotted (documented limitation: editor play/stop restores ECS columns, not grids) |

#### 2.4.12 Gaps and how each is handled

| # | Gap (checked against the code) | Way around in the demo | Engine follow-up |
|---|---|---|---|
| 1 | the world has no resource or singleton storage, `MAX_COMPONENT_SIZE` is 4,096 bytes and `system_extension_desc::run` carries no user data; the test path (`registerSystems`) bypasses `module_lifecycle` | module-static `colony_state` with one `acquire` / `release` pair for both paths; the `colony.grid` singleton as the declared-access and hashed stand-in; rebuild keyed on `builtSerial` | world resources with declared access and hashing; user data on system descriptors |
| 2 | `parallel_for_chunks` passes `(chunk_view &, command_buffer &)` and no grain index (`thread_index()` and `chunk_view::chunk_index()` do exist) | the per-row intent column with integer amounts | a grain index or per-grain scratch for the callable |
| 3 | `CONJURE_COMPONENT(conjure::renderer::sprite_component, ...)` is in a private header, so a plugin cannot name the type for a typed view | until P4: repeat the identical one-line binding in `colony_components.h` (`id_of<T>()` resolves by name), deleted when P4 lands | **P4**: move the renderer component bindings to a public header |
| 4 | C# has no sprite or camera mirror, no UI widgets, no gameplay text, no window size and no screen-to-world conversion | HUD bars and slider knobs are scene-authored SCREEN-space sprites laid out by `colony.hud`; fixed 1280 x 720 window; C# hit-tests with `colony.config` geometry | a `Screen` facade and `Camera.ScreenToWorld` (Grid Sweeper's 2D picking needs the same) |
| 5 | `world::FLUSH_INLINE_PAYLOADS = 4`: a fifth payload on one entity in one flush allocates | the payload rule of 2.4.5; the allocation test hatches and spawns raiders inside its window | a larger or pooled inline payload list |
| 6 | stages are packed by component mask, not archetype, and `scheduler_desc::minEntitiesPerJob` (1,024) is engine-owned, not settable per system or project | four consecutive stages; inline budget; `colony_20k` for visible fan-out | archetype-aware conflicts; a per-system grain hint |
| 7 | no benchmark row covers sprites; extraction locks a mutex and hashes a uuid per sprite | gates confirmed in S0; tile count is the first lever | **P3**: `renderer.sprites_10k`; a last-asset memo in the extract loop |
| 8 | a shared-engine executable cannot count allocations, `conjure::bench_harness` links the static core and exports no `main` | one source list compiled twice; `colony_benchmarks` carries a short `main` over `conjure::bench::parse_command_line` and `bench_runner` | export the benchmark entry point |
| 9 | no precedent for the real plugin plus the real CLR in one shared-engine process, nor for `[NativeComponent]` mirrors of plugin-registered components | the S0 spike; `u32`-only mirrors; `colony.mirrors_resolve`; the static fallback of 2.4.9 | none if the spike passes |
| 10 | no floating-point contraction policy in the repository's CMake | integer per-ant state, exact-product fold, no libm, contraction off on the module target | a repository-wide policy |
| 11 | not verified: scene unload, snapshot restore, the picker and the serializer over raw entities carrying only a sprite and a matrix; texture lifetime when only raw entities use it | `colony.setup` destroys and respawns what it owns; prototype entities stay in the scene (disabled) and hold the texture references; checked in S0. Fallback: give ants `local_transform` at 0.09 to 0.26 ms per frame (2.4.3) | none unless S0 finds a bug |

#### 2.4.13 For the owner to confirm

1. **Intent column instead of per-worker deposit buffers** (2.4.4): the stated preference is feasible; the column was
   chosen because grants need it anyway.
2. **A third field, DIG scent**: diggers need a signal to find painted zones without pathfinding. Fallback if
   rejected: diggers wander on HOME and dig marks they bump into; digging far from the queen becomes slow.
3. **HOME diffuses and has a constant source at the queen**: that is what lets nurses and raiders navigate tunnels
   with no extra field.
4. **Four role systems are kept** although only the forager pass fans out at 5,000 ants; the alternative (one steer
   system choosing the kernel per chunk) would use five grains but gives up "one system per role".

## 3. Build order and dependencies

Wave 6 runs after wave 5. Order: Grid Sweeper (also finishes the protocol end-to-end test), Barrage, Colony, Rolling
Course, Corridor Strike. Barrage precedes Colony because Colony builds on what Barrage establishes: the sprite
measurement (P3), the recorded-hash data file and the per-demo benchmark executable, and the C# query-scope tutorial
that the Colony tutorial links to. Colony is the last of the 2D demos rather than the last overall: it is the flagship
and the largest risk, its S0 exercises engine paths (module loading, the sprite path, `world_hash`) whose bugs should
surface while the wave still has room to fix them, and the 3D demos depend on none of it.

Each demo is one implementer agent (project + scripts + tutorial), followed by one reviewer who plays it headless and
through the real editor, checks the tutorial against the final project, and files engine bugs found along the way as
`docs/STATUS.md` entries or fixes them when trivial. Colony is the exception: two implementer tasks with a review
between them. Task 1 is stage S0 (2.4.1), reviewed on its numbers alone; task 2 is S1 to S4 and the tutorial.

Wave 6 also owes the plumbing the demos need outside `engine/`: `samples/CMakeLists.txt` adds `games/<demo>` ahead of
its early return for a missing `feature_test`; `docs/api/tutorials/` is created; `samples/tools/use_packs.py` and the
`samples/games_packed/` ignore entry (G4); the nightly workflow builds and runs the demo benchmark executables (4.3).

## 4. Tests

### 4.1 Engine prerequisites

Engine changes of their own (own PR, own tests, the cross-wave gates of `ARCHITECTURE.md`), owed before the step
named; none is demo code.

| Id | Change | Owed before |
|---|---|---|
| P1 | `ecs::world_hash` hashes only the leading 16 uuid bytes of an `ASSET_REF` field. Today `hash_fields` (`engine/src/ecs/world_hash.cpp`) hashes all 24 bytes of `assets::asset_ref`, including the slot pointer that scene load and `Entity.AddComponent` bind, so the hash of any world with a bound sprite differs between processes. Test: a bound and an unbound ref hash equal | any demo records a hash (Grid Sweeper included) |
| P2 | `component_add` from C# accepts an id reserved by the runtime's deferred buffer inside a query scope and records `add_raw` (test in `tests/scripting/test_dod_query.cpp`) | Barrage B0 |
| P3 | benchmark scenario `renderer.sprites_10k` (extraction, `sprite_batcher::build`, upload, null device) | Barrage B0; it confirms both frame gates |
| P4 | renderer component name bindings in a public header | Colony S0 (a way around exists) |

### 4.2 Per-demo tests

`samples.games.<demo>` (label `slow`): headless boot for 600 frames with scripted input
(`conjure::testing::input_script`), all `Diagnostics.Check` assertions passing, `world_hash` equal to the recorded
value at frame 600, exit code 0. `samples.games.<demo>.export` (label `process`, nightly): export and run the exported
folder for 120 frames.

A demo may add tests under `samples/games/<demo>/tests` (2.3.9, 2.4.9): unit suites for its own code, extra scenes,
and one benchmark executable. Which run carries which G5 hash:

| Demo | 600-frame boot run | Additional hashed run |
|---|---|---|
| Barrage | `barrage.scene.json`: phase 1 and its burst, about 970 bullets | `soak_burst.scene.json` at frame 580: about 4,096 bullets, scatter and cap active |
| Colony | `colony.scene.json` from one queen: a boot and input test; one ant has hatched by frame 600 | `colony_headstart.scene.json` (200 ants) for gameplay; `colony_5k.scene.json` for worker-count equality on the static build |

The from-one-queen run proves that the project boots, the plugin and the mirrors resolve, input reaches the simulation
and the first egg hatches; it cannot show foraging or 5,000 ants. Scale is tested on pre-seeded scenes
(`colony.config`), and growth from one queen over time by `samples.games.colony.fast_forward`. `slow` cases run in the
nightly Debug presets, not in the default `ctest` run; they assert deterministic facts and allocation counts, never
time.

### 4.3 Budget tests (G7)

| | Barrage | Colony |
|---|---|---|
| Executable | `barrage_benchmarks` in `samples/games/barrage/tests` | `colony_benchmarks` in `samples/games/colony/tests` |
| Scenarios | `barrage.soak_4k`, `barrage.burst_4k` | `colony.sim_step_5k`, `colony.full_frame_5k` |
| Scene | `soak.scene.json`, `soak_burst.scene.json` (4,096 bullets) | `colony_5k.scene.json` (5,000 ants, static registration) |
| Desktop p50 / p99 | frame 2.0 / 4.0 ms, `FIXED_UPDATE` 0.5 / 1.0 ms | frame 4.0 / 8.0 ms, `FIXED_UPDATE` 1.0 / 2.0 ms |
| Allocations | 0 native; 0 managed bytes (script-sampled) | 0 native |

- Harness: each executable links `conjure::bench_harness` (`tests/benchmarks/include/conjure/bench/benchmark.h`,
  `bench_runner.h`), registers its scenarios with `CONJURE_BENCHMARK`, has its own `thresholds.json` and CTest entries
  `benchmark.<scenario>` with the label `benchmark` (Barrage also `scripting` and `dotnet`; it skips with exit 77
  without the .NET runtime). The targets exist only when `TARGET conjure::bench_harness` does.
- Configuration and machine class: thresholds are enforced in Development (and Distribution) through
  `CONJURE_BENCH_ENFORCE_DEFAULT` set on each demo target as `tests/benchmarks` does, and reported only in Debug. The
  `desktop` class on an idle reference machine carries the G7 claims; the `ci` class (three times the desktop times,
  same allocation gates) runs in the nightly `benchmarks-ci-class` job, which must build and run both executables with
  `--class ci --enforce` next to `conjure_benchmarks`.
- Measurement: `bench_context::measure` runs the harness-wide 20 warm-up and 200 measured iterations. Each scenario
  first runs its own 600 pre-warm frames, which execute every path (one Barrage burst; Colony role moves, a hatch, a
  death, a raider wave and a checksum step). Periodic work is sized to fall inside any 200-frame window: Barrage
  bursts every 200 steps in `burst_4k`, Colony checksums every 60 steps and moves roles every step.
- No worker speed-up is asserted anywhere; `colony.sim_step_5k` is reported for 0 and auto threads.
- Allocation-only gates that need no timing also run as `slow` doctest cases (`samples.games.colony.allocations`).

## 5. Out of scope

Online features, controller rumble, localisation, accessibility options, achievements, and any asset production. For
Barrage and Colony the cut lists of 2.3.10 and 2.4.11 are binding. Also out of scope: any change under `engine/`,
`managed/Conjure.Managed`, `editor/`, `runtime/` or `tests/` inside a demo's own change (G3; the prerequisites of 4.1
are separate engine changes), timing assertions inside doctest cases, and asserting a speed-up between worker counts.
