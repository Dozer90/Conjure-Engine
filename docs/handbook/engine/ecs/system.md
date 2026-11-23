---
kind: header
---

[Engine](../../engine.md) → [ECS](../ecs.md)

# system.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/ecs/system.h` |
| API ↗ | [Command buffers and observers](../../../api/ecs/command-buffers-and-observers.md), [Queries, views and systems](../../../api/ecs/queries-views-and-systems.md) |

<p>Systems.</p>

## Structs

| File | Description |
|---|---|
| [`system_desc`](system/system-desc.md) | Everything the scheduler needs to run one system. |
| [`system_scratch`](system/system-scratch.md) | Per-system scratch storage reused across runs: grain command buffers and the chunk list of <code>parallel_for_chunks</code>. Owned by the scheduler&#x27;s system record (or by the caller of a standalone <code><a href="system/system-context.html">system_context</a></code>). |
| [`system_scratch::chunk_ref`](system/system-scratch-chunk-ref.md) | One flattened chunk of a <code>parallel_for_chunks</code> dispatch. |

## Classes

| File | Description |
|---|---|
| [`system_context`](system/system-context.md) | The arguments of one system run. |

## Enumerations

### `system_flags` {#symbol-system-flags}

```cpp
enum class system_flags : u32 {
    NONE = 0,
    MAIN_THREAD = 1 << 0,            // runs on the run_phase caller thread, last in its stage
    EXCLUSIVE = 1 << 1,              // alone in its stage, receives an unlocked world
    DISABLED = 1 << 2,
    RUNS_WHILE_PAUSED = 1 << 3       // selected by run_phase_filtered while the engine is PAUSED
};
```

Execution properties of a system. Neither thread bit set means PARALLEL: the system runs as a job alongside the other systems of its stage.

## Forward Declarations

### `scheduler` {#symbol-scheduler}

```cpp
class scheduler;
```

## Macros

### `CONJURE_ENUM_FLAGS` {#symbol-conjure-enum-flags}

```cpp
CONJURE_ENUM_FLAGS(system_flags)
```

## Functions

### Public

#### `detail::collect_pack_access` {#function-detail-collect-pack-access}

<div class="function-declaration" id="function-detail-collect-pack-access-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> PackType&gt; <span class="k">void</span> collect_pack_access(<span class="k">const</span> <a href="component-registry/component-registry.html">component_registry</a> &amp;registry, <a href="component/component-mask.html">component_mask</a> &amp;reads, <a href="component/component-mask.html">component_mask</a> &amp;writes);</code></pre></div>

#### `make_system` {#function-make-system}

<div class="function-declaration" id="function-make-system-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span>... ComponentTypes&gt; [[<span class="na">nodiscard</span>]] <a href="system/system-desc.html">system_desc</a> make_system(
    <a href="../core/string-id/string-id.html">core::string_id</a> name,
    <a href="system-phase.html#symbol-system-phase">system_phase</a> target,
    std::function&lt;<span class="k">void</span>(<a href="system/system-context.html">system_context</a> &amp;context)&gt; run);</code></pre></div>

<p><code>A</code> <code><a href="system/system-desc.html">system_desc</a></code> whose reads/writes derive from the pack (const <code>T</code> read, <code>T</code> write, <code>maybe&lt;T&gt;</code> by constness) so the common one-view system declares nothing by hand. The pack is resolved against the world&#x27;s registry when the system is added; a type that is not registered yet is left out and joins the access set once it is (the scheduler resolves again when the registry has grown).</p>

#### `parallel_for_each_chunk` {#function-parallel-for-each-chunk}

<div class="function-declaration" id="function-parallel-for-each-chunk-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span>... ComponentTypes&gt; <span class="k">template</span> &lt;<span class="k">typename</span> Callable&gt; <span class="k">void</span> <a href="view/view.html">view</a>&lt;ComponentTypes...&gt;::parallel_for_each_chunk(
    <a href="system/system-context.html">system_context</a> &amp;context,
    Callable &amp;&amp;callable);</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="collect_pack_access"></a>
<a id="conjure_enum_flags"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="detailcollect_pack_access"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="make_system"></a>
<a id="other-declarations"></a>
<a id="parallel_for_each_chunk"></a>
<a id="scheduler"></a>
<a id="structs-and-types"></a>
<a id="system_context"></a>
<a id="system_contextcommands"></a>
<a id="system_contextdelta_time"></a>
<a id="system_contextdesc"></a>
<a id="system_contextdispatch_chunks"></a>
<a id="system_contextframe"></a>
<a id="system_contextjobs"></a>
<a id="system_contextlast_grain_count"></a>
<a id="system_contextmcommands"></a>
<a id="system_contextmdesc"></a>
<a id="system_contextmframe"></a>
<a id="system_contextmjobs"></a>
<a id="system_contextmlastgraincount"></a>
<a id="system_contextmminentitiesperjob"></a>
<a id="system_contextmparallelchunks"></a>
<a id="system_contextmrunversion"></a>
<a id="system_contextmscratch"></a>
<a id="system_contextmsinceversion"></a>
<a id="system_contextmworld"></a>
<a id="system_contextoperator"></a>
<a id="system_contextparallel_for_chunks"></a>
<a id="system_contextrun_version"></a>
<a id="system_contextsince_version"></a>
<a id="system_contextsystem_context"></a>
<a id="system_contextsystem_context_1"></a>
<a id="system_contextvalidate_access"></a>
<a id="system_contextview"></a>
<a id="system_contextworld"></a>
<a id="system_desc"></a>
<a id="system_descafter"></a>
<a id="system_descbefore"></a>
<a id="system_descflags"></a>
<a id="system_descname"></a>
<a id="system_descphase"></a>
<a id="system_descreads"></a>
<a id="system_descvoid"></a>
<a id="system_descwrites"></a>
<a id="system_flags"></a>
<a id="system_flagsdisabled"></a>
<a id="system_flagsexclusive"></a>
<a id="system_flagsmain_thread"></a>
<a id="system_flagsnone"></a>
<a id="system_flagsruns_while_paused"></a>
<a id="system_scratch"></a>
<a id="system_scratchchunk_ref"></a>
<a id="system_scratchchunk_refchunkindex"></a>
<a id="system_scratchchunk_refowner"></a>
<a id="system_scratchchunks"></a>
<a id="system_scratchgrainbuffers"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
