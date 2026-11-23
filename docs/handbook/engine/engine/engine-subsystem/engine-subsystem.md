---
kind: class
---

[Engine](../../../engine.md) → [Engine](../../engine.md) → [engine_subsystem.h](../engine-subsystem.md)

# engine_subsystem
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::ecs` |
| Include File Path | `engine/include/conjure/engine/engine_subsystem.h` |
| Source File Path | `engine/src/engine/engine_subsystem.cpp` |
| API ↗ | [The frame, layers and time](../../../../api/engine/frame-layers-and-time.md), [Engine: the application and the frame](../../../../api/engine/index.md) |

## Syntax

```cpp
class CONJURE_API engine_subsystem
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">engine_subsystem();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">virtual</span> ~engine_subsystem();</code></td><td></td></tr>
</tbody>
</table>



## Functions

### Public

#### `begin_frame` {#function-begin-frame}

<div class="function-declaration" id="function-begin-frame-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> begin_frame(
    <a href="../engine-context/engine-context.html">engine_context</a> &amp;context);</code></pre></div>

<p>Main thread, frame step 5 (before <code>FRAME_BEGIN</code>).</p>

#### `end_frame` {#function-end-frame}

<div class="function-declaration" id="function-end-frame-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> end_frame(
    <a href="../engine-context/engine-context.html">engine_context</a> &amp;context);</code></pre></div>

<p>Main thread, frame step 13 (after <code>FRAME_END</code>).</p>

#### `name` {#function-name}

<div class="function-declaration" id="function-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> std::string_view name() <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

<p>The name used in startup and shutdown logs (&quot;platform&quot;, &quot;renderer&quot;, ...).</p>

#### `post_startup` {#function-post-startup}

<div class="function-declaration" id="function-post-startup-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; post_startup(
    <a href="../engine-context/engine-context.html">engine_context</a> &amp;context);</code></pre></div>

<p>Runs after every subsystem&#x27;s startup (step 17): the assets scan, the scripting handshake.</p>

#### `register_systems` {#function-register-systems}

<div class="function-declaration" id="function-register-systems-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> register_systems(
    <a href="../../ecs/scheduler/scheduler.html">ecs::scheduler</a> &amp;scheduler,
    <a href="../engine-context/engine-context.html">engine_context</a> &amp;context);</code></pre></div>

<p>Adds this subsystem&#x27;s systems to the scheduler (step 18); the application finalises afterwards.</p>

#### `shutdown` {#function-shutdown}

<div class="function-declaration" id="function-shutdown-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> shutdown(
    <a href="../engine-context/engine-context.html">engine_context</a> &amp;context) = <span class="mi">0</span>;</code></pre></div>

<p>Destroys the module object and clears its context pointer. Must be safe after a failed or never attempted startup and when called twice.</p>

#### `startup` {#function-startup}

<div class="function-declaration" id="function-startup-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; startup(
    <a href="../engine-context/engine-context.html">engine_context</a> &amp;context) = <span class="mi">0</span>;</code></pre></div>

<p>Startup step of this subsystem (3.4). Creates the module object, publishes its pointer in the context. On failure the already-started subsystems are shut down in reverse by the application.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->
