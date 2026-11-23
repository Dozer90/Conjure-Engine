# Export and diagnostics
<!-- doc-template:wiki -->
[API](../index.md) → [Engine](index.md)

<!-- doc-content:purpose:start -->
Two small services of the engine module: `export_pipeline` in `conjure/engine/export.h` turns a development
project into a standalone folder the runtime can run, and `check_registry` in
`conjure/engine/diagnostics/check_registry.h` collects the pass / fail checks a sample or a test scene reports
about itself (the C# `Diagnostics.Check` writes here).
<!-- doc-content:purpose:end -->

|---|---|
| Name | Export and diagnostics |
| Module | Engine |
| `#include` | `conjure/engine/diagnostics/check_registry.h`, `conjure/engine/export.h` |
| Engineering Handbook | [Engineering handbook](../../handbook/engine/engine.md) |

## Syntax

<!-- doc-content:syntax:start -->
Usage and complete examples are described in the reference sections below.
<!-- doc-content:syntax:end -->

## At a glance

<!-- doc-content:at_a_glance:start -->
| You want to... | Use |
|---|---|
| Export a project from code | `export_pipeline pipeline(app.context()); pipeline.run(settings)` |
| Export from the editor | `conjure_editor --headless --project <dir> --trust-project --export <out>` (`--trust-project` answers for a project the editor has not been told to trust, 📘 API: [Project trust](../editor/headless-and-commands.md#project-trust)) or the `project.export` command |
| Choose what goes into the folder | `export_settings`: `alwaysInclude`, `copyRuntime`, `copyShaders`, `publishManaged`, `buildNativeModule` |
| Know what was written | the returned `export_report` |
| Report a check from engine code | `context.checks->report(name, passed, detail, frame)` |
| Assert every check passed | `checks.all_passed()`, `failed_count()`, `records()` |
<!-- doc-content:at_a_glance:end -->

## Variables and Properties

<!-- doc-content:variables:start -->
Types, fields, and state are described in the reference sections below.
<!-- doc-content:variables:end -->

<!-- doc-content:body:start -->
## Exporting

`export_pipeline::run(settings, progress)` runs six steps and names each in `export_report::steps` and through
the optional `progress_fn(step, fraction)`; the first step that fails ends the export:

| Step | What happens | Skipped when |
|---|---|---|
| `build_game_module` | builds the project's native module in Distribution through CMake | `buildNativeModule` is false or the project has no `module.json` with a binary |
| `cook_assets` | `asset_cooker::cook` from the startup scene and `alwaysInclude` into `<out>/cooked/content.cpak` + `manifest.json`; a project without a startup scene, or whose startup scene is not a known asset, cooks every asset (`includeAll`): stray files no importer handles and deleted sources are skipped with an export warning (`'<path>' skipped: record is NO_IMPORTER`) unless `alwaysInclude` names them or a cooked asset depends on them, and any other unloadable asset still fails the step | never |
| `publish_managed` | `dotnet publish` of the bootstrap and `dotnet build` of the game scripts into `<out>/managed`; the `.pdb` symbols are left out unless the project's `scripting.debug_symbols` is true, and a framework-dependent export does not publish the bootstrap's apphost (a self-contained one must) | `publishManaged` is false or scripting is disabled |
| `export_modules` | `module_manager::export_layout(<out>, DISTRIBUTION)` | never (writes nothing without modules) |
| `copy_runtime` | copies `conjure_runtime` and `conjure_shared` from `distributionDirectory`, with them what an MSVC build placed there (`nethost.dll` and the Visual C++ runtime DLLs: every `vcruntime140*.dll` and `msvcp140*.dll` except the debug runtime) and the licence notices `THIRD_PARTY_NOTICES.txt`, plus `shaders/` (every `index.json` and `.dxil` / `.spv` blob, subfolders included; the build's `.d` depfiles are left out) | `copyRuntime` / `copyShaders` are false |
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
test the integration suite runs (📘 API: [The runtime](../runtime/index.md)). A headless run executes no C#:
add `--scripting` to smoke-test a scripted game with its scripts running, on a machine with .NET.

> [!IMPORTANT]
> The pipeline needs a *development* project: a cooked folder has no asset database and `run()` answers
> `INVALID_STATE`. Run it outside a frame (the editor runs `--export` before its first frame). The output must be
> a folder of its own: the project directory itself or a folder inside the assets root is `BAD_ARGUMENTS`
> (`<project>/export` is fine).

### The Visual C++ runtime on Windows

A Windows build of the runtime with MSVC imports the Visual C++ runtime DLLs (the default, `/MD`; a build
linked against the static runtime imports none). An MSVC Distribution build (`windows-msvc-dist`) copies the
toolchain's redistributable ones next to `conjure_runtime`, and `copy_runtime` ships every `vcruntime140*.dll`
and `msvcp140*.dll` it finds directly in `distributionDirectory`, the debug runtime excepted:

| File | What it is |
|---|---|
| `vcruntime140.dll`, `vcruntime140_1.dll` | the compiler's runtime support; `conjure_runtime` and `conjure_shared` import both |
| `msvcp140.dll`, `msvcp140_atomic_wait.dll` | the C++ standard library; `conjure_shared` imports both |
| `msvcp140_1.dll`, `msvcp140_2.dll`, `msvcp140_codecvt_ids.dll`, `vcruntime140_threads.dll` | further parts of the same runtime, copied for a native game module that uses them (which of them exist depends on the toolset) |

The exported game then starts on a machine without the Microsoft Visual C++ Redistributable, and each export
grows by about 1.3 MB. The Universal CRT (`ucrtbase.dll`, the `api-ms-win-crt-*` names) is part of Windows 10 and
later and is not copied. Linux and macOS exports have no such files.

> [!NOTE]
> Microsoft permits shipping these DLLs next to the application. Copies shipped this way are not serviced by
> Windows Update: a fixed runtime reaches players only with a new export from a rebuilt Distribution.

> [!WARNING]
> A `distributionDirectory` without these DLLs (the `bin` of a Debug or Development build, which copies none, or
> of a Distribution build whose configure warned that it found no redistributable folder of the compiler's
> version) still exports, with a warning in `export_report::warnings`: that game starts only where the
> redistributable is installed. The pipeline judges by its own build: the warning comes from one built with MSVC
> against the DLL runtime, and one built against the static runtime, or for Linux or macOS, says nothing. Only
> the two name patterns are shipped: a native game module that imports another redistributable library (the
> Concurrency Runtime `concrt140.dll`, an OpenMP runtime) must place it in the exported folder itself. The
> pipeline copies what it finds and never removes a file from the output folder, so export into an empty folder
> when the distribution directory changed.

> [!WARNING]
> The debug runtime is never shipped: Microsoft does not allow it to be redistributed. A file under the two
> patterns with a `d` directly after `140` or at the end of its name (`vcruntime140d.dll`, `msvcp140_1d.dll`,
> `msvcp140d_atomic_wait.dll`) stays in `distributionDirectory`, and `export_report::warnings` names every one
> that was left behind. A game exported from a Debug build therefore starts only where Visual Studio is installed.

### Licence notices

The engine files in an exported game are Conjure, under the Apache License 2.0, and the third-party software
linked into it (GLFW, Jolt, NVRHI, miniaudio and others), whose licences ask that their notices travel with the
binaries. Every build of the runtime writes them into `THIRD_PARTY_NOTICES.txt` next to `conjure_runtime`:
Conjure's notice and licence first, then one entry for each library that build links, with its licence text.
`copy_runtime` copies the file (`export_pipeline::NOTICES_FILE_NAME`) into the exported folder with the runtime,
so shipping the folder as it is covers the engine. The complete list for all platforms is
`THIRD_PARTY_NOTICES.md` in the source tree.

> [!IMPORTANT]
> The file covers the engine only. What your game adds (native modules and the libraries they link, NuGet
> packages, fonts, art and audio) is yours to list: ship your own notices next to it. Keep the file when you
> repackage the folder into an installer or an archive.

> [!WARNING]
> A `distributionDirectory` without the file still exports, with a warning in `export_report::warnings` that the
> game carries no licence notices: point the export at the `bin` folder of a build, which holds it, or copy the
> file yourself. An export that does not copy the runtime copies no notices either. The.NET entry is the licence
> of the .NET installation on the machine that built the engine, also when another machine exports the game.

> [!WARNING]
> `EXPORT_FAILED` names the step and the cause, and the steps before it keep their output: a failed
> `publish_managed` leaves a valid `cooked/` behind. `copy_runtime` copies file by file, so when one copy fails
> the files copied before it stay in the folder.

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
<!-- doc-content:body:end -->

## Functions

<!-- doc-content:functions:start -->
<a id="api-summary"></a>

| Signature (short) | Purpose |
|---|---|
| `explicit export_pipeline(engine_context &)` | bind to a running application |
| `result<export_report> run(const export_settings &, const progress_fn & = {})` | the six steps |
| `export_pipeline::STEP_NAMES` | the step names, in order |
| `export_pipeline::NOTICES_FILE_NAME` | `"THIRD_PARTY_NOTICES.txt"`, the licence notices `copy_runtime` ships |
| `struct export_settings { outputDirectory, distributionDirectory, alwaysInclude, buildNativeModule, cmakeExecutable, publishManaged, dotnetExecutable, bootstrapProject, scriptsProject, runtimeIdentifier, copyRuntime, copyShaders, processTimeoutSeconds }` | what and where |
| `struct export_report { assetCount, packBytes, contentHash, modulesExported, nativeModuleBuilt, managedPublished, steps, copiedFiles, warnings, elapsedSeconds }` | what was done |
| `void check_registry::report(string_view, bool, string_view, u64)` | one verdict |
| `vector<check_record> records() const` / `bool all_passed() const` / `u64 failed_count() const` / `usize record_count() const` / `void clear()` | reading the verdicts |
<!-- doc-content:functions:end -->

------------------------------------------

**See also:**

<!-- doc-content:related:start -->
- 📘 API: [Running the engine](running-the-engine.md), [Cooking, caches and packs](../assets/cooking-and-packs.md), [Loading and managing modules](../modules/loading-and-managing.md)
- 📘 API: [The editor](../editor/index.md) (`--export`, `project.export`), [The runtime](../runtime/index.md)
- 🛠 Handbook: [Engine module](../../handbook/engine/engine.md)
<!-- doc-content:related:end -->

<!-- doc-legacy-anchors:start -->
<a id="see-also"></a>
<!-- doc-legacy-anchors:end -->
