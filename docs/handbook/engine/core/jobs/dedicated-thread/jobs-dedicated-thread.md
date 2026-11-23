---
kind: class
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [dedicated_thread.h](../dedicated-thread.md)

# jobs::dedicated_thread
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core::jobs` |
| Include File Path | `engine/include/conjure/core/jobs/dedicated_thread.h` |
| Source File Path | `engine/src/core/jobs/dedicated_thread.cpp` |
| API ↗ | [Jobs and threading](../../../../../api/core/jobs.md) |

## Syntax

```cpp
class CONJURE_API dedicated_thread
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">dedicated_thread(
    std::string name,
    std::function&lt;<span class="k">void</span>(std::stop_token)&gt; body);</code></td><td>Starts the thread immediately. The name is applied to the OS thread (truncated on a code point boundary to 15 bytes on Linux and macOS and 63 UTF-16 units on Windows). An empty body asserts and leaves the object non-joinable. If the operating system refuses to create the thread, the <code>std::system_error</code> that <code>std::jthread</code> throws is not caught (engine code never catches) and ends the process like an allocation failure.</td></tr>
<tr><td>public</td><td><code class="signature">dedicated_thread(
    <a href="jobs-dedicated-thread.html">dedicated_thread</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Additional constructor overloads have been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~dedicated_thread();</code></td><td>Requests a stop and joins when the thread is still running.</td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mName</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mThread</code></td><td><code class="signature">std::jthread</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `get_id` {#function-get-id}

<div class="function-declaration" id="function-get-id-1"><pre><code>[[<span class="na">nodiscard</span>]] std::thread::id get_id() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Identifier of the underlying thread; default-constructed when not joinable.</p>

#### `join` {#function-join}

<div class="function-declaration" id="function-join-1"><pre><code><span class="k">void</span> join();</code></pre></div>

<p>Waits for the body to return. Does not request a stop; no-op when not joinable.</p>

#### `joinable` {#function-joinable}

<div class="function-declaration" id="function-joinable-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> joinable() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True while the thread has been started and not yet joined.</p>

#### `name` {#function-name}

<div class="function-declaration" id="function-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::string &amp;name() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The name given at construction.</p>

#### `request_stop` {#function-request-stop}

<div class="function-declaration" id="function-request-stop-1"><pre><code><span class="k">void</span> request_stop() <span class="k">noexcept</span>;</code></pre></div>

<p>Signals the body&#x27;s stop token. Does not wait.</p>

#### `stop_requested` {#function-stop-requested}

<div class="function-declaration" id="function-stop-requested-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> stop_requested() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True once <code><a href="#function-request-stop-1">request_stop</a>()</code> has been called (or the destructor has started).</p>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="jobs-dedicated-thread.html">dedicated_thread</a> &amp;<span class="k">operator</span>=(
    <a href="jobs-dedicated-thread.html">dedicated_thread</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
