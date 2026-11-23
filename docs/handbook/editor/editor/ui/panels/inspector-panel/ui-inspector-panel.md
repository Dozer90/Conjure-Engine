---
kind: class
---

[Editor](../../../../../editor.md) → [User Interface](../../../../user-interface.md) → [inspector_panel.h](../inspector-panel.md)

# ui::inspector_panel
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor::ui` |
| Include File Path | `engine/include/conjure/editor/ui/panels/inspector_panel.h` |
| Source File Path | `engine/src/editor/ui/panels/inspector_panel.cpp` |
| API ↗ | [The workspace UI](../../../../../../api/editor/workspace-ui.md) |

## Syntax

```cpp
class CONJURE_API inspector_panel final : public editor_panel
```

## Inheritance Hierarchy


[`ui::editor_panel`](../../editor-panel/ui-editor-panel.md) → `ui::inspector_panel`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> inspector_panel(
    <span class="k">const</span> <a href="../../property-drawer/ui-property-drawer-registry.html">property_drawer_registry</a> &amp;drawers);</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~inspector_panel() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">m<wbr>Add<wbr>Component<wbr>Filter</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">""</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Components<wbr>Drawn</code></td><td><code class="signature"><a href="../../../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mDrawers</code></td><td><code class="signature"><span class="k">const</span> <a href="../../property-drawer/ui-property-drawer-registry.html">property_<wbr>drawer_<wbr>registry</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mNameBuffer</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">""</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `components_drawn` {#function-components-drawn}

<div class="function-declaration" id="function-components-drawn-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../../engine/core/types.html#symbol-u32">u32</a> components_drawn() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `draw_contents` {#function-draw-contents}

<div class="function-declaration" id="function-draw-contents-1"><pre><code><span class="k">void</span> draw_contents(
    <a href="../../../editor-context/editor-context.html">editor_context</a> &amp;context) <span class="k">override</span>;</code></pre></div>

### Private

#### `draw_add_component` {#function-draw-add-component}

<div class="function-declaration" id="function-draw-add-component-1"><pre><code><span class="k">void</span> draw_add_component(
    <a href="../../../editor-context/editor-context.html">editor_context</a> &amp;context,
    <span class="k">const</span> <a href="../../../../../engine/core/uuid/uuid.html">core::uuid</a> &amp;guid);</code></pre></div>

#### `draw_component` {#function-draw-component}

<div class="function-declaration" id="function-draw-component-1"><pre><code><span class="k">void</span> draw_component(
    <a href="../../../editor-context/editor-context.html">editor_context</a> &amp;context,
    <span class="k">const</span> <a href="../../../../../engine/core/uuid/uuid.html">core::uuid</a> &amp;guid,
    std::string_view typeName);</code></pre></div>

#### `draw_entity_header` {#function-draw-entity-header}

<div class="function-declaration" id="function-draw-entity-header-1"><pre><code><span class="k">void</span> draw_entity_header(
    <a href="../../../editor-context/editor-context.html">editor_context</a> &amp;context,
    <span class="k">const</span> <a href="../../../../../engine/core/uuid/uuid.html">core::uuid</a> &amp;guid);</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
