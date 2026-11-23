---
kind: class
---

[Editor](../../../../../editor.md) → [User Interface](../../../../user-interface.md) → [export_dialog.h](../export-dialog.md)

# ui::export_dialog
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor::ui` |
| Include File Path | `engine/include/conjure/editor/ui/panels/export_dialog.h` |
| Source File Path | `engine/src/editor/ui/panels/export_dialog.cpp` |
| API ↗ | [The workspace UI](../../../../../../api/editor/workspace-ui.md) |

## Syntax

```cpp
class CONJURE_API export_dialog final : public editor_panel
```

## Inheritance Hierarchy


[`ui::editor_panel`](../../editor-panel/ui-editor-panel.md) → `ui::export_dialog`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">export_dialog();</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~export_dialog() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mContext</code></td><td><code class="signature"><a href="../../../editor-context/editor-context.html">editor_context</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mDirectory</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">""</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mProgress</code></td><td><code class="signature"><a href="../../../../../engine/core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">0.0</span>f</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRunning</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mState</code></td><td><code class="signature">std::<wbr>shared_<wbr>ptr&lt;<wbr><a href="../export-dialog.html">export_<wbr>state</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mStatus</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">""</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `draw_contents` {#function-draw-contents}

<div class="function-declaration" id="function-draw-contents-1"><pre><code><span class="k">void</span> draw_contents(
    <a href="../../../editor-context/editor-context.html">editor_context</a> &amp;context) <span class="k">override</span>;</code></pre></div>

#### `is_running` {#function-is-running}

<div class="function-declaration" id="function-is-running-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_running() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `set_directory` {#function-set-directory}

<div class="function-declaration" id="function-set-directory-1"><pre><code><span class="k">void</span> set_directory(
    std::string directory);</code></pre></div>

#### `start_export` {#function-start-export}

<div class="function-declaration" id="function-start-export-1"><pre><code><span class="k">void</span> start_export(
    <a href="../../../editor-context/editor-context.html">editor_context</a> &amp;context);</code></pre></div>

<p>Starts the export of the directory field (tests call it directly).</p>

#### `status` {#function-status}

<div class="function-declaration" id="function-status-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::string &amp;status() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `update` {#function-update}

<div class="function-declaration" id="function-update-1"><pre><code><span class="k">void</span> update(
    <a href="../../../editor-context/editor-context.html">editor_context</a> &amp;context) <span class="k">override</span>;</code></pre></div>

### Private

#### `release_callbacks` {#function-release-callbacks}

<div class="function-declaration" id="function-release-callbacks-1"><pre><code><span class="k">void</span> release_callbacks();</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
