---
kind: class
---

[Engine](../../../engine.md) → [Assets](../../assets.md) → [asset_database.h](../asset-database.md)

# asset_database
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::assets` |
| Include File Path | `engine/include/conjure/assets/asset_database.h` |
| Source File Path | `engine/src/assets/asset_database.cpp` |
| API ↗ | [Database, importers and hot reload](../../../../api/assets/database-and-import.md), [Assets](../../../../api/assets/index.md) |

## Syntax

```cpp
class CONJURE_API asset_database
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">asset_database(
    <a href="../../core/vfs/file-system/vfs-file-system.html">core::vfs::file_system</a> &amp;fileSystem,
    <a href="../asset-importer/importer-registry.html">importer_registry</a> &amp;importers,
    <span class="k">const</span> <a href="../asset-type-registry/asset-type-registry.html">asset_type_registry</a> &amp;types,
    <a href="../../core/jobs/job-system/jobs-job-system.html">core::jobs::job_system</a> *jobs = nullptr);</code></td><td><code>jobs</code> (optional) parallelises source hashing during <code>scan()</code>; without it hashing runs inline. Imports take their job system as an argument.</td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~asset_database();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mCache</code></td><td><code class="signature">std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../asset-cache/asset-cache.html">asset_<wbr>cache</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mCacheRoot</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mChildren</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr>std::<wbr>vector&lt;<wbr><a href="../../core/types.html#symbol-usize">usize</a>&gt;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mEntries</code></td><td><code class="signature">std::vector&lt;<a href="asset-database-entry.html">entry</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFileSystem</code></td><td><code class="signature"><a href="../../core/vfs/file-system/vfs-file-system.html">core::vfs::file_system</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Forward<wbr>Edges</code></td><td><code class="signature">std::vector&lt;<a href="../../core/types.html#symbol-usize">usize</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mImporters</code></td><td><code class="signature"><a href="../asset-importer/importer-registry.html">importer_registry</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mIndexById</code></td><td><code class="signature"><a href="../../core/containers/flat-hash-map/flat-hash-map.html">core::<wbr>flat_<wbr>hash_<wbr>map</a>&lt;<wbr>asset_<wbr>id,<wbr> <a href="../../core/types.html#symbol-usize">usize</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mIndexByPath</code></td><td><code class="signature"><a href="../../core/containers/flat-hash-map/flat-hash-map.html">core::<wbr>flat_<wbr>hash_<wbr>map</a>&lt;<wbr>std::<wbr>string,<wbr> <a href="../../core/types.html#symbol-usize">usize</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mJobs</code></td><td><code class="signature"><a href="../../core/jobs/job-system/jobs-job-system.html">core::jobs::job_system</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mMounts</code></td><td><code class="signature">std::vector&lt;<a href="asset-mount.html">asset_mount</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mMutex</code></td><td><code class="signature"><span class="k">mutable</span> std::<wbr>shared_<wbr>mutex</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPlatform</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Reverse<wbr>Edges</code></td><td><code class="signature">std::vector&lt;<a href="../../core/types.html#symbol-usize">usize</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Side<wbr>File<wbr>Owners</code></td><td><code class="signature"><a href="../../core/containers/flat-hash-map/flat-hash-map.html">core::<wbr>flat_<wbr>hash_<wbr>map</a>&lt;<wbr>std::<wbr>string,<wbr> std::<wbr>vector&lt;<wbr><a href="../asset-types.html#symbol-asset-id">asset_<wbr>id</a>&gt;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mStale</code></td><td><code class="signature">std::vector&lt;<a href="../asset-types.html#symbol-asset-id">asset_id</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mTypes</code></td><td><code class="signature"><span class="k">const</span> <a href="../asset-type-registry/asset-type-registry.html">asset_<wbr>type_<wbr>registry</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `add_file` {#function-add-file}

<div class="function-declaration" id="function-add-file-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../asset-types.html#symbol-asset-id">asset_id</a>&gt; add_file(
    std::string_view assetPath);</code></pre></div>

