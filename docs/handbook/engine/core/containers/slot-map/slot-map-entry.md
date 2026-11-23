---
kind: class
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [slot_map.h](../slot-map.md)

# slot_map_entry
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/containers/slot_map.h` |
| Source File Path | N/A |
| API ↗ | [Containers](../../../../../api/core/containers.md) |

## Syntax

```cpp
template <typename Key, typename T> class slot_map_entry
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">template</span> &lt;<span class="k">typename</span>... Args&gt; <span class="k">requires</span> std::is_constructible_v&lt;T, Args &amp;&amp;...&gt; <span class="k">explicit</span> slot_map_entry(
    <span class="k">const</span> Key &amp;key,
    Args &amp;&amp;...args);</code></td><td></td></tr>
</tbody>
</table>



## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mKey</code></td><td><code class="signature">Key</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mValue</code></td><td><code class="signature">T</code></td><td>—</td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `get` {#function-get}

<div class="function-declaration" id="function-get-1"><pre><code><span class="k">template</span> &lt;<a href="../../types.html#symbol-usize">usize</a> Index&gt; [[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">decltype</span>(<span class="k">auto</span>) get() <span class="k">noexcept</span>;</code></pre></div>

<p>Tuple protocol for structured bindings: <code>get&lt;0&gt;()</code> is the key, <code>get&lt;1&gt;()</code> the element.</p>

<div class="function-declaration" id="function-get-2"><pre><code><span class="k">template</span> &lt;<a href="../../types.html#symbol-usize">usize</a> Index&gt; [[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">decltype</span>(<span class="k">auto</span>) get() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `key` {#function-key}

<div class="function-declaration" id="function-key-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">const</span> Key &amp;key() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The key that addresses this element.</p>

#### `value` {#function-value}

<div class="function-declaration" id="function-value-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> T &amp;value() <span class="k">noexcept</span>;</code></pre></div>

<p>The element itself.</p>

<div class="function-declaration" id="function-value-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">const</span> T &amp;value() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
