---
kind: struct
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [slot_map.h](../slot-map.md)

# slot_key
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/containers/slot_map.h` |
| Source File Path | N/A |
| API ↗ | [Containers](../../../../../api/core/containers.md) |

## Syntax

```cpp
struct slot_key
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">generation</code></td><td><code class="signature"><a href="../../types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">index</code></td><td><code class="signature"><a href="../../types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `from_u64` {#function-from-u64}

<div class="function-declaration" id="function-from-u64-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <span class="k">constexpr</span> <a href="slot-key.html">slot_key</a> from_u64(
    <a href="../../types.html#symbol-u64">u64</a> packed) <span class="k">noexcept</span>;</code></pre></div>

<p>Inverse of <code><a href="#function-to-u64-1">to_u64</a>()</code>.</p>

#### `is_valid` {#function-is-valid}

<div class="function-declaration" id="function-is-valid-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_valid() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True for every key a <code><a href="slot-map.html">slot_map</a></code> has issued; <code>false</code> for the <code>null</code> key.</p>

#### `null` {#function-null}

<div class="function-declaration" id="function-null-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <span class="k">constexpr</span> <a href="slot-key.html">slot_key</a> null() <span class="k">noexcept</span>;</code></pre></div>

<p>The all-zero key. Means &quot;none&quot;.</p>

#### `to_u64` {#function-to-u64}

<div class="function-declaration" id="function-to-u64-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../../types.html#symbol-u64">u64</a> to_u64() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Packs the key into one integer (generation in the high 32 bits). Used for ImTextureID style opaque handles and serialisation.</p>

## Operator Overloads

### Public

#### `operator<=>` {#operator-3c--3d--3e}

<div class="function-declaration" id="operator-3c--3d--3e-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">auto</span> <span class="k">operator</span>&lt;=&gt;(
    <span class="k">const</span> <a href="slot-key.html">slot_key</a> &amp;) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="slot-key.html">slot_key</a> &amp;) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
