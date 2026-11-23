---
kind: header
---

[Engine](../../engine.md) → [Bindings](../bindings.md)

# native_api_entries.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/bindings/native_api_entries.h` |
| API ↗ | [The C# API](../../../api/bindings/csharp-api.md), [Scripting from C#: the bindings](../../../api/bindings/index.md), [The native table](../../../api/bindings/native-table.md) |

<p>The bindings-owned entries of the native table (3.1, B2, B3).</p>

## Structs

| File | Description |
|---|---|
| [`binding_context`](native-api-entries/binding-context.md) | Non-owning view of the engine services the entries act on. <code>A</code> <code>null</code> pointer means the subsystem is compiled out or disabled: <code>fill_native_table</code> then installs stubs for the groups that need it. Every pointer is stable from <code>application::initialise()</code> to <code>shutdown()</code>. |
| [`stubs::stub_thunk<Id, Ret(Args...)>`](native-api-entries/stubs-stub-thunk-id-ret-args.md) |  |

## Forward Declarations

### `asset_database` {#symbol-asset-database}

```cpp
class asset_database;
```

### `asset_manager` {#symbol-asset-manager}

```cpp
class asset_manager;
```

### `audio_engine` {#symbol-audio-engine}

```cpp
class audio_engine;
```

### `component_registry` {#symbol-component-registry}

```cpp
class component_registry;
```

### `debug_draw` {#symbol-debug-draw}

```cpp
class debug_draw;
```

### `diagnostics::check_registry` {#symbol-diagnostics-check-registry}

```cpp
class check_registry;
```

### `engine_context` {#symbol-engine-context}

```cpp
struct engine_context;
```

### `engine_time` {#symbol-engine-time}

```cpp
struct engine_time;
```

### `frame_stats` {#symbol-frame-stats}

```cpp
class frame_stats;
```

### `input` {#symbol-input}

```cpp
class input;
```

### `jobs::job_system` {#symbol-jobs-job-system}

```cpp
class job_system;
```

### `module_registry` {#symbol-module-registry}

```cpp
class module_registry;
```

### `physics_world` {#symbol-physics-world}

```cpp
class physics_world;
```

### `renderer` {#symbol-renderer}

```cpp
class renderer;
```

### `scene_manager` {#symbol-scene-manager}

```cpp
class scene_manager;
```

### `script_runtime` {#symbol-script-runtime}

```cpp
class script_runtime;
```

### `stubs::stub_thunk` {#symbol-stubs-stub-thunk}

```cpp
template <native_function_id Id, typename Fn> struct stub_thunk;
```

### `window` {#symbol-window}

```cpp
class window;
```

### `world` {#symbol-world}

```cpp
class world;
```

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-BINDINGS_ENTRY_COUNT"></a><code>BINDINGS_ENTRY_COUNT</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="na">CONJURE_<wbr>NATIVE_<wbr>API_<wbr>ENTRY_<wbr>COUNT</span> - <span class="na">CONJURE_<wbr>NATIVE_<wbr>API_<wbr>CORE_<wbr>ENTRY_<wbr>COUNT</span></code> | Number of bindings-owned entries (everything after the marker of <code>native_api.inl</code>). |
| <a id="variable-BINDINGS_GROUP_COUNT"></a><code>BINDINGS_GROUP_COUNT</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">10</span></code> | Number of groups the bindings own. |

## Macros

### `entries::CONJURE_BINDINGS_FOR_EACH_ENTRY` {#symbol-entries-conjure-bindings-for-each-entry}

```cpp
CONJURE_BINDINGS_FOR_EACH_ENTRY(CONJURE_BINDINGS_DECLARE_ENTRY)
```

## Functions

### Public

#### `active_context` {#function-active-context}

<div class="function-declaration" id="function-active-context-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">const</span> <a href="native-api-entries/binding-context.html">binding_context</a> &amp;active_context() <span class="k">noexcept</span>;</code></pre></div>

<p>The context the last <code>fill_native_table</code> installed (every pointer <code>null</code> before the first fill).</p>

#### `clear_active_context` {#function-clear-active-context}

<div class="function-declaration" id="function-clear-active-context-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> clear_active_context() <span class="k">noexcept</span>;</code></pre></div>

<p>Forgets the active context (shutdown; the entries then answer <code>NOT_SUPPORTED</code>).</p>

#### `detail::clear_frame_failure_log` {#function-detail-clear-frame-failure-log}

<div class="function-declaration" id="function-detail-clear-frame-failure-log-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> clear_frame_failure_log() <span class="k">noexcept</span>;</code></pre></div>

<p>Forgets which entries logged a failure this frame (<code>begin_frame</code>).</p>

#### `detail::fail_entry` {#function-detail-fail-entry}

<div class="function-declaration" id="function-detail-fail-entry-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> int32_t fail_entry( <a href="call-counters.html#symbol-native-function-id">native_function_id</a> id, <a href="bindings-error.html#symbol-bindings-error">bindings_error</a> code, std::string_view detail = {} ) <span class="k">noexcept</span>;</code></pre></div>

<p><code>fail()</code> for an entry by id: records the failure and, outside Distribution, logs it at <code>DEBUG</code> once per entry per frame (a dead-entity loop cannot flood the console).</p>

<div class="function-declaration" id="function-detail-fail-entry-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> int32_t fail_entry(<a href="call-counters.html#symbol-native-function-id">native_function_id</a> id, <span class="k">const</span> <a href="../core/error/error.html">core::error</a> &amp;failure) <span class="k">noexcept</span>;</code></pre></div>

<p><code>fail_entry</code> with the code and message of a <code><a href="../core/error/error.html">core::error</a></code> from a called module.</p>

#### `detail::failure_logged_this_frame` {#function-detail-failure-logged-this-frame}

<div class="function-declaration" id="function-detail-failure-logged-this-frame-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">bool</span> failure_logged_this_frame(<a href="call-counters.html#symbol-native-function-id">native_function_id</a> id) <span class="k">noexcept</span>;</code></pre></div>

<p>True when the entry already logged a failure since the last <code>clear_frame_failure_log</code>.</p>

#### `fill_native_table` {#function-fill-native-table}

<div class="function-declaration" id="function-fill-native-table-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; fill_native_table(
    <a href="../scripting/native-api/conjure-native-api.html">conjure_native_api</a> &amp;api,
    <span class="k">const</span> <a href="native-api-entries/binding-context.html">binding_context</a> &amp;context);</code></pre></div>

