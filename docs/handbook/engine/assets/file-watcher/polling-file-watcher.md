---
kind: class
---

[Engine](../../../engine.md) → [Assets](../../assets.md) → [file_watcher.h](../file-watcher.md)

# polling_file_watcher
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::assets` |
| Include File Path | `engine/include/conjure/assets/file_watcher.h` |
| Source File Path | `engine/src/assets/file_watcher.cpp` |
| API ↗ | [Database, importers and hot reload](../../../../api/assets/database-and-import.md) |

## Syntax

```cpp
class CONJURE_API polling_file_watcher final : public file_watcher
```

## Inheritance Hierarchy


[`file_watcher`](file-watcher.md) → `polling_file_watcher`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">polling_file_watcher(
    <a href="../../core/vfs/file-system/vfs-file-system.html">core::vfs::file_system</a> &amp;fileSystem,
    <a href="../../core/jobs/job-system/jobs-job-system.html">core::jobs::job_system</a> &amp;jobs,
    <a href="../../core/types.html#symbol-f64">f64</a> intervalSeconds = DEFAULT_WATCH_INTERVAL_SECONDS);</code></td><td>Watches through <code>fileSystem</code> and runs its passes on jobs. <code>intervalSeconds</code> is the least time between two passes <code>poll()</code> starts; with 0 every <code>poll()</code> starts one once the previous pass has finished. NaN and negative values count as 0, values above 1e9 seconds (infinity included) as 1e9.</td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~polling_file_watcher() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mFileSystem</code></td><td><code class="signature"><a href="../../core/vfs/file-system/vfs-file-system.html">core::vfs::file_system</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mHasBaseline</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mHasPassed</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mInterval</code></td><td><code class="signature">std::<wbr>chrono::<wbr>steady_<wbr>clock::<wbr>duration</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Interval<wbr>Seconds</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f64">f64</a></code></td><td><code class="signature">DEFAULT_<wbr>WATCH_<wbr>INTERVAL_<wbr>SECONDS</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mJobs</code></td><td><code class="signature"><a href="../../core/jobs/job-system/jobs-job-system.html">core::jobs::job_system</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Last<wbr>Pass<wbr>Failure</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Last<wbr>Pass<wbr>Start</code></td><td><code class="signature">std::<wbr>chrono::<wbr>steady_<wbr>clock::<wbr>time_<wbr>point</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mMutex</code></td><td><code class="signature">std::mutex</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPass</code></td><td><code class="signature"><a href="../../core/jobs/job-handle/jobs-job-handle.html">core::jobs::job_handle</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPassResult</code></td><td><code class="signature"><a href="../../core/containers/flat-hash-map/flat-hash-map.html">core::<wbr>flat_<wbr>hash_<wbr>map</a>&lt;<wbr>std::<wbr>string,<wbr> <a href="polling-file-watcher-file-state.html">file_<wbr>state</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Pass<wbr>Result<wbr>Ready</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPending</code></td><td><code class="signature"><a href="../../core/containers/flat-hash-map/flat-hash-map.html">core::<wbr>flat_<wbr>hash_<wbr>map</a>&lt;<wbr>std::<wbr>string,<wbr> <a href="polling-file-watcher-file-state.html">file_<wbr>state</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Pending<wbr>Removal</code></td><td><code class="signature"><a href="../../core/containers/flat-hash-map/flat-hash-map.html">core::<wbr>flat_<wbr>hash_<wbr>map</a>&lt;<wbr>std::<wbr>string,<wbr> <span class="k">bool</span>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mReady</code></td><td><code class="signature">std::vector&lt;<a href="file-change.html">file_change</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRoots</code></td><td><code class="signature">std::vector&lt;std::string&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSnapshot</code></td><td><code class="signature"><a href="../../core/containers/flat-hash-map/flat-hash-map.html">core::<wbr>flat_<wbr>hash_<wbr>map</a>&lt;<wbr>std::<wbr>string,<wbr> <a href="polling-file-watcher-file-state.html">file_<wbr>state</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `add_root` {#function-add-root}

<div class="function-declaration" id="function-add-root-1"><pre><code><span class="k">void</span> add_root(
    std::string_view rootPath) <span class="k">override</span>;</code></pre></div>

#### `interval_seconds` {#function-interval-seconds}

<div class="function-declaration" id="function-interval-seconds-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-f64">f64</a> interval_seconds() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The interval between passes in seconds, as the constructor clamped it.</p>

#### `poll` {#function-poll}

<div class="function-declaration" id="function-poll-1"><pre><code><span class="k">void</span> poll(
    std::vector&lt;<a href="file-change.html">file_change</a>&gt; &amp;outChanges) <span class="k">override</span>;</code></pre></div>

#### `scan_now` {#function-scan-now}

<div class="function-declaration" id="function-scan-now-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; scan_now();</code></pre></div>

<p>One synchronous stat pass on the caller; changes become visible to the next <code><a href="#function-poll-1">poll</a>()</code>. Fails when a root cannot be enumerated.</p>

#### `snapshot` {#function-snapshot}

<div class="function-declaration" id="function-snapshot-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; snapshot() <span class="k">override</span>;</code></pre></div>

#### `tracked_file_count` {#function-tracked-file-count}

<div class="function-declaration" id="function-tracked-file-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-usize">usize</a> tracked_file_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

### Private

#### `apply_pass` {#function-apply-pass}

<div class="function-declaration" id="function-apply-pass-1"><pre><code><span class="k">void</span> apply_pass(
    <a href="../../core/containers/flat-hash-map/flat-hash-map.html">core::flat_hash_map</a>&lt;std::string, <a href="polling-file-watcher-file-state.html">file_state</a>&gt; &amp;&amp;current);</code></pre></div>

#### `stat_roots` {#function-stat-roots}

<div class="function-declaration" id="function-stat-roots-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../../core/containers/flat-hash-map/flat-hash-map.html">core::flat_hash_map</a>&lt;std::string, <a href="polling-file-watcher-file-state.html">file_state</a>&gt;&gt; stat_roots() <span class="k">const</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="function-interval"></a>
<a id="function-interval-1"></a>
<!-- doc-legacy-anchors:end -->
