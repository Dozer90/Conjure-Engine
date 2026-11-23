---
kind: class
---

[Engine](../../../engine.md) → [Scripting](../../scripting.md) → [hot_reload.h](../hot-reload.md)

# hot_reload_controller
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::scripting` |
| Include File Path | `engine/include/conjure/scripting/hot_reload.h` |
| Source File Path | `engine/src/scripting/hot_reload.cpp` |
| API ↗ | [Hosting, the native API and hot reload](../../../../api/scripting/hosting-and-hot-reload.md), [Scripting](../../../../api/scripting/index.md) |

## Syntax

```cpp
class CONJURE_API hot_reload_controller
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">hot_reload_controller(
    <a href="../script-runtime/script-runtime.html">script_runtime</a> &amp;runtime,
    std::filesystem::path assemblyPath);</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~hot_reload_controller();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">m<wbr>Assembly<wbr>Path</code></td><td><code class="signature">std::filesystem::path</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Debounce<wbr>Seconds</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f64">f64</a></code></td><td><code class="signature"><span class="mi">0.2</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Has<wbr>Watch<wbr>Stamp</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mImmediate</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mLastResult</code></td><td><code class="signature"><a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt;</code></td><td><code class="signature"><a href="../../core/result/result.html">core::<wbr>result</a>&lt;<wbr><span class="k">void</span>&gt;::<wbr>success()</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Last<wbr>Watch<wbr>Time</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f64">f64</a></code></td><td><code class="signature"><span class="mi">0.0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mReloadCount</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Request<wbr>Count</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRequestTime</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f64">f64</a></code></td><td><code class="signature"><span class="mi">0.0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRuntime</code></td><td><code class="signature"><a href="../script-runtime/script-runtime.html">script_runtime</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mState</code></td><td><code class="signature"><a href="../hot-reload.html#symbol-reload-state">reload_state</a></code></td><td><code class="signature">reload_<wbr>state::<wbr>IDLE</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Watch<wbr>Interval<wbr>Seconds</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f64">f64</a></code></td><td><code class="signature"><span class="mi">0.0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Watch<wbr>Retries<wbr>Left</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mWatchStamp</code></td><td><code class="signature">std::<wbr>filesystem::<wbr>file_<wbr>time_<wbr>type</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Watch<wbr>Triggered</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">WATCH_<wbr>RETRY_<wbr>LIMIT</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">3</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `assembly_path` {#function-assembly-path}

<div class="function-declaration" id="function-assembly-path-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::filesystem::path &amp;assembly_path() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `debounce_seconds` {#function-debounce-seconds}

<div class="function-declaration" id="function-debounce-seconds-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-f64">f64</a> debounce_seconds() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `last_result` {#function-last-result}

<div class="function-declaration" id="function-last-result-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; &amp;last_result() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Outcome of the last reload <code><a href="#function-tick-1">tick</a>()</code> performed (success before any).</p>

#### `reload_count` {#function-reload-count}

<div class="function-declaration" id="function-reload-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> reload_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Reload attempts <code><a href="#function-tick-1">tick</a>()</code> performed, including the retries of a failed watched reload.</p>

#### `request` {#function-request}

<div class="function-declaration" id="function-request-1"><pre><code><span class="k">void</span> request(
    <span class="k">bool</span> immediate = false);</code></pre></div>

<p>Asks for a reload. Requests inside the debounce window coalesce into one; <code>immediate</code> makes the next <code><a href="#function-tick-1">tick</a>()</code> reload regardless of the window.</p>

#### `request_count` {#function-request-count}

<div class="function-declaration" id="function-request-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> request_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p><code><a href="#function-request-1">request</a>()</code> calls received (coalesced or not).</p>

#### `set_assembly_path` {#function-set-assembly-path}

<div class="function-declaration" id="function-set-assembly-path-1"><pre><code><span class="k">void</span> set_assembly_path(
    std::filesystem::path assemblyPath);</code></pre></div>

#### `set_debounce_seconds` {#function-set-debounce-seconds}

<div class="function-declaration" id="function-set-debounce-seconds-1"><pre><code><span class="k">void</span> set_debounce_seconds(
    <a href="../../core/types.html#symbol-f64">f64</a> seconds) <span class="k">noexcept</span>;</code></pre></div>

<p>Seconds a request waits before <code><a href="#function-tick-1">tick</a>()</code> executes it (default 0.2).</p>

#### `set_watch_interval_seconds` {#function-set-watch-interval-seconds}

<div class="function-declaration" id="function-set-watch-interval-seconds-1"><pre><code><span class="k">void</span> set_watch_interval_seconds(
    <a href="../../core/types.html#symbol-f64">f64</a> seconds) <span class="k">noexcept</span>;</code></pre></div>

<p>Polls the write time of the assembly and of the .pdb next to it (the newer of the two) every <code>seconds</code> inside <code><a href="#function-tick-1">tick</a>()</code> and requests a reload when it changed; 0 (default) disables the watch. <code>A</code> reload the watch requested that fails with <code>ASSEMBLY_LOAD_FAILED</code> (a build still writing or locking the files) is retried up to 3 times, one debounce window apart (a <code>WARN</code> each); a reload asked for with <code><a href="#function-request-1">request</a>()</code> is not retried.</p>

#### `state` {#function-state}

<div class="function-declaration" id="function-state-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../hot-reload.html#symbol-reload-state">reload_state</a> state() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `tick` {#function-tick}

<div class="function-declaration" id="function-tick-1"><pre><code><span class="k">void</span> tick(
    <a href="../../core/types.html#symbol-f64">f64</a> nowSeconds);</code></pre></div>

<p>Call once per frame before the first script phase with the frame time; performs a due reload (and polls the assembly file when watching).</p>

#### `watch_interval_seconds` {#function-watch-interval-seconds}

<div class="function-declaration" id="function-watch-interval-seconds-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-f64">f64</a> watch_interval_seconds() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

### Private

#### `perform_reload` {#function-perform-reload}

<div class="function-declaration" id="function-perform-reload-1"><pre><code><span class="k">void</span> perform_reload();</code></pre></div>

#### `poll_assembly_file` {#function-poll-assembly-file}

<div class="function-declaration" id="function-poll-assembly-file-1"><pre><code><span class="k">void</span> poll_assembly_file(
    <a href="../../core/types.html#symbol-f64">f64</a> nowSeconds);</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
