---
kind: class
---

[Engine](../../../engine.md) → [Core](../../core.md) → [uuid.h](../uuid.md)

# uuid
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/uuid.h` |
| Source File Path | `engine/src/core/foundation/uuid.cpp` |
| API ↗ | [Data types](../../../../api/core/data-types.md) |

## Syntax

```cpp
class CONJURE_API uuid
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">constexpr</span> uuid() <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature"><span class="k">constexpr</span> uuid(
    <a href="../types.html#symbol-u64">u64</a> high,
    <a href="../types.html#symbol-u64">u64</a> low) <span class="k">noexcept</span>;</code></td><td>Builds an identifier from its two 64-bit halves (most significant first).</td></tr>
</tbody>
</table>



## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mHigh</code></td><td><code class="signature"><a href="../types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mLow</code></td><td><code class="signature"><a href="../types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `generate` {#function-generate}

<div class="function-declaration" id="function-generate-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="uuid.html">uuid</a> generate();</code></pre></div>

<p>Generates a random version-4 identifier. Thread-safe. The per-thread generator is seeded from <code>std::random_device</code> on first use; a host without an entropy source terminates there, which is acceptable for every supported desktop platform.</p>

#### `hash` {#function-hash}

<div class="function-declaration" id="function-hash-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../types.html#symbol-u64">u64</a> hash() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>64-bit hash suitable for hash maps.</p>

#### `high` {#function-high}

<div class="function-declaration" id="function-high-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../types.html#symbol-u64">u64</a> high() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Most significant 64 bits.</p>

#### `is_null` {#function-is-null}

<div class="function-declaration" id="function-is-null-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_null() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True for the all-zero identifier.</p>

#### `low` {#function-low}

<div class="function-declaration" id="function-low-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../types.html#symbol-u64">u64</a> low() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Least significant 64 bits.</p>

#### `null` {#function-null}

<div class="function-declaration" id="function-null-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <span class="k">constexpr</span> <a href="uuid.html">uuid</a> null() <span class="k">noexcept</span>;</code></pre></div>

<p>The all-zero identifier. Means &quot;none&quot;.</p>

#### `parse` {#function-parse}

<div class="function-declaration" id="function-parse-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> std::optional&lt;<a href="uuid.html">uuid</a>&gt; parse(
    std::string_view text) <span class="k">noexcept</span>;</code></pre></div>

<p>Parses &quot;xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx&quot; (case-insensitive, braces optional). Returns nullopt for anything else.</p>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string to_string() <span class="k">const</span>;</code></pre></div>

<p>Lower-case canonical form &quot;xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx&quot;.</p>

## Operator Overloads

### Public

#### `operator<=>` {#operator-3c--3d--3e}

<div class="function-declaration" id="operator-3c--3d--3e-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">auto</span> <span class="k">operator</span>&lt;=&gt;(
    <span class="k">const</span> <a href="uuid.html">uuid</a> &amp;) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="uuid.html">uuid</a> &amp;) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
