---
kind: header
---

[Engine](../../engine.md) → [Core](../core.md)

# string_id.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/string_id.h` |
| API ↗ | [Containers](../../../api/core/containers.md), [Strings and identifiers](../../../api/core/strings-and-identifiers.md) |

<p><code>A</code> string reduced to its 64-bit FNV-1a hash. Trivially copyable and comparable by hash; the original spelling is available through <code>view()</code>/<code>c_str()</code> once the id has been interned. Meant for bounded vocabularies: an interned name is never freed, so unique per-spawn names grow memory for the life of the process (see the header comment).</p>

## Structs

| File | Description |
|---|---|
| [`std::formatter<conjure::core::string_id>`](string-id/std-formatter-conjure-core-string-id.md) |  |
| [`std::hash<conjure::core::string_id>`](string-id/std-hash-conjure-core-string-id.md) |  |

## Classes

| File | Description |
|---|---|
| [`string_id`](string-id/string-id.md) | <code>A</code> string reduced to its 64-bit FNV-1a hash. Trivially copyable and comparable by hash; the original spelling is available through <code>view()</code>/<code>c_str()</code> once the id has been interned. Meant for bounded vocabularies: an interned name is never freed, so unique per-spawn names grow memory for the life of the process (see the header comment). |

## Functions

### Public

#### `detail::find_string_id_name` {#function-detail-find-string-id-name}

<div class="function-declaration" id="function-detail-find-string-id-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view find_string_id_name(<a href="types.html#symbol-u64">u64</a> hash) <span class="k">noexcept</span>;</code></pre></div>

<p>The registered spelling of <code>hash</code>, or an empty, <code>null</code>-terminated view when unknown. The returned view stays valid for the lifetime of the process. Thread-safe.</p>

#### `detail::register_string_id_name` {#function-detail-register-string-id-name}

<div class="function-declaration" id="function-detail-register-string-id-name-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> register_string_id_name(<a href="types.html#symbol-u64">u64</a> hash, std::string_view name);</code></pre></div>

<p>Records <code>name</code> as the spelling of <code>hash</code>. Interning a different name under a hash that is already registered is a collision: it asserts, logs an ERROR on <code>LOG_CORE</code> and keeps the first name. Thread-safe. Exposed so tests and tooling can exercise the table directly.</p>

#### `detail::string_id_name_count` {#function-detail-string-id-name-count}

<div class="function-declaration" id="function-detail-string-id-name-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="types.html#symbol-usize">usize</a> string_id_name_count() <span class="k">noexcept</span>;</code></pre></div>

<p>Number of distinct hashes with a registered name. Thread-safe.</p>

#### `literals::operator""_sid` {#function-literals-operator--sid}

<div class="function-declaration" id="function-literals-operator--sid-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">consteval</span> <a href="string-id/string-id.html">string_id</a> <span class="k">operator</span><span class="s">""</span>_sid(<span class="k">const</span> <span class="k">char</span> *text, <a href="types.html#symbol-usize">usize</a> length);</code></pre></div>

<p>&quot;<code>ecs.component</code>&quot;<code>_sid</code>: a <code><a href="string-id/string-id.html">string_id</a></code> from a literal. Always evaluated at compile time, so it never interns the name and never takes the table mutex; <code>intern()</code> the name once elsewhere if tools must be able to resolve it.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="detailfind_string_id_name"></a>
<a id="detailregister_string_id_name"></a>
<a id="detailstring_id_name_count"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="find_string_id_name"></a>
<a id="internals-and-algorithms"></a>
<a id="literalsoperator_sid"></a>
<a id="operator_sid"></a>
<a id="register_string_id_name"></a>
<a id="std"></a>
<a id="stdformat"></a>
<a id="stdformatterconjurecorestring_id"></a>
<a id="stdformatterconjurecorestring_idformat"></a>
<a id="stdhashconjurecorestring_id"></a>
<a id="stdhashconjurecorestring_idoperator"></a>
<a id="stdoperator"></a>
<a id="string_id"></a>
<a id="string_id_name_count"></a>
<a id="string_idc_str"></a>
<a id="string_idfrom_hash"></a>
<a id="string_idhash"></a>
<a id="string_idintern"></a>
<a id="string_idinterned_count"></a>
<a id="string_idis_null"></a>
<a id="string_idmhash"></a>
<a id="string_idnull"></a>
<a id="string_idoperator"></a>
<a id="string_idoperator_1"></a>
<a id="string_idstring_id"></a>
<a id="string_idto_string"></a>
<a id="string_idview"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
