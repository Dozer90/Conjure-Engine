---
kind: struct
---

[Engine](../../../engine.md) → [Audio](../../audio.md) → [voice.h](../voice.md)

# voice_id
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::audio` |
| Include File Path | `engine/include/conjure/audio/voice.h` |
| Source File Path | `engine/src/audio/voice.cpp` |
| API ↗ | [Playing sounds](../../../../api/audio/playing-sounds.md) |

## Syntax

```cpp
struct voice_id
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">key</code></td><td><code class="signature"><a href="../../core/containers/slot-map/slot-key.html">core::slot_key</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `from_u64` {#function-from-u64}

<div class="function-declaration" id="function-from-u64-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <span class="k">constexpr</span> <a href="voice-id.html">voice_id</a> from_u64(
    <a href="../../core/types.html#symbol-u64">u64</a> packed) <span class="k">noexcept</span>;</code></pre></div>

<p>Inverse of <code><a href="#function-to-u64-1">to_u64</a>()</code>.</p>

#### `is_valid` {#function-is-valid}

<div class="function-declaration" id="function-is-valid-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_valid() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True for every id the engine issued; <code>false</code> for the <code>null</code> id.</p>

#### `null` {#function-null}

<div class="function-declaration" id="function-null-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <span class="k">constexpr</span> <a href="voice-id.html">voice_id</a> null() <span class="k">noexcept</span>;</code></pre></div>

<p>The <code>null</code> id. Equivalent to <code><a href="voice-id.html">voice_id</a></code>{}.</p>

#### `to_u64` {#function-to-u64}

<div class="function-declaration" id="function-to-u64-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../../core/types.html#symbol-u64">u64</a> to_u64() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Packs the id into one integer (generation in the high 32 bits).</p>

## Operator Overloads

### Public

#### `operator<=>` {#operator-3c--3d--3e}

<div class="function-declaration" id="operator-3c--3d--3e-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">auto</span> <span class="k">operator</span>&lt;=&gt;(
    <span class="k">const</span> <a href="voice-id.html">voice_id</a> &amp;) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="voice-id.html">voice_id</a> &amp;) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
