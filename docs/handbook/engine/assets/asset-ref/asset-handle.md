---
kind: class
---

[Engine](../../../engine.md) → [Assets](../../assets.md) → [asset_ref.h](../asset-ref.md)

# asset_handle
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::assets` |
| Include File Path | `engine/include/conjure/assets/asset_ref.h` |
| Source File Path | `engine/src/assets/asset_ref.cpp` |
| API ↗ | [Assets](../../../../api/assets/index.md), [Referencing and loading assets](../../../../api/assets/referencing-and-loading.md) |

## Syntax

```cpp
class CONJURE_API asset_handle
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">constexpr</span> asset_handle() <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> asset_handle(
    <a href="../asset-slot/asset-slot.html">asset_slot</a> *slot) <span class="k">noexcept</span>;</code></td><td>Acquires one strong reference on <code>slot</code> (<code>nullptr</code> yields an unbound handle).</td></tr>
<tr><td>public</td><td><code class="signature">asset_handle(
    <span class="k">const</span> <a href="asset-handle.html">asset_handle</a> &amp;other) <span class="k">noexcept</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">asset_handle(
    <a href="asset-handle.html">asset_handle</a> &amp;&amp;other) <span class="k">noexcept</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~asset_handle();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mSlot</code></td><td><code class="signature"><a href="../asset-slot/asset-slot.html">asset_slot</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `get` {#function-get}

<div class="function-declaration" id="function-get-1"><pre><code><span class="k">template</span> &lt;<a href="../asset-types.html#symbol-declared-asset-type">declared_asset_type</a> AssetType&gt; [[<span class="na">nodiscard</span>]] AssetType *get() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The payload as AssetType, or <code>nullptr</code> when unbound, not <code>LOADED</code> or of another type.</p>

#### `id` {#function-id}

<div class="function-declaration" id="function-id-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../asset-types.html#symbol-asset-id">asset_id</a> id() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The uuid, or the <code>null</code> uuid when unbound.</p>

#### `is_bound` {#function-is-bound}

<div class="function-declaration" id="function-is-bound-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_bound() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `is_terminal` {#function-is-terminal}

<div class="function-declaration" id="function-is-terminal-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_terminal() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the slot is <code>LOADED</code> or <code>FAILED</code> (also <code>true</code> for an unbound handle, which can never make progress).</p>

#### `ref` {#function-ref}

<div class="function-declaration" id="function-ref-1"><pre><code><span class="k">template</span> &lt;<a href="../asset-types.html#symbol-declared-asset-type">declared_asset_type</a> AssetType&gt; [[<span class="na">nodiscard</span>]] <a href="asset-ref.html">asset_ref</a>&lt;AssetType&gt; ref() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p><code>A</code> typed reference to the same slot. <code>A</code> type mismatch logs an ERROR once per uuid and yields an unbound reference that still carries the uuid; a slot whose type is not known yet (never bound typed, never loaded) yields an unbound reference quietly.</p>

#### `reset` {#function-reset}

<div class="function-declaration" id="function-reset-1"><pre><code><span class="k">void</span> reset() <span class="k">noexcept</span>;</code></pre></div>

<p>Releases the strong reference and makes the handle unbound.</p>

#### `slot` {#function-slot}

<div class="function-declaration" id="function-slot-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../asset-slot/asset-slot.html">asset_slot</a> *slot() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The bound slot, or <code>nullptr</code>.</p>

#### `state` {#function-state}

<div class="function-declaration" id="function-state-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../asset-types.html#symbol-asset-state">asset_state</a> state() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `type` {#function-type}

<div class="function-declaration" id="function-type-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../asset-types.html#symbol-asset-type-id">asset_type_id</a> type() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The slot&#x27;s type id (<code>INVALID</code> when unbound or not yet known).</p>

#### `version` {#function-version}

<div class="function-declaration" id="function-version-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> version() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="asset-handle.html">asset_handle</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="asset-handle.html">asset_handle</a> &amp;other) <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="operator-3d-2"><pre><code><a href="asset-handle.html">asset_handle</a> &amp;<span class="k">operator</span>=(
    <a href="asset-handle.html">asset_handle</a> &amp;&amp;other) <span class="k">noexcept</span>;</code></pre></div>

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="asset-handle.html">asset_handle</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Two handles are equal when they refer to the same slot.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
