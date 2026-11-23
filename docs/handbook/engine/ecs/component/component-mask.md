---
kind: class
---

[Engine](../../../engine.md) → [ECS](../../ecs.md) → [component.h](../component.md)

# component_mask
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core::json` |
| Include File Path | `engine/include/conjure/ecs/component.h` |
| Source File Path | N/A |
| API ↗ | [Entities and components](../../../../api/ecs/entities-and-components.md) |

## Syntax

```cpp
class CONJURE_API component_mask
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">constexpr</span> component_mask() <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>



## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mWords</code></td><td><code class="signature">std::<wbr>array&lt;<wbr>u64,<wbr> WORD_<wbr>COUNT&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">WORD_COUNT</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature">MAX_<wbr>COMPONENTS / <span class="mi">64</span></code></td><td>Number of 64-bit words.</td></tr>
</tbody>
</table>

## Functions

### Public

#### `clear` {#function-clear}

<div class="function-declaration" id="function-clear-1"><pre><code><span class="k">constexpr</span> <span class="k">void</span> clear(
    <a href="component-id.html">component_id</a> id) <span class="k">noexcept</span>;</code></pre></div>

<p>Clears the bit of <code>id</code>. Invalid ids assert and are ignored.</p>

#### `contains_all` {#function-contains-all}

<div class="function-declaration" id="function-contains-all-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> contains_all(
    <span class="k">const</span> <a href="component-mask.html">component_mask</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when every bit of <code>other</code> is set here.</p>

#### `contains_any` {#function-contains-any}

<div class="function-declaration" id="function-contains-any-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> contains_any(
    <span class="k">const</span> <a href="component-mask.html">component_mask</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when at least one bit of <code>other</code> is set here.</p>

#### `count` {#function-count}

<div class="function-declaration" id="function-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../../core/types.html#symbol-u32">u32</a> count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of set bits.</p>

#### `for_each_set` {#function-for-each-set}

<div class="function-declaration" id="function-for-each-set-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> Callable&gt; <span class="k">constexpr</span> <span class="k">void</span> for_each_set(
    Callable &amp;&amp;callable) <span class="k">const</span>;</code></pre></div>

<p>Invokes <code>callable(<a href="component-id.html">component_id</a>)</code> for every set bit in ascending id order.</p>

#### `hash` {#function-hash}

<div class="function-declaration" id="function-hash-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u64">u64</a> hash() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>FNV-1a 64 over the words; equal masks hash equal on every platform.</p>

#### `none` {#function-none}

<div class="function-declaration" id="function-none-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> none() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when no bit is set.</p>

#### `set` {#function-set}

<div class="function-declaration" id="function-set-1"><pre><code><span class="k">constexpr</span> <span class="k">void</span> set(
    <a href="component-id.html">component_id</a> id) <span class="k">noexcept</span>;</code></pre></div>

<p>Sets the bit of <code>id</code>. Invalid ids assert and are ignored.</p>

#### `test` {#function-test}

<div class="function-declaration" id="function-test-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> test(
    <a href="component-id.html">component_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the bit of <code>id</code> is set; <code>false</code> for an invalid id.</p>

#### `words` {#function-words}

<div class="function-declaration" id="function-words-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">const</span> std::array&lt;u64, WORD_COUNT&gt; &amp;words() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The raw words, lowest ids first.</p>

## Operator Overloads

### Public

#### `operator&` {#operator-26}

<div class="function-declaration" id="operator-26-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="component-mask.html">component_mask</a> <span class="k">operator</span>&amp;(
    <span class="k">const</span> <a href="component-mask.html">component_mask</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `operator&=` {#operator-26--3d}

<div class="function-declaration" id="operator-26--3d-1"><pre><code><span class="k">constexpr</span> <a href="component-mask.html">component_mask</a> &amp;<span class="k">operator</span>&amp;=(
    <span class="k">const</span> <a href="component-mask.html">component_mask</a> &amp;other) <span class="k">noexcept</span>;</code></pre></div>

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="component-mask.html">component_mask</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

#### `operator|` {#operator-7c}

<div class="function-declaration" id="operator-7c-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="component-mask.html">component_mask</a> <span class="k">operator</span>|(
    <span class="k">const</span> <a href="component-mask.html">component_mask</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `operator|=` {#operator-7c--3d}

<div class="function-declaration" id="operator-7c--3d-1"><pre><code><span class="k">constexpr</span> <a href="component-mask.html">component_mask</a> &amp;<span class="k">operator</span>|=(
    <span class="k">const</span> <a href="component-mask.html">component_mask</a> &amp;other) <span class="k">noexcept</span>;</code></pre></div>

#### `operator~` {#operator-7e}

<div class="function-declaration" id="operator-7e-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="component-mask.html">component_mask</a> <span class="k">operator</span>~() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
