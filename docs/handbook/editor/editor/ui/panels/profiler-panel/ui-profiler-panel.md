---
kind: class
---

[Editor](../../../../../editor.md) → [User Interface](../../../../user-interface.md) → [profiler_panel.h](../profiler-panel.md)

# ui::profiler_panel
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor::ui` |
| Include File Path | `engine/include/conjure/editor/ui/panels/profiler_panel.h` |
| Source File Path | `engine/src/editor/ui/panels/profiler_panel.cpp` |
| API ↗ | [The workspace UI](../../../../../../api/editor/workspace-ui.md) |

## Syntax

```cpp
class CONJURE_API profiler_panel final : public editor_panel
```

## Inheritance Hierarchy


[`ui::editor_panel`](../../editor-panel/ui-editor-panel.md) → `ui::profiler_panel`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">profiler_panel();</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~profiler_panel() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mHistory</code></td><td><code class="signature">std::<wbr>array&lt;<wbr>f32,<wbr> engine::<wbr>frame_<wbr>stats::<wbr>HISTORY_<wbr>SIZE&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPaused</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Samples<wbr>Plotted</code></td><td><code class="signature"><a href="../../../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `draw_contents` {#function-draw-contents}

<div class="function-declaration" id="function-draw-contents-1"><pre><code><span class="k">void</span> draw_contents(
    <a href="../../../editor-context/editor-context.html">editor_context</a> &amp;context) <span class="k">override</span>;</code></pre></div>

#### `samples_plotted` {#function-samples-plotted}

<div class="function-declaration" id="function-samples-plotted-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../../engine/core/types.html#symbol-u32">u32</a> samples_plotted() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
