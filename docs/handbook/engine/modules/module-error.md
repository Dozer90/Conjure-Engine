---
kind: header
---

[Engine](../../engine.md) → [Modules](../modules.md)

# module_error.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/modules/module_error.h` |
| API ↗ | [Modules and plugins](../../../api/modules/index.md) |

<p>Failure codes of the module system. Every code is returned as a <code><a href="../core/error/error.html">core::error</a></code> whose message names the module and, where relevant, the expected and actual values, so the editor can show it verbatim.</p>

## Enumerations

### `module_error` {#symbol-module-error}

```cpp
enum class module_error : u16 {
    NONE = 0,
    MANIFEST_NOT_FOUND,         // module.json (or the discover directory) does not exist
    MANIFEST_INVALID,           // JSON or schema error; the message carries file and JSON pointer
    INVALID_NAME,               // name outside [a-z0-9_.]{1,64}
    DUPLICATE_NAME,             // a module of that name is already registered
    PLATFORM_UNSUPPORTED,       // the manifest excludes the host platform (state SKIPPED)
    ENGINE_VERSION_MISMATCH,    // manifest "engine" range does not contain the host version
    FEATURE_UNAVAILABLE,        // manifest "requires" names a feature the host was built without
    DEPENDENCY_MISSING,         // a non-optional dependency is not registered (or SKIPPED)
    DEPENDENCY_VERSION,         // the dependency exists but its version is outside the range
    DEPENDENCY_CYCLE,           // the message lists the cycle
    DEPENDENCY_FAILED,          // a dependency failed; the message names the root cause
    MULTIPLE_GAME_MODULES,      // only one module of kind GAME may exist
    SHARED_LIBRARIES_DISABLED,  // module_manager_desc::allowSharedLibraries is false
    BINARY_NOT_FOUND,           // the derived bin/<triplet>/<binary> path does not exist
    BINARY_COPY_FAILED,         // the shadow copy could not be written
    BINARY_LOAD_FAILED,         // the operating system refused to load the binary (OS text)
    ENTRY_SYMBOL_MISSING,       // conjure_module_describe / conjure_module_entry not exported
    ABI_MISMATCH,               // a signature field differs (field, expected, actual)
    NAME_MISMATCH,              // the binary's name differs from the manifest name
    ENTRY_FAILED,               // the entry returned FAILED or called fail(); all reasons listed
    REGISTRATION_CONFLICT,      // a consumer vetoed the registrations (consumer + offending key)
    SERVICE_MISSING,            // module_context::require_service found nothing
    HAS_DEPENDENTS,             // unload refused: loaded modules depend on this one
    IN_USE,                     // unload refused under REFUSE_IF_IN_USE (count in the message)
    UNLOAD_BLOCKED,             // a consumer vetoed the unload (play mode)
    NOT_LOADED,                 // the operation needs a LOADED module
    ALREADY_LOADED,             // load of a LOADED module
    INVALID_ID,                 // the id never referred to a module
    STALE_ID,                   // the id's generation is behind the record
    FAULTED,                    // the record is FAULTED and accepts no further operation
    EXPORT_FAILED,              // export_layout could not produce the layout (see export_layout)
    COUNT
};
```

## Macros

### `CONJURE_DECLARE_ERROR_CATEGORY` {#symbol-conjure-declare-error-category}

```cpp
CONJURE_DECLARE_ERROR_CATEGORY(conjure::modules::module_error, "modules", conjure::modules::module_error_name)
```

## Functions

### Public

#### `make_module_error` {#function-make-module-error}

<div class="function-declaration" id="function-make-module-error-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span>... Args&gt; [[<span class="na">nodiscard</span>]] <a href="../core/error/error.html">core::error</a> make_module_error(
    <a href="module-error.html#symbol-module-error">module_error</a> code,
    std::format_string&lt;Args...&gt; format,
    Args &amp;&amp;...args);</code></pre></div>

<p>Builds a module error with a <code>std::format</code> style message.</p>

<div class="function-declaration" id="function-make-module-error-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">inline</span> <a href="../core/error/error.html">core::error</a> make_module_error(
    <a href="module-error.html#symbol-module-error">module_error</a> code,
    std::string_view detail = {});</code></pre></div>

<p>Builds a module error with a fixed detail message (possibly empty).</p>

#### `module_error_name` {#function-module-error-name}

<div class="function-declaration" id="function-module-error-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view module_error_name(
    <a href="module-error.html#symbol-module-error">module_error</a> code) <span class="k">noexcept</span>;</code></pre></div>

<p>The enumerator name of a code (&quot;<code>ABI_MISMATCH</code>&quot;), &quot;&quot; for values outside the enumeration.</p>

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
<a id="make_module_error"></a>
<a id="module_error"></a>
<a id="module_error_name"></a>
<a id="module_errorabi_mismatch"></a>
<a id="module_erroralready_loaded"></a>
<a id="module_errorbinary_copy_failed"></a>
<a id="module_errorbinary_load_failed"></a>
<a id="module_errorbinary_not_found"></a>
<a id="module_errorcount"></a>
<a id="module_errordependency_cycle"></a>
<a id="module_errordependency_failed"></a>
<a id="module_errordependency_missing"></a>
<a id="module_errordependency_version"></a>
<a id="module_errorduplicate_name"></a>
<a id="module_errorengine_version_mismatch"></a>
<a id="module_errorentry_failed"></a>
<a id="module_errorentry_symbol_missing"></a>
<a id="module_errorexport_failed"></a>
<a id="module_errorfaulted"></a>
<a id="module_errorfeature_unavailable"></a>
<a id="module_errorhas_dependents"></a>
<a id="module_errorin_use"></a>
<a id="module_errorinvalid_id"></a>
<a id="module_errorinvalid_name"></a>
<a id="module_errormanifest_invalid"></a>
<a id="module_errormanifest_not_found"></a>
<a id="module_errormultiple_game_modules"></a>
<a id="module_errorname_mismatch"></a>
<a id="module_errornone"></a>
<a id="module_errornot_loaded"></a>
<a id="module_errorplatform_unsupported"></a>
<a id="module_errorregistration_conflict"></a>
<a id="module_errorservice_missing"></a>
<a id="module_errorshared_libraries_disabled"></a>
<a id="module_errorstale_id"></a>
<a id="module_errorunload_blocked"></a>
<a id="other-declarations"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
