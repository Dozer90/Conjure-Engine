---
kind: class
---

[Editor](../../../../editor.md) → [Protocol](../../../protocol.md) → [protocol_server.h](../protocol-server.md)

# protocol_server
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/protocol/protocol_server.h` |
| Source File Path | `engine/src/editor/core/protocol/protocol_server.cpp` |
| API ↗ | [Access tokens and the audit log](../../../../../api/editor/access-and-audit.md), [The AI control protocol](../../../../../api/editor/ai-protocol.md) |

## Syntax

```cpp
class CONJURE_API protocol_server
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> protocol_server(
    <a href="../../event-broadcaster/event-broadcaster.html">event_broadcaster</a> &amp;events);</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~protocol_server();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">DEFAULT_<wbr>REQUESTS_<wbr>PER_<wbr>FRAME</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../../../engine/core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">256</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">DISCONNECT_<wbr>BACKLOG_<wbr>BYTES</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../../../engine/core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">64</span> * MIB</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">LOG_<wbr>DROP_<wbr>BACKLOG_<wbr>BYTES</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../../../engine/core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">8</span> * MIB</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mAudit</code></td><td><code class="signature"><a href="../audit-log/audit-log.html">audit_log</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mBacklog</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="../protocol-message/protocol-request.html">protocol_<wbr>request</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mBoundPort</code></td><td><code class="signature"><a href="../../../../engine/core/types.html#symbol-u16">u16</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mClients</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="client-state.html">client_<wbr>state</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mClosedQueue</code></td><td><code class="signature">std::vector&lt;<a href="../../../../engine/core/types.html#symbol-u32">u32</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mEvents</code></td><td><code class="signature"><a href="../../event-broadcaster/event-broadcaster.html">event_broadcaster</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Inspect<wbr>Token</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">""</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Inspect<wbr>Token<wbr>Explicit</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Inspect<wbr>Token<wbr>File</code></td><td><code class="signature">std::filesystem::path</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Local<wbr>Loopback</code></td><td><code class="signature"><a href="../loopback-transport/loopback-transport.html">loopback_transport</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPortFile</code></td><td><code class="signature">std::filesystem::path</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRunning</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Session<wbr>Directory</code></td><td><code class="signature">std::filesystem::path</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mStdioClosed</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mToken</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">""</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Token<wbr>Explicit</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mTokenFile</code></td><td><code class="signature">std::filesystem::path</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mTransports</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr>std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../protocol-transport/protocol-transport.html">protocol_<wbr>transport</a>&gt;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">TOKEN_<wbr>SEARCH_<wbr>BYTES</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../../../engine/core/types.html#symbol-usize">usize</a></code></td><td><code class="signature">KIB</code></td><td>The longest text <code>contains_token()</code> searches. <code>A</code> longer one is answered with <code>true</code> unsearched.</td></tr>
</tbody>
</table>

## Functions

### Public

#### `accept_hello` {#function-accept-hello}

<div class="function-declaration" id="function-accept-hello-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> accept_hello(
    <a href="../../../../engine/core/types.html#symbol-u32">u32</a> clientId,
    std::string_view presented) <span class="k">noexcept</span>;</code></pre></div>

<p>Decides the <code>session.hello</code> of a client that already passed the handshake (the command handler calls it); the last accepted hello decides what the client may do. On a transport that requires a token: <code>false</code> when <code>presented</code> is neither token, and the client keeps the access it had; otherwise the client gets what the token grants, which lowers a full-control client that presents the inspect token and raises an inspect-only one that presents the other. On a transport that requires none: always <code>true</code>, and a client that presents one of the tokens all the same gets what it grants, so a client can restrict itself; any other text leaves it as it was. True for a client id no transport knows: an in-process client (an --exec file, the UI) is trusted with any token or none.</p>

#### `access_of` {#function-access-of}

<div class="function-declaration" id="function-access-of-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../access-token.html#symbol-access-level">access_level</a> access_of(
    std::string_view presented) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>What a presented token grants: <code>FULL_CONTROL</code> for the full-control token, <code>INSPECT</code> for the inspect-only one, <code>NONE</code> for anything else and whenever the server has no tokens. Both comparisons are <code>constant_time_equals</code> and both always run.</p>

