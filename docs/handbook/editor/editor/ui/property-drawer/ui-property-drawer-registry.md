---
kind: class
---

[Editor](../../../../editor.md) → [User Interface](../../../user-interface.md) → [property_drawer.h](../property-drawer.md)

# ui::property_drawer_registry
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor::ui` |
| Include File Path | `engine/include/conjure/editor/ui/property_drawer.h` |
| Source File Path | `engine/src/editor/ui/property_drawer.cpp` |
| API ↗ | [The workspace UI](../../../../../api/editor/workspace-ui.md) |

## Syntax

```cpp
class CONJURE_API property_drawer_registry
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">property_drawer_registry();</code></td><td>Installs the built-in drawers for every kind.</td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Additional constructor overloads have been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~property_drawer_registry();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mDrawers</code></td><td><code class="signature"><a href="../property-drawer.html#symbol-ui-draw-fn">draw_<wbr>fn</a>[<span class="na">static_<wbr>cast</span>&lt;<wbr><a href="../../../../engine/core/types.html#symbol-usize">usize</a>&gt;(core::<wbr>reflect::<wbr>field_<wbr>kind::<wbr>COUNT)]</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFallback</code></td><td><code class="signature"><a href="../property-drawer.html#symbol-ui-draw-fn">draw_fn</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `draw` {#function-draw}

<div class="function-declaration" id="function-draw-1"><pre><code><span class="k">bool</span> draw(
    <a href="../../../../engine/core/reflect/field-info.html#symbol-reflect-field-kind">core::reflect::field_kind</a> kind,
    <span class="k">const</span> <a href="ui-property-draw-context.html">property_draw_context</a> &amp;context,
    <a href="../../../../engine/core/json/document/json-document.html">core::json::document</a> &amp;value) <span class="k">const</span>;</code></pre></div>

<p>Draws a value with the drawer of its kind. Returns <code>true</code> when it changed.</p>

#### `draw_json` {#function-draw-json}

<div class="function-declaration" id="function-draw-json-1"><pre><code><span class="k">bool</span> draw_json(
    std::string_view label,
    <a href="../../../../engine/core/json/document/json-document.html">core::json::document</a> &amp;value,
    <span class="k">bool</span> readOnly) <span class="k">const</span>;</code></pre></div>

<p>Draws an unreflected JSON value member by member (script fields, unknown components).</p>

#### `draw_object` {#function-draw-object}

<div class="function-declaration" id="function-draw-object-1"><pre><code><span class="k">bool</span> draw_object(
    <span class="k">const</span> <a href="../../../../engine/core/reflect/type-info/reflect-type-info.html">core::reflect::type_info</a> &amp;type,
    <a href="../../../../engine/core/json/document/json-document.html">core::json::document</a> &amp;object,
    <span class="k">bool</span> readOnly) <span class="k">const</span>;</code></pre></div>

<p>Draws a whole reflected object: every field of the description, nested structs as tree nodes. Returns <code>true</code> when any field changed; <code>object</code> is updated in place. <code>A</code> float member <code>object</code> does not hold is drawn as 0; it is NaN only where <code>object</code> holds <code>null</code>.</p>

#### `drawer` {#function-drawer}

<div class="function-declaration" id="function-drawer-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../property-drawer.html#symbol-ui-draw-fn">draw_fn</a> &amp;drawer(
    <a href="../../../../engine/core/reflect/field-info.html#symbol-reflect-field-kind">core::reflect::field_kind</a> kind) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The drawer for a kind (the fallback for <code>COUNT</code>: a read-only JSON text).</p>

#### `set` {#function-set}

<div class="function-declaration" id="function-set-1"><pre><code><span class="k">void</span> set(
    <a href="../../../../engine/core/reflect/field-info.html#symbol-reflect-field-kind">core::reflect::field_kind</a> kind,
    <a href="../property-drawer.html#symbol-ui-draw-fn">draw_fn</a> drawer);</code></pre></div>

<p>Replaces the drawer of a kind (an empty function restores the built-in one).</p>

### Private

#### `install_defaults` {#function-install-defaults}

<div class="function-declaration" id="function-install-defaults-1"><pre><code><span class="k">void</span> install_defaults();</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
