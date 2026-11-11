# Export and diagnostics

📘 **Public API wiki** · 🛠 Handbook: [Engine module](../../handbook/modules/engine.md)

Two small services of the engine module: `export_pipeline` in `conjure/engine/export.h` turns a development
project into a standalone folder the runtime can run, and `check_registry` in
`conjure/engine/diagnostics/check_registry.h` collects the pass / fail checks a sample or a test scene reports
about itself (the C# `Diagnostics.Check` writes here).

## At a glance

| You want to... | Use |
|---|---|
| Export a project from code | `export_pipeline pipeline(app.context()); pipeline.run(settings)` |
| Export from the editor | `conjure_editor --headless --project <dir> --export <out>` or the `project.export` command |
| Choose what goes into the folder | `export_settings`: `alwaysInclude`, `copyRuntime`, `copyShaders`, `publishManaged`, `buildNativeModule` |
| Know what was written | the returned `export_report` |
| Report a check from engine code | `context.checks->report(name, passed, detail, frame)` |
| Assert every check passed | `checks.all_passed()`, `failed_count()`, `records()` |

## Exporting

`export_pipeline::run(settings, progress)` runs six all-or-nothing steps and names each in `export_report::steps`
and through the optional `progress_fn(step, fraction)`:

| Step | What happens | Skipped when |
|---|---|---|
| `build_game_module` | builds the project's native module in Distribution through CMake | `buildNativeModule` is false or the project has no `module.json` with a binary |
| `cook_assets` | `asset_cooker::cook` from the startup scene and `alwaysInclude` into `<out>/cooked/content.cpak` + `manifest.json` | never |
| `publish_managed` | `dotnet publish` of the bootstrap and `dotnet build` of the game scripts into `<out>/managed` | `publishManaged` is false or scripting is disabled |
| `export_modules` | `module_manager::export_layout(<out>, DISTRIBUTION)` | never (writes nothing without modules) |
| `copy_runtime` | copies `conjure_runtime`, `conjure_shared` (and `nethost` on Windows) from `distributionDirectory`, plus `shaders/` | `copyRuntime` / `copyShaders` are false |
| `write_project` | writes `<out>/project.json` with `"cooked": true` and `cooked_root = "cooked"` | never |

```cpp
#include "conjure/engine/export.h"

engine::export_pipeline pipeline(app.context());          // a running application over the project
engine::export_settings settings = {};
settings.outputDirectory = "C:/builds/rolling_course";
settings.distributionDirectory = "C:/conjure/build/windows-msvc-dist/bin";
settings.alwaysInclude = {"assets://data/levels.bin"};  // cooked in addition to the scene closure
core::result<engine::export_report> report = pipeline.run(settings, [](std::string_view step, f32 done) {
    CONJURE_LOG_INFO(LOG_GAME, "export {} ({:.0f} %)", step, done * 100.0f);
});
CONJURE_TRY(report);
CONJURE_LOG_INFO(LOG_GAME, "{} assets, {} bytes", report->assetCount, report->packBytes);
```

The exported folder runs with `conjure_runtime --project <out>`; headless, `--headless --frames 60` is the smoke
test the integration suite runs (📘 API: [The runtime](../runtime/index.md)).

> [!IMPORTANT]
> The pipeline needs a *development* project: a cooked folder has no asset database and `run()` answers
> `INVALID_STATE`. Run it outside a frame (the editor runs `--export` before its first frame).

> [!WARNING]
> `EXPORT_FAILED` names the step and the cause, and nothing of a failed step is written, but earlier steps keep
> their output: a failed `publish_managed` leaves a valid `cooked/` behind.

## Diagnostics checks

`check_registry::report(name, passed, detail, frame)` is thread-safe and appends a `check_record` (`name`,
`passed`, `detail`, `frame`). `records()` copies them, `all_passed()` and `failed_count()` summarise,
`record_count()` counts and `clear()` empties. The engine creates one registry per application and publishes it
as `engine_context::checks`; `Conjure.Diagnostics.Check(name, passed, detail)` from a script lands in the same
registry, so a sample scene can verify itself and a test can read the verdicts after `step_frames`.

```cpp
engine::diagnostics::check_registry &checks = *app.context().checks;
checks.report("spawner.count", world.entity_count() == 500, "500 particles expected", app.time().frameIndex);
CHECK(checks.all_passed());
for (const engine::diagnostics::check_record &record : checks.records()) {
    if (!record.passed) {
        CONJURE_LOG_WARN(LOG_GAME, "check {} failed at frame {}: {}", record.name, record.frame, record.detail);
    }
}
```

> [!TIP]
> Report a check once, when the condition is decided, not every frame: the registry keeps every record.

## API summary

| Signature (short) | Purpose |
|---|---|
| `explicit export_pipeline(engine_context &)` | bind to a running application |
| `result<export_report> run(const export_settings &, const progress_fn & = {})` | the six steps |
| `export_pipeline::STEP_NAMES` | the step names, in order |
| `struct export_settings { outputDirectory, distributionDirectory, alwaysInclude, buildNativeModule, cmakeExecutable, publishManaged, dotnetExecutable, bootstrapProject, scriptsProject, runtimeIdentifier, copyRuntime, copyShaders, processTimeoutSeconds }` | what and where |
| `struct export_report { assetCount, packBytes, contentHash, modulesExported, nativeModuleBuilt, managedPublished, steps, copiedFiles, warnings, elapsedSeconds }` | what was done |
| `void check_registry::report(string_view, bool, string_view, u64)` | one verdict |
| `vector<check_record> records() const` / `bool all_passed() const` / `u64 failed_count() const` / `usize record_count() const` / `void clear()` | reading the verdicts |

## See also

- 📘 API: [Running the engine](running-the-engine.md), [Cooking, caches and packs](../assets/cooking-and-packs.md), [Loading and managing modules](../modules/loading-and-managing.md)
- 📘 API: [The editor](../editor/index.md) (`--export`, `project.export`), [The runtime](../runtime/index.md)
- 🛠 Handbook: [Engine module](../../handbook/modules/engine.md)
