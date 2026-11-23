---
kind: class
---

[Editor](../../../editor.md) → [User Interface](../../user-interface.md) → [selection_set.h](../selection-set.md)

# selection_set
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/selection_set.h` |
| Source File Path | `engine/src/editor/core/selection_set.cpp` |
| API ↗ | [Headless mode and the command model](../../../../api/editor/headless-and-commands.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
class CONJURE_API selection_set
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">selection_set();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~selection_set();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mByEntity</code></td><td><code class="signature"><a href="../../../engine/core/bit-vector/bit-vector.html">core::bit_vector</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mGuids</code></td><td><code class="signature">std::vector&lt;<a href="../../../engine/core/uuid/uuid.html">core::uuid</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mOnChanged</code></td><td><code class="signature"><a href="../../../engine/core/event/event.html">core::event</a>&lt;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPrimary</code></td><td><code class="signature"><a href="../../../engine/core/uuid/uuid.html">core::uuid</a></code></td><td><code class="signature">core::<wbr>uuid::<wbr>null()</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `add` {#function-add}

<div class="function-declaration" id="function-add-1"><pre><code><span class="k">void</span> add(
    std::span&lt;<span class="k">const</span> <a href="../../../engine/core/uuid/uuid.html">core::uuid</a>&gt; guids);</code></pre></div>

<p>Adds guids; the last non-<code>null</code> one (also one already selected) becomes the primary. Emits <code>on_changed</code> when anything new was added or the primary changed.</p>

#### `clear` {#function-clear}

<div class="function-declaration" id="function-clear-1"><pre><code><span class="k">void</span> clear();</code></pre></div>

<p>Empties the selection; emits <code>on_changed</code> when it was not empty.</p>

#### `contains` {#function-contains}

<div class="function-declaration" id="function-contains-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> contains(
    <span class="k">const</span> <a href="../../../engine/core/uuid/uuid.html">core::uuid</a> &amp;guid) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-contains-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> contains(
    <a href="../../../engine/ecs/entity/entity.html">ecs::entity</a> entity) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the entity&#x27;s index is marked by the last <code><a href="#function-rebuild-1">rebuild</a>()</code>.</p>

#### `empty` {#function-empty}

<div class="function-declaration" id="function-empty-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> empty() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `guids` {#function-guids}

<div class="function-declaration" id="function-guids-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="../../../engine/core/uuid/uuid.html">core::uuid</a>&gt; guids() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `on_changed` {#function-on-changed}

<div class="function-declaration" id="function-on-changed-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/event/event.html">core::event</a>&lt;&gt; &amp;on_changed() <span class="k">noexcept</span>;</code></pre></div>

#### `primary` {#function-primary}

<div class="function-declaration" id="function-primary-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/uuid/uuid.html">core::uuid</a> primary() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The active entity: the guid the caller named last in the <code><a href="#function-set-1">set</a>()</code> or <code><a href="#function-add-1">add</a>()</code> call that last changed it; when that guid leaves the selection (<code><a href="#function-remove-1">remove</a>()</code>, <code><a href="#function-rebuild-1">rebuild</a>()</code>), the lowest remaining guid; the <code>null</code> uuid when the selection is empty.</p>

#### `rebuild` {#function-rebuild}

<div class="function-declaration" id="function-rebuild-1"><pre><code><span class="k">void</span> rebuild(
    <span class="k">const</span> scene::scene &amp;target);</code></pre></div>

<p>Rebuilds the entity bit set from the live scene (after structural changes or a restore) and drops guids no live entity carries (a dropped primary falls back to the lowest remaining guid). Emits <code>on_changed</code> when guids were dropped.</p>

#### `remove` {#function-remove}

<div class="function-declaration" id="function-remove-1"><pre><code><span class="k">void</span> remove(
    std::span&lt;<span class="k">const</span> <a href="../../../engine/core/uuid/uuid.html">core::uuid</a>&gt; guids);</code></pre></div>

<p>Removes guids; when the primary leaves, the lowest remaining guid takes its place. Emits <code>on_changed</code> when anything was removed.</p>

#### `set` {#function-set}

<div class="function-declaration" id="function-set-1"><pre><code><span class="k">void</span> set(
    std::span&lt;<span class="k">const</span> <a href="../../../engine/core/uuid/uuid.html">core::uuid</a>&gt; guids);</code></pre></div>

<p>Replaces the selection. Duplicates and <code>null</code> guids are dropped; the last non-<code>null</code> guid in the caller&#x27;s order becomes the primary. Emits <code>on_changed</code> when the resulting set or the primary differs from the current one.</p>

#### `size` {#function-size}

<div class="function-declaration" id="function-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-usize">usize</a> size() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

### Private

#### `refresh_primary` {#function-refresh-primary}

<div class="function-declaration" id="function-refresh-primary-1"><pre><code><span class="k">void</span> refresh_primary() <span class="k">noexcept</span>;</code></pre></div>

<p>After guids left the selection: keeps the primary while it is still selected, otherwise falls back to the lowest remaining guid (<code>null</code> when empty).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
