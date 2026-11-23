---
kind: header
---

[Engine](../../engine.md) → [Core](../core.md)

# radix_sort.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/radix_sort.h` |
| API ↗ | [Strings and identifiers](../../../api/core/strings-and-identifiers.md) |

<p>Stable least-significant-digit radix sort for unsigned keys with optional 32-bit payloads.</p>

## Functions

### Public

#### `radix_sort_u32` {#function-radix-sort-u32}

<div class="function-declaration" id="function-radix-sort-u32-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> radix_sort_u32(
    std::span&lt;<a href="types.html#symbol-u32">u32</a>&gt; keys,
    std::span&lt;<a href="types.html#symbol-u32">u32</a>&gt; values,
    std::span&lt;<a href="types.html#symbol-u32">u32</a>&gt; scratchKeys,
    std::span&lt;<a href="types.html#symbol-u32">u32</a>&gt; scratchValues) <span class="k">noexcept</span>;</code></pre></div>

<p>32-bit variant of <code>radix_sort_u64</code> with payloads: four passes at most.</p>

<div class="function-declaration" id="function-radix-sort-u32-2"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> radix_sort_u32(
    std::span&lt;<a href="types.html#symbol-u32">u32</a>&gt; keys,
    std::span&lt;<a href="types.html#symbol-u32">u32</a>&gt; scratchKeys) <span class="k">noexcept</span>;</code></pre></div>

<p>32-bit variant of <code>radix_sort_u64</code> for keys only.</p>

#### `radix_sort_u64` {#function-radix-sort-u64}

<div class="function-declaration" id="function-radix-sort-u64-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> radix_sort_u64(
    std::span&lt;<a href="types.html#symbol-u64">u64</a>&gt; keys,
    std::span&lt;<a href="types.html#symbol-u32">u32</a>&gt; values,
    std::span&lt;<a href="types.html#symbol-u64">u64</a>&gt; scratchKeys,
    std::span&lt;<a href="types.html#symbol-u32">u32</a>&gt; scratchValues) <span class="k">noexcept</span>;</code></pre></div>

<p>Sorts <code>keys</code> ascending and permutes <code>values</code> alongside them. Stable. <code>values</code> must have the same size as <code>keys</code>, both scratch spans must be at least that large and no two of the four spans may overlap (all asserted; the input is left untouched on violation). On return the sorted data is in <code>keys</code>/<code>values</code>; the scratch contents are unspecified.</p>

<div class="function-declaration" id="function-radix-sort-u64-2"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> radix_sort_u64(
    std::span&lt;<a href="types.html#symbol-u64">u64</a>&gt; keys,
    std::span&lt;<a href="types.html#symbol-u64">u64</a>&gt; scratchKeys) <span class="k">noexcept</span>;</code></pre></div>

<p>Sorts <code>keys</code> ascending. Stable. <code>scratchKeys</code> must be at least as large as <code>keys</code> and must not overlap it.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="radix_sort_u32"></a>
<a id="radix_sort_u64"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
