---
kind: header
---

[Engine](../../engine.md) → [Modules](../modules.md)

# module_manifest.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/modules/module_manifest.h` |
| API ↗ | [Writing a module](../../../api/modules/writing-a-module.md) |

<p>The parsed manifest. Paths are relative to the module root (the manifest&#x27;s directory).</p>

## Structs

| File | Description |
|---|---|
| [`module_dependency`](module-manifest/module-dependency.md) | One entry of &quot;dependencies&quot;. |
| [`module_manifest`](module-manifest/module-manifest.md) | The parsed manifest. Paths are relative to the module root (the manifest&#x27;s directory). |

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-MAX_MODULE_NAME_LENGTH"></a><code>MAX_MODULE_NAME_LENGTH</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-usize">usize</a></code> | <code class="signature"><span class="mi">64</span></code> |  |
| <a id="variable-MODULE_MANIFEST_FILE_NAME"></a><code>MODULE_MANIFEST_FILE_NAME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"module.json"</span></code> |  |
| <a id="variable-MODULE_MANIFEST_SCHEMA_VERSION"></a><code>MODULE_MANIFEST_SCHEMA_VERSION</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">1</span></code> |  |

## Functions

### Public

#### `decorate_binary_name` {#function-decorate-binary-name}

<div class="function-declaration" id="function-decorate-binary-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string decorate_binary_name(
    std::string_view baseName,
    <a href="module-types.html#symbol-module-platform">module_platform</a> platform);</code></pre></div>

<p>The file name of a module binary on a platform: &quot;game&quot; becomes &quot;<code>game.dll</code>&quot;, &quot;<code>libgame.so</code>&quot; or &quot;<code>libgame.dylib</code>&quot;. Empty input yields an empty string.</p>

#### `is_valid_module_name` {#function-is-valid-module-name}

<div class="function-declaration" id="function-is-valid-module-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">bool</span> is_valid_module_name(
    std::string_view name) <span class="k">noexcept</span>;</code></pre></div>

<p>True for names matching [a-z0-9_.]{1,64}.</p>

#### `load_module_manifest` {#function-load-module-manifest}

<div class="function-declaration" id="function-load-module-manifest-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<a href="module-manifest/module-manifest.html">module_manifest</a>&gt; load_module_manifest(
    <span class="k">const</span> std::filesystem::path &amp;path);</code></pre></div>

<p>Reads and parses a manifest file once; messages name its generic path. <code>MANIFEST_NOT_FOUND</code> when the file is missing, otherwise as <code><a href="#function-parse-module-manifest-1">parse_module_manifest</a>()</code>.</p>

#### `module_binary_directory` {#function-module-binary-directory}

<div class="function-declaration" id="function-module-binary-directory-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::filesystem::path module_binary_directory(
    <span class="k">const</span> std::filesystem::path &amp;rootDirectory,
    <span class="k">const</span> <a href="module-types/binary-triplet.html">binary_triplet</a> &amp;triplet);</code></pre></div>

<p>&lt;root&gt;/bin/&lt;platform&gt;-&lt;arch&gt;-&lt;config&gt;: the directory <code><a href="#function-module-binary-path-1">module_binary_path</a>()</code> points into.</p>

#### `module_binary_path` {#function-module-binary-path}

<div class="function-declaration" id="function-module-binary-path-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::filesystem::path module_binary_path(
    <span class="k">const</span> <a href="module-manifest/module-manifest.html">module_manifest</a> &amp;manifest,
    <span class="k">const</span> std::filesystem::path &amp;rootDirectory,
    <span class="k">const</span> <a href="module-types/binary-triplet.html">binary_triplet</a> &amp;triplet);</code></pre></div>

<p>&lt;root&gt;/bin/&lt;platform&gt;-&lt;arch&gt;-&lt;config&gt;/&lt;decorated binary&gt;, or an empty path when the manifest declares no binary.</p>

#### `parse_module_manifest` {#function-parse-module-manifest}

<div class="function-declaration" id="function-parse-module-manifest-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<a href="module-manifest/module-manifest.html">module_manifest</a>&gt; parse_module_manifest(
    std::string_view jsonText,
    std::string_view sourceName = <span class="s">"&lt;memory&gt;"</span>);</code></pre></div>

<p>Parses and validates manifest text. <code>sourceName</code> is the file the text came from, used in messages together with the JSON pointer of the offending member. <code>MANIFEST_INVALID</code> on any JSON, schema, type or range error, for a &quot;binary&quot; with a directory part and for a &quot;managed&quot; or &quot;content&quot; entry that leaves the module root; <code>INVALID_NAME</code> for a bad name.</p>

#### `write_module_manifest` {#function-write-module-manifest}

<div class="function-declaration" id="function-write-module-manifest-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/json/document/json-document.html">core::json::document</a> write_module_manifest(
    <span class="k">const</span> <a href="module-manifest/module-manifest.html">module_manifest</a> &amp;manifest,
    <span class="k">bool</span> includeEditorOnly = true);</code></pre></div>

<p>Serialises a manifest. <code><a href="#function-parse-module-manifest-1">parse_module_manifest</a>(write(m).to_string())</code> equals m. Export drops &quot;<code>editorOnly</code>&quot; by passing <code>includeEditorOnly</code> = <code>false</code>.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="decorate_binary_name"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="is_valid_module_name"></a>
<a id="load_module_manifest"></a>
<a id="max_module_name_length"></a>
<a id="module_binary_directory"></a>
<a id="module_binary_path"></a>
<a id="module_dependency"></a>
<a id="module_dependencyname"></a>
<a id="module_dependencyoperator"></a>
<a id="module_dependencyoptional"></a>
<a id="module_dependencyversion"></a>
<a id="module_manifest"></a>
<a id="module_manifest_file_name"></a>
<a id="module_manifest_schema_version"></a>
<a id="module_manifestbinary"></a>
<a id="module_manifestcontentdirectories"></a>
<a id="module_manifestdependencies"></a>
<a id="module_manifestdescription"></a>
<a id="module_manifestdisplayname"></a>
<a id="module_manifesteditoronly"></a>
<a id="module_manifestengineversion"></a>
<a id="module_manifestkind"></a>
<a id="module_manifestmanagedassemblies"></a>
<a id="module_manifestname"></a>
<a id="module_manifestoperator"></a>
<a id="module_manifestplatforms"></a>
<a id="module_manifestrequiredfeatures"></a>
<a id="module_manifestschemaversion"></a>
<a id="module_manifestversion"></a>
<a id="parse_module_manifest"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<a id="write_module_manifest"></a>
<!-- doc-legacy-anchors:end -->
