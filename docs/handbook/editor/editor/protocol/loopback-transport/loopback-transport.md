---
kind: class
---

[Editor](../../../../editor.md) → [Protocol](../../../protocol.md) → [loopback_transport.h](../loopback-transport.md)

# loopback_transport
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/protocol/loopback_transport.h` |
| Source File Path | `engine/src/editor/core/protocol/loopback_transport.cpp` |
| API ↗ | [The AI control protocol](../../../../../api/editor/ai-protocol.md) |

## Syntax

```cpp
class CONJURE_API loopback_transport final : public protocol_transport
```

## Inheritance Hierarchy


[`protocol_transport`](../protocol-transport/protocol-transport.md) → `loopback_transport`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> loopback_transport(
    <a href="../../../../engine/core/types.html#symbol-usize">usize</a> lineLimit = <span class="mi">0</span>);</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~loopback_transport() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mClients</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr>std::<wbr>shared_<wbr>ptr&lt;<wbr><a href="../loopback-transport.html">loopback_<wbr>client::<wbr>state</a>&gt;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mLineLimit</code></td><td><code class="signature"><a href="../../../../engine/core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Pending<wbr>Connects</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr>std::<wbr>shared_<wbr>ptr&lt;<wbr><a href="../loopback-transport.html">loopback_<wbr>client::<wbr>state</a>&gt;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mStarted</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `client_count` {#function-client-count}

<div class="function-declaration" id="function-client-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../engine/core/types.html#symbol-usize">usize</a> client_count() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `connect` {#function-connect}

<div class="function-declaration" id="function-connect-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="loopback-client.html">loopback_client</a> connect();</code></pre></div>

<p>Creates a client; the server accepts it at its next <code><a href="#function-poll-1">poll</a>()</code>.</p>

#### `disconnect` {#function-disconnect}

<div class="function-declaration" id="function-disconnect-1"><pre><code><span class="k">void</span> disconnect(
    <a href="../../../../engine/core/types.html#symbol-u32">u32</a> clientId) <span class="k">override</span>;</code></pre></div>

#### `flush` {#function-flush}

<div class="function-declaration" id="function-flush-1"><pre><code><span class="k">void</span> flush() <span class="k">override</span>;</code></pre></div>

#### `kind` {#function-kind}

<div class="function-declaration" id="function-kind-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../protocol-transport.html#symbol-transport-kind">transport_kind</a> kind() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `pending_bytes` {#function-pending-bytes}

<div class="function-declaration" id="function-pending-bytes-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../engine/core/types.html#symbol-usize">usize</a> pending_bytes(
    <a href="../../../../engine/core/types.html#symbol-u32">u32</a> clientId) <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `poll` {#function-poll}

<div class="function-declaration" id="function-poll-1"><pre><code><span class="k">void</span> poll(
    std::vector&lt;<a href="../../../../engine/core/types.html#symbol-u32">u32</a>&gt; &amp;outOpened,
    std::vector&lt;<a href="../protocol-transport/inbound-line.html">inbound_line</a>&gt; &amp;outLines,
    std::vector&lt;<a href="../../../../engine/core/types.html#symbol-u32">u32</a>&gt; &amp;outClosed) <span class="k">override</span>;</code></pre></div>

#### `send` {#function-send}

<div class="function-declaration" id="function-send-1"><pre><code><span class="k">void</span> send(
    <a href="../../../../engine/core/types.html#symbol-u32">u32</a> clientId,
    std::string_view line) <span class="k">override</span>;</code></pre></div>

#### `start` {#function-start}

<div class="function-declaration" id="function-start-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../engine/core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; start() <span class="k">override</span>;</code></pre></div>

#### `stop` {#function-stop}

<div class="function-declaration" id="function-stop-1"><pre><code><span class="k">void</span> stop() <span class="k">override</span>;</code></pre></div>

### Private

#### `find` {#function-find}

<div class="function-declaration" id="function-find-1"><pre><code>[[<span class="na">nodiscard</span>]] std::shared_ptr&lt;<a href="../loopback-transport.html">loopback_client::state</a>&gt; find(
    <a href="../../../../engine/core/types.html#symbol-u32">u32</a> clientId) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