<p>Registers a source that exists on disk but was not scanned yet, creating its .meta. <code>NOT_FOUND</code> when the file is missing, <code>ALREADY_EXISTS</code> when it is known, <code>INVALID_ARGUMENT</code> when the path leaves its mount (&quot;assets://../x&quot;, an absolute path).</p>

#### `add_mount` {#function-add-mount}

<div class="function-declaration" id="function-add-mount-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; add_mount(
    std::string_view mountName,
    std::string_view rootPath);</code></pre></div>

<p>Adds a mount. <code>ALREADY_EXISTS</code> for a repeated name, <code>INVALID_ARGUMENT</code> for an empty one. Mounts are scanned in the order they were added.</p>

#### `asset_path_of` {#function-asset-path-of}

<div class="function-declaration" id="function-asset-path-of-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;std::string&gt; asset_path_of(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id) <span class="k">const</span>;</code></pre></div>

<p>&quot;mount://relative#sub&quot; of a record.</p>

#### `cache` {#function-cache}

<div class="function-declaration" id="function-cache-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../asset-cache/asset-cache.html">asset_cache</a> *cache() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The cache opened by <code><a href="#function-open-1">open</a>()</code>, or <code>nullptr</code>.</p>

#### `collect_dependents` {#function-collect-dependents}

<div class="function-declaration" id="function-collect-dependents-1"><pre><code><span class="k">void</span> collect_dependents(
    std::span&lt;<span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a>&gt; roots,
    std::vector&lt;<a href="../asset-types.html#symbol-asset-id">asset_id</a>&gt; &amp;outOrdered) <span class="k">const</span>;</code></pre></div>

<p>Transitive dependents including the roots, depth ascending, deduplicated.</p>

#### `contains` {#function-contains}

<div class="function-declaration" id="function-contains-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> contains(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id) <span class="k">const</span>;</code></pre></div>

<p>Allocation-free lookups for the load path (<code><a href="../asset-source/cache-asset-source.html">cache_asset_source</a></code> asks on every job): whether a record exists, its cooked type (nullopt when unknown or never imported) and its status.</p>

#### `create_asset` {#function-create-asset}

<div class="function-declaration" id="function-create-asset-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../asset-types.html#symbol-asset-id">asset_id</a>&gt; create_asset(
    std::string_view assetPath,
    std::string_view importerId,
    std::span&lt;<span class="k">const</span> std::byte&gt; initialBytes);</code></pre></div>

<p>Writes a new source and its .meta with the given importer, then registers it. The source is removed again when its .meta cannot be written; <code>INVALID_ARGUMENT</code> when the path leaves its mount or one of its names is not portable (<code>core::vfs::path_utils::is_portable_name</code>), before anything is written.</p>

#### `delete_asset` {#function-delete-asset}

<div class="function-declaration" id="function-delete-asset-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; delete_asset(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id);</code></pre></div>

<p>Deletes the source, its .meta, its blobs and its records.</p>

#### `dependencies_of` {#function-dependencies-of}

<div class="function-declaration" id="function-dependencies-of-1"><pre><code>[[<span class="na">nodiscard</span>]] std::vector&lt;<a href="../asset-types.html#symbol-asset-id">asset_id</a>&gt; dependencies_of(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id) <span class="k">const</span>;</code></pre></div>

#### `dependency_closure` {#function-dependency-closure}

<div class="function-declaration" id="function-dependency-closure-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;std::vector&lt;<a href="../asset-types.html#symbol-asset-id">asset_id</a>&gt;&gt; dependency_closure(
    std::span&lt;<span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a>&gt; roots) <span class="k">const</span>;</code></pre></div>

<p>The roots plus everything they depend on, transitively, in uuid order. <code>NOT_FOUND</code> (assets) when a root is unknown.</p>

#### `dependents_of` {#function-dependents-of}

<div class="function-declaration" id="function-dependents-of-1"><pre><code>[[<span class="na">nodiscard</span>]] std::vector&lt;<a href="../asset-types.html#symbol-asset-id">asset_id</a>&gt; dependents_of(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id) <span class="k">const</span>;</code></pre></div>

#### `file_path_of` {#function-file-path-of}

<div class="function-declaration" id="function-file-path-of-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;std::string&gt; file_path_of(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id) <span class="k">const</span>;</code></pre></div>

<p>File-system path of a record&#x27;s source.</p>

#### `find` {#function-find}

