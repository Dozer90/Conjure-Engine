---
kind: class
---

[Editor](../../../../../editor.md) → [User Interface](../../../../user-interface.md) → [toolbar_panel.h](../toolbar-panel.md)

# ui::toolbar_panel
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor::ui` |
| Include File Path | `engine/include/conjure/editor/ui/panels/toolbar_panel.h` |
| Source File Path | `engine/src/editor/ui/panels/toolbar_panel.cpp` |
| API ↗ | [The workspace UI](../../../../../../api/editor/workspace-ui.md) |

## Syntax

```cpp
class CONJURE_API toolbar_panel final : public editor_panel
```

## Inheritance Hierarchy


[`ui::editor_panel`](../../editor-panel/ui-editor-panel.md) → `ui::toolbar_panel`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">toolbar_panel();</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~toolbar_panel() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mStepCount</code></td><td><code class="signature"><a href="../../../../../engine/core/types.html#symbol-i32">i32</a></code></td><td><code class="signature"><span class="mi">1</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `draw_contents` {#function-draw-contents}

<div class="function-declaration" id="function-draw-contents-1"><pre><code><span class="k">void</span> draw_contents(
    <a href="../../../editor-context/editor-context.html">editor_context</a> &amp;context) <span class="k">override</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
