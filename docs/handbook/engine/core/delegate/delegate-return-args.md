---
kind: class
---

[Engine](../../../engine.md) → [Core](../../core.md) → [delegate.h](../delegate.md)

# delegate<Return(Args...)>
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/delegate.h` |
| Source File Path | N/A |
| API ↗ | [Time, events and random](../../../../api/core/time-and-events.md) |

## Syntax

```cpp
template <typename Return, typename... Args> class delegate<Return(Args...)>
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">constexpr</span> delegate() <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>



## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">CAN_BIND</code></td><td><code class="signature"><span class="k">template</span> &lt;<wbr><span class="k">typename</span> Callable&gt; <span class="k">static</span> <span class="k">constexpr</span> <span class="k">bool</span></code></td><td><code class="signature">sizeof(Callable) &lt;<wbr>= STORAGE_<wbr>BYTES &amp;&amp; alignof(Callable) &lt;<wbr>= alignof(<span class="k">void</span> *) &amp;&amp; std::<wbr>is_<wbr>trivially_<wbr>copyable_<wbr>v&lt;<wbr>Callable&gt; &amp;&amp; std::<wbr>is_<wbr>trivially_<wbr>destructible_<wbr>v&lt;<wbr>Callable&gt; &amp;&amp; std::<wbr>is_<wbr>invocable_<wbr>r_<wbr>v&lt;<wbr>Return,<wbr> <span class="k">const</span> Callable &amp;,<wbr> Args...&gt;</code></td><td>True when a callable type can be bound: trivially copyable, pointer-aligned, fits the inline storage and is invocable as const with the delegate&#x27;s signature.</td></tr>
<tr><td>private</td><td><code class="signature">mInvoke</code></td><td><code class="signature"><a href="../delegate.html#symbol-delegate-return-args-invoker-type">invoker_type</a></code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">STORAGE_<wbr>BYTES</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">2</span> * sizeof(<span class="k">void</span> *)</code></td><td>Bytes of inline storage for the bound target.</td></tr>
<tr><td>private</td><td><code class="signature">void</code></td><td><code class="signature"><span class="k">alignas</span>( *) std::<wbr>array&lt;<wbr>std::<wbr>byte,<wbr> STORAGE_<wbr>BYTES&gt; m<wbr>Storage</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `bind` {#function-bind}

<div class="function-declaration" id="function-bind-1"><pre><code><span class="k">template</span> &lt;<span class="k">auto</span> Function&gt; <span class="k">requires</span> std::is_pointer_v&lt;<span class="k">decltype</span>(Function)&gt; &amp;&amp; std::is_function_v&lt;std::remove_pointer_t&lt;<span class="k">decltype</span>(Function)&gt;&gt; &amp;&amp; std::is_invocable_r_v&lt;Return, <span class="k">decltype</span>(Function), Args...&gt; [[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="delegate-return-args.html">delegate</a> bind() <span class="k">noexcept</span>;</code></pre></div>

<p>Binds a free function known at compile time.</p>

<div class="function-declaration" id="function-bind-2"><pre><code><span class="k">template</span> &lt;<span class="k">auto</span> Method, <span class="k">typename</span> Class&gt; <span class="k">requires</span> std::is_member_function_pointer_v&lt;<span class="k">decltype</span>(Method)&gt; &amp;&amp; std::is_invocable_r_v&lt;Return, <span class="k">decltype</span>(Method), Class *, Args...&gt; [[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="delegate-return-args.html">delegate</a> bind(
    Class *instance) <span class="k">noexcept</span>;</code></pre></div>

<p>Binds a member function to an instance: <code>delegate&lt;void(int)&gt;</code>::<code>bind&lt;&amp;foo::bar&gt;(&amp;foo)</code>. Const member functions accept a pointer to const. The instance is not owned.</p>

<div class="function-declaration" id="function-bind-3"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="delegate-return-args.html">delegate</a> bind(
    <a href="../delegate.html#symbol-delegate-return-args-function-pointer">function_pointer</a> function) <span class="k">noexcept</span>;</code></pre></div>

<p>Binds a function pointer chosen at runtime. <code>A</code> <code>null</code> pointer yields an unbound delegate.</p>

<div class="function-declaration" id="function-bind-4"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> Callable&gt; <span class="k">requires</span> (!std::is_same_v&lt;std::remove_cvref_t&lt;Callable&gt;, <a href="delegate-return-args.html">delegate</a>&gt;) &amp;&amp; (!std::is_same_v&lt;std::remove_cvref_t&lt;Callable&gt;, <a href="../delegate.html#symbol-delegate-return-args-function-pointer">function_pointer</a>&gt;) [[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="delegate-return-args.html">delegate</a> bind(
    Callable callable) <span class="k">noexcept</span>;</code></pre></div>

<p>Binds a stateless or small trivially copyable callable (see <code>CAN_BIND</code>) by copying it into the inline storage.</p>

#### `is_bound` {#function-is-bound}

<div class="function-declaration" id="function-is-bound-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_bound() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when a target is bound.</p>

#### `reset` {#function-reset}

<div class="function-declaration" id="function-reset-1"><pre><code><span class="k">constexpr</span> <span class="k">void</span> reset() <span class="k">noexcept</span>;</code></pre></div>

<p>Unbinds the delegate.</p>

### Private

#### `invoke_callable` {#function-invoke-callable}

<div class="function-declaration" id="function-invoke-callable-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> Callable&gt; <span class="k">static</span> Return invoke_callable(
    <span class="k">const</span> <span class="k">void</span> *storage,
    Args... args);</code></pre></div>

#### `invoke_function_pointer` {#function-invoke-function-pointer}

<div class="function-declaration" id="function-invoke-function-pointer-1"><pre><code><span class="k">static</span> Return invoke_function_pointer(
    <span class="k">const</span> <span class="k">void</span> *storage,
    Args... args);</code></pre></div>

#### `invoke_member` {#function-invoke-member}

<div class="function-declaration" id="function-invoke-member-1"><pre><code><span class="k">template</span> &lt;<span class="k">auto</span> Method, <span class="k">typename</span> Class&gt; <span class="k">static</span> Return invoke_member(
    <span class="k">const</span> <span class="k">void</span> *storage,
    Args... args);</code></pre></div>

#### `invoke_static` {#function-invoke-static}

<div class="function-declaration" id="function-invoke-static-1"><pre><code><span class="k">template</span> &lt;<span class="k">auto</span> Function&gt; <span class="k">static</span> Return invoke_static(
    [[<span class="na">maybe_unused</span>]] <span class="k">const</span> <span class="k">void</span> *storage,
    Args... args);</code></pre></div>

## Operator Overloads

### Public

#### `operator bool` {#operator-bool}

<div class="function-declaration" id="operator-bool-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">explicit</span> <span class="k">operator</span> <span class="k">bool</span>() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Same as <code>is_bound()</code>.</p>

#### `operator()` {#operator-28--29}

<div class="function-declaration" id="operator-28--29-1"><pre><code>Return <span class="k">operator</span>()(
    Args... args) <span class="k">const</span>;</code></pre></div>

<p>Invokes the bound target. Calling an unbound delegate asserts and returns a default-constructed value (or nothing for void).</p>

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="delegate-return-args.html">delegate</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Two delegates are equal when they invoke the same target on the same instance (or the same captured state). Unbound delegates are equal to each other. The comparison is of the invoker and the raw inline storage, which is exact for free-function, member, function-pointer and stateless-lambda bindings. <code>A</code> capturing lambda is copied whole, padding included, so its equality is exact only when the closure has no padding (<code>std::has_unique_object_representations_v</code>); otherwise two bindings with equal captures (a bool next to a pointer, say) may compare unequal.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