<div class="function-declaration" id="function-find-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="asset-record.html">asset_record</a>&gt; find(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id) <span class="k">const</span>;</code></pre></div>

#### `find_by_file_path` {#function-find-by-file-path}

<div class="function-declaration" id="function-find-by-file-path-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="../asset-types.html#symbol-asset-id">asset_id</a>&gt; find_by_file_path(
    std::string_view filePath) <span class="k">const</span>;</code></pre></div>

<p>The primary whose source is the given file-system path (&quot;assets/<code>a.png</code>&quot;).</p>

#### `find_by_path` {#function-find-by-path}

<div class="function-declaration" id="function-find-by-path-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="../asset-types.html#symbol-asset-id">asset_id</a>&gt; find_by_path(
    std::string_view assetPath) <span class="k">const</span>;</code></pre></div>

#### `import` {#function-import}

<div class="function-declaration" id="function-import-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="import-report.html">import_report</a>&gt; import(
    std::span&lt;<span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a>&gt; ids,
    <a href="../../core/jobs/job-system/jobs-job-system.html">core::jobs::job_system</a> &amp;jobs);</code></pre></div>

<p>Imports the sources of the given ids (sub-asset ids import their primary), one job per source, writes blobs into the cache and merges the results. <code>A</code> failing source is recorded <code>IMPORT_FAILED</code> and keeps its previous blob; the call itself fails only with <code>INVALID_STATE</code> before <code><a href="#function-open-1">open</a>()</code>.</p>

#### `import_stale` {#function-import-stale}

<div class="function-declaration" id="function-import-stale-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="import-report.html">import_report</a>&gt; import_stale(
    <a href="../../core/jobs/job-system/jobs-job-system.html">core::jobs::job_system</a> &amp;jobs);</code></pre></div>

<p>Imports every stale primary.</p>

#### `is_open` {#function-is-open}

<div class="function-declaration" id="function-is-open-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_open() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `mark_source_missing` {#function-mark-source-missing}

<div class="function-declaration" id="function-mark-source-missing-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; mark_source_missing(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id);</code></pre></div>

<p>Marks a primary (and its sub-assets) <code>MISSING_SOURCE</code> without a full scan: the hot reload controller calls it when the watcher reports the source file removed, so later loads fail with <code>NOT_FOUND</code> while an already loaded payload stays loaded. <code>NOT_FOUND</code> for an unknown id; a sub-asset id marks its primary.</p>

#### `mark_source_present` {#function-mark-source-present}

<div class="function-declaration" id="function-mark-source-present-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; mark_source_present(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id);</code></pre></div>

<p>Reverses <code><a href="#function-mark-source-missing-1">mark_source_missing</a>()</code> once the source file exists again (the file was moved back or re-created): the status returns to <code>OK</code> (or <code>NO_IMPORTER</code>), the size and write time are refreshed and the source is stale if its content changed. <code>NOT_FOUND</code> for an unknown id or when the file is still absent.</p>

#### `mounts` {#function-mounts}

<div class="function-declaration" id="function-mounts-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="asset-mount.html">asset_mount</a>&gt; mounts() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `move_asset` {#function-move-asset}

<div class="function-declaration" id="function-move-asset-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; move_asset(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id,
    std::string_view newAssetPath);</code></pre></div>

<p>Renames the source and its .meta; the uuid and every sub-asset uuid are kept. <code>INVALID_ARGUMENT</code> when the new path leaves its mount or one of its names is not portable (<code>core::vfs::path_utils::is_portable_name</code>), before anything is moved.</p>

#### `open` {#function-open}

<div class="function-declaration" id="function-open-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; open(
    std::string_view cacheRoot,
    std::string_view platform);</code></pre></div>

<p>Opens the cache under <code>cacheRoot</code> for a platform and loads the <code>db.bin</code> snapshot when it is valid. Must precede <code><a href="#function-scan-1">scan</a>()</code>.</p>

#### `owners_of_side_file` {#function-owners-of-side-file}

<div class="function-declaration" id="function-owners-of-side-file-1"><pre><code>[[<span class="na">nodiscard</span>]] std::vector&lt;<a href="../asset-types.html#symbol-asset-id">asset_id</a>&gt; owners_of_side_file(
    std::string_view assetPath) <span class="k">const</span>;</code></pre></div>

