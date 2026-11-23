---
kind: class
---

[Engine](../../../engine.md) → [Scene](../../scene.md) → [transform_system.h](../transform-system.md)

# transform_system
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::scene` |
| Include File Path | `engine/include/conjure/scene/transform_system.h` |
| Source File Path | `engine/src/scene/transform_system.cpp` |
| API ↗ | [Scenes, hierarchy and prefabs](../../../../api/scene/index.md), [Scenes and the hierarchy](../../../../api/scene/scenes-and-hierarchy.md) |

## Syntax

```cpp
class CONJURE_API transform_system
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> transform_system(
    <a href="../scene/scene.html">scene</a> &amp;target);</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~transform_system();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mCounters</code></td><td><code class="signature"><a href="transform-system-run-counters.html">run_counters</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Interpolation<wbr>Enabled</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">true</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mJobBounds</code></td><td><code class="signature">std::vector&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mLevelChunks</code></td><td><code class="signature">std::vector&lt;<a href="transform-system-level-chunk.html">level_chunk</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Level<wbr>Queries</code></td><td><code class="signature">std::vector&lt;<a href="../../ecs/query/query.html">ecs::query</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRootQuery</code></td><td><code class="signature"><a href="../../ecs/query/query.html">ecs::query</a></code></td><td>—</td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRunCounter</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mScene</code></td><td><code class="signature"><a href="../scene/scene.html">scene</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mStats</code></td><td><code class="signature"><a href="propagation-stats.html">propagation_stats</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">SYSTEM_NAME</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code></td><td><code class="signature"><span class="s">"scene.propagate_<wbr>transforms"</span></code></td><td>Registered system name.</td></tr>
</tbody>
</table>

## Functions

### Public

#### `describe` {#function-describe}

<div class="function-declaration" id="function-describe-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../ecs/system/system-desc.html">ecs::system_desc</a> describe();</code></pre></div>

<p>The system description: name, <code>TRANSFORM</code> phase, <code>RUNS_WHILE_PAUSED</code>, reads (<code><a href="../components/local-transform.html">local_transform</a></code>, parent, children, <code><a href="../components/interpolated-transform.html">interpolated_transform</a></code>, <code><a href="../components/transform-dirty.html">transform_dirty</a></code>) and writes (<code><a href="../components/world-transform.html">world_transform</a></code>, <code><a href="../components/render-world-transform.html">render_world_transform</a></code>, <code><a href="../components/transform-state.html">transform_state</a></code>); <code><a href="#function-run-1">run</a>()</code> bound to this instance. children is read to mark a recomputed row&#x27;s children pending, so the scheduler keeps a children writer out of the pass&#x27;s stage.</p>

#### `interpolation_enabled` {#function-interpolation-enabled}

<div class="function-declaration" id="function-interpolation-enabled-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> interpolation_enabled() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Whether interpolated entities render the lerp of their poses (<code>true</code> by default).</p>

#### `last_stats` {#function-last-stats}

<div class="function-declaration" id="function-last-stats-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="propagation-stats.html">propagation_stats</a> &amp;last_stats() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Counters of the last run.</p>

#### `run` {#function-run}

<div class="function-declaration" id="function-run-1"><pre><code><span class="k">void</span> run(
    <a href="../../ecs/system/system-context.html">ecs::system_context</a> &amp;context);</code></pre></div>

<p>Runs the propagation pass (the body of <code>scene.propagate_transforms</code>).</p>

#### `run_count` {#function-run-count}

<div class="function-declaration" id="function-run-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> run_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of runs so far.</p>

#### `set_interpolation_enabled` {#function-set-interpolation-enabled}

<div class="function-declaration" id="function-set-interpolation-enabled-1"><pre><code><span class="k">void</span> set_interpolation_enabled(
    <span class="k">bool</span> enabled) <span class="k">noexcept</span>;</code></pre></div>

<p>When disabled, interpolated entities render their current pose (alpha 1).</p>

### Private

#### `collect_chunk` {#function-collect-chunk}

<div class="function-declaration" id="function-collect-chunk-1"><pre><code><span class="k">void</span> collect_chunk(
    <a href="../../ecs/chunk-view/chunk-view.html">ecs::chunk_view</a> &amp;chunk,
    <a href="../../core/types.html#symbol-u64">u64</a> since);</code></pre></div>

#### `mark_children_pending` {#function-mark-children-pending}

<div class="function-declaration" id="function-mark-children-pending-1"><pre><code><span class="k">void</span> mark_children_pending(
    <span class="k">const</span> <a href="../components/children.html">children</a> &amp;list);</code></pre></div>

#### `process_chunk` {#function-process-chunk}

<div class="function-declaration" id="function-process-chunk-1"><pre><code><span class="k">void</span> process_chunk(
    <span class="k">const</span> <a href="transform-system-level-chunk.html">level_chunk</a> &amp;item,
    <a href="../../core/types.html#symbol-u32">u32</a> runId,
    <a href="../../core/types.html#symbol-f32">f32</a> alpha,
    <a href="transform-system-chunk-tally.html">chunk_tally</a> &amp;tally);</code></pre></div>

#### `run_level` {#function-run-level}

<div class="function-declaration" id="function-run-level-1"><pre><code><span class="k">void</span> run_level(
    <a href="../../ecs/query/query.html">ecs::query</a> &amp;level,
    <a href="../../ecs/system/system-context.html">ecs::system_context</a> &amp;context,
    <a href="../../core/types.html#symbol-u32">u32</a> runId,
    <a href="../../core/types.html#symbol-u64">u64</a> since,
    <a href="../../core/types.html#symbol-f32">f32</a> alpha);</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
