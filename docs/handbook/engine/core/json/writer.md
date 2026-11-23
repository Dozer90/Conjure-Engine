---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# writer.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/json/writer.h` |
| API ↗ | [JSON](../../../../api/core/json.md) |

<p>Streaming JSON writer.</p>

## Structs

| File | Description |
|---|---|
| [`json::writer::frame`](writer/json-writer-frame.md) |  |

## Classes

| File | Description |
|---|---|
| [`json::writer`](writer/json-writer.md) | Builds minified JSON text into a <code>std::string</code> without an intermediate document. |

## Enumerations

### `json::writer::scope` {#symbol-json-writer-scope}

```cpp
enum class scope : u8 {
    OBJECT,
    ARRAY
};
```

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
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="jsonwriter"></a>
<a id="jsonwriterappend_string"></a>
<a id="jsonwriterbegin_array"></a>
<a id="jsonwriterbegin_object"></a>
<a id="jsonwriterbegin_value"></a>
<a id="jsonwriterdepth"></a>
<a id="jsonwriterend_array"></a>
<a id="jsonwriterend_object"></a>
<a id="jsonwriterfinish"></a>
<a id="jsonwriterframe"></a>
<a id="jsonwriterframeexpectkey"></a>
<a id="jsonwriterframehasitems"></a>
<a id="jsonwriterframekind"></a>
<a id="jsonwriteris_complete"></a>
<a id="jsonwriterkey"></a>
<a id="jsonwritermbuffer"></a>
<a id="jsonwritermrootwritten"></a>
<a id="jsonwritermstack"></a>
<a id="jsonwriteroperator"></a>
<a id="jsonwriterreset"></a>
<a id="jsonwriterscope"></a>
<a id="jsonwriterscopearray"></a>
<a id="jsonwriterscopeobject"></a>
<a id="jsonwritertext"></a>
<a id="jsonwritervalue"></a>
<a id="jsonwriterwrite_raw_json"></a>
<a id="jsonwriterwriter"></a>
<a id="jsonwriterwriter_1"></a>
<a id="other-declarations"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<a id="writer"></a>
<a id="writerappend_string"></a>
<a id="writerbegin_array"></a>
<a id="writerbegin_object"></a>
<a id="writerbegin_value"></a>
<a id="writerdepth"></a>
<a id="writerend_array"></a>
<a id="writerend_object"></a>
<a id="writerfinish"></a>
<a id="writerframe"></a>
<a id="writerframeexpectkey"></a>
<a id="writerframehasitems"></a>
<a id="writerframekind"></a>
<a id="writeris_complete"></a>
<a id="writerkey"></a>
<a id="writermbuffer"></a>
<a id="writermrootwritten"></a>
<a id="writermstack"></a>
<a id="writeroperator"></a>
<a id="writerreset"></a>
<a id="writerscope"></a>
<a id="writerscopearray"></a>
<a id="writerscopeobject"></a>
<a id="writertext"></a>
<a id="writervalue"></a>
<a id="writerwrite_raw_json"></a>
<a id="writerwriter"></a>
<a id="writerwriter_1"></a>
<!-- doc-legacy-anchors:end -->
