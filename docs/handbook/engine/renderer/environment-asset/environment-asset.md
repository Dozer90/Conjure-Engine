---
kind: class
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [environment_asset.h](../environment-asset.md)

# environment_asset
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/environment_asset.h` |
| Source File Path | `engine/src/renderer/assets/environment_asset.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Renderer assets](../../../../api/renderer/renderer-assets.md) |

## Syntax

```cpp
class CONJURE_API environment_asset
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">environment_asset();</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">environment_asset(
    <a href="cubemap-data.html">cubemap_data</a> sky,
    <a href="cubemap-data.html">cubemap_data</a> prefiltered,
    <span class="k">const</span> <a href="../environment-asset.html#symbol-sh9">sh9</a> &amp;irradiance,
    std::string name = {});</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">environment_asset(
    <span class="k">const</span> <a href="environment-asset.html">environment_asset</a> &amp;) = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">environment_asset(
    <a href="environment-asset.html">environment_asset</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~environment_asset();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mIrradiance</code></td><td><code class="signature"><a href="../environment-asset.html#symbol-sh9">sh9</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mName</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPrefiltered</code></td><td><code class="signature"><a href="cubemap-data.html">cubemap_data</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSky</code></td><td><code class="signature"><a href="cubemap-data.html">cubemap_data</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `gpu_byte_size` {#function-gpu-byte-size}

<div class="function-declaration" id="function-gpu-byte-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u64">u64</a> gpu_byte_size() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Bytes of both cubemaps together.</p>

#### `irradiance` {#function-irradiance}

<div class="function-declaration" id="function-irradiance-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../environment-asset.html#symbol-sh9">sh9</a> &amp;irradiance() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `name` {#function-name}

<div class="function-declaration" id="function-name-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string_view name() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `parse_blob` {#function-parse-blob}

<div class="function-declaration" id="function-parse-blob-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="environment-asset.html">environment_asset</a>&gt; parse_blob(
    std::span&lt;<span class="k">const</span> std::byte&gt; bytes);</code></pre></div>

<p>Parses a .cenv blob. Fails with <code>renderer_error::INVALID_ASSET</code> naming the problem, including a cubemap whose mip count exceeds the chain of its size (<code>bit_width(size)</code> levels).</p>

#### `prefiltered` {#function-prefiltered}

<div class="function-declaration" id="function-prefiltered-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="cubemap-data.html">cubemap_data</a> &amp;prefiltered() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `set_name` {#function-set-name}

<div class="function-declaration" id="function-set-name-1"><pre><code><span class="k">void</span> set_name(
    std::string name);</code></pre></div>

#### `sky` {#function-sky}

<div class="function-declaration" id="function-sky-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="cubemap-data.html">cubemap_data</a> &amp;sky() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `uniform` {#function-uniform}

<div class="function-declaration" id="function-uniform-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="environment-asset.html">environment_asset</a> uniform(
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;radiance,
    <a href="../../core/types.html#symbol-u32">u32</a> size = <span class="mi">8</span>);</code></pre></div>

<p><code>A</code> uniform grey environment (the fallback): constant sky and prefiltered chains of the given radiance, SH band 0 matching, tiny cubemaps.</p>

#### `write_blob` {#function-write-blob}

<div class="function-declaration" id="function-write-blob-1"><pre><code>[[<span class="na">nodiscard</span>]] std::vector&lt;std::byte&gt; write_blob() <span class="k">const</span>;</code></pre></div>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="environment-asset.html">environment_asset</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="environment-asset.html">environment_asset</a> &amp;) = <span class="k">default</span>;</code></pre></div>

<div class="function-declaration" id="operator-3d-2"><pre><code><a href="environment-asset.html">environment_asset</a> &amp;<span class="k">operator</span>=(
    <a href="environment-asset.html">environment_asset</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="environment-asset.html">environment_asset</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