<p>Fills every bindings-owned slot of <code>api</code> (3.1 step 3): the real implementation when the group is compiled in and available, the stub otherwise. <code>A</code> slot already holding a foreign pointer (a plugin&#x27;s <code>native_binding_desc</code>) is overridden with a <code>WARN</code> when the group is compiled in and kept when it is compiled out (B3). Core-owned slots are never touched. Installs <code>context</code> as the active context (one table per process). Idempotent. Fails with <code>TABLE_INCOMPLETE</code> when an owned slot is still <code>null</code> afterwards (only reachable with a broken build).</p>

#### `group_available` {#function-group-available}

<div class="function-declaration" id="function-group-available-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">bool</span> group_available(
    std::string_view group,
    <span class="k">const</span> <a href="native-api-entries/binding-context.html">binding_context</a> &amp;context) <span class="k">noexcept</span>;</code></pre></div>

<p>True when the group&#x27;s real implementations have what they need in <code>context</code> (the world and the scene manager for transform, entity, scene and prefab; the physics world; the audio engine and the world; the input and the window; the asset manager; the module registry); the diagnostics group is always available. Unknown groups are never available.</p>

#### `group_compiled` {#function-group-compiled}

<div class="function-declaration" id="function-group-compiled-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">bool</span> group_compiled(
    std::string_view group) <span class="k">noexcept</span>;</code></pre></div>

<p>True when the module behind the group is compiled into this build (<code>CONJURE_BINDINGS_GROUP_COMPILED</code>).</p>

#### `is_counting_installed` {#function-is-counting-installed}

<div class="function-declaration" id="function-is-counting-installed-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">bool</span> is_counting_installed(
    <span class="k">const</span> <a href="../scripting/native-api/conjure-native-api.html">conjure_native_api</a> &amp;api,
    <a href="call-counters.html#symbol-native-function-id">native_function_id</a> id) <span class="k">noexcept</span>;</code></pre></div>

<p>True when the slot of <code>id</code> holds the entry&#x27;s counting thunk.</p>

#### `is_implementation_installed` {#function-is-implementation-installed}

<div class="function-declaration" id="function-is-implementation-installed-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">bool</span> is_implementation_installed(
    <span class="k">const</span> <a href="../scripting/native-api/conjure-native-api.html">conjure_native_api</a> &amp;api,
    <a href="call-counters.html#symbol-native-function-id">native_function_id</a> id) <span class="k">noexcept</span>;</code></pre></div>

<p>True when the slot of <code>id</code> holds the entry&#x27;s real implementation (directly or behind its thunk).</p>

#### `is_scope_hook_installed` {#function-is-scope-hook-installed}

<div class="function-declaration" id="function-is-scope-hook-installed-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">bool</span> is_scope_hook_installed(
    <span class="k">const</span> <a href="../scripting/native-api/conjure-native-api.html">conjure_native_api</a> &amp;api) <span class="k">noexcept</span>;</code></pre></div>

<p>True while the scope-end hook of <code>wrap_with_counters</code> sits over <code>query_end</code>. False again after <code>script_runtime::initialise</code> refilled the core slots (a runtime re-initialised after <code>shutdown()</code>), which is the signal to call <code>wrap_with_counters</code> once more.</p>

#### `is_stub_installed` {#function-is-stub-installed}

<div class="function-declaration" id="function-is-stub-installed-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">bool</span> is_stub_installed(
    <span class="k">const</span> <a href="../scripting/native-api/conjure-native-api.html">conjure_native_api</a> &amp;api,
    <a href="call-counters.html#symbol-native-function-id">native_function_id</a> id) <span class="k">noexcept</span>;</code></pre></div>

<p>True when the slot of <code>id</code> holds the entry&#x27;s stub (directly or behind its counting thunk).</p>

#### `stubs::report_not_supported` {#function-stubs-report-not-supported}

<div class="function-declaration" id="function-stubs-report-not-supported-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> int32_t report_not_supported(<a href="call-counters.html#symbol-native-function-id">native_function_id</a> id) <span class="k">noexcept</span>;</code></pre></div>

<p>Records &quot;&lt;entry&gt;: subsystem unavailable&quot; as the last error and returns -<code>NOT_SUPPORTED</code>.</p>

#### `wrap_with_counters` {#function-wrap-with-counters}

<div class="function-declaration" id="function-wrap-with-counters-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> wrap_with_counters(
    <a href="../scripting/native-api/conjure-native-api.html">conjure_native_api</a> &amp;api) <span class="k">noexcept</span>;</code></pre></div>

