---
kind: class
---

[Editor](../../../editor.md) → [User Interface](../../user-interface.md) → [screenshot_service.h](../screenshot-service.md)

# screenshot_service
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/screenshot_service.h` |
| Source File Path | `engine/src/editor/core/screenshot_service.cpp` |
| API ↗ | [Headless mode and the command model](../../../../api/editor/headless-and-commands.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
class CONJURE_API screenshot_service
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> screenshot_service(
    <a href="../editor-context/editor-context.html">editor_context</a> &amp;context);</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~screenshot_service();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mAlive</code></td><td><code class="signature">std::shared_ptr&lt;<span class="k">bool</span>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mBackend</code></td><td><code class="signature">std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="screenshot-backend.html">screenshot_<wbr>backend</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mCompleted</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mContext</code></td><td><code class="signature"><a href="../editor-context/editor-context.html">editor_context</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mEncoding</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPending</code></td><td><code class="signature">std::vector&lt;<a href="screenshot-service-pending.html">pending</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `available` {#function-available}

<div class="function-declaration" id="function-available-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> available() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `cancel_pending` {#function-cancel-pending}

<div class="function-declaration" id="function-cancel-pending-1"><pre><code><span class="k">void</span> cancel_pending();</code></pre></div>

<p>Fails every waiting request with <code>CANCELLED</code> (engine restart, shutdown) and forgets it; captures already encoding finish writing their file but their reply was cancelled by the dispatcher.</p>

#### `completed_count` {#function-completed-count}

<div class="function-declaration" id="function-completed-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-u32">u32</a> completed_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `create_renderer_backend` {#function-create-renderer-backend}

<div class="function-declaration" id="function-create-renderer-backend-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> std::unique_ptr&lt;<a href="screenshot-backend.html">screenshot_backend</a>&gt; create_renderer_backend(
    <a href="../editor-context/editor-context.html">editor_context</a> &amp;context);</code></pre></div>

<p>The renderer-backed backend when the context has a renderer on a real device; <code>nullptr</code> on the <code>null</code> device, without rendering or without the renderer compiled in.</p>

#### `encoding_count` {#function-encoding-count}

<div class="function-declaration" id="function-encoding-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-u32">u32</a> encoding_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Captures handed to a job whose completion has not run yet.</p>

#### `parse_request` {#function-parse-request}

<div class="function-declaration" id="function-parse-request-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../../engine/core/result/result.html">core::result</a>&lt;<a href="screenshot-request.html">screenshot_request</a>&gt; parse_request(
    <span class="k">const</span> <a href="../../../engine/core/json/value/json-value.html">core::json::value</a> &amp;args);</code></pre></div>

<p>Parses the <code>view.screenshot</code> arguments: <code>INVALID_ARGS</code> for an empty path, a zero or oversized size (above 16384), a format other than &quot;png&quot; or &quot;jxl&quot;, a format the path&#x27;s &quot;.png&quot; / &quot;.jxl&quot; extension contradicts, an empty camera or a <code>waitFrames</code> outside 0..100000. Without &quot;format&quot; the extension decides (<code>screenshot_format_of</code>) and any other path is a <code>PNG</code>. Any camera other than &quot;editor&quot; / &quot;game&quot; names an entity, which <code>view.screenshot</code> resolves to its guid.</p>

#### `pending_count` {#function-pending-count}

<div class="function-declaration" id="function-pending-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-usize">usize</a> pending_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Requests still waiting for their frame (not the captures being encoded).</p>

#### `request` {#function-request}

<div class="function-declaration" id="function-request-1"><pre><code><span class="k">void</span> request(
    <span class="k">const</span> <a href="screenshot-request.html">screenshot_request</a> &amp;value,
    <a href="../editor-command/async-reply.html">async_reply</a> reply);</code></pre></div>

<p>Validates and parks a request: <code>INVALID_ARGS</code> for an empty path or a zero size; <code>NOT_AVAILABLE</code> for a format this build cannot write (<code><a href="#function-supports-1">supports</a>()</code>) and without a backend. On failure the reply is completed with that error. Pictures are taken one at a time, in request order: a request made while another waits is begun with the backend when its turn comes, and a refusal by the backend (a camera that does not exist) is answered then.</p>

#### `set_backend` {#function-set-backend}

<div class="function-declaration" id="function-set-backend-1"><pre><code><span class="k">void</span> set_backend(
    std::unique_ptr&lt;<a href="screenshot-backend.html">screenshot_backend</a>&gt; backend);</code></pre></div>

<p>Installs the backend (<code>nullptr</code> = none: requests fail with <code>NOT_AVAILABLE</code>).</p>

#### `supports` {#function-supports}

<div class="function-declaration" id="function-supports-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <span class="k">bool</span> supports(
    <a href="../screenshot-service.html#symbol-screenshot-format">screenshot_format</a> format) <span class="k">noexcept</span>;</code></pre></div>

<p>True when this build can write the format: <code>PNG</code> always, <code>JPEG_XL</code> when the renderer module (which holds the JPEG XL encoder) is compiled in.</p>

#### `tick` {#function-tick}

<div class="function-declaration" id="function-tick-1"><pre><code><span class="k">void</span> tick();</code></pre></div>

<p>Once per frame, for the first request: counts down its wait (a frame counts once the backend drew one for it), has the backend draw it again while it waits, captures it when due and hands the image to a job that encodes it in the request&#x27;s format (a JPEG XL encode also spreads over the job system&#x27;s workers), writes and completes the reply on the main thread (<code>IO_ERROR</code> when the file cannot be encoded or written); the request behind it is begun in the same call.</p>

### Private

#### `encode_and_complete` {#function-encode-and-complete}

<div class="function-declaration" id="function-encode-and-complete-1"><pre><code><span class="k">void</span> encode_and_complete(
    <a href="screenshot-request.html">screenshot_request</a> request,
    <a href="../editor-command/async-reply.html">async_reply</a> reply,
    <a href="../../../engine/core/image/image.html">core::image</a> image);</code></pre></div>

#### `finish` {#function-finish}

<div class="function-declaration" id="function-finish-1"><pre><code><span class="k">void</span> finish(
    <span class="k">const</span> <a href="screenshot-request.html">screenshot_request</a> &amp;request,
    <span class="k">const</span> <a href="../editor-command/async-reply.html">async_reply</a> &amp;reply,
    <span class="k">const</span> <a href="../../../engine/core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; &amp;written);</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
