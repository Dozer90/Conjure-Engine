---
kind: header
---

[Engine](../../engine.md) → [Modules](../modules.md)

# module_types.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/modules/module_types.h` |
| API ↗ | [Writing a module](../../../api/modules/writing-a-module.md) |

<p>Handles, enumerations and flag sets of the module system.</p>

## Structs

| File | Description |
|---|---|
| [`binary_triplet`](module-types/binary-triplet.md) | Names the bin/&lt;platform&gt;-&lt;arch&gt;-&lt;config&gt;/ directory a module binary lives in. |
| [`module_id`](module-types/module-id.md) | 32-bit module handle. The generation increments on every unload so a stale id is detected instead of addressing the slot&#x27;s next occupant. |
| [`std::formatter<conjure::modules::binary_triplet>`](module-types/std-formatter-conjure-modules-binary-triplet.md) |  |
| [`std::formatter<conjure::modules::module_id>`](module-types/std-formatter-conjure-modules-module-id.md) |  |
| [`std::formatter<conjure::modules::module_state>`](module-types/std-formatter-conjure-modules-module-state.md) |  |

## Enumerations

### `build_config` {#symbol-build-config}

```cpp
enum class build_config : u8 {
    DEBUG = 1,
    DEVELOPMENT = 2,
    DISTRIBUTION = 3
};
```

### `module_architecture` {#symbol-module-architecture}

```cpp
enum class module_architecture : u8 {
    X64,
    ARM64
};
```

### `module_entry_status` {#symbol-module-entry-status}

```cpp
enum class module_entry_status : u32 {
    OK = 0,
    FAILED = 1
};
```

What a module entry function returns through the <code>C</code> boundary.

### `module_feature_flags` {#symbol-module-feature-flags}

```cpp
enum class module_feature_flags : u32 {
    NONE = 0,
    RENDERER = 1 << 0,
    PHYSICS = 1 << 1,
    AUDIO = 1 << 2,
    SCRIPTING = 1 << 3,
    EDITOR = 1 << 4
};
```

Engine features a module may require (&quot;requires&quot; in <code>module.json</code>). Mirrors the <code>CONJURE_WITH_</code>* options and <code>CONJURE_BUILD_EDITOR</code>.

### `module_host_kind` {#symbol-module-host-kind}

```cpp
enum class module_host_kind : u8 {
    EDITOR,
    RUNTIME,
    TEST
};
```

### `module_kind` {#symbol-module-kind}

```cpp
enum class module_kind : u8 {
    ENGINE,     // a built-in engine library registered as a manifest-only static module
    PLUGIN,     // a shared or static module that is not the game
    GAME        // the single project module, always loaded last
};
```

### `module_platform` {#symbol-module-platform}

```cpp
enum class module_platform : u8 {
    WINDOWS,
    LINUX,
    MACOS
};
```

### `module_platform_flags` {#symbol-module-platform-flags}

```cpp
enum class module_platform_flags : u8 {
    NONE = 0,
    WINDOWS = 1 << 0,
    LINUX = 1 << 1,
    MACOS = 1 << 2,
    ALL = WINDOWS | LINUX | MACOS
};
```

### `module_state` {#symbol-module-state}

```cpp
enum class module_state : u8 {
    DISCOVERED,     // manifest accepted, not resolved yet
    SKIPPED,        // the manifest excludes the host platform; never loaded, never an error
    RESOLVED,       // part of a resolved load order, about to load
    LOADED,         // entry ran, every consumer attached
    FAILED,         // the last load attempt failed; last_error() says why; a retry is allowed
    UNLOADED,       // was LOADED once; may be loaded again
    FAULTED         // an unload went wrong half-way; the binary stays resident, nothing else works
};
```

### `reload_scope` {#symbol-reload-scope}

```cpp
enum class reload_scope : u8 {
    FULL,           // close and reopen the binary, every consumer detaches and restores
    MANAGED_ONLY    // only consumers with participates_in_managed_reload() take part
};
```

### `unload_policy` {#symbol-unload-policy}

```cpp
enum class unload_policy : u8 {
    REFUSE_IF_IN_USE,   // consumers veto with IN_USE when anything still uses the module
    REMOVE_INSTANCES    // consumers strip every instance the module owns
};
```

## Macros

### `CONJURE_ENUM_FLAGS` {#symbol-conjure-enum-flags}

```cpp
CONJURE_ENUM_FLAGS(module_platform_flags)
```

