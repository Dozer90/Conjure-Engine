---
kind: header
---

[Engine](../../engine.md) → [Scripting](../scripting.md)

# script_host.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/scripting/script_host.h` |
| API ↗ | [Hosting, the native API and hot reload](../../../api/scripting/hosting-and-hot-reload.md), [Scripting](../../../api/scripting/index.md) |

<p>The host interface: the object that owns the managed side of the two ABI tables. <code><a href="dotnet-script-host/dotnet-script-host.html">dotnet_script_host</a></code> runs the real CLR, <code><a href="null-script-host/null-script-host.html">null_script_host</a></code> is the <code>C</code>++ fake every other module&#x27;s tests use. Both are created through <code>create_script_host()</code>.</p>

## Structs

| File | Description |
|---|---|
| [`host_capabilities`](script-host/host-capabilities.md) | What hosting paths this build supports on this platform. |
| [`script_host_config`](script-host/script-host-config.md) |  |

## Classes

| File | Description |
|---|---|
| [`script_host`](script-host/script-host.md) |  |

## Enumerations

### `host_mode` {#symbol-host-mode}

```cpp
enum class host_mode : u8 {
    FRAMEWORK_DEPENDENT,
    SELF_CONTAINED
};
```

How the CLR is hosted: framework-dependent through the runtime config, self-contained through the bootstrap command line.

### `script_backend` {#symbol-script-backend}

```cpp
enum class script_backend : u8 {
    DOTNET,
    NULL_HOST
};
```

Which managed side a host talks to.

### `script_host_status` {#symbol-script-host-status}

```cpp
enum class script_host_status : u8 {
    UNINITIALISED,
    READY,
    DISABLED        // initialise failed; the host stays usable as an object but runs nothing
};
```

## Functions

### Public

#### `create_script_host` {#function-create-script-host}

<div class="function-declaration" id="function-create-script-host-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::unique_ptr&lt;<a href="script-host/script-host.html">script_host</a>&gt; create_script_host(
    <a href="script-host.html#symbol-script-backend">script_backend</a> backend);</code></pre></div>

<p>Creates the host for the backend. <code>NULL_HOST</code> never fails to initialise.</p>

#### `detect_host_mode` {#function-detect-host-mode}

<div class="function-declaration" id="function-detect-host-mode-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<a href="script-host.html#symbol-host-mode">host_mode</a>&gt; detect_host_mode(
    <span class="k">const</span> std::filesystem::path &amp;managedDirectory);</code></pre></div>

<p>Reads <code>Conjure.Bootstrap.runtimeconfig.json</code> in <code>managedDirectory</code>: <code>SELF_CONTAINED</code> when <code>runtimeOptions.includedFrameworks</code> is present, <code>FRAMEWORK_DEPENDENT</code> otherwise. <code>RUNTIME_CONFIG_INVALID</code> when the file is missing or malformed.</p>

#### `is_runtime_available` {#function-is-runtime-available}

<div class="function-declaration" id="function-is-runtime-available-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">bool</span> is_runtime_available() <span class="k">noexcept</span>;</code></pre></div>

<p>Static hostfxr probe: <code>true</code> when nethost finds a .NET runtime without starting one. Used as the <code>DOTNET_RUNTIME</code> test precondition.</p>

#### `query_host_capabilities` {#function-query-host-capabilities}

<div class="function-declaration" id="function-query-host-capabilities-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="script-host/host-capabilities.html">host_capabilities</a> query_host_capabilities() <span class="k">noexcept</span>;</code></pre></div>

<p>The hosting paths available on this platform and build.</p>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="script-host.html#symbol-script-host-status">script_host_status</a> status) <span class="k">noexcept</span>;</code></pre></div>

<p>The upper-case name of a status (&quot;<code>READY</code>&quot;).</p>

<div class="function-declaration" id="function-to-string-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="script-host.html#symbol-script-backend">script_backend</a> backend) <span class="k">noexcept</span>;</code></pre></div>

<p>The upper-case name of a backend (&quot;<code>NULL_HOST</code>&quot;).</p>

<div class="function-declaration" id="function-to-string-3"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="script-host.html#symbol-host-mode">host_mode</a> mode) <span class="k">noexcept</span>;</code></pre></div>

<p>The upper-case name of a mode (&quot;<code>SELF_CONTAINED</code>&quot;).</p>

#### `validate_managed_directory` {#function-validate-managed-directory}

<div class="function-declaration" id="function-validate-managed-directory-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; validate_managed_directory(
    <span class="k">const</span> std::filesystem::path &amp;managedDirectory);</code></pre></div>

<p>The anchor files a managed directory must hold: <code>Conjure.Bootstrap.dll</code>, its runtime config and <code>Conjure.Managed.dll</code>. <code>MANAGED_SDK_MISSING</code> naming the first absent file.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="create_script_host"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="detect_host_mode"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="host_capabilities"></a>
<a id="host_capabilitiesframeworkdependent"></a>
<a id="host_capabilitiesselfcontained"></a>
<a id="host_mode"></a>
<a id="host_modeframework_dependent"></a>
<a id="host_modeself_contained"></a>
<a id="internals-and-algorithms"></a>
<a id="is_runtime_available"></a>
<a id="other-declarations"></a>
<a id="query_host_capabilities"></a>
<a id="script_backend"></a>
<a id="script_backenddotnet"></a>
<a id="script_backendnull_host"></a>
<a id="script_host"></a>
<a id="script_host_config"></a>
<a id="script_host_configbackend"></a>
<a id="script_host_configdotnetrootoverride"></a>
<a id="script_host_configmanageddirectory"></a>
<a id="script_host_configwaitfordebugger"></a>
<a id="script_host_status"></a>
<a id="script_host_statusdisabled"></a>
<a id="script_host_statusready"></a>
<a id="script_host_statusuninitialized"></a>
<a id="script_hostbackend"></a>
<a id="script_hostinitialize"></a>
<a id="script_hostmanaged_api"></a>
<a id="script_hostoperator"></a>
<a id="script_hostscript_host"></a>
<a id="script_hostscript_host_1"></a>
<a id="script_hostshutdown"></a>
<a id="script_hoststatus"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="validate_managed_directory"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
