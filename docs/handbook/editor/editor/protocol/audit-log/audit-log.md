---
kind: class
---

[Editor](../../../../editor.md) → [Protocol](../../../protocol.md) → [audit_log.h](../audit-log.md)

# audit_log
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/protocol/audit_log.h` |
| Source File Path | `engine/src/editor/core/protocol/audit_log.cpp` |
| API ↗ | [Access tokens and the audit log](../../../../../api/editor/access-and-audit.md), [The AI control protocol](../../../../../api/editor/ai-protocol.md) |

## Syntax

```cpp
class CONJURE_API audit_log
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">audit_log();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~audit_log();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">DEFAULT_<wbr>MAX_<wbr>FILE_<wbr>BYTES</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../../../engine/core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">4</span> * MIB</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">FILE_NAME</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code></td><td><code class="signature"><span class="s">"ai_audit.log"</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">MAX_<wbr>ARGS_<wbr>BYTES</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../../../engine/core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">2</span> * KIB</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">MAX_<wbr>ARRAY_<wbr>ITEMS</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../../../engine/core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">16</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">MAX_<wbr>COMMAND_<wbr>BYTES</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../../../engine/core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">128</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">MAX_DEPTH</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../../../engine/core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">6</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">MAX_<wbr>LINE_<wbr>BYTES</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../../../engine/core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">4</span> * KIB</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">MAX_<wbr>OBJECT_<wbr>MEMBERS</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../../../engine/core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">32</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">MAX_<wbr>TEXT_<wbr>BYTES</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../../../engine/core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">160</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mDirectory</code></td><td><code class="signature">std::filesystem::path</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFailed</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Max<wbr>File<wbr>Bytes</code></td><td><code class="signature"><a href="../../../../engine/core/types.html#symbol-usize">usize</a></code></td><td><code class="signature">DEFAULT_<wbr>MAX_<wbr>FILE_<wbr>BYTES</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Recorded<wbr>Count</code></td><td><code class="signature"><a href="../../../../engine/core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSize</code></td><td><code class="signature"><a href="../../../../engine/core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mStream</code></td><td><code class="signature">std::ofstream</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">PREVIOUS_<wbr>FILE_<wbr>NAME</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code></td><td><code class="signature"><span class="s">"ai_audit.1.log"</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">TOKEN_MARK</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code></td><td><code class="signature"><span class="s">"[token]"</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">WITHHELD</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code></td><td><code class="signature"><span class="s">"[withheld]"</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `close` {#function-close}

<div class="function-declaration" id="function-close-1"><pre><code><span class="k">void</span> close();</code></pre></div>

<p>Flushes and closes the file. The folder stays set: the next record reopens the file.</p>

#### `directory` {#function-directory}

<div class="function-declaration" id="function-directory-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::filesystem::path &amp;directory() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The folder given to <code>set_directory</code> (empty when none).</p>

#### `file` {#function-file}

<div class="function-declaration" id="function-file-1"><pre><code>[[<span class="na">nodiscard</span>]] std::filesystem::path file() <span class="k">const</span>;</code></pre></div>

<p>The file records are appended to, <code><a href="#function-directory-1">directory</a>()</code> / <code>FILE_NAME</code>; empty without a folder.</p>

#### `flush` {#function-flush}

<div class="function-declaration" id="function-flush-1"><pre><code><span class="k">void</span> flush();</code></pre></div>

<p>Hands the lines recorded so far to the operating system. The server calls it once per frame, so a crash loses the records of at most the frame it happened in.</p>

#### `format_line` {#function-format-line}

<div class="function-declaration" id="function-format-line-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> std::string format_line(
    <span class="k">const</span> <a href="audit-entry.html">audit_entry</a> &amp;entry,
    std::span&lt;<span class="k">const</span> std::string_view&gt; secrets,
    std::chrono::system_clock::time_point time);</code></pre></div>

