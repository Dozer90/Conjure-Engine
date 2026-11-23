---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# slot_key_format.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/containers/slot_key_format.h` |
| API ↗ | [Containers](../../../../api/core/containers.md) |

<p>Text form of <code><a href="slot-map/slot-key.html">slot_key</a></code>: <code>to_string</code>, <code>parse_slot_key</code> and the <code><a href="slot-key-format/std-formatter-conjure-core-slot-key.html">std::formatter</a></code> specialisation. Kept apart from <code><a href="slot-map/slot-map.html">slot_map</a>.h</code> so code that only stores and looks up handles does not pay for &lt;format&gt; and the string headers.</p>

## Structs

| File | Description |
|---|---|
| [`std::formatter<conjure::core::slot_key>`](slot-key-format/std-formatter-conjure-core-slot-key.md) |  |

## Functions

### Public

#### `parse_slot_key` {#function-parse-slot-key}

<div class="function-declaration" id="function-parse-slot-key-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::optional&lt;<a href="slot-map/slot-key.html">slot_key</a>&gt; parse_slot_key(
    std::string_view text) <span class="k">noexcept</span>;</code></pre></div>

<p>Parses the <code><a href="#function-to-string-1">to_string</a>()</code> form. Returns nullopt for anything else.</p>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string to_string(
    <a href="slot-map/slot-key.html">slot_key</a> key);</code></pre></div>

<p>&quot;index:generation&quot; in decimal, e.g. &quot;12:3&quot;. The <code>null</code> key prints as &quot;0:0&quot;.</p>

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
<a id="parse_slot_key"></a>
<a id="std"></a>
<a id="stdformat"></a>
<a id="stdformatterconjurecoreslot_key"></a>
<a id="stdformatterconjurecoreslot_keyformat"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
