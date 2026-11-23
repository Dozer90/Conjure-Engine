---
kind: class
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [shader_asset.h](../shader-asset.md)

# shader_asset
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/shader_asset.h` |
| Source File Path | `engine/src/renderer/assets/shader_asset.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Renderer assets](../../../../api/renderer/renderer-assets.md) |

## Syntax

```cpp
class CONJURE_API shader_asset
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">shader_asset();</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> shader_asset(
    <a href="shader-program-desc.html">shader_program_desc</a> desc);</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">shader_asset(
    <span class="k">const</span> <a href="shader-asset.html">shader_asset</a> &amp;) = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">shader_asset(
    <a href="shader-asset.html">shader_asset</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~shader_asset();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mDesc</code></td><td><code class="signature"><a href="shader-program-desc.html">shader_program_desc</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `built_in_programs` {#function-built-in-programs}

<div class="function-declaration" id="function-built-in-programs-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> std::span&lt;<span class="k">const</span> <a href="shader-program-desc.html">shader_program_desc</a>&gt; built_in_programs() <span class="k">noexcept</span>;</code></pre></div>

<p>Every program the renderer loads, one entry per shader permutation: the pass programs (pipeline variants that differ only in render state share an entry), the blit and the Dear ImGui backend&#x27;s program. The ImGui entry is listed in every configuration but is loaded only by an editor build&#x27;s renderer created with enableImGui and a swapchain.</p>

#### `desc` {#function-desc}

<div class="function-declaration" id="function-desc-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="shader-program-desc.html">shader_program_desc</a> &amp;desc() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `find_built_in` {#function-find-built-in}

<div class="function-declaration" id="function-find-built-in-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="shader-asset.html">shader_asset</a>&gt; find_built_in(
    std::string_view name,
    std::string_view pixelDefines = {},
    std::string_view vertexDefines = {});</code></pre></div>

<p>The built-in program with this name whose pixel and vertex define strings both equal the given ones (&quot;&quot; matches a stage without defines, it is not a wildcard), or <code>core::error_code::NOT_FOUND</code> naming all three.</p>

#### `validate` {#function-validate}

<div class="function-declaration" id="function-validate-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; validate(
    <a href="../../rhi/shader-library/shader-library.html">rhi::shader_library</a> &amp;library) <span class="k">const</span>;</code></pre></div>

<p>Succeeds when the library&#x27;s index has a blob for its backend for every stage of the program (vertex, pixel, compute, in that order; a stage with an &quot;&quot; entry is skipped). Fails with <code>renderer_error::SHADER_NOT_FOUND</code> naming the first missing shader key. Reads the index only: a blob file that is listed but missing on disk is found when the renderer loads it, not here.</p>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="shader-asset.html">shader_asset</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="shader-asset.html">shader_asset</a> &amp;) = <span class="k">default</span>;</code></pre></div>

<div class="function-declaration" id="operator-3d-2"><pre><code><a href="shader-asset.html">shader_asset</a> &amp;<span class="k">operator</span>=(
    <a href="shader-asset.html">shader_asset</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="shader-asset.html">shader_asset</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
