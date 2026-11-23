---
kind: struct
---

[Engine](../../../engine.md) → [Platform](../../platform.md) → [input_state.h](../input-state.md)

# key_bitset
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::platform` |
| Include File Path | `engine/include/conjure/platform/input_state.h` |
| Source File Path | `engine/src/platform/input_state.cpp` |
| API ↗ | [Input](../../../../api/platform/input.md) |

## Syntax

```cpp
struct key_bitset
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">WORD_COUNT</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature">KEY_COUNT / <span class="mi">64</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">words</code></td><td><code class="signature">std::<wbr>array&lt;<wbr>u64,<wbr> WORD_<wbr>COUNT&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `any` {#function-any}

<div class="function-declaration" id="function-any-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> any() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when at least one bit is set.</p>

#### `clear` {#function-clear}

<div class="function-declaration" id="function-clear-1"><pre><code><span class="k">constexpr</span> <span class="k">void</span> clear() <span class="k">noexcept</span>;</code></pre></div>

<p>Clears every bit.</p>

#### `count` {#function-count}

<div class="function-declaration" id="function-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../../core/types.html#symbol-u32">u32</a> count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of set bits.</p>

#### `reset` {#function-reset}

<div class="function-declaration" id="function-reset-1"><pre><code><span class="k">constexpr</span> <span class="k">void</span> reset(
    <a href="../key.html#symbol-key">key</a> code) <span class="k">noexcept</span>;</code></pre></div>

<p>Clears the key&#x27;s bit.</p>

#### `set` {#function-set}

<div class="function-declaration" id="function-set-1"><pre><code><span class="k">constexpr</span> <span class="k">void</span> set(
    <a href="../key.html#symbol-key">key</a> code) <span class="k">noexcept</span>;</code></pre></div>

<p>Sets the key&#x27;s bit.</p>

#### `test` {#function-test}

<div class="function-declaration" id="function-test-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> test(
    <a href="../key.html#symbol-key">key</a> code) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the key&#x27;s bit is set.</p>

## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="key-bitset.html">key_bitset</a> &amp;) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
