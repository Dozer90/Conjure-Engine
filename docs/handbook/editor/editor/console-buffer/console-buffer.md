---
kind: class
---

[Editor](../../../editor.md) → [User Interface](../../user-interface.md) → [console_buffer.h](../console-buffer.md)

# console_buffer
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/console_buffer.h` |
| Source File Path | `engine/src/editor/core/console_buffer.cpp` |
| API ↗ | [Headless mode and the command model](../../../../api/editor/headless-and-commands.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
class CONJURE_API console_buffer
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> console_buffer(
    <a href="../../../engine/core/types.html#symbol-usize">usize</a> entryCapacity = DEFAULT_ENTRY_CAPACITY,
    <a href="../../../engine/core/types.html#symbol-usize">usize</a> textCapacity = DEFAULT_TEXT_CAPACITY);</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~console_buffer();</code></td><td>Removes the sink from the logger when <code>install_sink()</code> added it.</td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">DEFAULT_<wbr>ENTRY_<wbr>CAPACITY</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../../engine/core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">65536</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">DEFAULT_<wbr>TEXT_<wbr>CAPACITY</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../../engine/core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">16</span> * MIB</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mDropped</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mEntries</code></td><td><code class="signature">std::vector&lt;<a href="console-buffer-entry.html">entry</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mEntryCount</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mEntryHead</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFrame</code></td><td><code class="signature">std::atomic&lt;<a href="../../../engine/core/types.html#symbol-u64">u64</a>&gt;</code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mMutex</code></td><td><code class="signature"><span class="k">mutable</span> std::mutex</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mNextSeq</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">1</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSink</code></td><td><code class="signature">std::shared_ptr&lt;<a href="../console-buffer.html">sink</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mText</code></td><td><code class="signature">std::vector&lt;<span class="k">char</span>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mTextHead</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `clear` {#function-clear}

<div class="function-declaration" id="function-clear-1"><pre><code><span class="k">void</span> clear();</code></pre></div>

<p>Forgets every entry; sequence numbers keep counting.</p>

#### `entry_capacity` {#function-entry-capacity}

<div class="function-declaration" id="function-entry-capacity-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-usize">usize</a> entry_capacity() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `install_sink` {#function-install-sink}

<div class="function-declaration" id="function-install-sink-1"><pre><code><span class="k">void</span> install_sink();</code></pre></div>

<p>Registers a <code>core::log</code> sink that forwards every message into <code><a href="#function-push-1">push</a>()</code>. Idempotent.</p>

#### `last_seq` {#function-last-seq}

<div class="function-declaration" id="function-last-seq-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-u64">u64</a> last_seq() <span class="k">const</span>;</code></pre></div>

<p>The most recent sequence number handed out (0 before the first push).</p>

#### `push` {#function-push}

<div class="function-declaration" id="function-push-1"><pre><code><span class="k">void</span> push(
    <a href="../../../engine/core/log.html#symbol-log-level">core::log_level</a> level,
    std::string_view category,
    std::string_view text);</code></pre></div>

<p>Appends one line. Thread-safe. Text longer than the text ring is truncated.</p>

#### `read` {#function-read}

<div class="function-declaration" id="function-read-1"><pre><code>[[<span class="na">nodiscard</span>]] std::vector&lt;<a href="console-record.html">console_record</a>&gt; read(
    <span class="k">const</span> <a href="console-query.html">console_query</a> &amp;query) <span class="k">const</span>;</code></pre></div>

<p>Entries matching the query, oldest first, at most <code>query.limit</code>: the oldest matches by default (a reader paging forwards with <code>sinceSeq</code>), the newest ones with <code>query.newest</code> (a view of the tail).</p>

#### `remove_sink` {#function-remove-sink}

<div class="function-declaration" id="function-remove-sink-1"><pre><code><span class="k">void</span> remove_sink();</code></pre></div>

<p>Removes the sink registered by <code><a href="#function-install-sink-1">install_sink</a>()</code>.</p>

#### `set_frame` {#function-set-frame}

<div class="function-declaration" id="function-set-frame-1"><pre><code><span class="k">void</span> set_frame(
    <a href="../../../engine/core/types.html#symbol-u64">u64</a> frame) <span class="k">noexcept</span>;</code></pre></div>

<p>The frame index stamped on new entries (set once per frame by the editor).</p>

#### `size` {#function-size}

<div class="function-declaration" id="function-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-usize">usize</a> size() <span class="k">const</span>;</code></pre></div>

<p>Entries currently in the ring.</p>

#### `text_capacity` {#function-text-capacity}

<div class="function-declaration" id="function-text-capacity-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-usize">usize</a> text_capacity() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

### Private

#### `push_locked` {#function-push-locked}

<div class="function-declaration" id="function-push-locked-1"><pre><code><span class="k">void</span> push_locked(
    <a href="../../../engine/core/log.html#symbol-log-level">core::log_level</a> level,
    std::string_view category,
    std::string_view text);</code></pre></div>

#### `text_at` {#function-text-at}

<div class="function-declaration" id="function-text-at-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string_view text_at(
    <a href="../../../engine/core/types.html#symbol-usize">usize</a> offset,
    <a href="../../../engine/core/types.html#symbol-u32">u32</a> length) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
