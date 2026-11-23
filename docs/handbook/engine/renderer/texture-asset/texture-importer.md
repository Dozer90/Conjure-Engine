---
kind: class
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [texture_asset.h](../texture-asset.md)

# texture_importer
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/texture_asset.h` |
| Source File Path | `engine/src/renderer/assets/texture_asset.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [JPEG XL images](../../../../api/renderer/jpeg-xl.md), [Renderer assets](../../../../api/renderer/renderer-assets.md) |

## Syntax

```cpp
class CONJURE_API texture_importer final : public assets::asset_importer
```

## Inheritance Hierarchy


[`assets::asset_importer`](../../assets/asset-importer/asset-importer.md) → `texture_importer`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">texture_importer();</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> texture_importer(
    <a href="../../core/jobs/job-system/jobs-job-system.html">core::jobs::job_system</a> *jobs);</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~texture_importer() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mJobs</code></td><td><code class="signature"><a href="../../core/jobs/job-system/jobs-job-system.html">core::jobs::job_system</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `default_settings` {#function-default-settings}

<div class="function-declaration" id="function-default-settings-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/json/document/json-document.html">core::json::document</a> default_settings() <span class="k">const</span> <span class="k">override</span>;</code></pre></div>

#### `extensions` {#function-extensions}

<div class="function-declaration" id="function-extensions-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> std::string_view&gt; extensions() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `id` {#function-id}

<div class="function-declaration" id="function-id-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string_view id() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `import` {#function-import}

<div class="function-declaration" id="function-import-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../../assets/asset-importer/import-output.html">assets::import_output</a>&gt; import(
    <a href="../../assets/asset-importer/import-context.html">assets::import_context</a> &amp;context) <span class="k">const</span> <span class="k">override</span>;</code></pre></div>

#### `version` {#function-version}

<div class="function-declaration" id="function-version-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> version() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
