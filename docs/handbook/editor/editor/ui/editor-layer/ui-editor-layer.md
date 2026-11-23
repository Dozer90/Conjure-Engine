---
kind: class
---

[Editor](../../../../editor.md) → [User Interface](../../../user-interface.md) → [editor_layer.h](../editor-layer.md)

# ui::editor_layer
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor::ui` |
| Include File Path | `engine/include/conjure/editor/ui/editor_layer.h` |
| Source File Path | `engine/src/editor/ui/editor_layer.cpp` |
| API ↗ | [The editor](../../../../../api/editor/index.md), [The workspace UI](../../../../../api/editor/workspace-ui.md) |

## Syntax

```cpp
class CONJURE_API editor_layer final : public engine::layer
```

## Inheritance Hierarchy


[`engine::layer`](../../../../engine/engine/layer/layer.md) → `ui::editor_layer`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> editor_layer(
    <a href="../../editor-context/editor-context.html">editor_context</a> &amp;context);</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~editor_layer() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mContext</code></td><td><code class="signature"><a href="../../editor-context/editor-context.html">editor_context</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFramesDrawn</code></td><td><code class="signature"><a href="../../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mWorkspace</code></td><td><code class="signature">std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../editor-workspace/ui-editor-workspace.html">editor_<wbr>workspace</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `frames_drawn` {#function-frames-drawn}

<div class="function-declaration" id="function-frames-drawn-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../engine/core/types.html#symbol-u32">u32</a> frames_drawn() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `on_attach` {#function-on-attach}

<div class="function-declaration" id="function-on-attach-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../engine/core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; on_attach(
    engine::engine_context &amp;engineContext) <span class="k">override</span>;</code></pre></div>

#### `on_detach` {#function-on-detach}

<div class="function-declaration" id="function-on-detach-1"><pre><code><span class="k">void</span> on_detach(
    engine::engine_context &amp;engineContext) <span class="k">override</span>;</code></pre></div>

#### `on_event` {#function-on-event}

<div class="function-declaration" id="function-on-event-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> on_event(
    engine::engine_context &amp;engineContext,
    <span class="k">const</span> <a href="../../../../engine/platform/event.html#symbol-event">platform::event</a> &amp;event) <span class="k">override</span>;</code></pre></div>

#### `on_imgui` {#function-on-imgui}

<div class="function-declaration" id="function-on-imgui-1"><pre><code><span class="k">void</span> on_imgui(
    engine::engine_context &amp;engineContext) <span class="k">override</span>;</code></pre></div>

#### `on_update` {#function-on-update}

<div class="function-declaration" id="function-on-update-1"><pre><code><span class="k">void</span> on_update(
    engine::engine_context &amp;engineContext) <span class="k">override</span>;</code></pre></div>

#### `workspace` {#function-workspace}

<div class="function-declaration" id="function-workspace-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../editor-workspace/ui-editor-workspace.html">editor_workspace</a> *workspace() <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
