---
kind: class
---

[Editor](../../../../editor.md) → [User Interface](../../../user-interface.md) → [imgui_frame_scope.h](../imgui-frame-scope.md)

# ui::imgui_frame_scope
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor::ui` |
| Include File Path | `engine/include/conjure/editor/ui/imgui_frame_scope.h` |
| Source File Path | `engine/src/editor/ui/imgui_frame_scope.cpp` |
| API ↗ | [The workspace UI](../../../../../api/editor/workspace-ui.md) |

## Syntax

```cpp
class CONJURE_API imgui_frame_scope
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> imgui_frame_scope(
    <a href="../../../../engine/core/math/glm-types.html#symbol-vec2">math::vec2</a> displaySize = <a href="../../../../engine/core/math/glm-types.html#symbol-vec2">math::vec2</a>(<span class="mi">1280.0</span>f, <span class="mi">720.0</span>f));</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Additional constructor overloads have been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~imgui_frame_scope();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mContext</code></td><td><code class="signature"><a href="../imgui-frame-scope.html">ImGuiContext</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mDisplaySize</code></td><td><code class="signature"><a href="../../../../engine/core/math/glm-types.html#symbol-vec2">math::vec2</a></code></td><td><code class="signature"><a href="../../../../engine/core/math/glm-types.html#symbol-vec2">math::<wbr>vec2</a>(<span class="mi">1280.0</span>f,<wbr> <span class="mi">720.0</span>f)</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFrameOpen</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFrames</code></td><td><code class="signature"><a href="../../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mOwnsContext</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPrevious</code></td><td><code class="signature"><a href="../imgui-frame-scope.html">ImGuiContext</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `begin_frame` {#function-begin-frame}

<div class="function-declaration" id="function-begin-frame-1"><pre><code><span class="k">void</span> begin_frame();</code></pre></div>

<p><code>ImGui::NewFrame</code> with the display size and a fixed 1/60 s delta.</p>

#### `context` {#function-context}

<div class="function-declaration" id="function-context-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../imgui-frame-scope.html">ImGuiContext</a> *context() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `end_frame` {#function-end-frame}

<div class="function-declaration" id="function-end-frame-1"><pre><code><span class="k">void</span> end_frame();</code></pre></div>

<p><code>ImGui::Render</code> (the draw data is discarded).</p>

#### `frame_count` {#function-frame-count}

<div class="function-declaration" id="function-frame-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../engine/core/types.html#symbol-u32">u32</a> frame_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `is_frame_open` {#function-is-frame-open}

<div class="function-declaration" id="function-is-frame-open-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_frame_open() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `owns_context` {#function-owns-context}

<div class="function-declaration" id="function-owns-context-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> owns_context() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `set_mouse` {#function-set-mouse}

<div class="function-declaration" id="function-set-mouse-1"><pre><code><span class="k">void</span> set_mouse(
    <span class="k">const</span> <a href="../../../../engine/core/math/glm-types.html#symbol-vec2">math::vec2</a> &amp;position,
    <span class="k">bool</span> leftDown);</code></pre></div>

<p>Simulates a mouse position and button state for the frame about to begin.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
