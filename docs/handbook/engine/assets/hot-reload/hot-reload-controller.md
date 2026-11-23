---
kind: class
---

[Engine](../../../engine.md) → [Assets](../../assets.md) → [hot_reload.h](../hot-reload.md)

# hot_reload_controller
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::assets` |
| Include File Path | `engine/include/conjure/assets/hot_reload.h` |
| Source File Path | `engine/src/assets/hot_reload.cpp` |
| API ↗ | [Database, importers and hot reload](../../../../api/assets/database-and-import.md), [Assets](../../../../api/assets/index.md) |

## Syntax

```cpp
class CONJURE_API hot_reload_controller
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">hot_reload_controller(
    <a href="../asset-database/asset-database.html">asset_database</a> &amp;database,
    <a href="../asset-manager/asset-manager.html">asset_manager</a> &amp;manager,
    <a href="../file-watcher/file-watcher.html">file_watcher</a> &amp;watcher,
    <a href="../../core/jobs/job-system/jobs-job-system.html">core::jobs::job_system</a> &amp;jobs);</code></td><td></td></tr>
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
<tr><td>private</td><td><code class="signature">mChanges</code></td><td><code class="signature">std::vector&lt;<a href="../file-watcher/file-change.html">file_change</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mDatabase</code></td><td><code class="signature"><a href="../asset-database/asset-database.html">asset_database</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mDeferred</code></td><td><code class="signature">std::vector&lt;<a href="../asset-types.html#symbol-asset-id">asset_id</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mJobs</code></td><td><code class="signature"><a href="../../core/jobs/job-system/jobs-job-system.html">core::jobs::job_system</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mLastReport</code></td><td><code class="signature"><a href="hot-reload-report.html">hot_reload_report</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mManager</code></td><td><code class="signature"><a href="../asset-manager/asset-manager.html">asset_manager</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mWatcher</code></td><td><code class="signature"><a href="../file-watcher/file-watcher.html">file_watcher</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `deferred_count` {#function-deferred-count}

<div class="function-declaration" id="function-deferred-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-usize">usize</a> deferred_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Changed assets waiting for their in-flight load to finish before they reload.</p>

#### `last_report` {#function-last-report}

<div class="function-declaration" id="function-last-report-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="hot-reload-report.html">hot_reload_report</a> &amp;last_report() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `reloads_this_frame` {#function-reloads-this-frame}

<div class="function-declaration" id="function-reloads-this-frame-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> reloads_this_frame() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p><code>reload()</code> calls made by the last <code><a href="#function-update-1">update</a>()</code>.</p>

#### `update` {#function-update}

<div class="function-declaration" id="function-update-1"><pre><code><span class="k">void</span> update();</code></pre></div>

<p>Polls the watcher, classifies changes (a removed source marks its record <code>MISSING_SOURCE</code>, a re-added one restores it; a .meta change counts only when its importer or settings differ from the record&#x27;s, or the owner is not <code>OK</code> or stale), re-imports dirty sources (plus slots <code>FAILED</code> with <code>CACHE_MISS</code> whose record is <code>OK</code>: a source whose import failed waits for a change) and reloads changed assets and flagged dependents. <code>A</code> changed asset whose slot is still in flight is reloaded by a later <code><a href="#function-update-1">update</a>()</code>, once it is <code>LOADED</code>.</p>

### Private

#### `collect_dirty` {#function-collect-dirty}

<div class="function-declaration" id="function-collect-dirty-1"><pre><code><span class="k">void</span> collect_dirty(
    std::vector&lt;<a href="../asset-types.html#symbol-asset-id">asset_id</a>&gt; &amp;outDirty);</code></pre></div>

#### `reload_deferred` {#function-reload-deferred}

<div class="function-declaration" id="function-reload-deferred-1"><pre><code><span class="k">void</span> reload_deferred();</code></pre></div>

#### `reload_or_defer` {#function-reload-or-defer}

<div class="function-declaration" id="function-reload-or-defer-1"><pre><code><span class="k">void</span> reload_or_defer(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id);</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
