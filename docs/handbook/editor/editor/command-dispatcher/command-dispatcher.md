---
kind: class
---

[Editor](../../../editor.md) → [Commands and Undo](../../commands-and-undo.md) → [command_dispatcher.h](../command-dispatcher.md)

# command_dispatcher
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/command_dispatcher.h` |
| Source File Path | `engine/src/editor/core/command_dispatcher.cpp` |
| API ↗ | [Headless mode and the command model](../../../../api/editor/headless-and-commands.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
class CONJURE_API command_dispatcher
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">command_dispatcher(
    <a href="../editor-context/editor-context.html">editor_context</a> &amp;context,
    <a href="../command-registry/command-registry.html">command_registry</a> &amp;registry,
    <a href="../undo-stack/undo-stack.html">undo_stack</a> &amp;undo);</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~command_dispatcher();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">DEFAULT_<wbr>ASYNC_<wbr>TIMEOUT_<wbr>SECONDS</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../../engine/core/types.html#symbol-f64">f64</a></code></td><td><code class="signature"><span class="mi">120.0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">DEFAULT_<wbr>MAX_<wbr>PENDING_<wbr>PER_<wbr>CLIENT</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">8</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Async<wbr>Timeout<wbr>Seconds</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-f64">f64</a></code></td><td><code class="signature">DEFAULT_<wbr>ASYNC_<wbr>TIMEOUT_<wbr>SECONDS</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mBatch</code></td><td><code class="signature"><a href="command-dispatcher-batch-state.html">batch_state</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mClock</code></td><td><code class="signature"><span class="k">const</span> <a href="../../../engine/core/clock/clock.html">core::clock</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mCompleted</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="../protocol/protocol-message/protocol-response.html">protocol_<wbr>response</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mContext</code></td><td><code class="signature"><a href="../editor-context/editor-context.html">editor_context</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Dispatching<wbr>Ticket</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Max<wbr>Pending<wbr>Per<wbr>Client</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature">DEFAULT_<wbr>MAX_<wbr>PENDING_<wbr>PER_<wbr>CLIENT</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mNextTicket</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">1</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>On<wbr>Local<wbr>Response</code></td><td><code class="signature"><a href="../../../engine/core/event/event.html">core::<wbr>event</a>&lt;<wbr><span class="k">const</span> <a href="../protocol/protocol-message/protocol-response.html">protocol_<wbr>response</a> &amp;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPending</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="command-dispatcher-pending-request.html">pending_<wbr>request</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRegistry</code></td><td><code class="signature"><a href="../command-registry/command-registry.html">command_registry</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Suppress<wbr>Recording</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Suppress<wbr>Revision</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Synchronous<wbr>Response</code></td><td><code class="signature">std::<wbr>optional&lt;<wbr><a href="../protocol/protocol-message/protocol-response.html">protocol_<wbr>response</a>&gt;</code></td><td><code class="signature">std::nullopt</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mUndo</code></td><td><code class="signature"><a href="../undo-stack/undo-stack.html">undo_stack</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `async_timeout_seconds` {#function-async-timeout-seconds}

<div class="function-declaration" id="function-async-timeout-seconds-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-f64">f64</a> async_timeout_seconds() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `cancel_all` {#function-cancel-all}

<div class="function-declaration" id="function-cancel-all-1"><pre><code><span class="k">void</span> cancel_all(
    <a href="../editor-error.html#symbol-protocol-error-code">protocol_error_code</a> code,
    std::string_view message);</code></pre></div>

<p>Completes every pending request with the given error (<code>CANCELLED</code> when the engine restarts or shuts down); the responses are delivered by the next <code><a href="#function-resolve-completed-1">resolve_completed</a>()</code>.</p>

#### `cancel_client` {#function-cancel-client}

<div class="function-declaration" id="function-cancel-client-1"><pre><code><span class="k">void</span> cancel_client(
    <a href="../../../engine/core/types.html#symbol-u32">u32</a> clientId);</code></pre></div>

<p>Drops the pending requests of a disconnected client (<code>CANCELLED</code>, logged) and commits its open transaction.</p>

#### `complete` {#function-complete}

<div class="function-declaration" id="function-complete-1"><pre><code><span class="k">void</span> complete(
    <a href="../../../engine/core/types.html#symbol-u64">u64</a> ticket,
    <a href="../protocol/protocol-message/protocol-response.html">protocol_response</a> response);</code></pre></div>

<p>Completes a parked request (called by <code><a href="../editor-command/async-reply.html">async_reply</a></code>).</p>

#### `dispatch` {#function-dispatch}

<div class="function-declaration" id="function-dispatch-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="../protocol/protocol-message/protocol-response.html">protocol_response</a>&gt; dispatch(
    <span class="k">const</span> <a href="../protocol/protocol-message/protocol-request.html">protocol_request</a> &amp;request);</code></pre></div>

<p>Validates and executes one request. Returns the response, or nullopt when the command is asynchronous and will answer through <code><a href="#function-resolve-completed-1">resolve_completed</a>()</code> later. <code>A</code> known command the client&#x27;s token does not open (<code>required_access(flags)</code> above what <code>protocol_server::access_of_client</code> reports) answers <code>UNAUTHORISED</code> with details {required, granted} before its arguments are validated; the client stays connected. Every request of a transport client, refused ones and unknown commands included, is handed to <code>protocol_server::record_command</code> once it was dispatched; so is every child of an <code>editor.batch</code>.</p>

#### `dispatch_inverses` {#function-dispatch-inverses}

<div class="function-declaration" id="function-dispatch-inverses-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; dispatch_inverses(
    std::span&lt;<span class="k">const</span> <a href="../editor-command/command-inverse.html">command_inverse</a>&gt; inverses,
    <a href="../../../engine/core/types.html#symbol-u32">u32</a> clientId);</code></pre></div>

<p>Dispatches recorded inverses or replays with undo recording suppressed (<code>history.undo</code>/redo). The commands neither bump the revision nor emit <code>scene.changed</code>: the history command does both once per step.</p>

#### `dispatch_replays` {#function-dispatch-replays}

<div class="function-declaration" id="function-dispatch-replays-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; dispatch_replays(
    std::span&lt;<span class="k">const</span> <a href="../editor-command/command-request.html">command_request</a>&gt; replays,
    <a href="../../../engine/core/types.html#symbol-u32">u32</a> clientId);</code></pre></div>

#### `execute` {#function-execute}

<div class="function-declaration" id="function-execute-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/result/result.html">core::result</a>&lt;<a href="../../../engine/core/json/document/json-document.html">core::json::document</a>&gt; execute(
    <span class="k">const</span> <a href="../editor-command/command-request.html">command_request</a> &amp;request);</code></pre></div>

<p>UI convenience: dispatches from client 0 and returns the result object or the error (the error&#x27;s code is the protocol code). An asynchronous command answers through <code>on_local_response</code>.</p>

#### `in_batch` {#function-in-batch}

<div class="function-declaration" id="function-in-batch-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> in_batch() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True while a batch runs (nested batches are refused).</p>

#### `is_pending` {#function-is-pending}

<div class="function-declaration" id="function-is-pending-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_pending(
    <a href="../../../engine/core/types.html#symbol-u64">u64</a> ticket) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True while the ticket is parked and not completed.</p>

#### `on_local_response` {#function-on-local-response}

<div class="function-declaration" id="function-on-local-response-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/event/event.html">core::event</a>&lt;<span class="k">const</span> <a href="../protocol/protocol-message/protocol-response.html">protocol_response</a> &amp;&gt; &amp;on_local_response() <span class="k">noexcept</span>;</code></pre></div>

<p>Responses of asynchronous commands issued by client 0 (the UI).</p>

#### `pending_count` {#function-pending-count}

<div class="function-declaration" id="function-pending-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-usize">usize</a> pending_count(
    <a href="../../../engine/core/types.html#symbol-u32">u32</a> clientId) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-pending-count-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-usize">usize</a> pending_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `replaying` {#function-replaying}

<div class="function-declaration" id="function-replaying-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> replaying() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True while <code>history.undo</code> / redo or a batch rollback dispatch recorded inverses and replays. Handlers use it to accept values a client may not send but an earlier state legitimately held (a legacy entity name restored by undo).</p>

#### `resolve_completed` {#function-resolve-completed}

<div class="function-declaration" id="function-resolve-completed-1"><pre><code><span class="k">void</span> resolve_completed(
    std::vector&lt;<a href="../protocol/protocol-message/protocol-response.html">protocol_response</a>&gt; &amp;outResponses);</code></pre></div>

<p>Moves the responses of completed and timed-out asynchronous requests into <code>outResponses</code>.</p>

#### `run_batch` {#function-run-batch}

<div class="function-declaration" id="function-run-batch-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/result/result.html">core::result</a>&lt;std::vector&lt;<a href="../protocol/protocol-message/protocol-response.html">protocol_response</a>&gt;&gt; run_batch(
    <span class="k">const</span> <a href="../../../engine/core/json/value/json-value.html">core::json::value</a> &amp;commands,
    std::string_view label,
    <span class="k">bool</span> atomic,
    <a href="../../../engine/core/types.html#symbol-u32">u32</a> clientId,
    <a href="../../../engine/core/json/document/json-document.html">core::json::document</a> &amp;outDetails);</code></pre></div>

<p>Runs the children of <code>editor.batch</code> (docs 3.4): one undo entry, atomic rollback through the recorded inverses on a failure. Returns the children&#x27;s responses; the error of the failing child with <code>details.failedIndex</code> when atomic.</p>

#### `set_async_timeout_seconds` {#function-set-async-timeout-seconds}

<div class="function-declaration" id="function-set-async-timeout-seconds-1"><pre><code><span class="k">void</span> set_async_timeout_seconds(
    <a href="../../../engine/core/types.html#symbol-f64">f64</a> seconds) <span class="k">noexcept</span>;</code></pre></div>

#### `set_clock` {#function-set-clock}

<div class="function-declaration" id="function-set-clock-1"><pre><code><span class="k">void</span> set_clock(
    <span class="k">const</span> <a href="../../../engine/core/clock/clock.html">core::clock</a> *clock) <span class="k">noexcept</span>;</code></pre></div>

<p>The clock deadlines are measured with (the engine&#x27;s clock by default).</p>

#### `set_max_pending_per_client` {#function-set-max-pending-per-client}

<div class="function-declaration" id="function-set-max-pending-per-client-1"><pre><code><span class="k">void</span> set_max_pending_per_client(
    <a href="../../../engine/core/types.html#symbol-u32">u32</a> count) <span class="k">noexcept</span>;</code></pre></div>

### Private

#### `access_of_client` {#function-access-of-client}

<div class="function-declaration" id="function-access-of-client-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../protocol/access-token.html#symbol-access-level">access_level</a> access_of_client(
    <a href="../../../engine/core/types.html#symbol-u32">u32</a> clientId) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `audit` {#function-audit}

<div class="function-declaration" id="function-audit-1"><pre><code><span class="k">void</span> audit(
    <span class="k">const</span> <a href="../protocol/protocol-message/protocol-request.html">protocol_request</a> &amp;request,
    <span class="k">const</span> std::optional&lt;<a href="../protocol/protocol-message/protocol-response.html">protocol_response</a>&gt; &amp;response,
    <span class="k">bool</span> batchChild) <span class="k">const</span>;</code></pre></div>

#### `check_preconditions` {#function-check-preconditions}

<div class="function-declaration" id="function-check-preconditions-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="../editor-error.html#symbol-protocol-error-code">protocol_error_code</a>&gt; check_preconditions(
    <span class="k">const</span> <a href="../editor-command/command-descriptor.html">command_descriptor</a> &amp;descriptor,
    <a href="../../../engine/core/json/document/json-document.html">core::json::document</a> &amp;details) <span class="k">const</span>;</code></pre></div>

#### `dispatch_internal` {#function-dispatch-internal}

<div class="function-declaration" id="function-dispatch-internal-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="../protocol/protocol-message/protocol-response.html">protocol_response</a>&gt; dispatch_internal(
    <span class="k">const</span> <a href="../protocol/protocol-message/protocol-request.html">protocol_request</a> &amp;request);</code></pre></div>

#### `keep_play_edits` {#function-keep-play-edits}

<div class="function-declaration" id="function-keep-play-edits-1"><pre><code><span class="k">void</span> keep_play_edits(
    <a href="command-dispatcher-batch-state.html">batch_state</a> &amp;batch);</code></pre></div>

#### `now_seconds` {#function-now-seconds}

<div class="function-declaration" id="function-now-seconds-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-f64">f64</a> now_seconds() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `record_outcome` {#function-record-outcome}

<div class="function-declaration" id="function-record-outcome-1"><pre><code><span class="k">void</span> record_outcome(
    <span class="k">const</span> <a href="../editor-command/command-descriptor.html">command_descriptor</a> &amp;descriptor,
    <span class="k">const</span> <a href="../protocol/protocol-message/protocol-request.html">protocol_request</a> &amp;request,
    <a href="../editor-command/command-outcome.html">command_outcome</a> &amp;outcome);</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
