---
kind: header
---

[Engine](../../engine.md) → [RHI](../rhi.md)

# rhi_error.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/rhi/rhi_error.h` |
| API ↗ | [Devices and swapchains](../../../api/rhi/devices-and-swapchains.md), [Graphics device layer (RHI)](../../../api/rhi/index.md) |

<p>Errors of the rhi module.</p>

## Enumerations

### `rhi_error` {#symbol-rhi-error}

```cpp
enum class rhi_error : u16 {
    DEVICE_CREATION_FAILED = 1,     // the API refused to create the device; the message carries its text
    DEVICE_LOST,                    // removed: DXGI_ERROR_DEVICE_REMOVED, VK_ERROR_DEVICE_LOST or a Fatal message
    OUT_OF_DATE,                    // resize it: the surface changed or a failed resize left no back buffers
    SWAPCHAIN_CREATION_FAILED,      // the API refused to create or resize the swapchain
    INVALID_DESC,                   // a device_desc or swapchain_desc field is invalid; the message names it
    PRESENT_FAILED,                 // acquire or present refused for a reason other than device loss or OUT_OF_DATE
    COUNT
};
```

## Macros

### `CONJURE_DECLARE_ERROR_CATEGORY` {#symbol-conjure-declare-error-category}

```cpp
CONJURE_DECLARE_ERROR_CATEGORY(conjure::rhi::rhi_error, "rhi", conjure::rhi::rhi_error_name)
```

## Functions

### Public

#### `rhi_error_name` {#function-rhi-error-name}

<div class="function-declaration" id="function-rhi-error-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view rhi_error_name(
    <a href="rhi-error.html#symbol-rhi-error">rhi_error</a> code) <span class="k">noexcept</span>;</code></pre></div>

<p>The enumerator name of an rhi error code (&quot;<code>DEVICE_LOST</code>&quot;), or &quot;&quot; for unknown values.</p>

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
<a id="rhi_error"></a>
<a id="rhi_error_name"></a>
<a id="rhi_errorcount"></a>
<a id="rhi_errordevice_creation_failed"></a>
<a id="rhi_errordevice_lost"></a>
<a id="rhi_errorinvalid_desc"></a>
<a id="rhi_errorout_of_date"></a>
<a id="rhi_errorpresent_failed"></a>
<a id="rhi_errorswapchain_creation_failed"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
