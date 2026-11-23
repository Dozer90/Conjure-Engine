---
kind: header
---

[Engine](../../engine.md) → [Renderer](../renderer.md)

# renderer_error.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/renderer/renderer_error.h` |
| API ↗ | [Rendering](../../../api/renderer/index.md) |

<p>Errors of the renderer module. Device loss is reported with <code>rhi::rhi_error::DEVICE_LOST</code>; parse and lookup failures of importers use the generic core codes (<code>PARSE_FAILURE</code>, <code>NOT_FOUND</code>, <code>UNSUPPORTED</code>, <code>INVALID_ARGUMENT</code>). The codes below are the conditions only this module produces.</p>

## Enumerations

### `renderer_error` {#symbol-renderer-error}

```cpp
enum class renderer_error : u16 {
    SHADER_NOT_FOUND = 1,           // a shader permutation the renderer needs is missing from the library
    PIPELINE_CREATION_FAILED,       // the device refused a pipeline, binding layout or input layout
    RESOURCE_CREATION_FAILED,       // the device refused a texture, buffer or sampler
    DECODE_FAILED,                  // an image or HDR file did not decode
    INVALID_ASSET,                  // a cooked blob failed validation
    COUNT
};
```

## Macros

### `CONJURE_DECLARE_ERROR_CATEGORY` {#symbol-conjure-declare-error-category}

```cpp
CONJURE_DECLARE_ERROR_CATEGORY(conjure::renderer::renderer_error, "renderer", conjure::renderer::renderer_error_name)
```

## Functions

### Public

#### `renderer_error_name` {#function-renderer-error-name}

<div class="function-declaration" id="function-renderer-error-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view renderer_error_name(
    <a href="renderer-error.html#symbol-renderer-error">renderer_error</a> code) <span class="k">noexcept</span>;</code></pre></div>

<p>The enumerator name of a renderer error code (&quot;<code>SHADER_NOT_FOUND</code>&quot;), or &quot;&quot; for unknown values.</p>

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
<a id="conjure_declare_error_category"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="renderer_error"></a>
<a id="renderer_error_name"></a>
<a id="renderer_errorcount"></a>
<a id="renderer_errordecode_failed"></a>
<a id="renderer_errorinvalid_asset"></a>
<a id="renderer_errorpipeline_creation_failed"></a>
<a id="renderer_errorresource_creation_failed"></a>
<a id="renderer_errorshader_not_found"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
