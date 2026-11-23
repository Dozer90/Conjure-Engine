---
kind: class
---

[Engine](../../../engine.md) → [Bindings](../../bindings.md) → [bindings_subsystem.h](../bindings-subsystem.md)

# script_residency
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::bindings` |
| Include File Path | `engine/include/conjure/bindings/bindings_subsystem.h` |
| Source File Path | `engine/src/bindings/bindings_subsystem.cpp` |
| API ↗ | [Scripting from C#: the bindings](../../../../api/bindings/index.md), [The native table](../../../../api/bindings/native-table.md) |

## Syntax

```cpp
class CONJURE_API script_residency
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">script_residency();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~script_residency();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mHandles</code></td><td><code class="signature"><a href="../../core/containers/flat-hash-map/flat-hash-map.html">core::<wbr>flat_<wbr>hash_<wbr>map</a>&lt;<wbr>core::<wbr>uuid,<wbr> <a href="../../assets/asset-ref/asset-handle.html">assets::<wbr>asset_<wbr>handle</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `clear` {#function-clear}

<div class="function-declaration" id="function-clear-1"><pre><code><span class="k">void</span> clear() <span class="k">noexcept</span>;</code></pre></div>

<p>Drops every handle.</p>

#### `hold` {#function-hold}

<div class="function-declaration" id="function-hold-1"><pre><code><span class="k">bool</span> hold(
    <span class="k">const</span> <a href="../../core/uuid/uuid.html">core::uuid</a> &amp;id,
    <a href="../../assets/asset-ref/asset-handle.html">assets::asset_handle</a> handle);</code></pre></div>

<p>Keeps the handle under its uuid; a uuid already held keeps its first handle. False for an unbound handle or the <code>null</code> uuid.</p>

#### `holds` {#function-holds}

<div class="function-declaration" id="function-holds-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> holds(
    <span class="k">const</span> <a href="../../core/uuid/uuid.html">core::uuid</a> &amp;id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True while a handle is held for the uuid.</p>

#### `release` {#function-release}

<div class="function-declaration" id="function-release-1"><pre><code><span class="k">bool</span> release(
    <span class="k">const</span> <a href="../../core/uuid/uuid.html">core::uuid</a> &amp;id) <span class="k">noexcept</span>;</code></pre></div>

<p>Drops the handle of a uuid. False when none was held.</p>

#### `size` {#function-size}

<div class="function-declaration" id="function-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-usize">usize</a> size() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Handles held.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