#### `access_of_client` {#function-access-of-client}

<div class="function-declaration" id="function-access-of-client-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../access-token.html#symbol-access-level">access_level</a> access_of_client(
    <a href="../../../../engine/core/types.html#symbol-u32">u32</a> clientId) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>What the client may do right now: what its transport gave it or its last accepted <code>session.hello</code> granted. <code>FULL_CONTROL</code> for a client id no transport knows (client 0, an --exec run, an <code>editor.exec_file</code> task): in-process code is the editor itself.</p>

#### `add_transport` {#function-add-transport}

<div class="function-declaration" id="function-add-transport-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../engine/core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; add_transport(
    std::unique_ptr&lt;<a href="../protocol-transport/protocol-transport.html">protocol_transport</a>&gt; transport);</code></pre></div>

<p>Adds and starts a transport (tests add a <code><a href="../loopback-transport/loopback-transport.html">loopback_transport</a></code>). <code>A</code> <code>TCP</code> transport added while no token is set gets generated ones, as in <code><a href="#function-start-1">start</a>()</code>.</p>

#### `audit` {#function-audit}

<div class="function-declaration" id="function-audit-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../audit-log/audit-log.html">audit_log</a> &amp;audit() <span class="k">noexcept</span>;</code></pre></div>

<p>The audit log, for its file names and its size limit.</p>

#### `backlog_count` {#function-backlog-count}

<div class="function-declaration" id="function-backlog-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../engine/core/types.html#symbol-usize">usize</a> backlog_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Requests parsed but held back by the per-frame cap.</p>

#### `bound_port` {#function-bound-port}

<div class="function-declaration" id="function-bound-port-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../engine/core/types.html#symbol-u16">u16</a> bound_port() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The <code>TCP</code> port in use, 0 without a <code>TCP</code> transport.</p>

#### `client` {#function-client}

