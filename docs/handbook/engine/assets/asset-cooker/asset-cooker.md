---
kind: class
---

[Engine](../../../engine.md) → [Assets](../../assets.md) → [asset_cooker.h](../asset-cooker.md)

# asset_cooker
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::assets` |
| Include File Path | `engine/include/conjure/assets/asset_cooker.h` |
| Source File Path | `engine/src/assets/asset_cooker.cpp` |
| API ↗ | [Cooking, caches and packs](../../../../api/assets/cooking-and-packs.md), [Assets](../../../../api/assets/index.md) |

## Syntax

```cpp
class CONJURE_API asset_cooker
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">asset_cooker(
    <a href="../asset-database/asset-database.html">asset_database</a> &amp;database,
    <span class="k">const</span> <a href="../asset-cache/asset-cache.html">asset_cache</a> &amp;cache,
    <a href="../../core/vfs/file-system/vfs-file-system.html">core::vfs::file_system</a> &amp;fileSystem,
    <a href="../../core/jobs/job-system/jobs-job-system.html">core::jobs::job_system</a> &amp;jobs);</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~asset_cooker();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mCache</code></td><td><code class="signature"><span class="k">const</span> <a href="../asset-cache/asset-cache.html">asset_cache</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mDatabase</code></td><td><code class="signature"><a href="../asset-database/asset-database.html">asset_database</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFileSystem</code></td><td><code class="signature"><a href="../../core/vfs/file-system/vfs-file-system.html">core::vfs::file_system</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mJobs</code></td><td><code class="signature"><a href="../../core/jobs/job-system/jobs-job-system.html">core::jobs::job_system</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mLastReport</code></td><td><code class="signature"><a href="cook-report.html">cook_report</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `cook` {#function-cook}

<div class="function-declaration" id="function-cook-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="cook-report.html">cook_report</a>&gt; cook(
    <span class="k">const</span> <a href="cook-settings.html">cook_settings</a> &amp;settings);</code></pre></div>

<p>Scans, computes the closure (roots + <code>alwaysInclude</code> + every engine:// asset, or all records but the unnamed <code>NO_IMPORTER</code> and <code>MISSING_SOURCE</code> ones, which become report warnings unless a member depends on them), drops <code>EXCLUDE_FROM_PACK</code> types, imports stale members, verifies every blob and writes the pack. <code>IMPORT_FAILED</code> with every failure listed in the report when any member cannot be cooked; nothing is written then.</p>

#### `last_report` {#function-last-report}

<div class="function-declaration" id="function-last-report-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="cook-report.html">cook_report</a> &amp;last_report() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The report of the last <code><a href="#function-cook-1">cook</a>()</code>, successful or not.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
