---
kind: header
---

[Engine](../../engine.md) → [RHI](../rhi.md)

# shader_library.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/rhi/shader_library.h` |
| API ↗ | [Shaders](../../../api/rhi/shaders.md) |

<p>Compiled shader blobs. <code>conjure_add_shaders()</code> writes &lt;output&gt;/<code>index.json</code> listing every permutation as { name, entry, stage, defines, dxil, spirv, sourceHash }; <code><a href="shader-library/shader-library.html">shader_library</a></code> opens that index, selects the blob column for the device&#x27;s backend (DXIL on <code>D3D12</code>, SPIR-<code>V</code> on Vulkan, whichever exists on the <code>null</code> device) and loads blobs lazily.</p>

## Structs

| File | Description |
|---|---|
| [`shader_blob`](shader-library/shader-blob.md) |  |
| [`shader_key`](shader-library/shader-key.md) | Identifies one permutation: the source file stem, the entry point and the define string in the axis order declared in CMake (&quot;<code>ALPHA_MASK</code>=1&quot;, &quot;<code>A</code>=1\|<code>B</code>=0&quot;, &quot;&quot; for none). |
| [`shader_library::create_tag`](shader-library/shader-library-create-tag.md) |  |

## Classes

| File | Description |
|---|---|
| [`shader_library`](shader-library/shader-library.md) |  |

## Enumerations

### `shader_stage` {#symbol-shader-stage}

```cpp
enum class shader_stage : u8 {
    VERTEX = 0,
    PIXEL,
    COMPUTE,
    COUNT
};
```

## Forward Declarations

### `shader_library::impl` {#symbol-shader-library-impl}

```cpp
struct impl;
```

## Functions

### Public

#### `parse_shader_stage` {#function-parse-shader-stage}

<div class="function-declaration" id="function-parse-shader-stage-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<a href="shader-library.html#symbol-shader-stage">shader_stage</a>&gt; parse_shader_stage(
    std::string_view name) <span class="k">noexcept</span>;</code></pre></div>

<p>Parses &quot;vertex&quot; / &quot;pixel&quot; / &quot;compute&quot; (also the dxc profiles &quot;vs&quot;, &quot;ps&quot;, &quot;cs&quot;), case-insensitively. Fails with <code>core::error_code::INVALID_ARGUMENT</code> otherwise.</p>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="shader-library.html#symbol-shader-stage">shader_stage</a> stage) <span class="k">noexcept</span>;</code></pre></div>

<p>The <code>index.json</code> spelling of a stage: &quot;vertex&quot;, &quot;pixel&quot; or &quot;compute&quot;; &quot;&quot; for an out-of-range value.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="parse_shader_stage"></a>
<a id="shader_blob"></a>
<a id="shader_blobbytes"></a>
<a id="shader_blobstage"></a>
<a id="shader_key"></a>
<a id="shader_keydefines"></a>
<a id="shader_keyentry"></a>
<a id="shader_keyname"></a>
<a id="shader_library"></a>
<a id="shader_librarybackend"></a>
<a id="shader_librarycontains"></a>
<a id="shader_librarycreate_tag"></a>
<a id="shader_librarydirectory"></a>
<a id="shader_libraryentry_count"></a>
<a id="shader_libraryget"></a>
<a id="shader_libraryimpl"></a>
<a id="shader_libraryloaded_blob_count"></a>
<a id="shader_librarymimpl"></a>
<a id="shader_libraryopen"></a>
<a id="shader_libraryoperator"></a>
<a id="shader_libraryreload"></a>
<a id="shader_libraryshader_library"></a>
<a id="shader_libraryshader_library_1"></a>
<a id="shader_stage"></a>
<a id="shader_stagecompute"></a>
<a id="shader_stagecount"></a>
<a id="shader_stagepixel"></a>
<a id="shader_stagevertex"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
