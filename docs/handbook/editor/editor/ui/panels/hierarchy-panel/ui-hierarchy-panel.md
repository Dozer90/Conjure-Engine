---
kind: class
---

[Editor](../../../../../editor.md) → [User Interface](../../../../user-interface.md) → [hierarchy_panel.h](../hierarchy-panel.md)

# ui::hierarchy_panel
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor::ui` |
| Include File Path | `engine/include/conjure/editor/ui/panels/hierarchy_panel.h` |
| Source File Path | `engine/src/editor/ui/panels/hierarchy_panel.cpp` |
| API ↗ | [The workspace UI](../../../../../../api/editor/workspace-ui.md) |

## Syntax

```cpp
class CONJURE_API hierarchy_panel final : public editor_panel
```

## Inheritance Hierarchy


[`ui::editor_panel`](../../editor-panel/ui-editor-panel.md) → `ui::hierarchy_panel`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">hierarchy_panel();</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~hierarchy_panel() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mAnchor</code></td><td><code class="signature"><a href="../../../../../engine/core/uuid/uuid.html">core::uuid</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mCache</code></td><td><code class="signature"><a href="../../hierarchy-cache/ui-hierarchy-cache.html">hierarchy_cache</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mLastPrimary</code></td><td><code class="signature"><a href="../../../../../engine/core/uuid/uuid.html">core::uuid</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Rename<wbr>Buffer</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">""</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Rename<wbr>Focus<wbr>Pending</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRenaming</code></td><td><code class="signature"><a href="../../../../../engine/ecs/entity/entity.html">ecs::entity</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRowsDrawn</code></td><td><code class="signature"><a href="../../../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Scroll<wbr>Target</code></td><td><code class="signature"><a href="../../../../../engine/core/uuid/uuid.html">core::uuid</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Seen<wbr>Revision</code></td><td><code class="signature"><a href="../../../../../engine/core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Selected<wbr>Here</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `begin_rename` {#function-begin-rename}

<div class="function-declaration" id="function-begin-rename-1"><pre><code><span class="k">void</span> begin_rename(
    <a href="../../../editor-context/editor-context.html">editor_context</a> &amp;context,
    <a href="../../../../../engine/ecs/entity/entity.html">ecs::entity</a> entity);</code></pre></div>

<p>Starts the inline rename of an entity&#x27;s row (what <code>F2</code> and the context menu&#x27;s Rename do): reveals the row, fills the field with the current name and focuses it on the next draw. No-op for an entity the scene does not hold.</p>

#### `cache` {#function-cache}

<div class="function-declaration" id="function-cache-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../hierarchy-cache/ui-hierarchy-cache.html">hierarchy_cache</a> &amp;cache() <span class="k">noexcept</span>;</code></pre></div>

#### `draw_contents` {#function-draw-contents}

<div class="function-declaration" id="function-draw-contents-1"><pre><code><span class="k">void</span> draw_contents(
    <a href="../../../editor-context/editor-context.html">editor_context</a> &amp;context) <span class="k">override</span>;</code></pre></div>

#### `is_renaming` {#function-is-renaming}

<div class="function-declaration" id="function-is-renaming-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_renaming() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True while a row shows the inline rename field.</p>

#### `rows_drawn` {#function-rows-drawn}

<div class="function-declaration" id="function-rows-drawn-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../../engine/core/types.html#symbol-u32">u32</a> rows_drawn() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Rows drawn by the last <code>draw_contents</code> (the clipper&#x27;s visible range).</p>

#### `update` {#function-update}

<div class="function-declaration" id="function-update-1"><pre><code><span class="k">void</span> update(
    <a href="../../../editor-context/editor-context.html">editor_context</a> &amp;context) <span class="k">override</span>;</code></pre></div>

### Private

#### `draw_context_menu` {#function-draw-context-menu}

<div class="function-declaration" id="function-draw-context-menu-1"><pre><code><span class="k">void</span> draw_context_menu(
    <a href="../../../editor-context/editor-context.html">editor_context</a> &amp;context,
    <a href="../../../../../engine/ecs/entity/entity.html">ecs::entity</a> entity);</code></pre></div>

#### `draw_rename_field` {#function-draw-rename-field}

<div class="function-declaration" id="function-draw-rename-field-1"><pre><code><span class="k">void</span> draw_rename_field(
    <a href="../../../editor-context/editor-context.html">editor_context</a> &amp;context,
    <span class="k">const</span> <a href="../../hierarchy-cache/ui-hierarchy-row.html">hierarchy_row</a> &amp;row);</code></pre></div>

#### `draw_row` {#function-draw-row}

<div class="function-declaration" id="function-draw-row-1"><pre><code><span class="k">void</span> draw_row(
    <a href="../../../editor-context/editor-context.html">editor_context</a> &amp;context,
    <span class="k">const</span> <a href="../../hierarchy-cache/ui-hierarchy-row.html">hierarchy_row</a> &amp;row,
    <a href="../../../../../engine/core/types.html#symbol-usize">usize</a> index);</code></pre></div>

#### `scroll_to_target` {#function-scroll-to-target}

<div class="function-declaration" id="function-scroll-to-target-1"><pre><code><span class="k">void</span> scroll_to_target(
    <a href="../../../editor-context/editor-context.html">editor_context</a> &amp;context,
    <a href="../../../../../engine/core/types.html#symbol-f32">f32</a> rowHeight);</code></pre></div>

#### `select_range` {#function-select-range}

<div class="function-declaration" id="function-select-range-1"><pre><code><span class="k">void</span> select_range(
    <a href="../../../editor-context/editor-context.html">editor_context</a> &amp;context,
    <a href="../../../../../engine/ecs/entity/entity.html">ecs::entity</a> entity,
    <span class="k">bool</span> additive);</code></pre></div>

#### `select_row` {#function-select-row}

<div class="function-declaration" id="function-select-row-1"><pre><code><span class="k">void</span> select_row(
    <a href="../../../editor-context/editor-context.html">editor_context</a> &amp;context,
    <a href="../../../../../engine/ecs/entity/entity.html">ecs::entity</a> entity,
    <span class="k">bool</span> additive);</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
