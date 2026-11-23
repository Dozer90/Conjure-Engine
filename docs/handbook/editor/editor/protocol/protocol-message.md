---
kind: header
---

[Editor](../../../editor.md) → [Protocol](../../protocol.md)

# protocol_message.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/protocol/protocol_message.h` |
| API ↗ | [The AI control protocol](../../../../api/editor/ai-protocol.md), [The editor](../../../../api/editor/index.md) |

<p>The wire messages: newline-delimited JSON, UTF-8, at most <code>MAX_MESSAGE_BYTES</code> per line. Requests carry a caller id (integer, string or <code>null</code>, echoed verbatim), a command name and an object of arguments; responses echo the id and carry either a result or an error; events carry the monotonic sequence number and the frame they were emitted in.</p>

## Structs

| File | Description |
|---|---|
| [`protocol_event`](protocol-message/protocol-event.md) |  |
| [`protocol_request`](protocol-message/protocol-request.md) |  |
| [`protocol_response`](protocol-message/protocol-response.md) |  |

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-MAX_MESSAGE_BYTES"></a><code>MAX_MESSAGE_BYTES</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../../../engine/core/types.html#symbol-usize">usize</a></code> | <code class="signature"><span class="mi">16</span> * MIB</code> |  |
| <a id="variable-PROTOCOL_VERSION"></a><code>PROTOCOL_VERSION</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../../../engine/core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">1</span></code> |  |

## Functions

### Public

#### `make_error_response` {#function-make-error-response}

<div class="function-declaration" id="function-make-error-response-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="protocol-message/protocol-response.html">protocol_response</a> make_error_response(
    <span class="k">const</span> <a href="../../../engine/core/json/value/json-value.html">core::json::value</a> &amp;id,
    <a href="../editor-error.html#symbol-protocol-error-code">protocol_error_code</a> code,
    std::string message,
    <a href="../../../engine/core/json/document/json-document.html">core::json::document</a> details = core::json::document::object());</code></pre></div>

<p><code>A</code> failed response echoing the id.</p>

#### `make_id` {#function-make-id}

<div class="function-declaration" id="function-make-id-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../../../engine/core/json/document/json-document.html">core::json::document</a> make_id(
    <a href="../../../engine/core/types.html#symbol-i64">i64</a> value);</code></pre></div>

<p><code>A</code> document holding an integer id.</p>

<div class="function-declaration" id="function-make-id-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../../../engine/core/json/document/json-document.html">core::json::document</a> make_id(
    std::string_view value);</code></pre></div>

<p><code>A</code> document holding a string id.</p>

#### `make_ok_response` {#function-make-ok-response}

<div class="function-declaration" id="function-make-ok-response-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="protocol-message/protocol-response.html">protocol_response</a> make_ok_response(
    <span class="k">const</span> <a href="../../../engine/core/json/value/json-value.html">core::json::value</a> &amp;id,
    <a href="../../../engine/core/json/document/json-document.html">core::json::document</a> result);</code></pre></div>

<p><code>A</code> successful response echoing the id.</p>

#### `null_id` {#function-null-id}

<div class="function-declaration" id="function-null-id-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../../../engine/core/json/document/json-document.html">core::json::document</a> null_id();</code></pre></div>

<p><code>A</code> document holding a <code>null</code> id (the id of a response to an unparseable line).</p>

#### `parse_event` {#function-parse-event}

<div class="function-declaration" id="function-parse-event-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../../../engine/core/result/result.html">core::result</a>&lt;<a href="protocol-message/protocol-event.html">protocol_event</a>&gt; parse_event(
    std::string_view line);</code></pre></div>

#### `parse_request` {#function-parse-request}

<div class="function-declaration" id="function-parse-request-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../../../engine/core/result/result.html">core::result</a>&lt;<a href="protocol-message/protocol-request.html">protocol_request</a>&gt; parse_request(
    std::string_view line);</code></pre></div>

<p>Parses one line. <code>PARSE_ERROR</code> when it is not JSON, <code>INVALID_REQUEST</code> when it is not an object with a string &quot;cmd&quot; and an object (or absent) &quot;args&quot;, or when &quot;id&quot; is neither integer, string nor <code>null</code>, or &quot;<code>coalesceKey</code>&quot; is not an unsigned integer; <code>MESSAGE_TOO_LARGE</code> above <code>MAX_MESSAGE_BYTES</code>. <code>A</code> missing id yields a <code>null</code> id.</p>

#### `parse_response` {#function-parse-response}

<div class="function-declaration" id="function-parse-response-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../../../engine/core/result/result.html">core::result</a>&lt;<a href="protocol-message/protocol-response.html">protocol_response</a>&gt; parse_response(
    std::string_view line);</code></pre></div>

<p>Parses one line of a response or event (the client side; the headless runner&#x27;s tests). <code>PARSE_ERROR</code> / <code>INVALID_REQUEST</code> as above. <code>A</code> line with &quot;event&quot; is an event; one with &quot;ok&quot; is a response.</p>

#### `serialise` {#function-serialise}

<div class="function-declaration" id="function-serialise-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string serialise(
    <span class="k">const</span> <a href="protocol-message/protocol-response.html">protocol_response</a> &amp;response);</code></pre></div>

<p>The wire form without the trailing newline: {&quot;id&quot;:..,&quot;ok&quot;:<code>true</code>,&quot;result&quot;:{..}} or {&quot;id&quot;:..,&quot;ok&quot;:<code>false</code>,&quot;error&quot;:{&quot;code&quot;:&quot;..&quot;,&quot;message&quot;:&quot;..&quot;,&quot;details&quot;:{..}}}.</p>

<div class="function-declaration" id="function-serialise-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string serialise(
    <span class="k">const</span> <a href="protocol-message/protocol-event.html">protocol_event</a> &amp;event);</code></pre></div>

<p>{&quot;event&quot;:&quot;..&quot;,&quot;seq&quot;:n,&quot;frame&quot;:n,&quot;data&quot;:{..}} without the trailing newline.</p>

<div class="function-declaration" id="function-serialise-3"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string serialise(
    <span class="k">const</span> <a href="protocol-message/protocol-request.html">protocol_request</a> &amp;request);</code></pre></div>

<p>{&quot;id&quot;:..,&quot;cmd&quot;:&quot;..&quot;,&quot;args&quot;:{..}} (plus &quot;<code>coalesceKey</code>&quot; when non-zero) without the trailing newline.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="make_error_response"></a>
<a id="make_id"></a>
<a id="make_ok_response"></a>
<a id="max_message_bytes"></a>
<a id="null_id"></a>
<a id="parse_event"></a>
<a id="parse_request"></a>
<a id="parse_response"></a>
<a id="protocol_event"></a>
<a id="protocol_eventdata"></a>
<a id="protocol_eventframe"></a>
<a id="protocol_eventname"></a>
<a id="protocol_eventseq"></a>
<a id="protocol_request"></a>
<a id="protocol_requestargs"></a>
<a id="protocol_requestclientid"></a>
<a id="protocol_requestcoalescekey"></a>
<a id="protocol_requestcommand"></a>
<a id="protocol_requestid"></a>
<a id="protocol_response"></a>
<a id="protocol_responseclientid"></a>
<a id="protocol_responseerrorcode"></a>
<a id="protocol_responseerrordetails"></a>
<a id="protocol_responseerrormessage"></a>
<a id="protocol_responseid"></a>
<a id="protocol_responseok"></a>
<a id="protocol_responseresult"></a>
<a id="protocol_version"></a>
<a id="serialise"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
