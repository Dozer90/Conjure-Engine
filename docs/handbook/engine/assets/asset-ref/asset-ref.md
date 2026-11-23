---
kind: class
---

[Engine](../../../engine.md) → [Assets](../../assets.md) → [asset_ref.h](../asset-ref.md)

# asset_ref
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::assets` |
| Include File Path | `engine/include/conjure/assets/asset_ref.h` |
| Source File Path | `engine/src/assets/asset_ref.cpp` |
| API ↗ | [Assets](../../../../api/assets/index.md), [Referencing and loading assets](../../../../api/assets/referencing-and-loading.md) |

## Syntax

```cpp
template <typename AssetType> class asset_ref
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">constexpr</span> asset_ref() <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature"><span class="k">constexpr</span> <span class="k">explicit</span> asset_ref(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id) <span class="k">noexcept</span>;</code></td><td>An unbound reference: carries the uuid, reads <code>nullptr</code> until bound by the manager.</td></tr>
<tr><td>public</td><td><code class="signature"><span class="k">constexpr</span> asset_ref(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id,
    <span class="k">const</span> <a href="../asset-slot/asset-slot.html">asset_slot</a> *slot) <span class="k">noexcept</span>;</code></td><td><code>A</code> bound reference. The slot must belong to <code>id</code>.</td></tr>
</tbody>
</table>



## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mId</code></td><td><code class="signature"><a href="../asset-types.html#symbol-asset-id">asset_id</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSlot</code></td><td><code class="signature"><span class="k">const</span> <a href="../asset-slot/asset-slot.html">asset_slot</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `get` {#function-get}

<div class="function-declaration" id="function-get-1"><pre><code>[[<span class="na">nodiscard</span>]] AssetType *get() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Hot path: one atomic load, no hashing, no locking. Returns <code>nullptr</code> unless <code>LOADED</code>. The pointer is valid until the end of the next frame.</p>

#### `id` {#function-id}

<div class="function-declaration" id="function-id-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `is_bound` {#function-is-bound}

<div class="function-declaration" id="function-is-bound-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_bound() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `slot` {#function-slot}

<div class="function-declaration" id="function-slot-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">const</span> <a href="../asset-slot/asset-slot.html">asset_slot</a> *slot() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The bound slot, or <code>nullptr</code>.</p>

#### `state` {#function-state}

<div class="function-declaration" id="function-state-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../asset-types.html#symbol-asset-state">asset_state</a> state() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p><code>UNLOADED</code> when unbound.</p>

#### `version` {#function-version}

<div class="function-declaration" id="function-version-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> version() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Publish counter of the slot; 0 when unbound. Changes on every publish and unload.</p>

## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="asset-ref.html">asset_ref</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Equality is by uuid: two references to the same asset compare equal whether or not they are bound.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
