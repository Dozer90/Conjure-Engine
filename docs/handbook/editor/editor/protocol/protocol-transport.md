---
kind: header
---

[Editor](../../../editor.md) → [Protocol](../../protocol.md)

# protocol_transport.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/protocol/protocol_transport.h` |
| API ↗ | [The AI control protocol](../../../../api/editor/ai-protocol.md) |

<p><code>A</code> transport moves complete lines between clients and the protocol server on the main thread: <code>poll()</code> reports connects, lines and disconnects since the last call; <code>send()</code> queues a line; <code>flush()</code> writes what the peer accepts without blocking. Client ids come from the server through the allocator so every transport&#x27;s clients share one id space.</p>

## Structs

| File | Description |
|---|---|
| [`inbound_line`](protocol-transport/inbound-line.md) | One complete line received from a client. <code>oversized</code> marks a line that exceeded the limit (text is then empty): the server answers <code>MESSAGE_TOO_LARGE</code> and disconnects. |

## Classes

| File | Description |
|---|---|
| [`protocol_transport`](protocol-transport/protocol-transport.md) |  |

## Enumerations

### `transport_kind` {#symbol-transport-kind}

```cpp
enum class transport_kind : u8 {
    LOCAL = 0,          // the UI (client 0, no transport)
    TCP,
    STDIO,
    LOOPBACK
};
```

## Type Aliases

### `client_id_allocator` {#symbol-client-id-allocator}

```cpp
using client_id_allocator = std::function<u32()>;
```

Hands out client ids.

## Functions

### Public

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="protocol-transport.html#symbol-transport-kind">transport_kind</a> kind) <span class="k">noexcept</span>;</code></pre></div>

<p>The wire name of a kind (&quot;tcp&quot;).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="client_id_allocator"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="inbound_line"></a>
<a id="inbound_lineclientid"></a>
<a id="inbound_lineoversized"></a>
<a id="inbound_linetext"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="protocol_transport"></a>
<a id="protocol_transportallocate_client_id"></a>
<a id="protocol_transportclient_count"></a>
<a id="protocol_transportdisconnect"></a>
<a id="protocol_transportflush"></a>
<a id="protocol_transportkind"></a>
<a id="protocol_transportmallocator"></a>
<a id="protocol_transportmnextlocalid"></a>
<a id="protocol_transportoperator"></a>
<a id="protocol_transportpending_bytes"></a>
<a id="protocol_transportpoll"></a>
<a id="protocol_transportprotocol_transport"></a>
<a id="protocol_transportprotocol_transport_1"></a>
<a id="protocol_transportsend"></a>
<a id="protocol_transportset_client_id_allocator"></a>
<a id="protocol_transportstart"></a>
<a id="protocol_transportstop"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="transport_kind"></a>
<a id="transport_kindlocal"></a>
<a id="transport_kindloopback"></a>
<a id="transport_kindstdio"></a>
<a id="transport_kindtcp"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
