---
kind: class
---

[Editor](../../../../../editor.md) → [User Interface](../../../../user-interface.md) → [ai_session_panel.h](../ai-session-panel.md)

# ui::ai_session_panel
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor::ui` |
| Include File Path | `engine/include/conjure/editor/ui/panels/ai_session_panel.h` |
| Source File Path | `engine/src/editor/ui/panels/ai_session_panel.cpp` |
| API ↗ | [The workspace UI](../../../../../../api/editor/workspace-ui.md) |

## Syntax

```cpp
class CONJURE_API ai_session_panel final : public editor_panel
```

## Inheritance Hierarchy


[`ui::editor_panel`](../../editor-panel/ui-editor-panel.md) → `ui::ai_session_panel`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">ai_session_panel();</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~ai_session_panel() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">MAX_<wbr>TRANSCRIPT_<wbr>LINES</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../../../../engine/core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">500</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mClient</code></td><td><code class="signature"><a href="../../../protocol/loopback-transport/loopback-client.html">loopback_client</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mConnected</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mInput</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">""</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSent</code></td><td><code class="signature"><a href="../../../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mTranscript</code></td><td><code class="signature">std::deque&lt;std::string&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `draw_contents` {#function-draw-contents}

<div class="function-declaration" id="function-draw-contents-1"><pre><code><span class="k">void</span> draw_contents(
    <a href="../../../editor-context/editor-context.html">editor_context</a> &amp;context) <span class="k">override</span>;</code></pre></div>

#### `is_connected` {#function-is-connected}

<div class="function-declaration" id="function-is-connected-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_connected() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `send` {#function-send}

<div class="function-declaration" id="function-send-1"><pre><code><span class="k">void</span> send(
    <a href="../../../editor-context/editor-context.html">editor_context</a> &amp;context,
    std::string_view line);</code></pre></div>

<p>Sends one request line through the loopback client (connecting on first use).</p>

#### `sent_count` {#function-sent-count}

<div class="function-declaration" id="function-sent-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../../engine/core/types.html#symbol-u32">u32</a> sent_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `transcript` {#function-transcript}

<div class="function-declaration" id="function-transcript-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::deque&lt;std::string&gt; &amp;transcript() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `update` {#function-update}

<div class="function-declaration" id="function-update-1"><pre><code><span class="k">void</span> update(
    <a href="../../../editor-context/editor-context.html">editor_context</a> &amp;context) <span class="k">override</span>;</code></pre></div>

### Private

#### `append` {#function-append}

<div class="function-declaration" id="function-append-1"><pre><code><span class="k">void</span> append(
    std::string line);</code></pre></div>

#### `ensure_connected` {#function-ensure-connected}

<div class="function-declaration" id="function-ensure-connected-1"><pre><code><span class="k">void</span> ensure_connected(
    <a href="../../../editor-context/editor-context.html">editor_context</a> &amp;context);</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
