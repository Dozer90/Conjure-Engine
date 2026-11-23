---
kind: class
---

[Engine](../../../engine.md) → [Core](../../core.md) → [image.h](../image.md)

# image
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/image.h` |
| Source File Path | `engine/src/core/system/image.cpp` |
| API ↗ | [Processes, libraries and images](../../../../api/core/system.md) |

## Syntax

```cpp
class CONJURE_API image
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">image();</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">image(
    <a href="../types.html#symbol-u32">u32</a> width,
    <a href="../types.html#symbol-u32">u32</a> height);</code></td><td><code>A</code> zero-filled <code><a href="image.html">image</a></code> (transparent black). <code>A</code> zero width or height yields the empty <code><a href="image.html">image</a></code>. Dimensions above <code>MAX_DIMENSION</code>, or whose pixel data would exceed <code>MAX_PIXEL_BYTES</code>, are a programmer error and yield the empty <code><a href="image.html">image</a></code> too.</td></tr>
<tr><td>public</td><td><code class="signature">image(
    <span class="k">const</span> <a href="image.html">image</a> &amp;) = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">image(
    <a href="image.html">image</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~image();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">BYTES_<wbr>PER_<wbr>PIXEL</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">4</span></code></td><td>Size of one pixel in bytes.</td></tr>