<div class="function-declaration" id="function-client-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="client-state.html">client_state</a> *client(
    <a href="../../../../engine/core/types.html#symbol-u32">u32</a> clientId) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `client_count` {#function-client-count}

<div class="function-declaration" id="function-client-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../engine/core/types.html#symbol-usize">usize</a> client_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `contains_token` {#function-contains-token}

<div class="function-declaration" id="function-contains-token-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> contains_token(
    std::string_view text) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when <code>text</code> may hold one of the two tokens: it contains one, or it is longer than <code>TOKEN_SEARCH_BYTES</code> and is not searched. Text a client sent must be checked with this before it is logged or passed on to other clients, and withheld when the answer is <code>true</code>. The search is <code>constant_time_contains</code> for each token, both always run: how long it takes depends on the lengths of the text and of the tokens, not on how much of a token the text holds, so a client that may only look cannot time its way to the full-control token. Always <code>false</code> while the server has no tokens.</p>

#### `disconnect` {#function-disconnect}

<div class="function-declaration" id="function-disconnect-1"><pre><code><span class="k">void</span> disconnect(
    <a href="../../../../engine/core/types.html#symbol-u32">u32</a> clientId);</code></pre></div>

<p>Disconnects one client (its pending replies are the dispatcher&#x27;s to cancel).</p>

#### `flush` {#function-flush}

<div class="function-declaration" id="function-flush-1"><pre><code><span class="k">void</span> flush();</code></pre></div>

<p>Writes queued bytes without blocking, applies the backlog policy and flushes the audit log.</p>

#### `inspect_token` {#function-inspect-token}

<div class="function-declaration" id="function-inspect-token-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::string &amp;inspect_token() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The token that grants <code>INSPECT</code> (&quot;&quot; exactly when <code><a href="#function-token-1">token</a>()</code> is). Never log it or send it anywhere.</p>

#### `inspect_token_file` {#function-inspect-token-file}

<div class="function-declaration" id="function-inspect-token-file-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::filesystem::path &amp;inspect_token_file() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The file the inspect-only token was written to (empty when none, as for <code><a href="#function-token-file-1">token_file</a>()</code>).</p>

#### `is_running` {#function-is-running}

<div class="function-declaration" id="function-is-running-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_running() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `local_loopback` {#function-local-loopback}

<div class="function-declaration" id="function-local-loopback-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../loopback-transport/loopback-transport.html">loopback_transport</a> &amp;local_loopback();</code></pre></div>

<p>The one loopback transport in-process clients connect through (the AI session panel): created and added on the first call, the same instance afterwards, so reconnecting clients never accumulate transports. <code><a href="#function-stop-1">stop</a>()</code> releases it with the other transports.</p>

#### `poll` {#function-poll}

<div class="function-declaration" id="function-poll-1"><pre><code><span class="k">void</span> poll(
    std::vector&lt;<a href="../protocol-message/protocol-request.html">protocol_request</a>&gt; &amp;outRequests,
    <a href="../../../../engine/core/types.html#symbol-usize">usize</a> maxRequests = DEFAULT_REQUESTS_PER_FRAME);</code></pre></div>

<p>Accepts, reads and parses: new clients are registered with the broadcaster (<code>client.connected</code> is emitted to the others), lines become requests (<code>PARSE_ERROR</code> / <code>INVALID_REQUEST</code> responses are sent at once), the token handshake is enforced, closed clients are unregistered. Three things disconnect a client and drop its later lines of the same poll: <code>MESSAGE_TOO_LARGE</code>, a failed handshake, and a first line that is not a request (<code>PARSE_ERROR</code> or <code>INVALID_REQUEST</code> before any line of the client parsed; on every transport, so that an HTTP request sent to the port by a web page ends at its request line and the JSON in its body is never read). At most <code>maxRequests</code> are returned; the rest wait for the next poll.</p>

#### `port_file` {#function-port-file}

<div class="function-declaration" id="function-port-file-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::filesystem::path &amp;port_file() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The file the bound port was written to (empty when none).</p>

#### `record_command` {#function-record-command}

<div class="function-declaration" id="function-record-command-1"><pre><code><span class="k">void</span> record_command(
    <a href="../audit-log/audit-entry.html">audit_entry</a> entry);</code></pre></div>

<p>Records one dispatched command in the audit log (the dispatcher calls it; <code>entry</code> carries the command, its arguments and its outcome, the server adds the client&#x27;s transport and access). Only commands of transport clients are recorded: a client id no transport knows is the editor&#x27;s own code, and nothing is written for it. Nothing is written either while no project is open, because the log lives in the project.</p>

#### `requires_token` {#function-requires-token}

<div class="function-declaration" id="function-requires-token-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> requires_token(
    <a href="../protocol-transport.html#symbol-transport-kind">transport_kind</a> kind) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when a client of this kind has to present a token before anything else: <code>TCP</code> whenever tokens exist (always, once a <code>TCP</code> transport runs), stdio and loopback only when a token of either level was set explicitly.</p>

#### `send` {#function-send}

<div class="function-declaration" id="function-send-1"><pre><code><span class="k">void</span> send(
    <span class="k">const</span> <a href="../protocol-message/protocol-response.html">protocol_response</a> &amp;response);</code></pre></div>

<p>Queues a response for its client (<code>clientId</code> 0 is the local UI: nothing is sent).</p>

#### `set_inspect_token` {#function-set-inspect-token}

<div class="function-declaration" id="function-set-inspect-token-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../engine/core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; set_inspect_token(
    std::string token);</code></pre></div>

<p>Sets the inspect-only token explicitly, as --ai-inspect-token does, with the rules of <code>set_token</code>: every transport is then gated, an empty token returns to a generated one (or to none when nothing needs a token), and a token equal to the full-control one is <code>INVALID_ARGS</code>.</p>

#### `set_project_root` {#function-set-project-root}

<div class="function-declaration" id="function-set-project-root-1"><pre><code><span class="k">void</span> set_project_root(
    <span class="k">const</span> std::filesystem::path &amp;projectRoot);</code></pre></div>

<p>Follows a project switch: removes the current session files and, while a <code>TCP</code> transport is bound (<code><a href="#function-bound-port-1">bound_port</a>()</code> != 0), writes three files under &lt;<code>projectRoot</code>&gt;/.conjure: first <code>ai_token</code> and <code>ai_token_inspect</code>, each a token and a newline, through <code>core::vfs::write_private_file</code> so that only the user running the editor can read them, then <code>ai_port</code>, the port and a newline. <code>A</code> client that finds <code>ai_port</code> therefore finds the tokens beside it. When a token file cannot be written (a FAT or exFAT volume keeps no owner-only files, or something is in the way) a <code>WARN</code> says so, its accessor is empty, a file of that name left by an earlier session is removed if it can be, and <code>TCP</code> clients need a token given with --ai-token or --ai-inspect-token. An empty root only removes the files. The files are not shared: a second editor on the same project overwrites them, and whichever editor stops first removes them. The audit log moves with the project as well, whatever the transports: later commands are recorded under &lt;<code>projectRoot</code>&gt;/.conjure, or nowhere for an empty root.</p>

#### `set_token` {#function-set-token}

<div class="function-declaration" id="function-set-token-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../engine/core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; set_token(
    std::string token);</code></pre></div>

