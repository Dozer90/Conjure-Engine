---
kind: class
---

[Editor](../../../../editor.md) → [User Interface](../../../user-interface.md) → [editor_panel.h](../editor-panel.md)

# ui::editor_panel
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor::ui` |
| Include File Path | `engine/include/conjure/editor/ui/editor_panel.h` |
| Source File Path | `engine/src/editor/ui/editor_panel.cpp` |
| API ↗ | [The workspace UI](../../../../../api/editor/workspace-ui.md) |

## Syntax

```cpp
class CONJURE_API editor_panel
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> editor_panel(
    std::string title,
    <span class="k">bool</span> open = true);</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Additional constructor overloads have been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">virtual</span> ~editor_panel();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mOpen</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">true</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mTitle</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">""</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `draw` {#function-draw}

<div class="function-declaration" id="function-draw-1"><pre><code><span class="k">void</span> draw(
    <a href="../../editor-context/editor-context.html">editor_context</a> &amp;context);</code></pre></div>

<p>Begins the ImGui window (with the close button bound to the open flag), draws the body and ends it. Does nothing while closed.</p>

#### `draw_contents` {#function-draw-contents}

<div class="function-declaration" id="function-draw-contents-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> draw_contents(
    <a href="../../editor-context/editor-context.html">editor_context</a> &amp;context) = <span class="mi">0</span>;</code></pre></div>

<p>The body between <code>ImGui::Begin</code> and <code>ImGui::End</code>.</p>

#### `is_open` {#function-is-open}

<div class="function-declaration" id="function-is-open-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_open() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `set_open` {#function-set-open}

<div class="function-declaration" id="function-set-open-1"><pre><code><span class="k">void</span> set_open(
    <span class="k">bool</span> open) <span class="k">noexcept</span>;</code></pre></div>

#### `title` {#function-title}

<div class="function-declaration" id="function-title-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string_view title() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `update` {#function-update}

<div class="function-declaration" id="function-update-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> update(
    <a href="../../editor-context/editor-context.html">editor_context</a> &amp;context);</code></pre></div>

<p>Called once per frame before drawing, open or not (caches, polling).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
