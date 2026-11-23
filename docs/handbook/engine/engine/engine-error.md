---
kind: header
---

[Engine](../../engine.md) → [Engine](../engine.md)

# engine_error.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/engine/engine_error.h` |
| API ↗ | [Engine: the application and the frame](../../../api/engine/index.md), [Running the engine](../../../api/engine/running-the-engine.md) |

<p>Failure conditions reported by the engine module.</p>

## Enumerations

### `engine_error` {#symbol-engine-error}

```cpp
enum class engine_error : u16 {
    NONE = 0,
    INVALID_CONFIG,                 // engine_config::validate rejected a field; the message names it
    BAD_ARGUMENTS,                  // a known command-line flag lacks its value or carries a bad one, or a
                                    // caller argument is malformed; unknown flags are not errors
                                    // (parse_command_line returns them in unrecognisedArguments)
    PROJECT_NOT_FOUND,              // project.json (or the project directory) does not exist
    PROJECT_PARSE_FAILED,           // project.json is not valid JSON; the message carries line and column
    PROJECT_INVALID,                // project.json lacks a required member or has one of the wrong type
    PROJECT_SCHEMA_TOO_NEW,         // schema_version above PROJECT_SCHEMA_VERSION
    JOBS_START_FAILED,              // core::jobs::job_system::start refused the configuration
    WINDOW_CREATE_FAILED,           // platform::create_window failed (startup step 6)
    DEVICE_CREATE_FAILED,           // rhi::device::create or rhi::swapchain::create failed (step 7)
    ASSET_DATABASE_FAILED,          // the asset database, cache or pack could not be opened or scanned (step 8)
    RENDERER_INIT_FAILED,           // renderer::create or the shader library failed (step 13)
    PHYSICS_INIT_FAILED,            // physics_settings::from_json / validate or attach failed (step 12)
    SCRIPT_RUNTIME_NOT_FOUND,       // no .NET runtime while scripting.required (step 17)
    SCRIPT_ASSEMBLY_LOAD_FAILED,    // the game assembly did not load while scripting.required (step 17)
    SCRIPT_RUNTIME_INIT_FAILED,     // a present runtime failed its handshake or version check (step 17)
    MODULE_LOAD_FAILED,             // a module named in modules.required did not load (step 15)
    SCHEDULER_CONFLICT,             // scheduler::finalise rejected the registered systems (step 18)
    LAYER_ATTACH_FAILED,            // a layer's on_attach failed (step 19)
    STARTUP_SCENE_FAILED,           // the startup scene could not be loaded, or its source no longer
                                    // imports, in a runtime build (step 20)
    ALREADY_INITIALISED,            // initialise() on an application that is not CREATED
    NOT_INITIALISED,                // step_frame() before initialise() or after shutdown()
    DEVICE_LOST,                    // the device was lost during a frame
    EXPORT_FAILED,                  // a step of export_pipeline::run failed; the message names it
    COUNT
};
```

Failure conditions reported by the engine module.

## Macros

### `CONJURE_DECLARE_ERROR_CATEGORY` {#symbol-conjure-declare-error-category}

```cpp
CONJURE_DECLARE_ERROR_CATEGORY(conjure::engine::engine_error, "engine", conjure::engine::engine_error_name)
```

## Functions

### Public

#### `engine_error_name` {#function-engine-error-name}

<div class="function-declaration" id="function-engine-error-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view engine_error_name(
    <a href="engine-error.html#symbol-engine-error">engine_error</a> code) <span class="k">noexcept</span>;</code></pre></div>

<p>The stable upper-case enumerator name of a code (&quot;<code>INVALID_CONFIG</code>&quot;); &quot;&quot; for unknown values.</p>

#### `make_error` {#function-make-error}

<div class="function-declaration" id="function-make-error-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">inline</span> <a href="../core/error/error.html">core::error</a> make_error(
    <a href="engine-error.html#symbol-engine-error">engine_error</a> code,
    std::string message = {});</code></pre></div>

<p>Builds an error of the engine category with a fixed detail message (possibly empty).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="classes"></a>
<a id="conjure_declare_error_category"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="engine_error"></a>
<a id="engine_error_name"></a>
<a id="engine_erroralready_initialised"></a>
<a id="engine_errorasset_database_failed"></a>
<a id="engine_errorbad_arguments"></a>
<a id="engine_errorcount"></a>
<a id="engine_errordevice_create_failed"></a>
<a id="engine_errordevice_lost"></a>
<a id="engine_errorexport_failed"></a>
<a id="engine_errorinvalid_config"></a>
<a id="engine_errorjobs_start_failed"></a>
<a id="engine_errorlayer_attach_failed"></a>
<a id="engine_errormodule_load_failed"></a>
<a id="engine_errornone"></a>
<a id="engine_errornot_initialised"></a>
<a id="engine_errorphysics_init_failed"></a>
<a id="engine_errorproject_invalid"></a>
<a id="engine_errorproject_not_found"></a>
<a id="engine_errorproject_parse_failed"></a>
<a id="engine_errorproject_schema_too_new"></a>
<a id="engine_errorrenderer_init_failed"></a>
<a id="engine_errorscheduler_conflict"></a>
<a id="engine_errorscript_assembly_load_failed"></a>
<a id="engine_errorscript_runtime_init_failed"></a>
<a id="engine_errorscript_runtime_not_found"></a>
<a id="engine_errorstartup_scene_failed"></a>
<a id="engine_errorwindow_create_failed"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="make_error"></a>
<a id="other-declarations"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
