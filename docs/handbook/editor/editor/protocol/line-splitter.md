---
kind: header
---

[Editor](../../../editor.md) → [Protocol](../../protocol.md)

# line_splitter.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/protocol/line_splitter.h` |
| API ↗ | [The AI control protocol](../../../../api/editor/ai-protocol.md) |

<p>Newline framing: bytes arrive in arbitrary chunks; complete lines come out one at a time with a trailing &#x27;\r&#x27; removed. <code>A</code> single line that grows past the limit without a newline is replaced by an <code>OVERSIZED</code> marker at its position in the stream and sets the overflow flag: the transport answers <code>MESSAGE_TOO_LARGE</code> and disconnects the client; the rest of the oversized line is discarded until the next newline. The limit applies to one line only, never to the complete lines that were framed but not yet taken.</p>

## Structs

| File | Description |
|---|---|
| [`line_splitter::framed_line`](line-splitter/line-splitter-framed-line.md) |  |

## Classes

| File | Description |
|---|---|
| [`line_splitter`](line-splitter/line-splitter.md) |  |

## Enumerations

### `line_splitter::frame` {#symbol-line-splitter-frame}

```cpp
enum class frame : u8 {
    NONE = 0,
    LINE,
    OVERSIZED
};
```

What <code>next_frame()</code> hands out: a complete line, the marker of a line that exceeded the limit, or nothing yet.

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
<a id="line_splitter"></a>
<a id="line_splitterfeed"></a>
<a id="line_splitterframe"></a>
<a id="line_splitterframe_count"></a>
<a id="line_splitterframed_line"></a>
<a id="line_splitterframed_lineoversized"></a>
<a id="line_splitterframed_linetext"></a>
<a id="line_splitterframeline"></a>
<a id="line_splitterframenone"></a>
<a id="line_splitterframeoversized"></a>
<a id="line_splitterlimit"></a>
<a id="line_splitterline_splitter"></a>
<a id="line_splitterline_splitter_1"></a>
<a id="line_splittermcurrent"></a>
<a id="line_splittermdiscarding"></a>
<a id="line_splittermframes"></a>
<a id="line_splittermlimit"></a>
<a id="line_splittermoverflowed"></a>
<a id="line_splitternext_frame"></a>
<a id="line_splitternext_line"></a>
<a id="line_splitteroperator"></a>
<a id="line_splitteroverflowed"></a>
<a id="line_splitterpending_bytes"></a>
<a id="line_splitterreset"></a>
<a id="other-declarations"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
