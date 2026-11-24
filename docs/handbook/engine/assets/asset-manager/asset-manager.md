---
kind: class
---

[Engine](../../../engine.md) → [Assets](../../assets.md) → [asset_manager.h](../asset-manager.md)

# asset_manager
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::assets` |
| Include File Path | `engine/include/conjure/assets/asset_manager.h` |
| Source File Path | `engine/src/assets/asset_manager.cpp` |
| API ↗ | [Cooking, caches and packs](../../../../api/assets/cooking-and-packs.md), [Database, importers and hot reload](../../../../api/assets/database-and-import.md), [Assets](../../../../api/assets/index.md), [Referencing and loading assets](../../../../api/assets/referencing-and-loading.md) |

## Syntax

```cpp
class CONJURE_API asset_manager
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> asset_manager(
    <a href="asset-manager-desc.html">asset_manager_desc</a> desc);</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~asset_manager();</code></td><td>Releases internal handles, destroys payloads and asserts that no external strong reference is left (logs and continues in Distribution).</td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mState</code></td><td><code class="signature">std::unique_ptr&lt;<a href="../../core/event/event-state.html">state</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `add_source` {#function-add-source}

<div class="function-declaration" id="function-add-source-1"><pre><code><span class="k">void</span> add_source(
    std::unique_ptr&lt;<a href="../asset-source/asset-source.html">asset_source</a>&gt; source);</code></pre></div>

<p>Adds a secondary source searched after the primary (plugin packs).</p>

#### `begin_frame` {#function-begin-frame}

<div class="function-declaration" id="function-begin-frame-1"><pre><code><span class="k">void</span> begin_frame(
    <a href="../../core/types.html#symbol-u64">u64</a> frameIndex);</code></pre></div>

#### `bind` {#function-bind}

<div class="function-declaration" id="function-bind-1"><pre><code><span class="k">template</span> &lt;<a href="../asset-types.html#symbol-declared-asset-type">declared_asset_type</a> AssetType&gt; [[<span class="na">nodiscard</span>]] <a href="../asset-ref/asset-ref.html">asset_ref</a>&lt;AssetType&gt; bind(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id);</code></pre></div>

<p>Finds or creates the slot and binds a typed reference. When the slot&#x27;s type is not yet known it is taken from the source, else from AssetType; a recorded type that differs from AssetType logs an ERROR once per uuid and yields an unbound reference.</p>

#### `bind_slot` {#function-bind-slot}

<div class="function-declaration" id="function-bind-slot-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../asset-slot/asset-slot.html">asset_slot</a> *bind_slot(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id);</code></pre></div>

<p>Untyped bind for the scene bind pass. Null for the <code>null</code> uuid.</p>

#### `collect` {#function-collect}

<div class="function-declaration" id="function-collect-1"><pre><code><a href="../../core/types.html#symbol-u32">u32</a> collect();</code></pre></div>

<p>Unloads every <code>LOADED</code> slot with zero strong references that is not pinned, releasing its dependency handles, until nothing changes. Returns how many were unloaded.</p>

#### `end_frame` {#function-end-frame}

<div class="function-declaration" id="function-end-frame-1"><pre><code><span class="k">void</span> end_frame();</code></pre></div>

#### `failed_ids` {#function-failed-ids}

<div class="function-declaration" id="function-failed-ids-1"><pre><code>[[<span class="na">nodiscard</span>]] std::vector&lt;<a href="../asset-types.html#symbol-asset-id">asset_id</a>&gt; failed_ids(
    <a href="../assets-error.html#symbol-assets-error">assets_error</a> code) <span class="k">const</span>;</code></pre></div>

<p>Slots that are <code>FAILED</code> with the given code (the hot reload controller repairs <code>CACHE_MISS</code>). Main thread.</p>

#### `fallback` {#function-fallback}

<div class="function-declaration" id="function-fallback-1"><pre><code><span class="k">template</span> &lt;<a href="../asset-types.html#symbol-declared-asset-type">declared_asset_type</a> AssetType&gt; [[<span class="na">nodiscard</span>]] AssetType *fallback() <span class="k">noexcept</span>;</code></pre></div>

<p>The type&#x27;s fallback payload, created lazily from its loader; <code>nullptr</code> when the type is unregistered or has no fallback. Pinned for the manager&#x27;s lifetime.</p>

#### `find` {#function-find}

<div class="function-declaration" id="function-find-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="../asset-types.html#symbol-asset-id">asset_id</a>&gt; find(
    std::string_view assetPath) <span class="k">const</span>;</code></pre></div>

