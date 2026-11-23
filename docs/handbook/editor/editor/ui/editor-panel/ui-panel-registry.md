---
kind: class
---

[Editor](../../../../editor.md) → [User Interface](../../../user-interface.md) → [editor_panel.h](../editor-panel.md)

# ui::panel_registry
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor::ui` |
| Include File Path | `engine/include/conjure/editor/ui/editor_panel.h` |
| Source File Path | `engine/src/editor/ui/editor_panel.cpp` |
| API ↗ | [The workspace UI](../../../../../api/editor/workspace-ui.md) |

## Syntax

```cpp
class CONJURE_API panel_registry
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">panel_registry();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Additional constructor overloads have been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~panel_registry();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mDescriptors</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="ui-panel-descriptor.html">panel_<wbr>descriptor</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `add` {#function-add}

<div class="function-declaration" id="function-add-1"><pre><code><span class="k">void</span> add(
    <a href="ui-panel-descriptor.html">panel_descriptor</a> descriptor);</code></pre></div>

<p>Adds a descriptor; a duplicate name replaces the previous one.</p>

#### `add_module_panel` {#function-add-module-panel}

<div class="function-declaration" id="function-add-module-panel-1"><pre><code><span class="k">void</span> add_module_panel(
    std::string_view owner,
    <span class="k">const</span> <a href="../../panel-desc/panel-desc.html">panel_desc</a> &amp;desc);</code></pre></div>

<p>Wraps a module panel record into a descriptor named &quot;&lt;owner&gt;/&lt;title&gt;&quot;.</p>

#### `descriptors` {#function-descriptors}

<div class="function-declaration" id="function-descriptors-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="ui-panel-descriptor.html">panel_descriptor</a>&gt; descriptors() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `find` {#function-find}

<div class="function-declaration" id="function-find-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="ui-panel-descriptor.html">panel_descriptor</a> *find(
    std::string_view name) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `remove_owned_by` {#function-remove-owned-by}

<div class="function-declaration" id="function-remove-owned-by-1"><pre><code><a href="../../../../engine/core/types.html#symbol-usize">usize</a> remove_owned_by(
    std::string_view owner);</code></pre></div>

<p>Removes every descriptor of an owner and returns how many.</p>

#### `size` {#function-size}

<div class="function-declaration" id="function-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../engine/core/types.html#symbol-usize">usize</a> size() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
