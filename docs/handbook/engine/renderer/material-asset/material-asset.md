---
kind: class
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [material_asset.h](../material-asset.md)

# material_asset
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/material_asset.h` |
| Source File Path | `engine/src/renderer/assets/material_asset.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Renderer assets](../../../../api/renderer/renderer-assets.md) |

## Syntax

```cpp
class CONJURE_API material_asset
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">material_asset();</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> material_asset(
    <a href="material-desc.html">material_desc</a> desc,
    std::string name = {});</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">material_asset(
    <span class="k">const</span> <a href="material-asset.html">material_asset</a> &amp;) = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">material_asset(
    <a href="material-asset.html">material_asset</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~material_asset();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mDesc</code></td><td><code class="signature"><a href="material-desc.html">material_desc</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mName</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `desc` {#function-desc}

<div class="function-declaration" id="function-desc-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="material-desc.html">material_desc</a> &amp;desc() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-desc-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="material-desc.html">material_desc</a> &amp;desc() <span class="k">noexcept</span>;</code></pre></div>

#### `fallback` {#function-fallback}

<div class="function-declaration" id="function-fallback-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="material-asset.html">material_asset</a> fallback();</code></pre></div>

<p>The magenta fallback material.</p>

#### `name` {#function-name}

<div class="function-declaration" id="function-name-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string_view name() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `parse_blob` {#function-parse-blob}

<div class="function-declaration" id="function-parse-blob-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="material-asset.html">material_asset</a>&gt; parse_blob(
    std::span&lt;<span class="k">const</span> std::byte&gt; bytes);</code></pre></div>

<p><code>parse_json</code> over a blob&#x27;s bytes.</p>

#### `parse_json` {#function-parse-json}

<div class="function-declaration" id="function-parse-json-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="material-asset.html">material_asset</a>&gt; parse_json(
    std::string_view text);</code></pre></div>

<p>Parses the JSON form. Fails with <code>PARSE_FAILURE</code> for invalid JSON and <code>INVALID_ARGUMENT</code> for a wrong member type, an unknown enumerator or a malformed uuid.</p>

#### `set_name` {#function-set-name}

<div class="function-declaration" id="function-set-name-1"><pre><code><span class="k">void</span> set_name(
    std::string name);</code></pre></div>

#### `texture_flags` {#function-texture-flags}

<div class="function-declaration" id="function-texture-flags-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> texture_flags() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Bit i set when texture slot i references an asset (non-<code>null</code> uuid), bound or not.</p>

#### `to_json` {#function-to-json}

<div class="function-declaration" id="function-to-json-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string to_json() <span class="k">const</span>;</code></pre></div>

<p>The canonical JSON form (sorted keys, deterministic).</p>

#### `write_blob` {#function-write-blob}

<div class="function-declaration" id="function-write-blob-1"><pre><code>[[<span class="na">nodiscard</span>]] std::vector&lt;std::byte&gt; write_blob() <span class="k">const</span>;</code></pre></div>

<p>The JSON form as blob bytes.</p>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="material-asset.html">material_asset</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="material-asset.html">material_asset</a> &amp;) = <span class="k">default</span>;</code></pre></div>

<div class="function-declaration" id="operator-3d-2"><pre><code><a href="material-asset.html">material_asset</a> &amp;<span class="k">operator</span>=(
    <a href="material-asset.html">material_asset</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="material-asset.html">material_asset</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
