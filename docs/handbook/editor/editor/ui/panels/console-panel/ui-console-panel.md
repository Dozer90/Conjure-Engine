---
kind: class
---

[Editor](../../../../../editor.md) → [User Interface](../../../../user-interface.md) → [console_panel.h](../console-panel.md)

# ui::console_panel
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor::ui` |
| Include File Path | `engine/include/conjure/editor/ui/panels/console_panel.h` |
| Source File Path | `engine/src/editor/ui/panels/console_panel.cpp` |
| API ↗ | [The workspace UI](../../../../../../api/editor/workspace-ui.md) |

## Syntax

```cpp
class CONJURE_API console_panel final : public editor_panel
```

## Inheritance Hierarchy


[`ui::editor_panel`](../../editor-panel/ui-editor-panel.md) → `ui::console_panel`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">console_panel();</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~console_panel() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mAutoScroll</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">true</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Category<wbr>Filter</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">""</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mLinesDrawn</code></td><td><code class="signature"><a href="../../../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mMinLevel</code></td><td><code class="signature"><a href="../../../../../engine/core/log.html#symbol-log-level">core::log_level</a></code></td><td><code class="signature">core::<wbr>log_<wbr>level::<wbr>INFO</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Newest<wbr>Seq<wbr>Shown</code></td><td><code class="signature"><a href="../../../../../engine/core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `draw_contents` {#function-draw-contents}

<div class="function-declaration" id="function-draw-contents-1"><pre><code><span class="k">void</span> draw_contents(
    <a href="../../../editor-context/editor-context.html">editor_context</a> &amp;context) <span class="k">override</span>;</code></pre></div>

#### `lines_drawn` {#function-lines-drawn}

<div class="function-declaration" id="function-lines-drawn-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../../engine/core/types.html#symbol-u32">u32</a> lines_drawn() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `min_level` {#function-min-level}

<div class="function-declaration" id="function-min-level-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../../engine/core/log.html#symbol-log-level">core::log_level</a> min_level() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `newest_seq_shown` {#function-newest-seq-shown}

<div class="function-declaration" id="function-newest-seq-shown-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../../engine/core/types.html#symbol-u64">u64</a> newest_seq_shown() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The sequence number of the newest record the last draw listed (0 when it listed none). The panel lists the newest matching records, so this follows <code>console_buffer::last_seq()</code> for unfiltered logs.</p>

#### `set_min_level` {#function-set-min-level}

<div class="function-declaration" id="function-set-min-level-1"><pre><code><span class="k">void</span> set_min_level(
    <a href="../../../../../engine/core/log.html#symbol-log-level">core::log_level</a> level) <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
