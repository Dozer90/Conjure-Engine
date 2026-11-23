---
kind: class
---

[Engine](../../../engine.md) → [Scripting](../../scripting.md) → [script_host.h](../script-host.md)

# script_host
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::scripting` |
| Include File Path | `engine/include/conjure/scripting/script_host.h` |
| Source File Path | `engine/src/scripting/script_host.cpp` |
| API ↗ | [Hosting, the native API and hot reload](../../../../api/scripting/hosting-and-hot-reload.md), [Scripting](../../../../api/scripting/index.md) |

## Syntax

```cpp
class CONJURE_API script_host
```

## Derived Classes


<details><summary>Classes that use this as a base</summary>

<ul>

<li><a href="../dotnet-script-host/dotnet-script-host.html">dotnet_script_host</a></li>

<li><a href="../null-script-host/null-script-host.html">null_script_host</a></li>

</ul>

</details>

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">script_host();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">virtual</span> ~script_host();</code></td><td></td></tr>
</tbody>
</table>



## Functions

### Public

#### `backend` {#function-backend}

<div class="function-declaration" id="function-backend-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../script-host.html#symbol-script-backend">script_backend</a> backend() <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

#### `initialise` {#function-initialise}

<div class="function-declaration" id="function-initialise-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; initialise(
    <span class="k">const</span> <a href="script-host-config.html">script_host_config</a> &amp;config,
    <a href="../native-api/conjure-native-api.html">conjure_native_api</a> &amp;nativeApi) = <span class="mi">0</span>;</code></pre></div>

<p>Starts (or attaches to) the runtime and performs the ABI handshake over <code>nativeApi</code>, which the caller must have filled and which must outlive the host. Main thread only. Failure leaves the host <code>DISABLED</code>.</p>

#### `managed_api` {#function-managed-api}

<div class="function-declaration" id="function-managed-api-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <span class="k">const</span> <a href="../native-api/conjure-managed-api.html">conjure_managed_api</a> &amp;managed_api() <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

<p>The table the managed side filled during the handshake; all <code>null</code> before <code>READY</code>.</p>

#### `shutdown` {#function-shutdown}

<div class="function-declaration" id="function-shutdown-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> shutdown() = <span class="mi">0</span>;</code></pre></div>

<p>Stops managed execution for this host (instances destroyed, game assembly unloaded). The CLR stays resident until process exit.</p>

#### `status` {#function-status}

<div class="function-declaration" id="function-status-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../script-host.html#symbol-script-host-status">script_host_status</a> status() <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="function-initialize"></a>
<a id="function-initialize-1"></a>
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->
