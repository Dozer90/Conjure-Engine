---
kind: header
---

[Editor](../../editor.md) → [User Interface](../user-interface.md)

# console_buffer.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/console_buffer.h` |
| API ↗ | [Headless mode and the command model](../../../api/editor/headless-and-commands.md), [The editor](../../../api/editor/index.md) |

<p>The console ring: 65,536 entries and 16 MiB of text fed by a log sink that may be called from any thread, read on the main thread by the console panel, the <code>log</code> event and the <code>console.read</code> command. The mutex is never held across an allocation: the text ring is preallocated.</p>

## Structs

| File | Description |
|---|---|
| [`console_buffer::entry`](console-buffer/console-buffer-entry.md) |  |
| [`console_query`](console-buffer/console-query.md) | <code>A</code> filter for <code>read()</code>. |
| [`console_record`](console-buffer/console-record.md) | One captured line as readers see it (copies; the ring may overwrite the original meanwhile). |

## Classes

| File | Description |
|---|---|
| [`console_buffer`](console-buffer/console-buffer.md) |  |

## Forward Declarations

### `console_buffer::sink` {#symbol-console-buffer-sink}

```cpp
class sink;
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
<a id="console_buffer"></a>
<a id="console_bufferclear"></a>
<a id="console_bufferconsole_buffer"></a>
<a id="console_bufferconsole_buffer_1"></a>
<a id="console_bufferdefault_entry_capacity"></a>
<a id="console_bufferdefault_text_capacity"></a>
<a id="console_bufferentry"></a>
<a id="console_bufferentry_capacity"></a>
<a id="console_bufferentrycategorylength"></a>
<a id="console_bufferentryframe"></a>
<a id="console_bufferentrylevel"></a>
<a id="console_bufferentryseq"></a>
<a id="console_bufferentrytextlength"></a>
<a id="console_bufferentrytextoffset"></a>
<a id="console_bufferinstall_sink"></a>
<a id="console_bufferlast_seq"></a>
<a id="console_buffermdropped"></a>
<a id="console_buffermentries"></a>
<a id="console_buffermentrycount"></a>
<a id="console_buffermentryhead"></a>
<a id="console_buffermframe"></a>
<a id="console_buffermmutex"></a>
<a id="console_buffermnextseq"></a>
<a id="console_buffermsink"></a>
<a id="console_buffermtext"></a>
<a id="console_buffermtexthead"></a>
<a id="console_bufferoperator"></a>
<a id="console_bufferpush"></a>
<a id="console_bufferpush_locked"></a>
<a id="console_bufferread"></a>
<a id="console_bufferremove_sink"></a>
<a id="console_bufferset_frame"></a>
<a id="console_buffersink"></a>
<a id="console_buffersize"></a>
<a id="console_buffertext_at"></a>
<a id="console_buffertext_capacity"></a>
<a id="console_query"></a>
<a id="console_querycategory"></a>
<a id="console_querylimit"></a>
<a id="console_queryminlevel"></a>
<a id="console_querynewest"></a>
<a id="console_querysinceseq"></a>
<a id="console_record"></a>
<a id="console_recordcategory"></a>
<a id="console_recordframe"></a>
<a id="console_recordlevel"></a>
<a id="console_recordseq"></a>
<a id="console_recordtext"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
