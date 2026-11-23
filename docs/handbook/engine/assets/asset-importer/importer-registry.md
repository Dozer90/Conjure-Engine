---
kind: class
---

[Engine](../../../engine.md) → [Assets](../../assets.md) → [asset_importer.h](../asset-importer.md)

# importer_registry
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::assets` |
| Include File Path | `engine/include/conjure/assets/asset_importer.h` |
| Source File Path | `engine/src/assets/asset_importer.cpp` |
| API ↗ | [Database, importers and hot reload](../../../../api/assets/database-and-import.md), [Assets](../../../../api/assets/index.md) |

## Syntax

```cpp
class CONJURE_API importer_registry
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">importer_registry();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~importer_registry();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mImporters</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr>std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="asset-importer.html">asset_<wbr>importer</a>&gt;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Imports<wbr>In<wbr>Flight</code></td><td><code class="signature">std::atomic&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt;</code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mView</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><span class="k">const</span> <a href="asset-importer.html">asset_<wbr>importer</a> *&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `begin_imports` {#function-begin-imports}

<div class="function-declaration" id="function-begin-imports-1"><pre><code><span class="k">void</span> begin_imports() <span class="k">noexcept</span>;</code></pre></div>

<p>Marks the start and end of an import batch; <code>unregister_importer</code> is refused between them. Used by <code><a href="../asset-database/asset-database.html">asset_database</a></code>.</p>

#### `end_imports` {#function-end-imports}

<div class="function-declaration" id="function-end-imports-1"><pre><code><span class="k">void</span> end_imports() <span class="k">noexcept</span>;</code></pre></div>

#### `find` {#function-find}

<div class="function-declaration" id="function-find-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="asset-importer.html">asset_importer</a> *find(
    std::string_view id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `find_for_extension` {#function-find-for-extension}

<div class="function-declaration" id="function-find-for-extension-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="asset-importer.html">asset_importer</a> *find_for_extension(
    std::string_view extension) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The importer handling an extension, matched ignoring case; the dot is optional.</p>

#### `find_for_path` {#function-find-for-path}

<div class="function-declaration" id="function-find-for-path-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="asset-importer.html">asset_importer</a> *find_for_path(
    std::string_view path) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The importer whose registered extension is the longest suffix of the path&#x27;s file name, ignoring case, so a &quot;.<code>scene.json</code>&quot; importer wins over a &quot;.json&quot; one for &quot;<code>main.scene.json</code>&quot;. <code>nullptr</code> when no extension matches. An extension registered without its dot (&quot;json&quot;) matches the same names as the dotted spelling. <code>A</code> suffix never consumes the whole file name: a leading dot is part of the name (<code>path_utils</code>), so &quot;.<code>scene.json</code>&quot; matches &quot;.json&quot; and &quot;.json&quot; matches nothing.</p>

#### `importers` {#function-importers}

<div class="function-declaration" id="function-importers-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="asset-importer.html">asset_importer</a> *<span class="k">const</span>&gt; importers() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Every importer in registration order.</p>

#### `imports_in_flight` {#function-imports-in-flight}

<div class="function-declaration" id="function-imports-in-flight-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> imports_in_flight() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `register_importer` {#function-register-importer}

<div class="function-declaration" id="function-register-importer-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; register_importer(
    std::unique_ptr&lt;<a href="asset-importer.html">asset_importer</a>&gt; importer);</code></pre></div>

<p><code>ALREADY_EXISTS</code> when an importer with the same id is registered; <code>INVALID_ARGUMENT</code> for a <code>null</code> importer or an empty id. An extension another importer already claims is accepted with a <code>WARN</code> naming both: the earlier importer stays the one chosen by extension and the new one applies only to a file whose .meta names it.</p>

#### `unregister_importer` {#function-unregister-importer}

<div class="function-declaration" id="function-unregister-importer-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; unregister_importer(
    std::string_view id);</code></pre></div>

<p><code>NOT_FOUND</code> (core) for an unknown id; <code>INVALID_STATE</code> while imports are in flight.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