<tr><td>public</td><td><code class="signature">MAX_<wbr>DIMENSION</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">1</span>u &lt;&lt; <span class="mi">24</span></code></td><td>Largest width or height accepted; the <code>PNG</code> codec rejects anything larger.</td></tr>
<tr><td>public</td><td><code class="signature">MAX_<wbr>ENCODED_<wbr>BYTES</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span>x7FFFFFFF</code></td><td>Largest value of width * height * <code>BYTES_PER_PIXEL</code> + height that <code>encode_png()</code> accepts (the <code>PNG</code> encoder works in 32-bit signed arithmetic).</td></tr>
<tr><td>public</td><td><code class="signature">MAX_<wbr>PIXEL_<wbr>BYTES</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../types.html#symbol-usize">usize</a></code></td><td><code class="signature">MAX_<wbr>ENCODED_<wbr>BYTES</code></td><td>Largest pixel buffer (width * height * <code>BYTES_PER_PIXEL</code>) the constructor accepts; the <code>PNG</code> codecs work in 32-bit signed sizes.</td></tr>
<tr><td>private</td><td><code class="signature">mHeight</code></td><td><code class="signature"><a href="../types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPixels</code></td><td><code class="signature">std::vector&lt;std::byte&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mWidth</code></td><td><code class="signature"><a href="../types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `compare` {#function-compare}

<div class="function-declaration" id="function-compare-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../types.html#symbol-f64">f64</a> compare(
    <span class="k">const</span> <a href="image.html">image</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Mean absolute difference over every channel of every pixel, normalised to 0 (identical) .. 1 (every <code><a href="../types.html#symbol-byte">byte</a></code> differs by 255). Images of different dimensions compare as 1. Two empty images compare as 0. Used by golden-<code><a href="image.html">image</a></code> tests.</p>

#### `encode_png` {#function-encode-png}

<div class="function-declaration" id="function-encode-png-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../result/result.html">result</a>&lt;std::vector&lt;std::byte&gt;&gt; encode_png() <span class="k">const</span>;</code></pre></div>

<p>Encodes the <code><a href="image.html">image</a></code> as <code>PNG</code>. Fails with <code>INVALID_STATE</code> for an empty <code><a href="image.html">image</a></code> and <code>UNSUPPORTED</code> for one whose pixel data (plus one filter <code><a href="../types.html#symbol-byte">byte</a></code> per row) exceeds <code>MAX_ENCODED_BYTES</code>, the limit of the encoder&#x27;s 32-bit arithmetic.</p>

#### `fill` {#function-fill}

<div class="function-declaration" id="function-fill-1"><pre><code><span class="k">void</span> fill(
    <a href="rgba8.html">rgba8</a> color) <span class="k">noexcept</span>;</code></pre></div>

<p>Sets every pixel to one colour.</p>

#### `flip_vertical` {#function-flip-vertical}

<div class="function-declaration" id="function-flip-vertical-1"><pre><code><span class="k">void</span> flip_vertical() <span class="k">noexcept</span>;</code></pre></div>

<p>Reverses the order of the rows in place (top becomes bottom).</p>

#### `height` {#function-height}

<div class="function-declaration" id="function-height-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../types.html#symbol-u32">u32</a> height() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Height in pixels; 0 for the empty <code><a href="image.html">image</a></code>.</p>

#### `is_empty` {#function-is-empty}

<div class="function-declaration" id="function-is-empty-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_empty() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the <code><a href="image.html">image</a></code> has no pixels.</p>

#### `load_file` {#function-load-file}

<div class="function-declaration" id="function-load-file-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../result/result.html">result</a>&lt;<a href="image.html">image</a>&gt; load_file(
    <span class="k">const</span> std::filesystem::path &amp;path);</code></pre></div>

<p>Reads a <code>PNG</code> file. Fails with <code>NOT_FOUND</code> when the file does not exist, <code>IO_FAILURE</code> when it cannot be read and <code>PARSE_FAILURE</code> when it is not a <code>PNG</code>.</p>

#### `load_png` {#function-load-png}

<div class="function-declaration" id="function-load-png-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../result/result.html">result</a>&lt;<a href="image.html">image</a>&gt; load_png(
    std::span&lt;<span class="k">const</span> std::byte&gt; data);</code></pre></div>

<p>Decodes a <code>PNG</code> held in memory. Grey, grey + alpha and RGB files are expanded to RGBA8 (alpha 255 when absent). Fails with <code>INVALID_ARGUMENT</code> for empty data and <code>PARSE_FAILURE</code> for anything that is not a well-formed <code>PNG</code>.</p>

#### `pixel` {#function-pixel}

<div class="function-declaration" id="function-pixel-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="rgba8.html">rgba8</a> pixel(
    <a href="../types.html#symbol-u32">u32</a> x,
    <a href="../types.html#symbol-u32">u32</a> y) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The pixel at (x, y). Out-of-range coordinates assert and return a default <code><a href="rgba8.html">rgba8</a></code>.</p>

#### `pixels` {#function-pixels}

<div class="function-declaration" id="function-pixels-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;std::byte&gt; pixels() <span class="k">noexcept</span>;</code></pre></div>

<p>The pixel bytes, width * height * 4 of them: for pixel (x, y) the red <code><a href="../types.html#symbol-byte">byte</a></code> is at index (y * width + x) * 4, followed by green, blue and alpha.</p>

<div class="function-declaration" id="function-pixels-2"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> std::byte&gt; pixels() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `save_png` {#function-save-png}

<div class="function-declaration" id="function-save-png-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../result/result.html">result</a>&lt;<span class="k">void</span>&gt; save_png(
    <span class="k">const</span> std::filesystem::path &amp;path) <span class="k">const</span>;</code></pre></div>

<p>Writes the <code><a href="image.html">image</a></code> as a <code>PNG</code> file, replacing any existing file. Same failures as <code><a href="#function-encode-png-1">encode_png</a>()</code> plus <code>IO_FAILURE</code> when the file cannot be written.</p>

#### `set_pixel` {#function-set-pixel}

<div class="function-declaration" id="function-set-pixel-1"><pre><code><span class="k">void</span> set_pixel(
    <a href="../types.html#symbol-u32">u32</a> x,
    <a href="../types.html#symbol-u32">u32</a> y,
    <a href="rgba8.html">rgba8</a> color) <span class="k">noexcept</span>;</code></pre></div>

<p>Sets the pixel at (x, y). Out-of-range coordinates assert and change nothing.</p>

#### `width` {#function-width}

<div class="function-declaration" id="function-width-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../types.html#symbol-u32">u32</a> width() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Width in pixels; 0 for the empty <code><a href="image.html">image</a></code>.</p>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="image.html">image</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="image.html">image</a> &amp;) = <span class="k">default</span>;</code></pre></div>

<div class="function-declaration" id="operator-3d-2"><pre><code><a href="image.html">image</a> &amp;<span class="k">operator</span>=(
    <a href="image.html">image</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="image.html">image</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when dimensions and every pixel <code><a href="../types.html#symbol-byte">byte</a></code> are equal.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
