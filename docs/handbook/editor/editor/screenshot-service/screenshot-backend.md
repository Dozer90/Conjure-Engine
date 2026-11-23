---
kind: class
---

[Editor](../../../editor.md) → [User Interface](../../user-interface.md) → [screenshot_service.h](../screenshot-service.md)

# screenshot_backend
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/screenshot_service.h` |
| Source File Path | `engine/src/editor/core/screenshot_service.cpp` |
| API ↗ | [Headless mode and the command model](../../../../api/editor/headless-and-commands.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
class CONJURE_API screenshot_backend
```

## Derived Classes


<details><summary>Classes that use this as a base</summary>

<ul>

<li><a href="memory-screenshot-backend.html">memory_screenshot_backend</a></li>

</ul>

</details>

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">screenshot_backend();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">virtual</span> ~screenshot_backend();</code></td><td></td></tr>
</tbody>
</table>



## Functions

### Public

#### `abandon` {#function-abandon}

<div class="function-declaration" id="function-abandon-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> abandon() <span class="k">noexcept</span>;</code></pre></div>

<p>Forgets what <code><a href="#function-begin-1">begin</a>()</code> set up without touching the engine. The service calls it when it cancels its waiting requests, which happens when the engine restarts or shuts down: the renderer the backend drew with may already be gone, and takes its own resources with it. The default does nothing.</p>

#### `begin` {#function-begin}

<div class="function-declaration" id="function-begin-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../../engine/core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; begin(
    <span class="k">const</span> <a href="screenshot-request.html">screenshot_request</a> &amp;request) = <span class="mi">0</span>;</code></pre></div>

#### `capture` {#function-capture}

<div class="function-declaration" id="function-capture-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../../engine/core/result/result.html">core::result</a>&lt;<a href="../../../engine/core/image/image.html">core::image</a>&gt; capture(
    <span class="k">const</span> <a href="screenshot-request.html">screenshot_request</a> &amp;request) = <span class="mi">0</span>;</code></pre></div>

#### `frame_rendered` {#function-frame-rendered}

<div class="function-declaration" id="function-frame-rendered-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <span class="k">bool</span> frame_rendered(
    <span class="k">const</span> <a href="screenshot-request.html">screenshot_request</a> &amp;request);</code></pre></div>

<p>True when a frame was drawn for the request since the last call (or since <code><a href="#function-begin-1">begin</a>()</code>). The service counts a waited frame only then, so a request is never captured before the engine has rendered what <code><a href="#function-begin-1">begin</a>()</code> asked for. The default is <code>true</code>.</p>

#### `prepare_frame` {#function-prepare-frame}

<div class="function-declaration" id="function-prepare-frame-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> prepare_frame(
    <span class="k">const</span> <a href="screenshot-request.html">screenshot_request</a> &amp;request);</code></pre></div>

<p>Asks for the picture to be drawn in the frame being built. The default does nothing (a backend that needs no frame, such as <code><a href="memory-screenshot-backend.html">memory_screenshot_backend</a></code>).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->