## Functions

### Public

#### `host_architecture` {#function-host-architecture}

<div class="function-declaration" id="function-host-architecture-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="module-types.html#symbol-module-architecture">module_architecture</a> host_architecture() <span class="k">noexcept</span>;</code></pre></div>

#### `host_binary_triplet` {#function-host-binary-triplet}

<div class="function-declaration" id="function-host-binary-triplet-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="module-types/binary-triplet.html">binary_triplet</a> host_binary_triplet(
    <a href="module-types.html#symbol-build-config">build_config</a> config = host_build_config()) <span class="k">noexcept</span>;</code></pre></div>

<p>The host platform and architecture combined with the given configuration.</p>

#### `host_build_config` {#function-host-build-config}

<div class="function-declaration" id="function-host-build-config-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="module-types.html#symbol-build-config">build_config</a> host_build_config() <span class="k">noexcept</span>;</code></pre></div>

#### `host_feature_flags` {#function-host-feature-flags}

<div class="function-declaration" id="function-host-feature-flags-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="module-types.html#symbol-module-feature-flags">module_feature_flags</a> host_feature_flags() <span class="k">noexcept</span>;</code></pre></div>

<p>The features compiled into this engine build (<code>CONJURE_WITH_RENDERER</code>, ..., <code>CONJURE_BUILD_EDITOR</code>).</p>

#### `host_platform` {#function-host-platform}

<div class="function-declaration" id="function-host-platform-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="module-types.html#symbol-module-platform">module_platform</a> host_platform() <span class="k">noexcept</span>;</code></pre></div>

#### `host_platform_flag` {#function-host-platform-flag}

<div class="function-declaration" id="function-host-platform-flag-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="module-types.html#symbol-module-platform-flags">module_platform_flags</a> host_platform_flag() <span class="k">noexcept</span>;</code></pre></div>

#### `parse_build_config` {#function-parse-build-config}

<div class="function-declaration" id="function-parse-build-config-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::optional&lt;<a href="module-types.html#symbol-build-config">build_config</a>&gt; parse_build_config(
    std::string_view text) <span class="k">noexcept</span>;</code></pre></div>

#### `parse_module_feature` {#function-parse-module-feature}

<div class="function-declaration" id="function-parse-module-feature-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::optional&lt;<a href="module-types.html#symbol-module-feature-flags">module_feature_flags</a>&gt; parse_module_feature(
    std::string_view text) <span class="k">noexcept</span>;</code></pre></div>

#### `parse_module_kind` {#function-parse-module-kind}

<div class="function-declaration" id="function-parse-module-kind-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::optional&lt;<a href="module-types.html#symbol-module-kind">module_kind</a>&gt; parse_module_kind(
    std::string_view text) <span class="k">noexcept</span>;</code></pre></div>

<p>Parses the manifest spellings; nullopt for anything else (case-sensitive).</p>

#### `parse_module_platform` {#function-parse-module-platform}

<div class="function-declaration" id="function-parse-module-platform-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::optional&lt;<a href="module-types.html#symbol-module-platform">module_platform</a>&gt; parse_module_platform(
    std::string_view text) <span class="k">noexcept</span>;</code></pre></div>

#### `to_flag` {#function-to-flag}

<div class="function-declaration" id="function-to-flag-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="module-types.html#symbol-module-platform-flags">module_platform_flags</a> to_flag(
    <a href="module-types.html#symbol-module-platform">module_platform</a> platform) <span class="k">noexcept</span>;</code></pre></div>

<p>The flag bit of a platform.</p>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="module-types.html#symbol-module-kind">module_kind</a> kind) <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-to-string-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="module-types.html#symbol-module-host-kind">module_host_kind</a> host) <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-to-string-3"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="module-types.html#symbol-module-state">module_state</a> state) <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-to-string-4"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="module-types.html#symbol-unload-policy">unload_policy</a> policy) <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-to-string-5"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="module-types.html#symbol-reload-scope">reload_scope</a> scope) <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-to-string-6"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="module-types.html#symbol-build-config">build_config</a> config) <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-to-string-7"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="module-types.html#symbol-module-platform">module_platform</a> platform) <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-to-string-8"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="module-types.html#symbol-module-architecture">module_architecture</a> architecture) <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-to-string-9"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string to_string(
    <span class="k">const</span> <a href="module-types/binary-triplet.html">binary_triplet</a> &amp;triplet);</code></pre></div>

