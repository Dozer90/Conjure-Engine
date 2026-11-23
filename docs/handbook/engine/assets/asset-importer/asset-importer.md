---
kind: class
---

[Engine](../../../engine.md) → [Assets](../../assets.md) → [asset_importer.h](../asset-importer.md)

# asset_importer
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::assets` |
| Include File Path | `engine/include/conjure/assets/asset_importer.h` |
| Source File Path | `engine/src/assets/asset_importer.cpp` |
| API ↗ | [Database, importers and hot reload](../../../../api/assets/database-and-import.md), [Assets](../../../../api/assets/index.md) |

## Syntax

```cpp
class CONJURE_API asset_importer
```

## Derived Classes


<details><summary>Classes that use this as a base</summary>

<ul>

<li><a href="../binary-asset/binary-importer.html">binary_importer</a></li>

</ul>

</details>

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">asset_importer();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">virtual</span> ~asset_importer();</code></td><td></td></tr>
</tbody>
</table>



## Functions

### Public

#### `default_settings` {#function-default-settings}

<div class="function-declaration" id="function-default-settings-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../core/json/document/json-document.html">core::json::document</a> default_settings() <span class="k">const</span>;</code></pre></div>

<p>Settings written into a new .meta. The default is an empty object.</p>

#### `extensions` {#function-extensions}

<div class="function-declaration" id="function-extensions-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> std::span&lt;<span class="k">const</span> std::string_view&gt; extensions() <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

<p>The extensions this importer claims for a file without a .meta: lower-case, with the dot (&quot;.png&quot;), matched case-insensitively. An importer may read formats it does not claim because another importer owns them by default; those files reach it when their .meta names it (a .glb as collision geometry instead of a render model).</p>

#### `id` {#function-id}

<div class="function-declaration" id="function-id-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> std::string_view id() <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

<p>Stable identifier recorded in every .meta (&quot;<code>conjure.renderer.gltf</code>&quot;).</p>

#### `import` {#function-import}

<div class="function-declaration" id="function-import-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="import-output.html">import_output</a>&gt; import(
    <a href="import-context.html">import_context</a> &amp;context) <span class="k">const</span> = <span class="mi">0</span>;</code></pre></div>

<p>Worker thread. Const, deterministic, touches no shared state beyond the context.</p>

#### `version` {#function-version}

<div class="function-declaration" id="function-version-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../core/types.html#symbol-u32">u32</a> version() <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

<p>Bump to invalidate every blob this importer produced.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->
