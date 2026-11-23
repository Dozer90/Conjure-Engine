---
kind: class
---

[Engine](../../../engine.md) → [Assets](../../assets.md) → [asset_importer.h](../asset-importer.md)

# import_context
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::assets` |
| Include File Path | `engine/include/conjure/assets/asset_importer.h` |
| Source File Path | `engine/src/assets/asset_importer.cpp` |
| API ↗ | [Database, importers and hot reload](../../../../api/assets/database-and-import.md), [Assets](../../../../api/assets/index.md) |

## Syntax

```cpp
class CONJURE_API import_context
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">import_context(
    <a href="../../core/vfs/file-system/vfs-file-system.html">core::vfs::file_system</a> &amp;fileSystem,
    std::string assetPath,
    std::string filePath,
    std::span&lt;<span class="k">const</span> std::byte&gt; sourceBytes,
    <a href="../../core/json/value/json-value.html">core::json::value</a> settings,
    std::string platform,
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;primaryId,
    <a href="sub-asset-table.html">sub_asset_table</a> &amp;subAssets,
    <a href="../asset-importer.html#symbol-asset-path-resolver">asset_path_resolver</a> resolver);</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~import_context();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mAssetPath</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFilePath</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFileSystem</code></td><td><code class="signature"><a href="../../core/vfs/file-system/vfs-file-system.html">core::vfs::file_system</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPlatform</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPrimaryId</code></td><td><code class="signature"><a href="../asset-types.html#symbol-asset-id">asset_id</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mResolver</code></td><td><code class="signature"><a href="../asset-importer.html#symbol-asset-path-resolver">asset_path_resolver</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSettings</code></td><td><code class="signature"><a href="../../core/json/value/json-value.html">core::json::value</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSideFiles</code></td><td><code class="signature">std::vector&lt;std::string&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSourceBytes</code></td><td><code class="signature">std::<wbr>span&lt;<wbr><span class="k">const</span> std::<wbr>byte&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSubAssets</code></td><td><code class="signature"><a href="sub-asset-table.html">sub_asset_table</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mWarnings</code></td><td><code class="signature">std::vector&lt;std::string&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `add_warning` {#function-add-warning}

<div class="function-declaration" id="function-add-warning-1"><pre><code><span class="k">void</span> add_warning(
    std::string message);</code></pre></div>

<p>Records a non-fatal message that ends up in the import report and the import log.</p>

#### `find_asset_by_path` {#function-find-asset-by-path}

<div class="function-declaration" id="function-find-asset-by-path-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="../asset-types.html#symbol-asset-id">asset_id</a>&gt; find_asset_by_path(
    std::string_view path) <span class="k">const</span>;</code></pre></div>

<p>Uuid of another source of the project by asset path (&quot;assets://textures/<code>a.png</code>&quot;) or by a path relative to this source&#x27;s directory (&quot;textures/<code>a.png</code>&quot;).</p>

#### `platform` {#function-platform}

<div class="function-declaration" id="function-platform-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string_view platform() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Target platform: &quot;windows&quot;, &quot;linux&quot; or &quot;macos&quot;.</p>

#### `primary_id` {#function-primary-id}

<div class="function-declaration" id="function-primary-id-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;primary_id() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Uuid of the primary asset (the source file itself).</p>

#### `read_side_file` {#function-read-side-file}

<div class="function-declaration" id="function-read-side-file-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;std::vector&lt;std::byte&gt;&gt; read_side_file(
    std::string_view relativePath);</code></pre></div>

<p>Reads a file next to the source (relative to the source&#x27;s directory) and records it as a side file (even when the read fails, so a missing side file is watched): hashed into the source hash and watched for changes.</p>

#### `settings` {#function-settings}

<div class="function-declaration" id="function-settings-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../../core/json/value/json-value.html">core::json::value</a> &amp;settings() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The importer settings from the .meta (an object; invalid when the meta had none).</p>

#### `side_files` {#function-side-files}

<div class="function-declaration" id="function-side-files-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> std::string&gt; side_files() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Asset paths of every side file asked for so far, including ones that could not be read.</p>

#### `source_bytes` {#function-source-bytes}

<div class="function-declaration" id="function-source-bytes-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> std::byte&gt; source_bytes() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The whole source file.</p>

#### `source_path` {#function-source-path}

<div class="function-declaration" id="function-source-path-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string_view source_path() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The source&#x27;s asset path, &quot;assets://models/<code>robot.gltf</code>&quot;.</p>

#### `sub_asset_id` {#function-sub-asset-id}

<div class="function-declaration" id="function-sub-asset-id-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../asset-types.html#symbol-asset-id">asset_id</a> sub_asset_id(
    std::string_view subPath);</code></pre></div>

<p>Stable uuid for a named sub-asset; allocated on first use, persisted to the meta.</p>

#### `warnings` {#function-warnings}

<div class="function-declaration" id="function-warnings-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> std::string&gt; warnings() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