<p>Replaces every non-<code>null</code> slot (core-owned ones included) with its <code><a href="call-counters/counting-thunk-id-ret-args.html">counting_thunk</a></code> and remembers the original as the thunk&#x27;s target (B6), then installs the scope-end hook over <code>query_end</code> that applies the scene fixups of the locked entries (3.3). <code>A</code> slot already pointing at its thunk is left alone and the hook is re-installed over whatever it already covered, so a second call never double-counts and never chains the hook behind itself. In Distribution only the hook is installed.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="active_context"></a>
<a id="asset_database"></a>
<a id="asset_manager"></a>
<a id="audio_engine"></a>
<a id="binding_context"></a>
<a id="binding_contextassetdatabase"></a>
<a id="binding_contextassets"></a>
<a id="binding_contextaudio"></a>
<a id="binding_contextchecks"></a>
<a id="binding_contextcomponents"></a>
<a id="binding_contextdebugdraw"></a>
<a id="binding_contextfrom"></a>
<a id="binding_contextinput"></a>
<a id="binding_contextmodules"></a>
<a id="binding_contextphysics"></a>
<a id="binding_contextrenderer"></a>
<a id="binding_contextscenes"></a>
<a id="binding_contextscripting"></a>
<a id="binding_contextstats"></a>
<a id="binding_contexttime"></a>
<a id="binding_contextwindow"></a>
<a id="binding_contextworld"></a>
<a id="bindings_entry_count"></a>
<a id="bindings_group_count"></a>
<a id="check_registry"></a>
<a id="classes"></a>
<a id="clear_active_context"></a>
<a id="clear_frame_failure_log"></a>
<a id="component_registry"></a>
<a id="conjure_bindings_for_each_entry"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="debug_draw"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="detailclear_frame_failure_log"></a>
<a id="detailfail_entry"></a>
<a id="detailfailure_logged_this_frame"></a>
<a id="diagnosticscheck_registry"></a>
<a id="direct-header-dependencies"></a>
<a id="engine_context"></a>
<a id="engine_time"></a>
<a id="entriesconjure_bindings_for_each_entry"></a>
<a id="extension-points"></a>
<a id="fail_entry"></a>
<a id="failure_logged_this_frame"></a>
<a id="fill_native_table"></a>
<a id="frame_stats"></a>
<a id="group_available"></a>
<a id="group_compiled"></a>
<a id="input"></a>
<a id="internals-and-algorithms"></a>
<a id="is_counting_installed"></a>
<a id="is_implementation_installed"></a>
<a id="is_scope_hook_installed"></a>
<a id="is_stub_installed"></a>
<a id="job_system"></a>
<a id="jobsjob_system"></a>
<a id="module_registry"></a>
<a id="other-declarations"></a>
<a id="physics_world"></a>
<a id="renderer"></a>
<a id="report_not_supported"></a>
<a id="scene_manager"></a>
<a id="script_runtime"></a>
<a id="structs-and-types"></a>
<a id="stub_thunk"></a>
<a id="stub_thunk_1"></a>
<a id="stub_thunkinvoke"></a>
<a id="stubsreport_not_supported"></a>
<a id="stubsstub_thunk"></a>
<a id="stubsstub_thunkid-retargs"></a>
<a id="stubsstub_thunkid-retargsinvoke"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<a id="window"></a>
<a id="world"></a>
<a id="wrap_with_counters"></a>
<!-- doc-legacy-anchors:end -->
