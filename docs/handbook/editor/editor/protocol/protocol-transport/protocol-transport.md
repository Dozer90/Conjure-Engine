---
kind: class
---

[Editor](../../../../editor.md) → [Protocol](../../../protocol.md) → [protocol_transport.h](../protocol-transport.md)

# protocol_transport
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/protocol/protocol_transport.h` |
| Source File Path | `engine/src/editor/core/protocol/protocol_transport.cpp` |
| API ↗ | [The AI control protocol](../../../../../api/editor/ai-protocol.md) |

## Syntax

```cpp
class CONJURE_API protocol_transport
```

## Derived Classes


<details><summary>Classes that use this as a base</summary>

<ul>

<li><a href="../loopback-transport/loopback-transport.html">loopback_transport</a></li>

</ul>

</details>

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">protocol_transport();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">virtual</span> ~protocol_transport();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mAllocator</code></td><td><code class="signature"><a href="../protocol-transport.html#symbol-client-id-allocator">client_id_allocator</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mNextLocalId</code></td><td><code class="signature"><a href="../../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">1</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `client_count` {#function-client-count}

<div class="function-declaration" id="function-client-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../../../engine/core/types.html#symbol-usize">usize</a> client_count() <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

<p>Connected clients.</p>

#### `disconnect` {#function-disconnect}

<div class="function-declaration" id="function-disconnect-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> disconnect(
    <a href="../../../../engine/core/types.html#symbol-u32">u32</a> clientId) = <span class="mi">0</span>;</code></pre></div>

<p>Closes a client; the next <code><a href="#function-poll-1">poll</a>()</code> reports it closed.</p>

#### `flush` {#function-flush}

<div class="function-declaration" id="function-flush-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> flush() = <span class="mi">0</span>;</code></pre></div>

<p>Writes queued bytes without blocking.</p>

#### `kind` {#function-kind}

<div class="function-declaration" id="function-kind-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../protocol-transport.html#symbol-transport-kind">transport_kind</a> kind() <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

#### `pending_bytes` {#function-pending-bytes}

<div class="function-declaration" id="function-pending-bytes-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../../../engine/core/types.html#symbol-usize">usize</a> pending_bytes(
    <a href="../../../../engine/core/types.html#symbol-u32">u32</a> clientId) <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

<p>Bytes queued for a client and not yet written (the backlog policy reads it).</p>

#### `poll` {#function-poll}

<div class="function-declaration" id="function-poll-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> poll(
    std::vector&lt;<a href="../../../../engine/core/types.html#symbol-u32">u32</a>&gt; &amp;outOpened,
    std::vector&lt;<a href="inbound-line.html">inbound_line</a>&gt; &amp;outLines,
    std::vector&lt;<a href="../../../../engine/core/types.html#symbol-u32">u32</a>&gt; &amp;outClosed) = <span class="mi">0</span>;</code></pre></div>

<p>Collects what happened since the last call: new clients (ids allocated through the allocator), complete lines and closed clients, in that order. Never blocks.</p>

#### `send` {#function-send}

<div class="function-declaration" id="function-send-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> send(
    <a href="../../../../engine/core/types.html#symbol-u32">u32</a> clientId,
    std::string_view line) = <span class="mi">0</span>;</code></pre></div>

<p>Queues a line (a newline is appended) for a client. Unknown ids are ignored.</p>

#### `set_client_id_allocator` {#function-set-client-id-allocator}

<div class="function-declaration" id="function-set-client-id-allocator-1"><pre><code><span class="k">void</span> set_client_id_allocator(
    <a href="../protocol-transport.html#symbol-client-id-allocator">client_id_allocator</a> allocator);</code></pre></div>

<p>The server installs its id allocator before <code><a href="#function-start-1">start</a>()</code>. Without one ids count from 1.</p>

#### `start` {#function-start}

<div class="function-declaration" id="function-start-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../../../engine/core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; start() = <span class="mi">0</span>;</code></pre></div>

<p>Opens the transport (binds the socket, starts the reader thread).</p>

#### `stop` {#function-stop}

<div class="function-declaration" id="function-stop-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> stop() = <span class="mi">0</span>;</code></pre></div>

<p>Closes every client and releases the resources. Idempotent.</p>

### Protected

#### `allocate_client_id` {#function-allocate-client-id}

<div class="function-declaration" id="function-allocate-client-id-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../engine/core/types.html#symbol-u32">u32</a> allocate_client_id();</code></pre></div>

<p>The next client id from the allocator (or a local counter).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
