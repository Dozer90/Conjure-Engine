---
kind: header
---

[Engine](../../engine.md) → [RHI](../rhi.md)

# backend.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/rhi/backend.h` |
| API ↗ | [Devices and swapchains](../../../api/rhi/devices-and-swapchains.md) |

<p>Graphics backends of the rhi module.</p>

## Structs

| File | Description |
|---|---|
| [`device_capabilities`](backend/device-capabilities.md) | What the created device can do; filled by <code>device::create</code> from the adapter. |

## Enumerations

### `graphics_backend` {#symbol-graphics-backend}

```cpp
enum class graphics_backend : u8 {
    NULL_DEVICE = 0,
    D3D12,
    VULKAN,
    COUNT
};
```

## Functions

### Public

#### `default_graphics_backend` {#function-default-graphics-backend}

<div class="function-declaration" id="function-default-graphics-backend-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="backend.html#symbol-graphics-backend">graphics_backend</a> default_graphics_backend() <span class="k">noexcept</span>;</code></pre></div>

<p>The backend the engine tries first: <code>D3D12</code> when compiled, otherwise <code>VULKAN</code> when compiled, otherwise <code>NULL_DEVICE</code>.</p>

#### `is_backend_compiled` {#function-is-backend-compiled}

<div class="function-declaration" id="function-is-backend-compiled-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">bool</span> is_backend_compiled(
    <a href="backend.html#symbol-graphics-backend">graphics_backend</a> backend) <span class="k">noexcept</span>;</code></pre></div>

<p>True when the backend was compiled into this build. <code>NULL_DEVICE</code> is always <code>true</code>.</p>

#### `parse_graphics_backend` {#function-parse-graphics-backend}

<div class="function-declaration" id="function-parse-graphics-backend-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<a href="backend.html#symbol-graphics-backend">graphics_backend</a>&gt; parse_graphics_backend(
    std::string_view name) <span class="k">noexcept</span>;</code></pre></div>

<p>Parses a backend name case-insensitively, ignoring surrounding whitespace (&quot;<code>null</code>&quot;, &quot;d3d12&quot;, &quot;vulkan&quot;). Fails with <code>core::error_code::INVALID_ARGUMENT</code> for anything else.</p>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="backend.html#symbol-graphics-backend">graphics_backend</a> backend) <span class="k">noexcept</span>;</code></pre></div>

<p>The lower-case name of a backend: &quot;<code>null</code>&quot;, &quot;d3d12&quot; or &quot;vulkan&quot;; &quot;&quot; for an out-of-range value.</p>

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
<a id="default_graphics_backend"></a>
<a id="dependencies"></a>
<a id="device_capabilities"></a>
<a id="device_capabilitiesadaptername"></a>
<a id="device_capabilitiesbackend"></a>
<a id="device_capabilitiescomputequeue"></a>
<a id="device_capabilitiesdedicatedvideomemorybytes"></a>
<a id="device_capabilitiesmaxtexturedimension2d"></a>
<a id="device_capabilitiessoftwareadapter"></a>
<a id="device_capabilitiestimerqueries"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="graphics_backend"></a>
<a id="graphics_backendcount"></a>
<a id="graphics_backendd3d12"></a>
<a id="graphics_backendnull_device"></a>
<a id="graphics_backendvulkan"></a>
<a id="internals-and-algorithms"></a>
<a id="is_backend_compiled"></a>
<a id="other-declarations"></a>
<a id="parse_graphics_backend"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
