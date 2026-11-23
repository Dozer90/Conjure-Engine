---
kind: class
---

[Engine](../../../engine.md) → [Core](../../core.md) → [string_id.h](../string-id.md)

# string_id
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/string_id.h` |
| Source File Path | `engine/src/core/strings/string_id.cpp` |
| API ↗ | [Containers](../../../../api/core/containers.md), [Strings and identifiers](../../../../api/core/strings-and-identifiers.md) |

## Syntax

```cpp
class CONJURE_API string_id
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">constexpr</span> string_id() <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td>The <code>null</code> id: hash 0, no name.</td></tr>
<tr><td>public</td><td><code class="signature"><span class="k">constexpr</span> <span class="k">explicit</span> string_id(
    std::string_view name);</code></td><td>Hashes <code>name</code>. When constant-evaluated (constexpr variables, static or namespace- scope ids with a literal name) the id is only hashed; otherwise the name is also interned so <code>view()</code>/<code>c_str()</code> can resolve it later. See the header comment for the exact rules; use <code>intern()</code> where interning must be guaranteed.</td></tr>
</tbody>
</table>



## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mHash</code></td><td><code class="signature"><a href="../types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `c_str` {#function-c-str}

<div class="function-declaration" id="function-c-str-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <span class="k">char</span> *c_str() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The interned name as a <code>null</code>-terminated string, or &quot;&quot; when unknown.</p>

#### `from_hash` {#function-from-hash}

<div class="function-declaration" id="function-from-hash-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <span class="k">constexpr</span> <a href="string-id.html">string_id</a> from_hash(
    <a href="../types.html#symbol-u64">u64</a> hash) <span class="k">noexcept</span>;</code></pre></div>

<p>Wraps an already computed hash (deserialisation, <code>C</code># interop). Nothing is interned.</p>

#### `hash` {#function-hash}

<div class="function-declaration" id="function-hash-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../types.html#symbol-u64">u64</a> hash() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The 64-bit FNV-1a hash of the name. 0 for the <code>null</code> id.</p>

#### `intern` {#function-intern}

<div class="function-declaration" id="function-intern-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="string-id.html">string_id</a> intern(
    std::string_view name);</code></pre></div>

<p>Hashes and interns <code>name</code> in every context, including for ids that were first created in constant expressions. Never constexpr, so it is the right initialiser for static and namespace-scope ids whose names must be visible to tools.</p>

#### `interned_count` {#function-interned-count}

<div class="function-declaration" id="function-interned-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../types.html#symbol-usize">usize</a> interned_count() <span class="k">noexcept</span>;</code></pre></div>

<p>Number of names currently interned in the process-wide table.</p>

#### `is_null` {#function-is-null}

<div class="function-declaration" id="function-is-null-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_null() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True for the default-constructed id (hash 0).</p>

#### `null` {#function-null}

<div class="function-declaration" id="function-null-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <span class="k">constexpr</span> <a href="string-id.html">string_id</a> null() <span class="k">noexcept</span>;</code></pre></div>

<p>The <code>null</code> id. Equivalent to <code><a href="string-id.html">string_id</a>()</code>.</p>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string to_string() <span class="k">const</span>;</code></pre></div>

<p>The interned name when known, otherwise &quot;#&quot; followed by the 16-digit hex hash.</p>

#### `view` {#function-view}

<div class="function-declaration" id="function-view-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string_view view() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The interned name, or an empty view when this id was never interned.</p>

## Operator Overloads

### Public

#### `operator<=>` {#operator-3c--3d--3e}

<div class="function-declaration" id="operator-3c--3d--3e-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">auto</span> <span class="k">operator</span>&lt;=&gt;(
    <span class="k">const</span> <a href="string-id.html">string_id</a> &amp;) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="string-id.html">string_id</a> &amp;) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
