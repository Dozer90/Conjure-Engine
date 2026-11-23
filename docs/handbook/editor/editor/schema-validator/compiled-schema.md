---
kind: class
---

[Editor](../../../editor.md) → [Commands and Undo](../../commands-and-undo.md) → [schema_validator.h](../schema-validator.md)

# compiled_schema
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/schema_validator.h` |
| Source File Path | `engine/src/editor/core/schema_validator.cpp` |
| API ↗ | [Headless mode and the command model](../../../../api/editor/headless-and-commands.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
class CONJURE_API compiled_schema
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">compiled_schema();</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">compiled_schema(
    <span class="k">const</span> <a href="compiled-schema.html">compiled_schema</a> &amp;) = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">compiled_schema(
    <a href="compiled-schema.html">compiled_schema</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~compiled_schema();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mSchema</code></td><td><code class="signature"><a href="../../../engine/core/json/document/json-document.html">core::json::document</a></code></td><td><code class="signature">core::<wbr>json::<wbr>document::<wbr>object()</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `compile` {#function-compile}

<div class="function-declaration" id="function-compile-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../../engine/core/result/result.html">core::result</a>&lt;<a href="compiled-schema.html">compiled_schema</a>&gt; compile(
    std::string_view schemaJson);</code></pre></div>

<p>Parses the text and checks every keyword recursively. <code>INVALID_ARGS</code> names the offending pointer (&quot;/properties/count/type&quot;) for an unknown keyword, a non-object schema, a type name outside the subset or an ill-typed keyword value.</p>

#### `schema` {#function-schema}

<div class="function-declaration" id="function-schema-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../../../engine/core/json/document/json-document.html">core::json::document</a> &amp;schema() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The schema document.</p>

#### `validate` {#function-validate}

<div class="function-declaration" id="function-validate-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; validate(
    <span class="k">const</span> <a href="../../../engine/core/json/value/json-value.html">core::json::value</a> &amp;instance,
    std::string_view rootName = <span class="s">"args"</span>) <span class="k">const</span>;</code></pre></div>

<p>Checks a value against the schema. <code>INVALID_ARGS</code> whose message is &quot;&lt;path&gt;: &lt;problem&gt;&quot; with the path rooted at <code>rootName</code> (&quot;<code>args.count</code>: expected integer&quot;).</p>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="compiled-schema.html">compiled_schema</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="compiled-schema.html">compiled_schema</a> &amp;) = <span class="k">default</span>;</code></pre></div>

<div class="function-declaration" id="operator-3d-2"><pre><code><a href="compiled-schema.html">compiled_schema</a> &amp;<span class="k">operator</span>=(
    <a href="compiled-schema.html">compiled_schema</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