#### `platform` {#function-platform}

<div class="function-declaration" id="function-platform-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string_view platform() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `read_meta` {#function-read-meta}

<div class="function-declaration" id="function-read-meta-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../asset-meta/asset-meta.html">asset_meta</a>&gt; read_meta(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id) <span class="k">const</span>;</code></pre></div>

<p>Reads the .meta of a primary (or of a sub-asset&#x27;s primary).</p>

#### `record_count` {#function-record-count}

<div class="function-declaration" id="function-record-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-usize">usize</a> record_count() <span class="k">const</span>;</code></pre></div>

#### `records` {#function-records}

<div class="function-declaration" id="function-records-1"><pre><code>[[<span class="na">nodiscard</span>]] std::vector&lt;<a href="asset-record.html">asset_record</a>&gt; records() <span class="k">const</span>;</code></pre></div>

#### `save_snapshot` {#function-save-snapshot}

<div class="function-declaration" id="function-save-snapshot-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; save_snapshot() <span class="k">const</span>;</code></pre></div>

<p>Persists the records to &lt;<code>cacheRoot</code>&gt;/<code>db.bin</code>. Purely a cache; rebuilt when missing.</p>

#### `scan` {#function-scan}

<div class="function-declaration" id="function-scan-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="scan-report.html">scan_report</a>&gt; scan();</code></pre></div>

<p>Walks every mount in sorted order, creates missing .meta files, applies the duplicate-uuid policy, hashes changed sources, marks removed sources <code>MISSING_SOURCE</code>, computes the stale set, deletes leftover *.tmp blobs and saves the snapshot. <code>A</code> .meta that cannot be written leaves its record <code>META_INVALID</code> (retried by the next scan) instead of failing the scan. <code>INVALID_STATE</code> before <code><a href="#function-open-1">open</a>()</code>.</p>

#### `set_importer` {#function-set-importer}

<div class="function-declaration" id="function-set-importer-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; set_importer(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id,
    std::string_view importerId);</code></pre></div>

<p>Makes a registered importer the one a primary&#x27;s .meta names (&quot;import as&quot;): the way to import a file with an importer that reads its format without claiming its extension, such as a .glb as collision geometry instead of a render model. The meta takes the importer&#x27;s version and default settings, since the previous importer&#x27;s settings mean nothing to it, and the asset is stale until its next import; its uuid is kept. Naming the importer the meta already has changes nothing. <code>NO_IMPORTER</code> for an id that is not registered; otherwise the errors of <code>read_meta</code> and <code>write_meta</code>.</p>

#### `stale_ids` {#function-stale-ids}

<div class="function-declaration" id="function-stale-ids-1"><pre><code>[[<span class="na">nodiscard</span>]] std::vector&lt;<a href="../asset-types.html#symbol-asset-id">asset_id</a>&gt; stale_ids() <span class="k">const</span>;</code></pre></div>

<p>The stale primaries computed by the last <code><a href="#function-scan-1">scan</a>()</code> or <code><a href="#function-import-1">import</a>()</code>.</p>

#### `status_of` {#function-status-of}

<div class="function-declaration" id="function-status-of-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="../asset-database.html#symbol-asset-record-status">asset_record_status</a>&gt; status_of(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id) <span class="k">const</span>;</code></pre></div>

#### `type_of` {#function-type-of}

<div class="function-declaration" id="function-type-of-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="../asset-types.html#symbol-asset-type-id">asset_type_id</a>&gt; type_of(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id) <span class="k">const</span>;</code></pre></div>

#### `types` {#function-types}

