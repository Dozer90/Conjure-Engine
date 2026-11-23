---
kind: header
---

[Engine](../../engine.md) → [Core](../core.md)

# hash.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/hash.h` |
| API ↗ | [Strings and identifiers](../../../api/core/strings-and-identifiers.md) |

<p>FNV-1a hashing. These are the engine&#x27;s canonical content hashes: <code><a href="string-id/string-id.html">string_id</a></code>, error categories, asset identity and the <code>C</code># NameHash all use them, so the constants and the exact <code><a href="types.html#symbol-byte">byte</a></code> order are part of the ABI and must never change. Test vectors live in tests/core/foundation/<code>test_hash.cpp</code> and are mirrored by <code>Conjure.Managed</code>.</p>

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-hash::FNV1A32_OFFSET_BASIS"></a><code>hash::FNV1A32_OFFSET_BASIS</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">0</span>x811C9DC5u</code> |  |
| <a id="variable-hash::FNV1A32_PRIME"></a><code>hash::FNV1A32_PRIME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">0</span>x01000193u</code> |  |
| <a id="variable-hash::FNV1A64_OFFSET_BASIS"></a><code>hash::FNV1A64_OFFSET_BASIS</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="types.html#symbol-u64">u64</a></code> | <code class="signature"><span class="mi">0</span>xCBF29CE484222325ull</code> |  |
| <a id="variable-hash::FNV1A64_PRIME"></a><code>hash::FNV1A64_PRIME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="types.html#symbol-u64">u64</a></code> | <code class="signature"><span class="mi">0</span>x00000100000001B3ull</code> |  |

## Functions

### Public

#### `hash::combine` {#function-hash-combine}

<div class="function-declaration" id="function-hash-combine-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="types.html#symbol-u64">u64</a> combine(<a href="types.html#symbol-u64">u64</a> seed, <a href="types.html#symbol-u64">u64</a> value) <span class="k">noexcept</span>;</code></pre></div>

<p>Combines two 64-bit hashes order-dependently (<code>boost::hash_combine</code> style with a 64-bit mix).</p>

#### `hash::fnv1a32` {#function-hash-fnv1a32}

<div class="function-declaration" id="function-hash-fnv1a32-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="types.html#symbol-u32">u32</a> fnv1a32(std::string_view text, <a href="types.html#symbol-u32">u32</a> seed = FNV1A32_OFFSET_BASIS) <span class="k">noexcept</span>;</code></pre></div>

<p>32-bit FNV-1a of the UTF-8 bytes of a string. Usable in constant expressions.</p>

<div class="function-declaration" id="function-hash-fnv1a32-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="types.html#symbol-u32">u32</a> fnv1a32(std::span&lt;<span class="k">const</span> std::byte&gt; bytes, <a href="types.html#symbol-u32">u32</a> seed = FNV1A32_OFFSET_BASIS) <span class="k">noexcept</span>;</code></pre></div>

<p>32-bit FNV-1a of a <code><a href="types.html#symbol-byte">byte</a></code> range.</p>

#### `hash::fnv1a32_step` {#function-hash-fnv1a32-step}

<div class="function-declaration" id="function-hash-fnv1a32-step-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="types.html#symbol-u32">u32</a> fnv1a32_step(<a href="types.html#symbol-u32">u32</a> hash, <a href="types.html#symbol-u8">u8</a> byteValue) <span class="k">noexcept</span>;</code></pre></div>

<p>Continues a 32-bit FNV-1a hash with one more <code><a href="types.html#symbol-byte">byte</a></code>.</p>

#### `hash::fnv1a64` {#function-hash-fnv1a64}

<div class="function-declaration" id="function-hash-fnv1a64-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="types.html#symbol-u64">u64</a> fnv1a64(std::string_view text, <a href="types.html#symbol-u64">u64</a> seed = FNV1A64_OFFSET_BASIS) <span class="k">noexcept</span>;</code></pre></div>

<p>64-bit FNV-1a of the UTF-8 bytes of a string. Usable in constant expressions.</p>

<div class="function-declaration" id="function-hash-fnv1a64-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="types.html#symbol-u64">u64</a> fnv1a64(std::span&lt;<span class="k">const</span> std::byte&gt; bytes, <a href="types.html#symbol-u64">u64</a> seed = FNV1A64_OFFSET_BASIS) <span class="k">noexcept</span>;</code></pre></div>

<p>64-bit FNV-1a of a <code><a href="types.html#symbol-byte">byte</a></code> range.</p>

#### `hash::fnv1a64_step` {#function-hash-fnv1a64-step}

<div class="function-declaration" id="function-hash-fnv1a64-step-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="types.html#symbol-u64">u64</a> fnv1a64_step(<a href="types.html#symbol-u64">u64</a> hash, <a href="types.html#symbol-u8">u8</a> byteValue) <span class="k">noexcept</span>;</code></pre></div>

<p>Continues a 64-bit FNV-1a hash with one more <code><a href="types.html#symbol-byte">byte</a></code>.</p>

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
<a id="combine"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="fnv1a32"></a>
<a id="fnv1a32_offset_basis"></a>
<a id="fnv1a32_prime"></a>
<a id="fnv1a32_step"></a>
<a id="fnv1a64"></a>
<a id="fnv1a64_offset_basis"></a>
<a id="fnv1a64_prime"></a>
<a id="fnv1a64_step"></a>
<a id="hashcombine"></a>
<a id="hashfnv1a32"></a>
<a id="hashfnv1a32_offset_basis"></a>
<a id="hashfnv1a32_prime"></a>
<a id="hashfnv1a32_step"></a>
<a id="hashfnv1a64"></a>
<a id="hashfnv1a64_offset_basis"></a>
<a id="hashfnv1a64_prime"></a>
<a id="hashfnv1a64_step"></a>
<a id="internals-and-algorithms"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
