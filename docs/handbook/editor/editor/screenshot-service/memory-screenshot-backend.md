---
kind: class
---

[Editor](../../../editor.md) → [User Interface](../../user-interface.md) → [screenshot_service.h](../screenshot-service.md)

# memory_screenshot_backend
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/screenshot_service.h` |
| Source File Path | `engine/src/editor/core/screenshot_service.cpp` |
| API ↗ | [Headless mode and the command model](../../../../api/editor/headless-and-commands.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
class CONJURE_API memory_screenshot_backend final : public screenshot_backend
```

## Inheritance Hierarchy


[`screenshot_backend`](screenshot-backend.md) → `memory_screenshot_backend`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> memory_screenshot_backend(
    <a href="../../../engine/core/image/rgba8.html">core::rgba8</a> colour = <a href="../../../engine/core/image/rgba8.html">core::rgba8</a>{<span class="mi">32</span>, <span class="mi">64</span>, <span class="mi">96</span>, <span class="mi">255</span>});</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~memory_screenshot_backend() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mBegun</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mCaptured</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mColour</code></td><td><code class="signature"><a href="../../../engine/core/image/rgba8.html">core::rgba8</a></code></td><td><code class="signature"><a href="../../../engine/core/image/rgba8.html">core::<wbr>rgba8</a>{<span class="mi">32</span>,<wbr> <span class="mi">64</span>,<wbr> <span class="mi">96</span>,<wbr> <span class="mi">255</span>}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `begin` {#function-begin}

<div class="function-declaration" id="function-begin-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; begin(
    <span class="k">const</span> <a href="screenshot-request.html">screenshot_request</a> &amp;request) <span class="k">override</span>;</code></pre></div>

#### `begun` {#function-begun}

<div class="function-declaration" id="function-begun-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-u32">u32</a> begun() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `capture` {#function-capture}

<div class="function-declaration" id="function-capture-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/result/result.html">core::result</a>&lt;<a href="../../../engine/core/image/image.html">core::image</a>&gt; capture(
    <span class="k">const</span> <a href="screenshot-request.html">screenshot_request</a> &amp;request) <span class="k">override</span>;</code></pre></div>

#### `captured` {#function-captured}

<div class="function-declaration" id="function-captured-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-u32">u32</a> captured() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
