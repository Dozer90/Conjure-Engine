# 17 - Demo games (wave 6)

Status: **binding** (owner: lead architect). Added 2026-11-10 from the customer's request: concrete demo games
for 2D and 3D that double as the user's introduction to the engine. Designs are generic and not derived from
any copyrighted title; the customer supplies art, audio and model assets they hold the rights to.

## 1. Goals

| Id | Decision |
|---|---|
| G1 | Five demos under `samples/games/`: three 2D (small, one mechanic each) and two 3D (larger, several systems each). Each is a complete Conjure project: `project.json`, scenes, prefabs, C# scripts, assets, README, and a CTest entry that boots it headless for N frames with `Conjure.Diagnostics.Check` assertions. |
| G2 | Every demo is also a **wiki tutorial** (📘 `docs/api/tutorials/<demo>.md`): the order in which it was built, the engine features it shows, and links to the API pages used. The 2D demos are the "first hour" path; the 3D demos are the "first weekend" path. |
| G3 | Demos are built through the public surface only (editor protocol, C# scripting API, project files). No engine code may be added for a demo; a missing capability is an engine bug or a scoped follow-up, recorded in the demo README. |
| G4 | Assets: the customer supplies CC0 packs under `samples/assets/` (ignored by git; layout, licences and the demo-to-kit mapping are in `samples/assets/README.md`): the Kenney all-in-one bundle (GLB models, PNG spritesheets with XML atlases, OGG audio) and Poly Haven 4K `.hdr` skies. Demos reference them through stable asset ids and ship with generated placeholders (primitive meshes, flat-colour sprites, synthesised tones) behind the same ids, so a clone without the packs still runs every demo and its tests; the README of each demo lists the slots. The CTest runs use placeholders only, so they never depend on the packs. |
| G5 | Each demo has a "known good" headless run recorded as a `world_hash` after N fixed steps (determinism) and a screenshot reference for the golden image job where applicable. |
| G6 | Scope is bounded: a demo is finished when its feature list below is met, its checks pass, and the tutorial reads cleanly. Polish beyond that is explicitly out of scope. |

## 2. The demos

### 2.1 2D

| Demo | Mechanics | Engine features shown |
|---|---|---|
| **Grid Sweeper** (`samples/games/grid_sweeper`, minesweeper-style) | grid of hidden cells, numbers, flags, win/lose, timer, difficulty presets | protocol-built scene (this replaces the earlier PacMan-style walkthrough as the AI-protocol end-to-end test: an agent authors the whole project from a command file), prefab instancing per cell, sprite atlas, orthographic camera, mouse picking in 2D, UI text via sprites/fonts, game state in C#, scene reload |
| **Tap Tycoon** (`samples/games/tap_tycoon`, endless clicker) | click to earn, buy generators, offline progress, save/load | C# scripting with save files through the VFS (`project://saves`), audio one-shots and groups, UI layout, number formatting, timers, data-driven upgrades from JSON |
| **Vault Runner** (`samples/games/vault_runner`, basic platformer) | run/jump/double-jump, moving platforms, hazards, collectibles, checkpoints, 3 short levels | 2D movement on Jolt with a Z-locked character controller, trigger volumes, sprite animation, parallax layers, camera follow with dead zone, additive level loading, input remapping, pause menu |

### 2.2 3D

| Demo | Mechanics | Engine features shown |
|---|---|---|
| **Rolling Course** (`samples/games/rolling_course`, marble-run style) | control a rolling ball by applying forces relative to the camera, ramps/bumpers/boosters/moving platforms, gems, checkpoints, par times, 5 courses, respawn on fall | dynamic rigid bodies with friction and restitution materials, contact events, kinematic movers, trigger volumes, third-person orbit camera with collision, PBR materials and IBL skies, cascaded shadows, SSAO, bloom on emissive gems, audio spatialisation (ball rolling loop pitched by speed), timer and best-times persistence |
| **Corridor Strike** (`samples/games/corridor_strike`, simple first-person shooter) | first-person movement, hitscan and projectile weapons, pickups (health/ammo/keys), doors and switches, 3 enemy types with simple state-machine AI (patrol/chase/attack), 2 levels, HUD | character controller, raycasts and shape casts, physics layers (player/enemy/projectile/world), prefab-heavy level assembly, navigation by waypoints, animation-less enemies using billboards or static meshes (assets decide), spot/point shadow lights, decals via sprites, muzzle flash lights, 3D audio, scene transitions, exported standalone build |

## 3. Build order and dependencies

Wave 6 runs after wave 5. Order: Grid Sweeper (also finishes the protocol end-to-end test), Tap Tycoon, Vault
Runner, Rolling Course, Corridor Strike. Each demo is one implementer agent (project + scripts + tutorial), followed
by one reviewer who plays it headless and through the real editor, checks the tutorial against the final project,
and files engine bugs found along the way as `docs/STATUS.md` entries or fixes them when trivial.

## 4. Tests

`samples.games.<demo>` (label `slow`): headless boot for 600 frames with scripted input (`conjure::testing::input_script`),
all `Diagnostics.Check` assertions passing, `world_hash` equal to the recorded value at frame 600, exit code 0.
`samples.games.<demo>.export` (label `process`, nightly): export and run the exported folder for 120 frames.

## 5. Out of scope

Online features, controller rumble, localisation, accessibility options, achievements, and any asset production.
