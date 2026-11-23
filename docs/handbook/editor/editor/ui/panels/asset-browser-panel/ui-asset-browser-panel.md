---
kind: class
---

[Editor](../../../../../editor.md) → [User Interface](../../../../user-interface.md) → [asset_browser_panel.h](../asset-browser-panel.md)

# ui::asset_browser_panel
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor::ui` |
| Include File Path | `engine/include/conjure/editor/ui/panels/asset_browser_panel.h` |
| Source File Path | `engine/src/editor/ui/panels/asset_browser_panel.cpp` |
| API ↗ | [The workspace UI](../../../../../../api/editor/workspace-ui.md) |

## Syntax

```cpp
class CONJURE_API asset_browser_panel final : public editor_panel
```

## Inheritance Hierarchy


[`ui::editor_panel`](../../editor-panel/ui-editor-panel.md) → `ui::asset_browser_panel`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">asset_browser_panel();</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~asset_browser_panel() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">m<wbr>Confirm<wbr>Open<wbr>Requested</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFilter</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">""</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mImportPath</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">""</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Pending<wbr>Delete<wbr>Path</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">""</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Pending<wbr>Delete<wbr>Uuid</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">""</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRowsDrawn</code></td><td><code class="signature"><a href="../../../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Selected<wbr>Path</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">""</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `cancel_pending_delete` {#function-cancel-pending-delete}

<div class="function-declaration" id="function-cancel-pending-delete-1"><pre><code><span class="k">void</span> cancel_pending_delete() <span class="k">noexcept</span>;</code></pre></div>

<p>The dialog&#x27;s &quot;Cancel&quot;: forgets the pending delete; the scene and its file stay.</p>

#### `confirm_pending_delete` {#function-confirm-pending-delete}

<div class="function-declaration" id="function-confirm-pending-delete-1"><pre><code><span class="k">void</span> confirm_pending_delete(
    <a href="../../../editor-context/editor-context.html">editor_context</a> &amp;context);</code></pre></div>

<p>The dialog&#x27;s &quot;Close and Delete&quot;: one <code>asset.delete</code> with closeOpenScene, which deletes the scene&#x27;s file and closes the scene (unsaved changes are dropped). Nothing happens when no delete is pending. While playing or paused the command refuses closeOpenScene, so nothing is sent and the delete stays pending: the dialog disables the button and asks to stop playing first.</p>

#### `draw_contents` {#function-draw-contents}

<div class="function-declaration" id="function-draw-contents-1"><pre><code><span class="k">void</span> draw_contents(
    <a href="../../../editor-context/editor-context.html">editor_context</a> &amp;context) <span class="k">override</span>;</code></pre></div>

#### `has_pending_delete` {#function-has-pending-delete}

<div class="function-declaration" id="function-has-pending-delete-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> has_pending_delete() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True while the confirmation for deleting the open scene waits for an answer.</p>

#### `pending_delete_path` {#function-pending-delete-path}

<div class="function-declaration" id="function-pending-delete-path-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::string &amp;pending_delete_path() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The asset path the confirmation asks about, &quot;&quot; when none is pending.</p>

#### `request_delete` {#function-request-delete}

<div class="function-declaration" id="function-request-delete-1"><pre><code><span class="k">void</span> request_delete(
    <a href="../../../editor-context/editor-context.html">editor_context</a> &amp;context,
    std::string_view uuid,
    std::string_view path);</code></pre></div>

<p>The Delete action of a row (tests call it directly). An asset is deleted at once through <code>asset.delete</code>. The file behind the open scene is not: it becomes the pending delete, and the next draw opens the confirmation dialog whose buttons are <code><a href="#function-confirm-pending-delete-1">confirm_pending_delete</a>()</code> and <code><a href="#function-cancel-pending-delete-1">cancel_pending_delete</a>()</code>.</p>

#### `rows_drawn` {#function-rows-drawn}

<div class="function-declaration" id="function-rows-drawn-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../../engine/core/types.html#symbol-u32">u32</a> rows_drawn() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `selected_path` {#function-selected-path}

<div class="function-declaration" id="function-selected-path-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::string &amp;selected_path() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

### Private

#### `draw_delete_confirmation` {#function-draw-delete-confirmation}

<div class="function-declaration" id="function-draw-delete-confirmation-1"><pre><code><span class="k">void</span> draw_delete_confirmation(
    <a href="../../../editor-context/editor-context.html">editor_context</a> &amp;context);</code></pre></div>

#### `draw_table` {#function-draw-table}

<div class="function-declaration" id="function-draw-table-1"><pre><code><span class="k">void</span> draw_table(
    <a href="../../../editor-context/editor-context.html">editor_context</a> &amp;context,
    <span class="k">const</span> <a href="../../../../../engine/assets/asset-database/asset-database.html">assets::asset_database</a> &amp;database);</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