<div class="function-declaration" id="function-types-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../asset-type-registry/asset-type-registry.html">asset_type_registry</a> &amp;types() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `vfs` {#function-vfs}

<div class="function-declaration" id="function-vfs-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/vfs/file-system/vfs-file-system.html">core::vfs::file_system</a> &amp;vfs() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `write_meta` {#function-write-meta}

<div class="function-declaration" id="function-write-meta-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; write_meta(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id,
    <span class="k">const</span> <a href="../asset-meta/asset-meta.html">asset_meta</a> &amp;meta);</code></pre></div>

<p>Writes the .meta of a primary and refreshes the record (settings hash, importer). <code>A</code> changed uuid is refused with <code>INVALID_ARGUMENT</code> and logged: it would orphan references. <code>A</code> sub-asset the last import dropped (no blob) gets no record back from the meta. An imported primary switched to another importer is stale until its next import, even when both importers share a version and settings.</p>

### Private

#### `apply_import_results` {#function-apply-import-results}

<div class="function-declaration" id="function-apply-import-results-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; apply_import_results(
    std::vector&lt;<a href="../asset-database.html">source_job</a>&gt; &amp;jobs,
    <a href="import-report.html">import_report</a> &amp;report);</code></pre></div>

#### `detect_cycles_locked` {#function-detect-cycles-locked}

<div class="function-declaration" id="function-detect-cycles-locked-1"><pre><code><span class="k">void</span> detect_cycles_locked();</code></pre></div>

#### `ensure_open` {#function-ensure-open}

<div class="function-declaration" id="function-ensure-open-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; ensure_open() <span class="k">const</span>;</code></pre></div>

#### `file_path_for` {#function-file-path-for}

<div class="function-declaration" id="function-file-path-for-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string file_path_for(
    <a href="../../core/types.html#symbol-u32">u32</a> mountIndex,
    std::string_view relativePath) <span class="k">const</span>;</code></pre></div>

#### `find_entry_locked` {#function-find-entry-locked}

<div class="function-declaration" id="function-find-entry-locked-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="asset-database-entry.html">entry</a> *find_entry_locked(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-find-entry-locked-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="asset-database-entry.html">entry</a> *find_entry_locked(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id) <span class="k">noexcept</span>;</code></pre></div>

#### `hash_source` {#function-hash-source}

<div class="function-declaration" id="function-hash-source-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u64">u64</a> hash_source(
    <a href="../../core/types.html#symbol-u32">u32</a> mountIndex,
    std::string_view relativePath,
    std::span&lt;<span class="k">const</span> std::string&gt; sideFiles) <span class="k">const</span>;</code></pre></div>

#### `hash_source_bytes` {#function-hash-source-bytes}

<div class="function-declaration" id="function-hash-source-bytes-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u64">u64</a> hash_source_bytes(
    std::span&lt;<span class="k">const</span> std::byte&gt; primaryBytes,
    std::span&lt;<span class="k">const</span> std::string&gt; sideFiles) <span class="k">const</span>;</code></pre></div>

#### `load_snapshot` {#function-load-snapshot}

<div class="function-declaration" id="function-load-snapshot-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; load_snapshot();</code></pre></div>

#### `mount_index` {#function-mount-index}

<div class="function-declaration" id="function-mount-index-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt; mount_index(
    std::string_view mountName) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `rebuild_graphs_locked` {#function-rebuild-graphs-locked}

<div class="function-declaration" id="function-rebuild-graphs-locked-1"><pre><code><span class="k">void</span> rebuild_graphs_locked();</code></pre></div>

#### `rebuild_indexes_locked` {#function-rebuild-indexes-locked}

<div class="function-declaration" id="function-rebuild-indexes-locked-1"><pre><code><span class="k">void</span> rebuild_indexes_locked();</code></pre></div>

#### `recompute_stale` {#function-recompute-stale}

<div class="function-declaration" id="function-recompute-stale-1"><pre><code><span class="k">void</span> recompute_stale();</code></pre></div>

#### `register_source` {#function-register-source}

<div class="function-declaration" id="function-register-source-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../asset-types.html#symbol-asset-id">asset_id</a>&gt; register_source(
    <a href="../../core/types.html#symbol-u32">u32</a> mountIndex,
    std::string_view relativePath,
    <span class="k">bool</span> &amp;outMetaCreated);</code></pre></div>

#### `set_source_status` {#function-set-source-status}

<div class="function-declaration" id="function-set-source-status-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; set_source_status(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id,
    <span class="k">bool</span> present);</code></pre></div>

#### `write_import_log` {#function-write-import-log}

<div class="function-declaration" id="function-write-import-log-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; write_import_log(
    <span class="k">const</span> <a href="import-report.html">import_report</a> &amp;report) <span class="k">const</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