<p>Sets the full-control token explicitly, as --ai-token does: clients of every transport must then present a token in <code>session.hello</code>. An empty token returns to the default: a new token is generated if one is needed (a <code>TCP</code> transport runs, or the inspect token was set explicitly), and unless the inspect token was set explicitly stdio and loopback clients present nothing again. <code>INVALID_ARGS</code>, with nothing changed, when the token is not empty and equals the inspect token: one text cannot grant two levels. Clients that are connected keep the access they have. The token files of the session are written again with the tokens that now hold, also in a session whose port file could not be written.</p>

#### `start` {#function-start}

<div class="function-declaration" id="function-start-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../engine/core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; start(
    <span class="k">const</span> <a href="../../editor-options/editor-options.html">editor_options</a> &amp;options,
    <span class="k">const</span> std::filesystem::path &amp;projectRoot);</code></pre></div>

<p>Starts the transports the options ask for: <code>TCP</code> on <code>options.first_port()</code> (nine fallback ports when the port was not explicit) and stdio with --ai-stdio. The tokens are <code>options.aiToken</code> and <code>options.aiInspectToken</code>; either one, when given, gates every transport. <code>A</code> token that was not given is generated (<code>generate_access_token</code>) as soon as one is needed, that is, when a <code>TCP</code> transport starts or the other token was given; tokens that are both generated gate the <code>TCP</code> transport alone. With a <code>TCP</code> transport and a project directory the session files are written as <code>set_project_root</code> describes, and the audit log follows the project directory whatever the transports. Fails with <code>INVALID_ARGS</code>, before anything starts, when the two options name the same token, and with <code>IO_ERROR</code> when an explicit port is busy or stdio cannot start. Without any transport the server runs for the local client.</p>

#### `stdio_closed` {#function-stdio-closed}

<div class="function-declaration" id="function-stdio-closed-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> stdio_closed() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True once a stdio transport reported its client closed (standard input ended, or the client was disconnected). <code>A</code> headless --ai-stdio editor without a <code>TCP</code> server quits on it.</p>

#### `stop` {#function-stop}

<div class="function-declaration" id="function-stop-1"><pre><code><span class="k">void</span> stop();</code></pre></div>

<p>Disconnects every client, stops the transports, removes the session files (the port and the two tokens) and closes the audit log, which stays on disk. The tokens themselves are kept until the next <code><a href="#function-start-1">start</a>()</code>, <code><a href="#function-set-token-1">set_token</a>()</code> or <code><a href="#function-set-inspect-token-1">set_inspect_token</a>()</code>.</p>

#### `take_closed` {#function-take-closed}

<div class="function-declaration" id="function-take-closed-1"><pre><code><span class="k">void</span> take_closed(
    std::vector&lt;<a href="../../../../engine/core/types.html#symbol-u32">u32</a>&gt; &amp;out);</code></pre></div>

<p>Moves the ids of the clients that disconnected since the last call into <code>out</code> (the dispatcher cancels their pending requests).</p>

#### `token` {#function-token}

<div class="function-declaration" id="function-token-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::string &amp;token() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The token that grants <code>FULL_CONTROL</code> (&quot;&quot; when no client has to present one). For in-process clients such as the AI session panel; never log it or send it anywhere.</p>

#### `token_file` {#function-token-file}

<div class="function-declaration" id="function-token-file-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::filesystem::path &amp;token_file() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The file the full-control token was written to (empty when none: no <code>TCP</code> transport, no project, or a volume that cannot restrict a file to its owner).</p>

