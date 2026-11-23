---
kind: class
---

[Engine](../../../engine.md) → [RHI](../../rhi.md) → [swapchain.h](../swapchain.md)

# swapchain
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::rhi` |
| Include File Path | `engine/include/conjure/rhi/swapchain.h` |
| Source File Path | `engine/src/rhi/swapchain.cpp` |
| API ↗ | [Devices and swapchains](../../../../api/rhi/devices-and-swapchains.md) |

## Syntax

```cpp
class CONJURE_API swapchain
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> swapchain(
    create_tag);</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~swapchain();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mImpl</code></td><td><code class="signature">std::unique_ptr&lt;<a href="../swapchain.html">impl</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `acquire` {#function-acquire}

<div class="function-declaration" id="function-acquire-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt; acquire();</code></pre></div>

<p>Acquires the next back buffer and returns its index. Fails with <code>rhi_error::DEVICE_LOST</code>, <code>rhi_error::OUT_OF_DATE</code> (resize and try again: the surface changed on Vulkan, or a previous <code><a href="#function-resize-1">resize</a>()</code> failed and left no back buffers on any backend) or <code>rhi_error::PRESENT_FAILED</code> for any other refusal by the API.</p>

#### `back_buffer` {#function-back-buffer}

<div class="function-declaration" id="function-back-buffer-1"><pre><code>[[<span class="na">nodiscard</span>]] nvrhi::ITexture *back_buffer(
    <a href="../../core/types.html#symbol-u32">u32</a> index) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Non-owning back buffer texture, or <code>nullptr</code> for an index &gt;= <code><a href="#function-buffer-count-1">buffer_count</a>()</code>.</p>

#### `buffer_count` {#function-buffer-count}

<div class="function-declaration" id="function-buffer-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> buffer_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of back buffers that exist right now: <code><a href="#function-back-buffer-1">back_buffer</a>(i)</code> is non-<code>null</code> exactly for i &lt; <code><a href="#function-buffer-count-1">buffer_count</a>()</code>. Equals <code><a href="#function-desc-1">desc</a>()</code>.<code>bufferCount</code> except after a failed <code><a href="#function-resize-1">resize</a>()</code>.</p>

#### `create` {#function-create}

<div class="function-declaration" id="function-create-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;std::unique_ptr&lt;<a href="swapchain.html">swapchain</a>&gt;&gt; create(
    <a href="../device/device.html">device</a> &amp;device,
    <span class="k">const</span> <a href="../native-window/native-window.html">native_window</a> &amp;window,
    <span class="k">const</span> <a href="swapchain-desc.html">swapchain_desc</a> &amp;desc);</code></pre></div>

<p>Creates a <code><a href="swapchain.html">swapchain</a></code> presenting to <code>window</code> (ignored by the <code>null</code> device; a <code>NONE</code> handle on <code>D3D12</code> creates a windowless composition <code><a href="swapchain.html">swapchain</a></code>). Fails with <code>rhi_error::INVALID_DESC</code> for a zero size, a buffer count outside 2..16 or a format the API cannot present, <code>rhi_error::DEVICE_LOST</code> on a lost device and <code>rhi_error::SWAPCHAIN_CREATION_FAILED</code> when the API refuses.</p>

#### `current_index` {#function-current-index}

<div class="function-declaration" id="function-current-index-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> current_index() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Index of the back buffer the next draws target (the last <code><a href="#function-acquire-1">acquire</a>()</code> result).</p>

#### `desc` {#function-desc}

<div class="function-declaration" id="function-desc-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="swapchain-desc.html">swapchain_desc</a> &amp;desc() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The current description: size after <code><a href="#function-resize-1">resize</a>()</code>, format and buffer count as created.</p>

#### `owner` {#function-owner}

<div class="function-declaration" id="function-owner-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../device/device.html">device</a> &amp;owner() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The device this <code><a href="swapchain.html">swapchain</a></code> was created on.</p>

#### `present` {#function-present}

<div class="function-declaration" id="function-present-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; present();</code></pre></div>

<p>Presents the acquired back buffer. Fails with <code>rhi_error::DEVICE_LOST</code> (the device is then marked lost), <code>rhi_error::OUT_OF_DATE</code> (resize and retry: the surface changed on Vulkan, or a previous <code><a href="#function-resize-1">resize</a>()</code> failed and left no back buffers on any backend) or <code>rhi_error::PRESENT_FAILED</code> for any other refusal by the API.</p>

#### `resize` {#function-resize}

<div class="function-declaration" id="function-resize-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; resize(
    <a href="../../core/types.html#symbol-u32">u32</a> width,
    <a href="../../core/types.html#symbol-u32">u32</a> height);</code></pre></div>

<p>Recreates the back buffers at a new size after waiting for the GPU (the last present included); an image acquired but not yet presented is discarded. The caller must first release everything it holds over a back buffer (a framebuffer, a texture handle): while a reference is left, <code>D3D12</code> and the <code>null</code> backend refuse with <code>SWAPCHAIN_CREATION_FAILED</code>. <code>renderer::resize_swapchain</code> does both in the right order. Fails with <code>rhi_error::INVALID_DESC</code> for a zero size, <code>rhi_error::DEVICE_LOST</code>, <code>rhi_error::OUT_OF_DATE</code> (Vulkan: the surface is currently empty, e.g. the window is minimised; retry later) or <code>rhi_error::SWAPCHAIN_CREATION_FAILED</code>. Every <code><a href="#function-back-buffer-1">back_buffer</a>()</code> pointer is invalid afterwards; after a failure <code><a href="#function-buffer-count-1">buffer_count</a>()</code> may be 0, and while it is, <code><a href="#function-acquire-1">acquire</a>()</code> and <code><a href="#function-present-1">present</a>()</code> fail with <code>rhi_error::OUT_OF_DATE</code>, until a later <code><a href="#function-resize-1">resize</a>()</code> succeeds.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
