---
kind: header
---

[Engine](../../engine.md) → [Assets](../assets.md)

# cooked_blob.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/assets/cooked_blob.h` |
| API ↗ | [Cooking, caches and packs](../../../api/assets/cooking-and-packs.md) |

<p><code>A</code> blob with its bytes. Owns everything it holds.</p>

## Structs

| File | Description |
|---|---|
| [`cooked_blob`](cooked-blob/cooked-blob.md) | <code>A</code> blob with its bytes. Owns everything it holds. |
| [`cooked_blob_header`](cooked-blob/cooked-blob-header.md) | Fixed header of a cooked blob. |

## Functions

### Public

#### `hash_bytes` {#function-hash-bytes}

<div class="function-declaration" id="function-hash-bytes-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/types.html#symbol-u64">u64</a> hash_bytes(
    std::span&lt;<span class="k">const</span> std::byte&gt; bytes) <span class="k">noexcept</span>;</code></pre></div>

<p>64-bit content hash used for payloads, source files and pack contents.</p>

#### `make_cooked_blob` {#function-make-cooked-blob}

<div class="function-declaration" id="function-make-cooked-blob-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="cooked-blob/cooked-blob.html">cooked_blob</a> make_cooked_blob(
    <a href="cooked-blob/cooked-blob-header.html">cooked_blob_header</a> header,
    std::vector&lt;<a href="asset-types.html#symbol-asset-id">asset_id</a>&gt; dependencies,
    std::vector&lt;std::byte&gt; payload);</code></pre></div>

<p>Builds a consistent blob from its parts: <code>payloadHash</code>, <code>payloadSize</code> and <code>dependencyCount</code> are derived from the payload and the dependency list.</p>

#### `parse_cooked_blob` {#function-parse-cooked-blob}

<div class="function-declaration" id="function-parse-cooked-blob-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<a href="cooked-blob/cooked-blob.html">cooked_blob</a>&gt; parse_cooked_blob(
    std::span&lt;<span class="k">const</span> std::byte&gt; bytes);</code></pre></div>

<p>Parses and validates a blob: magic, format version, declared sizes against the buffer and the payload hash. Anything wrong is <code>CACHE_CORRUPT</code>.</p>

<div class="function-declaration" id="function-parse-cooked-blob-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<a href="cooked-blob/cooked-blob.html">cooked_blob</a>&gt; parse_cooked_blob(
    std::vector&lt;std::byte&gt; &amp;&amp;bytes);</code></pre></div>

<p>Same validation as the span overload; the buffer becomes the payload, with no second allocation (the bytes in front of the payload are shifted out in place).</p>

#### `parse_cooked_blob_header` {#function-parse-cooked-blob-header}

<div class="function-declaration" id="function-parse-cooked-blob-header-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<a href="cooked-blob/cooked-blob-header.html">cooked_blob_header</a>&gt; parse_cooked_blob_header(
    std::span&lt;<span class="k">const</span> std::byte&gt; bytes);</code></pre></div>

<p>Parses only the header. <code>CACHE_CORRUPT</code> for a short buffer, bad magic or unknown format.</p>

#### `write_cooked_blob` {#function-write-cooked-blob}

<div class="function-declaration" id="function-write-cooked-blob-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::vector&lt;std::byte&gt; write_cooked_blob(
    <span class="k">const</span> <a href="cooked-blob/cooked-blob.html">cooked_blob</a> &amp;blob);</code></pre></div>

<p>Serialises a blob. The header&#x27;s derived fields are recomputed first so the output is always self-consistent.</p>

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
<a id="cooked_blob"></a>
<a id="cooked_blob_header"></a>
<a id="cooked_blob_headerdependencycount"></a>
<a id="cooked_blob_headerflags"></a>
<a id="cooked_blob_headerformatversion"></a>
<a id="cooked_blob_headerid"></a>
<a id="cooked_blob_headerimporterversion"></a>
<a id="cooked_blob_headermagic"></a>
<a id="cooked_blob_headerpayload_alignment"></a>
<a id="cooked_blob_headerpayload_offset"></a>
<a id="cooked_blob_headerpayloadhash"></a>
<a id="cooked_blob_headerpayloadsize"></a>
<a id="cooked_blob_headersettingshash"></a>
<a id="cooked_blob_headersize"></a>
<a id="cooked_blob_headersourcehash"></a>
<a id="cooked_blob_headertotal_size"></a>
<a id="cooked_blob_headertype"></a>
<a id="cooked_blob_headertypeversion"></a>
<a id="cooked_blobdependencies"></a>
<a id="cooked_blobheader"></a>
<a id="cooked_blobpayload"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="hash_bytes"></a>
<a id="internals-and-algorithms"></a>
<a id="make_cooked_blob"></a>
<a id="parse_cooked_blob"></a>
<a id="parse_cooked_blob_header"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<a id="write_cooked_blob"></a>
<!-- doc-legacy-anchors:end -->
