---
kind: class
---

[Engine](../../../engine.md) → [Modules](../../modules.md) → [module_consumer.h](../module-consumer.md)

# reload_context
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::modules` |
| Include File Path | `engine/include/conjure/modules/module_consumer.h` |
| Source File Path | `engine/src/modules/module_consumer.cpp` |
| API ↗ | [Loading and managing modules](../../../../api/modules/loading-and-managing.md) |

## Syntax

```cpp
class CONJURE_API reload_context
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">reload_context();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~reload_context();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mBlobs</code></td><td><code class="signature"><a href="../../core/containers/flat-map/flat-map.html">core::<wbr>flat_<wbr>map</a>&lt;<wbr>core::<wbr>string_<wbr>id,<wbr> std::<wbr>vector&lt;<wbr>std::<wbr>byte&gt;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `contains` {#function-contains}

<div class="function-declaration" id="function-contains-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> contains(
    <a href="../../core/string-id/string-id.html">core::string_id</a> key) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `fetch` {#function-fetch}

<div class="function-declaration" id="function-fetch-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> std::byte&gt; fetch(
    <a href="../../core/string-id/string-id.html">core::string_id</a> key) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The blob stored under the key; empty when missing.</p>

#### `size` {#function-size}

<div class="function-declaration" id="function-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-usize">usize</a> size() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of stored blobs.</p>

#### `store` {#function-store}

<div class="function-declaration" id="function-store-1"><pre><code><span class="k">void</span> store(
    <a href="../../core/string-id/string-id.html">core::string_id</a> key,
    std::span&lt;<span class="k">const</span> std::byte&gt; bytes);</code></pre></div>

<p>Stores a copy of the bytes under the key, replacing an earlier blob.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
