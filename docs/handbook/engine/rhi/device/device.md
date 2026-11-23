---
kind: class
---

[Engine](../../../engine.md) → [RHI](../../rhi.md) → [device.h](../device.md)

# device
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::rhi` |
| Include File Path | `engine/include/conjure/rhi/device.h` |
| Source File Path | `engine/src/rhi/device.cpp` |
| API ↗ | [Devices and swapchains](../../../../api/rhi/devices-and-swapchains.md), [Shaders](../../../../api/rhi/shaders.md) |

## Syntax

```cpp
class CONJURE_API device
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> device(
    create_tag);</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~device();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mImpl</code></td><td><code class="signature">std::unique_ptr&lt;<a href="../device.html">impl</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `backend` {#function-backend}

<div class="function-declaration" id="function-backend-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../backend.html#symbol-graphics-backend">graphics_backend</a> backend() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The backend this <code><a href="device.html">device</a></code> runs on.</p>

#### `capabilities` {#function-capabilities}

<div class="function-declaration" id="function-capabilities-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../backend/device-capabilities.html">device_capabilities</a> &amp;capabilities() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Adapter name, limits and queue availability of the created <code><a href="device.html">device</a></code>.</p>

#### `create` {#function-create}

<div class="function-declaration" id="function-create-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;std::unique_ptr&lt;<a href="device.html">device</a>&gt;&gt; create(
    <span class="k">const</span> <a href="device-desc.html">device_desc</a> &amp;desc);</code></pre></div>

<p>Creates the NVRHI <code><a href="device.html">device</a></code> for the requested backend. Fails with <code>core::error_code::UNSUPPORTED</code> when the backend is not compiled in, <code>rhi_error::INVALID_DESC</code> when the description is inconsistent, and <code>rhi_error::DEVICE_CREATION_FAILED</code> when the API refuses (its message is quoted).</p>

#### `desc` {#function-desc}

<div class="function-declaration" id="function-desc-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="device-desc.html">device_desc</a> &amp;desc() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The description the <code><a href="device.html">device</a></code> was created from.</p>

#### `inject_null_fault` {#function-inject-null-fault}

<div class="function-declaration" id="function-inject-null-fault-1"><pre><code><span class="k">void</span> inject_null_fault(
    <a href="../device.html#symbol-null-fault">null_fault</a> fault);</code></pre></div>

<p>Arms one failure on the <code>null</code> backend (<code><a href="../device.html#symbol-null-fault">null_fault</a></code>); the next matching call reports it and disarms. <code>NONE</code> disarms without a failure. No effect on <code>D3D12</code> and Vulkan.</p>

#### `is_lost` {#function-is-lost}

<div class="function-declaration" id="function-is-lost-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_lost() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True once the API reported <code><a href="device.html">device</a></code> removal or NVRHI raised a Fatal message. Every later present fails with <code>rhi_error::DEVICE_LOST</code>; the owner must recreate everything.</p>

#### `native_device` {#function-native-device}

<div class="function-declaration" id="function-native-device-1"><pre><code>[[<span class="na">nodiscard</span>]] nvrhi::IDevice *native_device() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Non-owning NVRHI <code><a href="device.html">device</a></code> for in-engine modules. Never exposed to game code.</p>

#### `null_stats` {#function-null-stats}

<div class="function-declaration" id="function-null-stats-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="../null-device-stats/null-device-stats.html">null_device_stats</a>&gt; null_stats() <span class="k">const</span>;</code></pre></div>

<p>Snapshot of what the <code>null</code> backend recorded; nullopt on <code>D3D12</code> and Vulkan.</p>

#### `reset_null_stats` {#function-reset-null-stats}

<div class="function-declaration" id="function-reset-null-stats-1"><pre><code><span class="k">void</span> reset_null_stats();</code></pre></div>

<p>Resets the <code>null</code> backend&#x27;s counters; no effect on other backends.</p>

#### `run_garbage_collection` {#function-run-garbage-collection}

<div class="function-declaration" id="function-run-garbage-collection-1"><pre><code><span class="k">void</span> run_garbage_collection();</code></pre></div>

<p>Releases the resources of command lists that finished executing. Call once per frame.</p>

#### `simulate_device_loss` {#function-simulate-device-loss}

<div class="function-declaration" id="function-simulate-device-loss-1"><pre><code><span class="k">void</span> simulate_device_loss();</code></pre></div>

<p>Behaves as if the API reported <code><a href="device.html">device</a></code> removal: routes a Fatal message through the message callback so <code><a href="#function-is-lost-1">is_lost</a>()</code> becomes <code>true</code>. Exists for the engine&#x27;s <code><a href="device.html">device</a></code>-loss tests on every backend.</p>

#### `wait_idle` {#function-wait-idle}

<div class="function-declaration" id="function-wait-idle-1"><pre><code><span class="k">void</span> wait_idle();</code></pre></div>

<p>Blocks until the GPU is idle. <code>A</code> failed wait marks the <code><a href="device.html">device</a></code> lost.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