<p>Resolves &quot;mount://path#sub&quot; through the sources.</p>

#### `find_slot` {#function-find-slot}

<div class="function-declaration" id="function-find-slot-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../asset-slot/asset-slot.html">asset_slot</a> *find_slot(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The slot of a uuid without creating one: <code>nullptr</code> until something binds it. Read-only callers (the scripting bindings&#x27; state queries) use it so a uuid a script merely asks about never costs a permanent slot.</p>

#### `flush` {#function-flush}

<div class="function-declaration" id="function-flush-1"><pre><code><span class="k">void</span> flush();</code></pre></div>

<p>Runs <code><a href="#function-update-1">update</a>()</code> until every queued load is terminal.</p>

#### `frame_index` {#function-frame-index}

<div class="function-declaration" id="function-frame-index-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u64">u64</a> frame_index() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `get_or_fallback` {#function-get-or-fallback}

<div class="function-declaration" id="function-get-or-fallback-1"><pre><code><span class="k">template</span> &lt;<a href="../asset-types.html#symbol-declared-asset-type">declared_asset_type</a> AssetType&gt; [[<span class="na">nodiscard</span>]] AssetType *get_or_fallback(
    <span class="k">const</span> <a href="../asset-ref/asset-ref.html">asset_ref</a>&lt;AssetType&gt; &amp;ref) <span class="k">noexcept</span>;</code></pre></div>

<p><code>ref.get()</code>, or the type&#x27;s fallback while the asset is not <code>LOADED</code>.</p>

#### `is_pinned` {#function-is-pinned}

<div class="function-declaration" id="function-is-pinned-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_pinned(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `is_shut_down` {#function-is-shut-down}

<div class="function-declaration" id="function-is-shut-down-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_shut_down() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `last_error` {#function-last-error}

<div class="function-declaration" id="function-last-error-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../../core/error/error.html">core::error</a> *last_error(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The error of a <code>FAILED</code> slot (or of the last failed reload, until <code><a href="#function-collect-1">collect</a>()</code> unloads the slot), else <code>nullptr</code>. Valid until the slot changes state.</p>

#### `load` {#function-load}

<div class="function-declaration" id="function-load-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../asset-ref/asset-handle.html">asset_handle</a> load(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id,
    <a href="../asset-types.html#symbol-asset-load-priority">asset_load_priority</a> priority = asset_load_priority::NORMAL);</code></pre></div>

<p>Queues a load (<code>UNLOADED</code> -&gt; <code>QUEUED</code>) and returns a handle. Other states are left alone (<code>FAILED</code> needs <code><a href="#function-retry-failed-1">retry_failed</a>()</code>). Unbound handle for the <code>null</code> uuid.</p>

#### `load_batch` {#function-load-batch}

<div class="function-declaration" id="function-load-batch-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="asset-load-batch.html">asset_load_batch</a> load_batch(
    std::span&lt;<span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a>&gt; ids,
    <a href="../asset-types.html#symbol-asset-load-priority">asset_load_priority</a> priority = asset_load_priority::NORMAL);</code></pre></div>

#### `load_blocking` {#function-load-blocking}

<div class="function-declaration" id="function-load-blocking-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../asset-ref/asset-handle.html">asset_handle</a>&gt; load_blocking(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id,
    <a href="../asset-manager.html#symbol-blocking-load-warning">blocking_load_warning</a> warning = blocking_load_warning::WARN_INSIDE_FRAME);</code></pre></div>

<p>Main thread. Runs the whole pipeline inline, dependencies first, and returns the <code>LOADED</code> handle or the failure (the slot is then <code>FAILED</code>). Waits for an in-flight job up to <code>blockingTimeoutSeconds</code> (<code>TIMEOUT</code>; the slot then stays with the async pipeline, which finishes it on a later <code><a href="#function-update-1">update</a>()</code>). Inside a frame it logs one <code>WARN</code> per frame unless <code>warning</code> is <code>blocking_load_warning::EXPECTED</code>; an <code>EXPECTED</code> call does not use up the frame&#x27;s <code>WARN</code>.</p>

#### `make_handle` {#function-make-handle}

<div class="function-declaration" id="function-make-handle-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../asset-ref/asset-handle.html">asset_handle</a> make_handle(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id);</code></pre></div>

<p>An owning handle on the slot; binds, never loads. Unbound for the <code>null</code> uuid.</p>

#### `pin` {#function-pin}

<div class="function-declaration" id="function-pin-1"><pre><code><span class="k">void</span> pin(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id,
    <span class="k">bool</span> pinned);</code></pre></div>

<p><code>A</code> pinned slot is never unloaded by <code><a href="#function-collect-1">collect</a>()</code>.</p>

#### `reload` {#function-reload}

<div class="function-declaration" id="function-reload-1"><pre><code><span class="k">void</span> reload(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id);</code></pre></div>

<p><code>LOADED</code> -&gt; <code>RELOADING</code> through the normal pipeline; the old payload stays published until the new one finalises. Ignored in other states.</p>

#### `retry_failed` {#function-retry-failed}

<div class="function-declaration" id="function-retry-failed-1"><pre><code><span class="k">void</span> retry_failed(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id);</code></pre></div>

<p><code>FAILED</code> -&gt; <code>QUEUED</code>. Ignored in other states.</p>

#### `shutdown` {#function-shutdown}

<div class="function-declaration" id="function-shutdown-1"><pre><code><span class="k">void</span> shutdown();</code></pre></div>

<p>Refuses new work, waits for in-flight jobs, discards their completions and returns queued slots to <code>UNLOADED</code>.</p>

#### `state_of` {#function-state-of}

<div class="function-declaration" id="function-state-of-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../asset-types.html#symbol-asset-state">asset_state</a> state_of(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p><code>UNLOADED</code> for unknown uuids.</p>

#### `stats` {#function-stats}

<div class="function-declaration" id="function-stats-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="asset-manager-stats.html">asset_manager_stats</a> stats() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `subscribe` {#function-subscribe}

<div class="function-declaration" id="function-subscribe-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="asset-subscription.html">asset_subscription</a> subscribe(
    <a href="../asset-manager.html#symbol-asset-event-callback">asset_event_callback</a> callback);</code></pre></div>

#### `swap_source` {#function-swap-source}

<div class="function-declaration" id="function-swap-source-1"><pre><code><span class="k">void</span> swap_source(
    std::unique_ptr&lt;<a href="../asset-source/asset-source.html">asset_source</a>&gt; source);</code></pre></div>

<p>Replaces the primary source (editor: cache -&gt; pack preview). Loaded payloads stay. Safe while loads are in flight: a job reading from the old source finishes its read first, and jobs that have not started resolve the new source.</p>

#### `type_of` {#function-type-of}

<div class="function-declaration" id="function-type-of-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="../asset-types.html#symbol-asset-type-id">asset_type_id</a>&gt; type_of(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id) <span class="k">const</span>;</code></pre></div>

<p>The asset type of a uuid: the bound slot&#x27;s recorded type, else the first source that contains it; nullopt when no slot records it and no source knows it. Never binds.</p>

#### `types` {#function-types}

<div class="function-declaration" id="function-types-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../asset-type-registry/asset-type-registry.html">asset_type_registry</a> &amp;types() <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-types-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../asset-type-registry/asset-type-registry.html">asset_type_registry</a> &amp;types() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `unsubscribe` {#function-unsubscribe}

<div class="function-declaration" id="function-unsubscribe-1"><pre><code><span class="k">void</span> unsubscribe(
    <a href="asset-subscription.html">asset_subscription</a> subscription);</code></pre></div>

<p>Takes effect immediately, including for events the current <code><a href="#function-update-1">update</a>()</code> has not delivered yet.</p>

#### `update` {#function-update}

<div class="function-declaration" id="function-update-1"><pre><code><span class="k">void</span> update();</code></pre></div>

<p>Dispatches queued loads, drains completions, finalises under the budget (at least one slot per call), destroys retired payloads and fires events.</p>

### Private

#### `bind_typed` {#function-bind-typed}

<div class="function-declaration" id="function-bind-typed-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../asset-slot/asset-slot.html">asset_slot</a> *bind_typed(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id,
    <a href="../asset-types.html#symbol-asset-type-id">asset_type_id</a> type);</code></pre></div>

#### `fallback_payload` {#function-fallback-payload}

<div class="function-declaration" id="function-fallback-payload-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">void</span> *fallback_payload(
    <a href="../asset-types.html#symbol-asset-type-id">asset_type_id</a> type) <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
