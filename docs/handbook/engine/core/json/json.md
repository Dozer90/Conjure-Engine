---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# json.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/json/json.h` |
| API ↗ | [JSON](../../../../api/core/json.md) |

<p>Umbrella header for the json area. Include this unless you need only one of the pieces.</p>

## Functions

### Public

#### `json::parse` {#function-json-parse}

<div class="function-declaration" id="function-json-parse-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../result/result.html">result</a>&lt;<a href="document/json-document.html">document</a>&gt; parse(std::string_view text);</code></pre></div>

<p>Parses strict JSON text into a new document. Same as <code>document::parse()</code>.</p>

#### `json::to_string` {#function-json-to-string}

<div class="function-declaration" id="function-json-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string to_string(<span class="k">const</span> <a href="value/json-value.html">value</a> &amp;source, <span class="k">bool</span> canonical = false);</code></pre></div>

<p>Serialises one value (any node of a document). Canonical output sorts object members by key; both forms are minified. An invalid value yields an empty string.</p>

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
<a id="jsonparse"></a>
<a id="jsonto_string"></a>
<a id="parse"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
