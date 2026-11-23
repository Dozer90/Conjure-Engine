---
kind: header
---

[Engine](../../engine.md) → [RHI](../rhi.md)

# swapchain.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/rhi/swapchain.h` |
| API ↗ | [Devices and swapchains](../../../api/rhi/devices-and-swapchains.md) |

<p>The <code><a href="swapchain/swapchain.html">swapchain</a></code>. Back buffers are NVRHI textures wrapped around the API&#x27;s buffers (createHandleForNativeTexture), in the Present state with keepInitialState, so the renderer&#x27;s framebuffer cache and automatic barriers treat them like any other target.</p>

## Structs

| File | Description |
|---|---|
| [`swapchain::create_tag`](swapchain/swapchain-create-tag.md) |  |
| [`swapchain_desc`](swapchain/swapchain-desc.md) |  |

## Classes

| File | Description |
|---|---|
| [`swapchain`](swapchain/swapchain.md) |  |

## Forward Declarations

### `ITexture` {#symbol-itexture}

```cpp
class ITexture;
```

### `swapchain::impl` {#symbol-swapchain-impl}

```cpp
struct impl;
```

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
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="itexture"></a>
<a id="structs-and-types"></a>
<a id="swapchain"></a>
<a id="swapchain_desc"></a>
<a id="swapchain_descbuffercount"></a>
<a id="swapchain_descformat"></a>
<a id="swapchain_descheight"></a>
<a id="swapchain_descvsync"></a>
<a id="swapchain_descwidth"></a>
<a id="swapchainacquire"></a>
<a id="swapchainback_buffer"></a>
<a id="swapchainbuffer_count"></a>
<a id="swapchaincreate"></a>
<a id="swapchaincreate_tag"></a>
<a id="swapchaincurrent_index"></a>
<a id="swapchaindesc"></a>
<a id="swapchainimpl"></a>
<a id="swapchainmimpl"></a>
<a id="swapchainoperator"></a>
<a id="swapchainowner"></a>
<a id="swapchainpresent"></a>
<a id="swapchainresize"></a>
<a id="swapchainswapchain"></a>
<a id="swapchainswapchain_1"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
