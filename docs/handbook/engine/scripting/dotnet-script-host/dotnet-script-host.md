---
kind: class
---

[Engine](../../../engine.md) → [Scripting](../../scripting.md) → [dotnet_script_host.h](../dotnet-script-host.md)

# dotnet_script_host
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::scripting` |
| Include File Path | `engine/include/conjure/scripting/dotnet_script_host.h` |
| Source File Path | `engine/src/scripting/dotnet_script_host.cpp` |
| API ↗ | [Hosting, the native API and hot reload](../../../../api/scripting/hosting-and-hot-reload.md), [Scripting](../../../../api/scripting/index.md) |

## Syntax

```cpp
class CONJURE_API dotnet_script_host final : public script_host
```

## Inheritance Hierarchy


[`script_host`](../script-host/script-host.md) → `dotnet_script_host`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">dotnet_script_host();</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~dotnet_script_host() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mManagedApi</code></td><td><code class="signature"><a href="../native-api/conjure-managed-api.html">conjure_managed_api</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Managed<wbr>Directory</code></td><td><code class="signature">std::filesystem::path</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mMode</code></td><td><code class="signature"><a href="../script-host.html#symbol-host-mode">host_mode</a></code></td><td><code class="signature">host_<wbr>mode::<wbr>FRAMEWORK_<wbr>DEPENDENT</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mNativeApi</code></td><td><code class="signature"><a href="../native-api/conjure-native-api.html">conjure_native_api</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mStatus</code></td><td><code class="signature"><a href="../script-host.html#symbol-script-host-status">script_host_status</a></code></td><td><code class="signature">script_<wbr>host_<wbr>status::<wbr>UNINITIALISED</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `backend` {#function-backend}

<div class="function-declaration" id="function-backend-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../script-host.html#symbol-script-backend">script_backend</a> backend() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `initialise` {#function-initialise}

<div class="function-declaration" id="function-initialise-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; initialise(
    <span class="k">const</span> <a href="../script-host/script-host-config.html">script_host_config</a> &amp;config,
    <a href="../native-api/conjure-native-api.html">conjure_native_api</a> &amp;nativeApi) <span class="k">override</span>;</code></pre></div>

<p>Steps 1-10 of the hosting sequence: locate and load hostfxr, start (or attach to) the runtime, load <code>Conjure.Managed</code> into the default load context, resolve Initialize and hand it both tables. <code>RUNTIME_NOT_FOUND</code>, <code>HOSTFXR_INCOMPATIBLE</code>, <code>RUNTIME_CONFIG_INVALID</code>, <code>MANAGED_SDK_MISSING</code>, <code>HOST_ALREADY_RUNNING</code>, <code>ABI_MISMATCH</code>, <code>WRONG_THREAD</code>.</p>

#### `is_runtime_started` {#function-is-runtime-started}

<div class="function-declaration" id="function-is-runtime-started-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <span class="k">bool</span> is_runtime_started() <span class="k">noexcept</span>;</code></pre></div>

<p>True once any host in this process started the CLR.</p>

#### `managed_api` {#function-managed-api}

<div class="function-declaration" id="function-managed-api-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../native-api/conjure-managed-api.html">conjure_managed_api</a> &amp;managed_api() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `managed_directory` {#function-managed-directory}

<div class="function-declaration" id="function-managed-directory-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::filesystem::path &amp;managed_directory() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The managed directory the process runtime was started from (meaningful once <code>READY</code>). <code>A</code> later host asking for another directory attaches to this one with a <code>WARN</code>.</p>

#### `mode` {#function-mode}

<div class="function-declaration" id="function-mode-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../script-host.html#symbol-host-mode">host_mode</a> mode() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The hosting mode the process runtime was started in (meaningful once <code>READY</code>).</p>

#### `shutdown` {#function-shutdown}

<div class="function-declaration" id="function-shutdown-1"><pre><code><span class="k">void</span> shutdown() <span class="k">override</span>;</code></pre></div>

<p><code>managed.<a href="#function-shutdown-1">shutdown</a>()</code>: destroys instances, unloads the game load context. <code>READY</code> -&gt; <code>UNINITIALISED</code>.</p>

#### `status` {#function-status}

<div class="function-declaration" id="function-status-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../script-host.html#symbol-script-host-status">script_host_status</a> status() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="function-initialize"></a>
<a id="function-initialize-1"></a>
<!-- doc-legacy-anchors:end -->
