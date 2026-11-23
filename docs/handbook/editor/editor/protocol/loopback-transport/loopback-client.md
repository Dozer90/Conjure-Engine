---
kind: class
---

[Editor](../../../../editor.md) → [Protocol](../../../protocol.md) → [loopback_transport.h](../loopback-transport.md)

# loopback_client
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/protocol/loopback_transport.h` |
| Source File Path | `engine/src/editor/core/protocol/loopback_transport.cpp` |
| API ↗ | [The AI control protocol](../../../../../api/editor/ai-protocol.md) |

## Syntax

```cpp
class CONJURE_API loopback_client
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature"><span class="k">explicit</span> loopback_client(
    std::shared_ptr&lt;<a href="../../../../engine/core/event/event-state.html">state</a>&gt; shared) <span class="k">noexcept</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">loopback_client();</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">loopback_client(
    <span class="k">const</span> <a href="loopback-client.html">loopback_client</a> &amp;) = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~loopback_client();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mState</code></td><td><code class="signature">std::shared_ptr&lt;<a href="../../../../engine/core/event/event-state.html">state</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `client_id` {#function-client-id}

<div class="function-declaration" id="function-client-id-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../engine/core/types.html#symbol-u32">u32</a> client_id() <span class="k">const</span>;</code></pre></div>

<p>The id the server assigned (0 until the server&#x27;s first <code>poll()</code> accepted the client).</p>

#### `disconnect` {#function-disconnect}

<div class="function-declaration" id="function-disconnect-1"><pre><code><span class="k">void</span> disconnect();</code></pre></div>

<p>Closes the client side; the server sees the disconnect at its next <code>poll()</code>.</p>

#### `is_connected` {#function-is-connected}

<div class="function-declaration" id="function-is-connected-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_connected() <span class="k">const</span>;</code></pre></div>

#### `pending` {#function-pending}

<div class="function-declaration" id="function-pending-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../engine/core/types.html#symbol-usize">usize</a> pending() <span class="k">const</span>;</code></pre></div>

<p>Lines waiting to be received.</p>

#### `receive` {#function-receive}

<div class="function-declaration" id="function-receive-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;std::string&gt; receive();</code></pre></div>

<p>The next line the server sent, without its newline; nullopt when none is waiting.</p>

#### `receive_all` {#function-receive-all}

<div class="function-declaration" id="function-receive-all-1"><pre><code>[[<span class="na">nodiscard</span>]] std::vector&lt;std::string&gt; receive_all();</code></pre></div>

<p>Every waiting line.</p>

#### `send` {#function-send}

<div class="function-declaration" id="function-send-1"><pre><code><span class="k">void</span> send(
    std::string_view line);</code></pre></div>

<p>Queues one line (a newline is appended when missing) for the server&#x27;s next <code>poll()</code>.</p>

#### `send_raw` {#function-send-raw}

<div class="function-declaration" id="function-send-raw-1"><pre><code><span class="k">void</span> send_raw(
    std::string_view bytes);</code></pre></div>

<p>Queues raw bytes without framing (partial lines, several lines in one chunk).</p>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="loopback-client.html">loopback_client</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="loopback-client.html">loopback_client</a> &amp;) = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
