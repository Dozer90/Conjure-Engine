---
kind: class
---

[Editor](../../../../editor.md) → [User Interface](../../../user-interface.md) → [hierarchy_cache.h](../hierarchy-cache.md)

# ui::hierarchy_cache
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor::ui` |
| Include File Path | `engine/include/conjure/editor/ui/hierarchy_cache.h` |
| Source File Path | `engine/src/editor/ui/hierarchy_cache.cpp` |
| API ↗ | [The workspace UI](../../../../../api/editor/workspace-ui.md) |

## Syntax

```cpp
class CONJURE_API hierarchy_cache
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">hierarchy_cache();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Additional constructor overloads have been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~hierarchy_cache();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mDirty</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">true</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mExpanded</code></td><td><code class="signature">std::<wbr>unordered_<wbr>set&lt;<wbr><a href="../../../../engine/core/uuid/uuid.html">core::<wbr>uuid</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFilter</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">""</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mMatches</code></td><td><code class="signature"><a href="../../../../engine/core/bit-vector/bit-vector.html">core::bit_vector</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRebuilds</code></td><td><code class="signature"><a href="../../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRows</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="ui-hierarchy-row.html">hierarchy_<wbr>row</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Structure<wbr>Version</code></td><td><code class="signature"><a href="../../../../engine/core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `filter` {#function-filter}

<div class="function-declaration" id="function-filter-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::string &amp;filter() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `invalidate` {#function-invalidate}

<div class="function-declaration" id="function-invalidate-1"><pre><code><span class="k">void</span> invalidate() <span class="k">noexcept</span>;</code></pre></div>

<p>Forces a rebuild at the next refresh (selection changes).</p>

#### `is_expanded` {#function-is-expanded}

<div class="function-declaration" id="function-is-expanded-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_expanded(
    <span class="k">const</span> scene::scene &amp;target,
    <a href="../../../../engine/ecs/entity/entity.html">ecs::entity</a> entity) <span class="k">const</span>;</code></pre></div>

<p>True when the entity&#x27;s guid is marked expanded (<code>false</code> for a <code>null</code> entity).</p>

#### `rebuild_count` {#function-rebuild-count}

<div class="function-declaration" id="function-rebuild-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../engine/core/types.html#symbol-u32">u32</a> rebuild_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `refresh_if_dirty` {#function-refresh-if-dirty}

<div class="function-declaration" id="function-refresh-if-dirty-1"><pre><code><span class="k">bool</span> refresh_if_dirty(
    <span class="k">const</span> scene::scene &amp;target,
    <span class="k">const</span> <a href="../../selection-set/selection-set.html">selection_set</a> &amp;selection);</code></pre></div>

<p>Rebuilds when the scene&#x27;s structure version, the expansion set or the filter changed since the last call. Returns <code>true</code> when a rebuild happened.</p>

#### `refresh_labels` {#function-refresh-labels}

<div class="function-declaration" id="function-refresh-labels-1"><pre><code><span class="k">void</span> refresh_labels(
    <span class="k">const</span> scene::scene &amp;target,
    <a href="../../../../engine/core/types.html#symbol-usize">usize</a> first,
    <a href="../../../../engine/core/types.html#symbol-usize">usize</a> last) <span class="k">noexcept</span>;</code></pre></div>

<p>Re-reads the names and enabled state of the rows already built in [first, last) (<code>O(range)</code>, no rebuild): renames and enable toggles do not change the structure version. Rows of entities that no longer exist are left as they are; <code>last</code> is clamped to the row count.</p>

#### `reveal` {#function-reveal}

<div class="function-declaration" id="function-reveal-1"><pre><code><span class="k">void</span> reveal(
    <span class="k">const</span> scene::scene &amp;target,
    <a href="../../../../engine/ecs/entity/entity.html">ecs::entity</a> entity);</code></pre></div>

<p>Expands every ancestor of the entity so its row is visible.</p>

#### `rows` {#function-rows}

<div class="function-declaration" id="function-rows-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="ui-hierarchy-row.html">hierarchy_row</a>&gt; rows() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `set_expanded` {#function-set-expanded}

<div class="function-declaration" id="function-set-expanded-1"><pre><code><span class="k">void</span> set_expanded(
    <span class="k">const</span> scene::scene &amp;target,
    <a href="../../../../engine/ecs/entity/entity.html">ecs::entity</a> entity,
    <span class="k">bool</span> expanded);</code></pre></div>

<p>Expands or collapses an entity&#x27;s row. The state is keyed by the entity&#x27;s guid, so it follows the entity through a snapshot restore and is never inherited by an entity that reuses the index. <code>A</code> <code>null</code> entity or one without a guid is ignored.</p>

#### `set_filter` {#function-set-filter}

<div class="function-declaration" id="function-set-filter-1"><pre><code><span class="k">void</span> set_filter(
    std::string_view filter);</code></pre></div>

<p>Case-insensitive substring filter on names; rows whose subtree holds a match are kept.</p>

#### `structure_version` {#function-structure-version}

<div class="function-declaration" id="function-structure-version-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../engine/core/types.html#symbol-u64">u64</a> structure_version() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

### Private

#### `compute_matches` {#function-compute-matches}

<div class="function-declaration" id="function-compute-matches-1"><pre><code><span class="k">void</span> compute_matches(
    <span class="k">const</span> scene::scene &amp;target);</code></pre></div>

#### `rebuild` {#function-rebuild}

<div class="function-declaration" id="function-rebuild-1"><pre><code><span class="k">void</span> rebuild(
    <span class="k">const</span> scene::scene &amp;target,
    <span class="k">const</span> <a href="../../selection-set/selection-set.html">selection_set</a> &amp;selection);</code></pre></div>

#### `subtree_matches` {#function-subtree-matches}

<div class="function-declaration" id="function-subtree-matches-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> subtree_matches(
    <span class="k">const</span> scene::scene &amp;target,
    <a href="../../../../engine/ecs/entity/entity.html">ecs::entity</a> entity) <span class="k">const</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
