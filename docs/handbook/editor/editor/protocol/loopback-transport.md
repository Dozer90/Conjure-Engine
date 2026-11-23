---
kind: header
---

[Editor](../../../editor.md) → [Protocol](../../protocol.md)

# loopback_transport.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/protocol/loopback_transport.h` |
| API ↗ | [The AI control protocol](../../../../api/editor/ai-protocol.md) |

<p>The in-process transport: tests and the AI session panel connect a <code><a href="loopback-transport/loopback-client.html">loopback_client</a></code>, write lines into it and read the responses and events back, all on the main thread (a mutex still guards the queues so a test may drive a client from a job).</p>



## Classes

| File | Description |
|---|---|
| [`loopback_client`](loopback-transport/loopback-client.md) | One connected in-process client. |
| [`loopback_transport`](loopback-transport/loopback-transport.md) |  |

## Forward Declarations

### `loopback_client::state` {#symbol-loopback-client-state}

```cpp
struct state;
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
<a id="loopback_client"></a>
<a id="loopback_clientclient_id"></a>
<a id="loopback_clientdisconnect"></a>
<a id="loopback_clientis_connected"></a>
<a id="loopback_clientloopback_client"></a>
<a id="loopback_clientloopback_client_1"></a>
<a id="loopback_clientloopback_transport"></a>
<a id="loopback_clientmstate"></a>
<a id="loopback_clientoperator"></a>
<a id="loopback_clientpending"></a>
<a id="loopback_clientreceive"></a>
<a id="loopback_clientreceive_all"></a>
<a id="loopback_clientsend"></a>
<a id="loopback_clientsend_raw"></a>
<a id="loopback_clientstate"></a>
<a id="loopback_transport"></a>
<a id="loopback_transportclient_count"></a>
<a id="loopback_transportconnect"></a>
<a id="loopback_transportdisconnect"></a>
<a id="loopback_transportfind"></a>
<a id="loopback_transportflush"></a>
<a id="loopback_transportkind"></a>
<a id="loopback_transportloopback_transport"></a>
<a id="loopback_transportloopback_transport_1"></a>
<a id="loopback_transportmclients"></a>
<a id="loopback_transportmlinelimit"></a>
<a id="loopback_transportmpendingconnects"></a>
<a id="loopback_transportmstarted"></a>
<a id="loopback_transportpending_bytes"></a>
<a id="loopback_transportpoll"></a>
<a id="loopback_transportsend"></a>
<a id="loopback_transportstart"></a>
<a id="loopback_transportstop"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
