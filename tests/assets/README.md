# tests/assets - repository fixtures

The in-repo fixtures `conjure::testing::repo_asset(relativePath)` resolves (`tests/support/include/conjure/testing/
test_assets.h`; `docs/design/08_testing.md` 2.1). They are small, hand-checked and read-only: a test that needs to
write next to a fixture copies it into a `temp_directory` first (`integration_test::copy_minimal_project`). Downloaded
fixtures live in `tests/.downloaded` behind `precondition::DOWNLOADED_ASSETS` (`tools/download_test_assets.py`), not
here.

The C++ sources in this directory (`test_*.cpp`, `assets_test_support.*`, `CMakeLists.txt`) are the unit tests of the
`assets` module (`conjure_assets_tests`), which the directory name also stands for; the fixtures are the
subdirectories below.

| Fixture | Owner test | Purpose | Regenerating |
|---|---|---|---|
| `scenes/roundtrip.scene.json` | `tests/integration/test_scene_roundtrip.cpp` | Five entities (`Root` with `Child/Leaf` and `Sibling`, a disabled `Other` tagged `Pickup`), the canonical `scene_serializer::save_scene` text. The first case loads it and saves it back byte for byte; the second authors the same scene through the API and compares. | A byte-exact snapshot of `integration_test::author_roundtrip_scene` (`tests/integration/integration_engine_support.cpp`). Whenever the serializer's canonical form or the helper changes, run `conjure_integration_tests -tc="*programmatic scene*" --conjure-update-golden`, which rewrites this file from the helper, then review the diff. The guids are the ones the helper's entities received when the snapshot was taken; a regeneration changes them. |
| `scenes/determinism.scene.json` | `tests/integration/test_determinism.cpp` | 51 entities in a fixed shape: 3 `Group` roots, 4 `Node` children each, 3 `Leaf` children per node, every entity with a `local_transform`, a `name`, a `guid` and (below the roots) a `parent`. The scene is the stable starting world whose per-frame `world_hash` sequence must repeat across runs and worker counts (R41); the spawning is done by the test's own systems. | Authored once through `scene_serializer::save_scene` from a scene built with `scene::create_entity` / `set_local_transform` in that shape. Any scene of the same shape works: the test asserts only a non-empty load and the hash sequences it computes itself. Edit the file with the serializer (load, change, save), never by hand, so it stays canonical. |
| `prefabs/crate.prefab.json` | `tests/integration/test_prefab_spawn_destroy.cpp` | A two-entity prefab (`Crate` with a `Lid` child), the smallest archive with a parent link, instantiated and destroyed 100 times per frame. `CRATE_ENTITIES` in the test is its entity count. | `scene_serializer::save_prefab` of a two-entity scene; update `CRATE_ENTITIES` when the shape changes. |
| `projects/minimal/` | `tests/integration/test_engine_project_boot.cpp`, `tests/integration/test_export_roundtrip.cpp`; copied by the CI export smoke steps (`.github/workflows/ci.yml`, `nightly.yml`) | The smallest development project: `project.json` (schema 1, assets root `assets`, startup scene `scenes/main.scene.json`, scripting off), a four-entity startup scene (`Root` with `Child` and `Child2`, a `Camera`) and `data/notes.bin` (a binary asset the export's `alwaysInclude` names). Stands in for `samples/feature_test` until wave 5 lands it (`docs/design/09_cross_area_resolutions.md` 4 item 15c). | Edit `project.json` by hand; save `main.scene.json` through the serializer. `test_engine_project_boot` reads its entity count from the scene file, `test_export_roundtrip` compares the development and exported loads, so neither pins the count. |

Layout notes:

- `08_testing.md` 2.1 also lists `commands/` and `protocol_corpus/` here; the editor owner keeps both under
  `tests/editor/data/` (`commands/build_grid_sweeper.json`, `protocol_corpus/`), and
  `tests/integration/test_ai_protocol_e2e.cpp` reads the command file from there through
  `CONJURE_INTEGRATION_EDITOR_COMMAND_DIR` (`tests/integration/CMakeLists.txt`).
- The glTF, texture, HDR and audio fixtures the same section names are not in the repository yet; the assets,
  renderer and audio unit tests generate what they need in memory or fetch it through `tools/test_assets.json`.
- The asset database writes `.meta` sidecars and its cache next to a project's sources, which is why a project
  fixture is always copied before it is opened (`integration_engine_support.h`).