<p>The record of <code>entry</code> as one line of JSON without the line end, at most <code>MAX_LINE_BYTES</code> long: {&quot;time&quot;, &quot;<code>clientId</code>&quot;, &quot;transport&quot;, &quot;access&quot;, &quot;cmd&quot;, &quot;args&quot;, &quot;ok&quot;}, then &quot;error&quot; (the code&#x27;s name) when the command failed, &quot;async&quot;: <code>true</code> when it was accepted and is still running, and &quot;batch&quot;: <code>true</code> for a child of an <code>editor.batch</code>. The tokens in <code>secrets</code> and every member named &quot;token&quot; are kept out as the top of this header describes.</p>

#### `format_time` {#function-format-time}

<div class="function-declaration" id="function-format-time-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> std::string format_time(
    std::chrono::system_clock::time_point time);</code></pre></div>

<p><code>A</code> point in time as the log writes it: UTC, to the second, &quot;2026-11-17T09:15:02Z&quot;.</p>

#### `is_open` {#function-is-open}

<div class="function-declaration" id="function-is-open-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_open() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True while the file is open, that is, after a record and before <code><a href="#function-close-1">close</a>()</code> or <code><a href="#function-set-directory-1">set_directory</a>()</code>.</p>

#### `max_file_bytes` {#function-max-file-bytes}

<div class="function-declaration" id="function-max-file-bytes-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../engine/core/types.html#symbol-usize">usize</a> max_file_bytes() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `previous_file` {#function-previous-file}

<div class="function-declaration" id="function-previous-file-1"><pre><code>[[<span class="na">nodiscard</span>]] std::filesystem::path previous_file() <span class="k">const</span>;</code></pre></div>

<p>The file the current one becomes when it is full, <code><a href="#function-directory-1">directory</a>()</code> / <code>PREVIOUS_FILE_NAME</code>; empty without a folder.</p>

#### `record` {#function-record}

<div class="function-declaration" id="function-record-1"><pre><code><span class="k">void</span> record(
    <span class="k">const</span> <a href="audit-entry.html">audit_entry</a> &amp;entry,
    std::span&lt;<span class="k">const</span> std::string_view&gt; secrets);</code></pre></div>

<p>Appends one record, as <code>format_line</code> writes it with the current time. <code>secrets</code> are the tokens to keep out of it; empty ones are ignored. The first record of a folder creates the folder and the file. When the file cannot be created or written, one <code>WARN</code> says so and nothing more is recorded until <code>set_directory</code> is called again; the editor carries on.</p>

#### `recorded_count` {#function-recorded-count}

<div class="function-declaration" id="function-recorded-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../engine/core/types.html#symbol-u64">u64</a> recorded_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Lines written since construction, whatever the folder.</p>

#### `set_directory` {#function-set-directory}

<div class="function-declaration" id="function-set-directory-1"><pre><code><span class="k">void</span> set_directory(
    <span class="k">const</span> std::filesystem::path &amp;directory);</code></pre></div>

<p>Names the folder the log lives in (&lt;project&gt;/.conjure) and closes the files of the previous one. An empty path means there is nowhere to write: <code><a href="#function-record-1">record</a>()</code> then does nothing. Nothing is created before the first record.</p>

#### `set_max_file_bytes` {#function-set-max-file-bytes}

<div class="function-declaration" id="function-set-max-file-bytes-1"><pre><code><span class="k">void</span> set_max_file_bytes(
    <a href="../../../../engine/core/types.html#symbol-usize">usize</a> bytes);</code></pre></div>

<p>The size a file is allowed to reach (<code>DEFAULT_MAX_FILE_BYTES</code> unless changed). <code>A</code> value below <code>MAX_LINE_BYTES</code> is raised to it, so that every line fits a file. The log is closed; the next record opens it under the new limit, which removes a file that is larger.</p>

### Private

#### `fail` {#function-fail}

<div class="function-declaration" id="function-fail-1"><pre><code><span class="k">void</span> fail(
    std::string_view reason);</code></pre></div>

#### `open` {#function-open}

<div class="function-declaration" id="function-open-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> open();</code></pre></div>

#### `rotate` {#function-rotate}

<div class="function-declaration" id="function-rotate-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> rotate();</code></pre></div>

#### `start_file` {#function-start-file}

<div class="function-declaration" id="function-start-file-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> start_file(
    <span class="k">const</span> std::filesystem::path &amp;target);</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