<p>&quot;windows-x64-debug&quot;.</p>

<div class="function-declaration" id="function-to-string-10"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string to_string(
    <a href="module-types/module-id.html">module_id</a> id);</code></pre></div>

<p>&quot;index:generation&quot;, or &quot;invalid&quot; for the default id.</p>

<div class="function-declaration" id="function-to-string-11"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string to_string(
    <a href="module-types.html#symbol-module-feature-flags">module_feature_flags</a> flags);</code></pre></div>

<p>The manifest names of the set bits joined with &quot;, &quot; (&quot;renderer, physics&quot;); &quot;&quot; for <code>NONE</code>.</p>

<div class="function-declaration" id="function-to-string-12"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string to_string(
    <a href="module-types.html#symbol-module-platform-flags">module_platform_flags</a> flags);</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="binary_triplet"></a>
<a id="binary_tripletarchitecture"></a>
<a id="binary_tripletconfig"></a>
<a id="binary_tripletoperator"></a>
<a id="binary_tripletplatform"></a>
<a id="build_config"></a>
<a id="build_configdebug"></a>
<a id="build_configdevelopment"></a>
<a id="build_configdistribution"></a>
<a id="classes"></a>
<a id="conjure_enum_flags"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="host_architecture"></a>
<a id="host_binary_triplet"></a>
<a id="host_build_config"></a>
<a id="host_feature_flags"></a>
<a id="host_platform"></a>
<a id="host_platform_flag"></a>
<a id="internals-and-algorithms"></a>
<a id="module_architecture"></a>
<a id="module_architecturearm64"></a>
<a id="module_architecturex64"></a>
<a id="module_entry_status"></a>
<a id="module_entry_statusfailed"></a>
<a id="module_entry_statusok"></a>
<a id="module_feature_flags"></a>
<a id="module_feature_flagsaudio"></a>
<a id="module_feature_flagseditor"></a>
<a id="module_feature_flagsnone"></a>
<a id="module_feature_flagsphysics"></a>
<a id="module_feature_flagsrenderer"></a>
<a id="module_feature_flagsscripting"></a>
<a id="module_host_kind"></a>
<a id="module_host_kindeditor"></a>
<a id="module_host_kindruntime"></a>
<a id="module_host_kindtest"></a>
<a id="module_id"></a>
<a id="module_idgeneration"></a>
<a id="module_idindex"></a>
<a id="module_idinvalid_index"></a>
<a id="module_idis_valid"></a>
<a id="module_idoperator"></a>
<a id="module_kind"></a>
<a id="module_kindengine"></a>
<a id="module_kindgame"></a>
<a id="module_kindplugin"></a>
<a id="module_platform"></a>
<a id="module_platform_flags"></a>
<a id="module_platform_flagsall"></a>
<a id="module_platform_flagslinux"></a>
<a id="module_platform_flagsmacos"></a>
<a id="module_platform_flagsnone"></a>
<a id="module_platform_flagswindows"></a>
<a id="module_platformlinux"></a>
<a id="module_platformmacos"></a>
<a id="module_platformwindows"></a>
<a id="module_state"></a>
<a id="module_statediscovered"></a>
<a id="module_statefailed"></a>
<a id="module_statefaulted"></a>
<a id="module_stateloaded"></a>
<a id="module_stateresolved"></a>
<a id="module_stateskipped"></a>
<a id="module_stateunloaded"></a>
<a id="other-declarations"></a>
<a id="parse_build_config"></a>
<a id="parse_module_feature"></a>
<a id="parse_module_kind"></a>
<a id="parse_module_platform"></a>
<a id="reload_scope"></a>
<a id="reload_scopefull"></a>
<a id="reload_scopemanaged_only"></a>
<a id="std"></a>
<a id="stdformat"></a>
<a id="stdformatterconjuremodulesbinary_triplet"></a>
<a id="stdformatterconjuremodulesbinary_tripletformat"></a>
<a id="stdformatterconjuremodulesmodule_id"></a>
<a id="stdformatterconjuremodulesmodule_idformat"></a>
<a id="stdformatterconjuremodulesmodule_state"></a>
<a id="stdformatterconjuremodulesmodule_stateformat"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_flag"></a>
<a id="to_string"></a>
<a id="unload_policy"></a>
<a id="unload_policyrefuse_if_in_use"></a>
<a id="unload_policyremove_instances"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
