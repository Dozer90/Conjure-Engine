---
kind: class
---

[Editor](../../../editor.md) → [Commands and Undo](../../commands-and-undo.md) → [reflection_path.h](../reflection-path.md)

# reflection_path
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/reflection_path.h` |
| Source File Path | `engine/src/editor/core/reflection_path.cpp` |
| API ↗ | [Headless mode and the command model](../../../../api/editor/headless-and-commands.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
class CONJURE_API reflection_path
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">reflection_path();</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">reflection_path(
    <span class="k">const</span> <a href="reflection-path.html">reflection_path</a> &amp;) = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">reflection_path(
    <a href="reflection-path.html">reflection_path</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~reflection_path();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mSegments</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="path-segment.html">path_<wbr>segment</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `component` {#function-component}

<div class="function-declaration" id="function-component-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string_view component() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The first segment: the component type name as written.</p>

#### `fields` {#function-fields}

<div class="function-declaration" id="function-fields-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="path-segment.html">path_segment</a>&gt; fields() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The segments after the component (empty = the whole component).</p>

#### `merge_component` {#function-merge-component}

<div class="function-declaration" id="function-merge-component-1"><pre><code><span class="k">bool</span> merge_component(
    <a href="../../../engine/core/types.html#symbol-usize">usize</a> segmentCount);</code></pre></div>

<p>Folds the first <code>segmentCount</code> segments into one component segment joined with dots, for type names written in full (&quot;<code>conjure.renderer.sprite.size</code>&quot; -&gt; component &quot;<code>conjure.renderer.sprite</code>&quot;, field &quot;size&quot;). False, and no change, for fewer than two segments, more than the path has, or an indexed segment among them.</p>

#### `parse` {#function-parse}

<div class="function-declaration" id="function-parse-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../../engine/core/result/result.html">core::result</a>&lt;<a href="reflection-path.html">reflection_path</a>&gt; parse(
    std::string_view text);</code></pre></div>

<p>Parses &quot;<code>a.b</code>[2].c&quot;. <code>INVALID_ARGS</code> for an empty path, an empty segment (&quot;a..b&quot;, a trailing dot), a malformed index (&quot;a[x]&quot;, &quot;a[&quot;, &quot;a[-1]&quot;) or characters outside identifiers, dots and brackets.</p>

#### `resolve` {#function-resolve}

<div class="function-declaration" id="function-resolve-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/result/result.html">core::result</a>&lt;<a href="field-ref.html">field_ref</a>&gt; resolve(
    <span class="k">const</span> <a href="../../../engine/core/reflect/type-info/reflect-type-info.html">core::reflect::type_info</a> *componentType) <span class="k">const</span>;</code></pre></div>

<p>Resolves the field segments against a component description. <code>VEC2</code>/3/4 and <code>QUAT</code> accept x, y, z, w (or an index), <code>TRANSFORM</code> accepts position, rotation and scale, <code>MAT4</code> an index 0..15, <code>ARRAY</code> an index below the count, <code>STRUCT</code> its member names. Without a description (unreflected components such as the script component) names and indices map straight onto JSON members and elements. <code>PATH_NOT_FOUND</code> names the first segment that does not resolve.</p>

#### `segments` {#function-segments}

<div class="function-declaration" id="function-segments-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="path-segment.html">path_segment</a>&gt; segments() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string to_string() <span class="k">const</span>;</code></pre></div>

<p>The normalised text (&quot;<code>a.b</code>[2].c&quot;).</p>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="reflection-path.html">reflection_path</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="reflection-path.html">reflection_path</a> &amp;) = <span class="k">default</span>;</code></pre></div>

<div class="function-declaration" id="operator-3d-2"><pre><code><a href="reflection-path.html">reflection_path</a> &amp;<span class="k">operator</span>=(
    <a href="reflection-path.html">reflection_path</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
