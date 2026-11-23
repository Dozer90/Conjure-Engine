---
kind: class
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [type_registry.h](../type-registry.md)

# reflect::type_registry
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core::reflect` |
| Include File Path | `engine/include/conjure/core/reflect/type_registry.h` |
| Source File Path | `engine/src/core/reflect/type_registry.cpp` |
| API ↗ | [Reflection](../../../../../api/core/reflection.md) |

## Syntax

```cpp
class CONJURE_API type_registry
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">type_registry();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Additional constructor overloads have been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~type_registry();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mState</code></td><td><code class="signature">std::unique_ptr&lt;<a href="../../event/event-state.html">state</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `enum_count` {#function-enum-count}

<div class="function-declaration" id="function-enum-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> enum_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of live enumeration descriptions.</p>

#### `enums` {#function-enums}

<div class="function-declaration" id="function-enums-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="../field-info/reflect-enum-info.html">enum_info</a> *<span class="k">const</span>&gt; enums() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Every live enumeration in registration order. Same validity as <code><a href="#function-types-1">types</a>()</code>.</p>

#### `find` {#function-find}

<div class="function-declaration" id="function-find-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../type-info/reflect-type-info.html">type_info</a> *find(
    <a href="../type-id/reflect-type-id.html">type_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The registered struct with this id, or <code>nullptr</code>. Lock-free.</p>

<div class="function-declaration" id="function-find-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../type-info/reflect-type-info.html">type_info</a> *find(
    std::string_view name) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The registered struct with this exact name, or <code>nullptr</code>. Lock-free.</p>

#### `find_enum` {#function-find-enum}

<div class="function-declaration" id="function-find-enum-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../field-info/reflect-enum-info.html">enum_info</a> *find_enum(
    <a href="../type-id/reflect-type-id.html">type_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The registered enumeration with this id, or <code>nullptr</code>. Lock-free.</p>

<div class="function-declaration" id="function-find-enum-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../field-info/reflect-enum-info.html">enum_info</a> *find_enum(
    std::string_view name) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The registered enumeration with this exact name, or <code>nullptr</code>. Lock-free.</p>

#### `on_registered` {#function-on-registered}

<div class="function-declaration" id="function-on-registered-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../event/event.html">event</a>&lt;<span class="k">const</span> <a href="../type-info/reflect-type-info.html">type_info</a> &amp;&gt; &amp;on_registered() <span class="k">noexcept</span>;</code></pre></div>

<p>Emitted once per new struct registration, on the registering thread.</p>

#### `on_unregistered` {#function-on-unregistered}

<div class="function-declaration" id="function-on-unregistered-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../event/event.html">event</a>&lt;<span class="k">const</span> <a href="../type-info/reflect-type-info.html">type_info</a> &amp;&gt; &amp;on_unregistered() <span class="k">noexcept</span>;</code></pre></div>

<p>Emitted once per successful <code><a href="#function-unregister-type-1">unregister_type</a>()</code>, on the calling thread. The description stays valid for the duration of the emit.</p>

#### `register_enum` {#function-register-enum}

<div class="function-declaration" id="function-register-enum-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;<span class="k">const</span> <a href="../field-info/reflect-enum-info.html">enum_info</a> *&gt; register_enum(
    <a href="../field-info/reflect-enum-info.html">enum_info</a> info);</code></pre></div>

<p>Registers an enumeration description: non-empty name, underlying size 1, 2, 4 or 8, unique non-empty enumerator names (<code>INVALID_ARGUMENT</code> otherwise). The same id with an equal description returns the existing one; a different description is <code>ALREADY_EXISTS</code>.</p>

#### `register_type` {#function-register-type}

<div class="function-declaration" id="function-register-type-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;<span class="k">const</span> <a href="../type-info/reflect-type-info.html">type_info</a> *&gt; register_type(
    <a href="../type-info/reflect-type-info.html">type_info</a> info);</code></pre></div>

<p>Registers a struct description after validating it (see <code>validate_layout</code>). <code>A</code> <code>null</code> id is derived from the name; an id that does not match the name is <code>INVALID_ARGUMENT</code>, and so is a <code>STRUCT</code> or <code>ENUM</code> field whose description pointer is not a live description of this registry (a stack-local or foreign <code><a href="../type-info/reflect-type-info.html">type_info</a></code> would dangle). Registering the same id again returns the existing description when the layout is identical and fails with <code>ALREADY_EXISTS</code> otherwise. New registrations emit <code><a href="#function-on-registered-1">on_registered</a>()</code> on the calling thread after the registry lock is released (see the event ordering note above).</p>

#### `type_count` {#function-type-count}

<div class="function-declaration" id="function-type-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> type_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of live struct descriptions.</p>

#### `types` {#function-types}

<div class="function-declaration" id="function-types-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="../type-info/reflect-type-info.html">type_info</a> *<span class="k">const</span>&gt; types() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Every live struct in registration order. The view is invalidated by the next registration or unregistration.</p>

#### `unregister_enum` {#function-unregister-enum}

<div class="function-declaration" id="function-unregister-enum-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;<span class="k">void</span>&gt; unregister_enum(
    <a href="../type-id/reflect-type-id.html">type_id</a> id);</code></pre></div>

<p>Removes an enumeration. <code>NOT_FOUND</code> when unknown; <code>INVALID_STATE</code> when a live struct has an <code>ENUM</code> field (or array of <code>ENUM</code>) of this type.</p>

#### `unregister_type` {#function-unregister-type}

<div class="function-declaration" id="function-unregister-type-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;<span class="k">void</span>&gt; unregister_type(
    <a href="../type-id/reflect-type-id.html">type_id</a> id);</code></pre></div>

<p>Removes a struct (module unload). <code>NOT_FOUND</code> when unknown; <code>INVALID_STATE</code> when a live struct has a <code>STRUCT</code> field (or array of <code>STRUCT</code>) of this type. The description&#x27;s memory stays allocated until the registry is destroyed so lock-free lookups racing the removal never dereference freed memory.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
