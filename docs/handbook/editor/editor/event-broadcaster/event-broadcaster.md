---
kind: class
---

[Editor](../../../editor.md) → [Protocol](../../protocol.md) → [event_broadcaster.h](../event-broadcaster.md)

# event_broadcaster
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/event_broadcaster.h` |
| Source File Path | `engine/src/editor/core/event_broadcaster.cpp` |
| API ↗ | [Headless mode and the command model](../../../../api/editor/headless-and-commands.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
class CONJURE_API event_broadcaster
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">event_broadcaster();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~event_broadcaster();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mClients</code></td><td><code class="signature">std::vector&lt;<a href="event-broadcaster-client.html">client</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mEmitted</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFrame</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Next<wbr>Client<wbr>Id</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">1</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mNextSeq</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">1</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `client_count` {#function-client-count}

<div class="function-declaration" id="function-client-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-usize">usize</a> client_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `client_ids` {#function-client-ids}

<div class="function-declaration" id="function-client-ids-1"><pre><code>[[<span class="na">nodiscard</span>]] std::vector&lt;<a href="../../../engine/core/types.html#symbol-u32">u32</a>&gt; client_ids() <span class="k">const</span>;</code></pre></div>

<p>Every registered client id.</p>

#### `emit` {#function-emit}

<div class="function-declaration" id="function-emit-1"><pre><code><a href="../../../engine/core/types.html#symbol-u64">u64</a> emit(
    <a href="../event-broadcaster.html#symbol-editor-event">editor_event</a> event,
    <a href="../../../engine/core/json/document/json-document.html">core::json::document</a> data);</code></pre></div>

<p>Delivers an event to every subscribed client, with the next sequence number and the current frame. Returns the sequence number used.</p>

#### `emit_log` {#function-emit-log}

<div class="function-declaration" id="function-emit-log-1"><pre><code><a href="../../../engine/core/types.html#symbol-u64">u64</a> emit_log(
    <a href="../../../engine/core/log.html#symbol-log-level">core::log_level</a> level,
    <a href="../../../engine/core/json/document/json-document.html">core::json::document</a> data);</code></pre></div>

<p>Delivers a <code>log</code> event to the subscribed clients whose minimum level allows it.</p>

#### `emit_to` {#function-emit-to}

<div class="function-declaration" id="function-emit-to-1"><pre><code><a href="../../../engine/core/types.html#symbol-u64">u64</a> emit_to(
    <a href="../../../engine/core/types.html#symbol-u32">u32</a> clientId,
    <a href="../event-broadcaster.html#symbol-editor-event">editor_event</a> event,
    <a href="../../../engine/core/json/document/json-document.html">core::json::document</a> data);</code></pre></div>

<p>Delivers an event to one client only (<code>events.dropped</code>, <code>client.connected</code> acknowledgements).</p>

#### `emitted_count` {#function-emitted-count}

<div class="function-declaration" id="function-emitted-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-u64">u64</a> emitted_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Events delivered to at least one client since construction.</p>

#### `frame` {#function-frame}

<div class="function-declaration" id="function-frame-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-u64">u64</a> frame() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `has_client` {#function-has-client}

<div class="function-declaration" id="function-has-client-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> has_client(
    <a href="../../../engine/core/types.html#symbol-u32">u32</a> clientId) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `min_log_level` {#function-min-log-level}

<div class="function-declaration" id="function-min-log-level-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/log.html#symbol-log-level">core::log_level</a> min_log_level(
    <a href="../../../engine/core/types.html#symbol-u32">u32</a> clientId) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `next_seq` {#function-next-seq}

<div class="function-declaration" id="function-next-seq-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-u64">u64</a> next_seq() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `register_client` {#function-register-client}

<div class="function-declaration" id="function-register-client-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-u32">u32</a> register_client(
    <a href="../event-broadcaster.html#symbol-event-sink">event_sink</a> sink);</code></pre></div>

<p>Registers a client with a fresh id (1, 2, ...) and the default subscriptions.</p>

<div class="function-declaration" id="function-register-client-2"><pre><code><span class="k">void</span> register_client(
    <a href="../../../engine/core/types.html#symbol-u32">u32</a> clientId,
    <a href="../event-broadcaster.html#symbol-event-sink">event_sink</a> sink);</code></pre></div>

<p>Registers a client under a chosen id (0 is the local UI); an existing client keeps its subscriptions and gets the new sink.</p>

#### `set_frame` {#function-set-frame}

<div class="function-declaration" id="function-set-frame-1"><pre><code><span class="k">void</span> set_frame(
    <a href="../../../engine/core/types.html#symbol-u64">u64</a> frame) <span class="k">noexcept</span>;</code></pre></div>

<p>The frame stamped on the next events.</p>

#### `set_min_log_level` {#function-set-min-log-level}

<div class="function-declaration" id="function-set-min-log-level-1"><pre><code><span class="k">void</span> set_min_log_level(
    <a href="../../../engine/core/types.html#symbol-u32">u32</a> clientId,
    <a href="../../../engine/core/log.html#symbol-log-level">core::log_level</a> level);</code></pre></div>

<p>The lowest level of <code>log</code> events the client receives.</p>

#### `set_subscriptions` {#function-set-subscriptions}

<div class="function-declaration" id="function-set-subscriptions-1"><pre><code><span class="k">void</span> set_subscriptions(
    <a href="../../../engine/core/types.html#symbol-u32">u32</a> clientId,
    <a href="../../../engine/core/types.html#symbol-u64">u64</a> mask);</code></pre></div>

<p>Replaces the subscription set of a client; unknown ids are ignored.</p>

#### `subscriptions` {#function-subscriptions}

<div class="function-declaration" id="function-subscriptions-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-u64">u64</a> subscriptions(
    <a href="../../../engine/core/types.html#symbol-u32">u32</a> clientId) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `unregister_client` {#function-unregister-client}

<div class="function-declaration" id="function-unregister-client-1"><pre><code><span class="k">void</span> unregister_client(
    <a href="../../../engine/core/types.html#symbol-u32">u32</a> clientId);</code></pre></div>

<p>Forgets a client; unknown ids are ignored.</p>

### Private

#### `find` {#function-find}

<div class="function-declaration" id="function-find-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="event-broadcaster-client.html">client</a> *find(
    <a href="../../../engine/core/types.html#symbol-u32">u32</a> clientId) <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-find-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="event-broadcaster-client.html">client</a> *find(
    <a href="../../../engine/core/types.html#symbol-u32">u32</a> clientId) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `make_event` {#function-make-event}

<div class="function-declaration" id="function-make-event-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../protocol/protocol-message/protocol-event.html">protocol_event</a> make_event(
    <a href="../event-broadcaster.html#symbol-editor-event">editor_event</a> event,
    <a href="../../../engine/core/json/document/json-document.html">core::json::document</a> data);</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