#### `transport_count` {#function-transport-count}

<div class="function-declaration" id="function-transport-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../engine/core/types.html#symbol-usize">usize</a> transport_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Transports currently polled.</p>

### Private

#### `check_handshake` {#function-check-handshake}

<div class="function-declaration" id="function-check-handshake-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> check_handshake(
    <a href="client-state.html">client_state</a> &amp;client,
    <span class="k">const</span> <a href="../protocol-message/protocol-request.html">protocol_request</a> &amp;request);</code></pre></div>

#### `deliver_event` {#function-deliver-event}

<div class="function-declaration" id="function-deliver-event-1"><pre><code><span class="k">void</span> deliver_event(
    <a href="../../../../engine/core/types.html#symbol-u32">u32</a> clientId,
    <span class="k">const</span> <a href="../protocol-message/protocol-event.html">protocol_event</a> &amp;event);</code></pre></div>

#### `ensure_tokens` {#function-ensure-tokens}

<div class="function-declaration" id="function-ensure-tokens-1"><pre><code><span class="k">void</span> ensure_tokens();</code></pre></div>

#### `find` {#function-find}

<div class="function-declaration" id="function-find-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="client-state.html">client_state</a> *find(
    <a href="../../../../engine/core/types.html#symbol-u32">u32</a> clientId) <span class="k">noexcept</span>;</code></pre></div>

#### `gates_every_transport` {#function-gates-every-transport}

<div class="function-declaration" id="function-gates-every-transport-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> gates_every_transport() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `has_tcp_transport` {#function-has-tcp-transport}

<div class="function-declaration" id="function-has-tcp-transport-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> has_tcp_transport() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `on_client_closed` {#function-on-client-closed}

<div class="function-declaration" id="function-on-client-closed-1"><pre><code><span class="k">void</span> on_client_closed(
    <a href="../../../../engine/core/types.html#symbol-u32">u32</a> clientId);</code></pre></div>

#### `on_client_opened` {#function-on-client-opened}

<div class="function-declaration" id="function-on-client-opened-1"><pre><code><span class="k">void</span> on_client_opened(
    <a href="../../../../engine/core/types.html#symbol-usize">usize</a> transportIndex,
    <a href="../../../../engine/core/types.html#symbol-u32">u32</a> clientId);</code></pre></div>

#### `remove_session_files` {#function-remove-session-files}

<div class="function-declaration" id="function-remove-session-files-1"><pre><code><span class="k">void</span> remove_session_files();</code></pre></div>

#### `rewrite_token_files` {#function-rewrite-token-files}

<div class="function-declaration" id="function-rewrite-token-files-1"><pre><code><span class="k">void</span> rewrite_token_files();</code></pre></div>

#### `send_line` {#function-send-line}

<div class="function-declaration" id="function-send-line-1"><pre><code><span class="k">void</span> send_line(
    <a href="../../../../engine/core/types.html#symbol-u32">u32</a> clientId,
    std::string_view line);</code></pre></div>

#### `transport_of` {#function-transport-of}

<div class="function-declaration" id="function-transport-of-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../protocol-transport/protocol-transport.html">protocol_transport</a> *transport_of(
    <span class="k">const</span> <a href="client-state.html">client_state</a> &amp;client) <span class="k">noexcept</span>;</code></pre></div>

#### `write_session_files` {#function-write-session-files}

<div class="function-declaration" id="function-write-session-files-1"><pre><code><span class="k">void</span> write_session_files(
    <span class="k">const</span> std::filesystem::path &amp;projectRoot);</code></pre></div>

#### `write_token_file` {#function-write-token-file}

<div class="function-declaration" id="function-write-token-file-1"><pre><code>[[<span class="na">nodiscard</span>]] std::filesystem::path write_token_file(
    <span class="k">const</span> std::filesystem::path &amp;directory,
    <a href="../access-token.html#symbol-access-level">access_level</a> level);</code></pre></div>

#### `write_token_files` {#function-write-token-files}

<div class="function-declaration" id="function-write-token-files-1"><pre><code><span class="k">void</span> write_token_files(
    <span class="k">const</span> std::filesystem::path &amp;directory);</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
