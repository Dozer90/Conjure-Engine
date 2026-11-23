---
kind: class
---

[Engine](../../../engine.md) → [Core](../../core.md) → [process.h](../process.md)

# process
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/process.h` |
| Source File Path | `engine/src/core/system/process.cpp` |
| API ↗ | [Processes, libraries and images](../../../../api/core/system.md) |

## Syntax

```cpp
class CONJURE_API process
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">process();</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">process(
    <a href="process.html">process</a> &amp;&amp;other) <span class="k">noexcept</span>;</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~process();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">KILLED_<wbr>EXIT_<wbr>CODE</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../types.html#symbol-i32">i32</a></code></td><td><code class="signature">-<span class="mi">1</span></code></td><td>Exit code reported for a child killed by <code>kill()</code> or by the timeout, and for a child whose exit status was lost (POSIX: SIGCHLD ignored by the host, or the child reaped elsewhere), so a lost status never reads as success.</td></tr>
<tr><td>public</td><td><code class="signature">MAX_<wbr>TIMEOUT_<wbr>SECONDS</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../types.html#symbol-f64">f64</a></code></td><td><code class="signature"><span class="mi">1.0</span>e9</code></td><td>Largest timeout honoured (about 31 years); larger finite values are clamped to it.</td></tr>
<tr><td>private</td><td><code class="signature">mState</code></td><td><code class="signature">std::unique_ptr&lt;<a href="../event/event-state.html">state</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `is_running` {#function-is-running}

<div class="function-declaration" id="function-is-running-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_running() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True while the child has not exited and <code><a href="#function-wait-1">wait</a>()</code> has not been called.</p>

#### `kill` {#function-kill}

<div class="function-declaration" id="function-kill-1"><pre><code><span class="k">void</span> kill() <span class="k">noexcept</span>;</code></pre></div>

<p>Kills the child and every <code><a href="process.html">process</a></code> it spawned, including descendants that outlived the child itself. No-op once <code><a href="#function-wait-1">wait</a>()</code> returned. <code><a href="#function-wait-1">wait</a>()</code> still has to be called to collect the result.</p>

#### `process_id` {#function-process-id}

<div class="function-declaration" id="function-process-id-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../types.html#symbol-u32">u32</a> process_id() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The operating system&#x27;s identifier of the child, 0 when not started.</p>

#### `read_error` {#function-read-error}

<div class="function-declaration" id="function-read-error-1"><pre><code><a href="../types.html#symbol-usize">usize</a> read_error(
    std::string &amp;out);</code></pre></div>

<p>Appends the standard error captured since the previous call and returns the number of bytes appended. Never blocks.</p>

#### `read_output` {#function-read-output}

<div class="function-declaration" id="function-read-output-1"><pre><code><a href="../types.html#symbol-usize">usize</a> read_output(
    std::string &amp;out);</code></pre></div>

<p>Appends the standard output captured since the previous call and returns the number of bytes appended. Never blocks.</p>

#### `run` {#function-run}

<div class="function-declaration" id="function-run-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../result/result.html">result</a>&lt;<a href="process-result.html">process_result</a>&gt; run(
    <span class="k">const</span> <a href="process-desc.html">process_desc</a> &amp;desc);</code></pre></div>

<p>Starts the child and blocks until it exits or the timeout kills it. Fails with <code>INVALID_ARGUMENT</code> for an inconsistent description, <code>NOT_FOUND</code> when the executable or working directory does not exist, <code>PERMISSION_DENIED</code> or <code>BACKEND_FAILURE</code> when the operating system refuses to start it. <code>A</code> non-zero exit code is not an error.</p>

#### `start` {#function-start}

<div class="function-declaration" id="function-start-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../result/result.html">result</a>&lt;<a href="process.html">process</a>&gt; start(
    <span class="k">const</span> <a href="process-desc.html">process_desc</a> &amp;desc);</code></pre></div>

<p>Starts the child and returns immediately. Same failures as <code><a href="#function-run-1">run</a>()</code>.</p>

#### `wait` {#function-wait}

<div class="function-declaration" id="function-wait-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../result/result.html">result</a>&lt;<a href="process-result.html">process_result</a>&gt; wait();</code></pre></div>

<p>Blocks until the child exits and its captured output is drained (killing the whole tree when the description&#x27;s timeout elapses first), then returns its exit code and the output not yet consumed by <code><a href="#function-read-output-1">read_output</a>()</code> / <code><a href="#function-read-error-1">read_error</a>()</code>. Without a timeout a descendant that inherited the output pipe delays the return until it closes it. Fails with <code>INVALID_STATE</code> when the <code><a href="process.html">process</a></code> was never started or <code><a href="#function-wait-1">wait</a>()</code> was already called.</p>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="process.html">process</a> &amp;<span class="k">operator</span>=(
    <a href="process.html">process</a> &amp;&amp;other) <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
