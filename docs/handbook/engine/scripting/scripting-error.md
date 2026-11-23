---
kind: header
---

[Engine](../../engine.md) → [Scripting](../scripting.md)

# scripting_error.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/scripting/scripting_error.h` |
| API ↗ | [Scripting](../../../api/scripting/index.md) |

<p>Error codes of the scripting module. Built with the generic <code>core::make_error(script_error::X, detail)</code> and bound to the &quot;scripting&quot; category below. The codes start at 1 so the negated value the native table entries return (-code, <code>native_api.inl</code>) is never 0.</p>

## Enumerations

### `script_error` {#symbol-script-error}

```cpp
enum class script_error : u16 {
    RUNTIME_NOT_FOUND = 1,      // hostfxr could not be located: no .NET runtime installed
    HOSTFXR_INCOMPATIBLE,       // hostfxr lacks a required export or delegate: older than .NET 10
    RUNTIME_CONFIG_INVALID,     // Conjure.Bootstrap.runtimeconfig.json missing or malformed
    MANAGED_SDK_MISSING,        // Conjure.Managed.dll / Conjure.Bootstrap.dll not in managed_directory
    HOST_ALREADY_RUNNING,       // a runtime of another mode already lives in this process
    ABI_MISMATCH,               // size, version or layout hash of a table differs between the sides
    ASSEMBLY_LOAD_FAILED,       // the game assembly did not load; the previous one keeps running
    ASSEMBLY_LEAKED,            // the collectible load context survived unload (zombie)
    CLASS_NOT_FOUND,            // no script class with this hash in the active registry
    FIELD_NOT_FOUND,            // the class has no field with this hash
    FIELD_BLOB_INVALID,         // a field blob failed validation (magic, version, bounds)
    INSTANCE_NOT_FOUND,         // the entry or slot does not resolve to a live instance
    WRONG_THREAD,               // called off the main thread
    NOT_IN_PHASE,               // a query scope was requested outside a script phase
    BUILD_FAILED,               // dotnet build reported errors
    NOT_INITIALISED,            // the host or runtime is not READY
    NOT_SUPPORTED,              // the entry or feature is not available in this configuration
    COUNT
};
```

## Macros

### `CONJURE_DECLARE_ERROR_CATEGORY` {#symbol-conjure-declare-error-category}

```cpp
CONJURE_DECLARE_ERROR_CATEGORY(conjure::scripting::script_error, "scripting", conjure::scripting::script_error_name)
```

## Functions

### Public

#### `script_error_name` {#function-script-error-name}

<div class="function-declaration" id="function-script-error-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view script_error_name(
    <a href="scripting-error.html#symbol-script-error">script_error</a> code) <span class="k">noexcept</span>;</code></pre></div>

<p>The enumerator name of a code (&quot;<code>ABI_MISMATCH</code>&quot;); &quot;&quot; for unknown values.</p>

#### `status_of` {#function-status-of}

<div class="function-declaration" id="function-status-of-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../core/types.html#symbol-i32">i32</a> status_of(
    <a href="scripting-error.html#symbol-script-error">script_error</a> code) <span class="k">noexcept</span>;</code></pre></div>

<p>The negative status a native table entry returns for this error (-code).</p>

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
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="script_error"></a>
<a id="script_error_name"></a>
<a id="script_errorabi_mismatch"></a>
<a id="script_errorassembly_leaked"></a>
<a id="script_errorassembly_load_failed"></a>
<a id="script_errorbuild_failed"></a>
<a id="script_errorclass_not_found"></a>
<a id="script_errorcount"></a>
<a id="script_errorfield_blob_invalid"></a>
<a id="script_errorfield_not_found"></a>
<a id="script_errorhost_already_running"></a>
<a id="script_errorhostfxr_incompatible"></a>
<a id="script_errorinstance_not_found"></a>
<a id="script_errormanaged_sdk_missing"></a>
<a id="script_errornot_in_phase"></a>
<a id="script_errornot_initialized"></a>
<a id="script_errornot_supported"></a>
<a id="script_errorruntime_config_invalid"></a>
<a id="script_errorruntime_not_found"></a>
<a id="script_errorwrong_thread"></a>
<a id="status_of"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
