---
kind: header
---

[Engine](../../engine.md) → [RHI](../rhi.md)

# device.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/rhi/device.h` |
| API ↗ | [Devices and swapchains](../../../api/rhi/devices-and-swapchains.md), [Shaders](../../../api/rhi/shaders.md) |

<p>The graphics <code><a href="device/device.html">device</a></code>. <code>A</code> <code><a href="device/device.html">device</a></code> owns one <code>nvrhi::IDevice</code> created over the requested backend; <code>conjure_renderer</code> records into it through <code>native_device()</code>. NVRHI itself stays in engine/src: this header forward-declares <code>nvrhi::IDevice</code> only.</p>

## Structs

| File | Description |
|---|---|
| [`device::create_tag`](device/device-create-tag.md) |  |
| [`device_desc`](device/device-desc.md) |  |

## Classes

| File | Description |
|---|---|
| [`device`](device/device.md) |  |

## Enumerations

### `null_fault` {#symbol-null-fault}

```cpp
enum class null_fault : u8 {
    NONE = 0,
    WAIT_IDLE,
    PRESENT,
    ACQUIRE_OUT_OF_DATE,
    PRESENT_FAILED
};
```

<code>A</code> failure the <code>null</code> backend can be told to report once, so the engine&#x27;s error paths run in CTest without a GPU: <code>WAIT_IDLE</code> makes the next waitForIdle fail (<code>wait_idle()</code> then marks the <code><a href="device/device.html">device</a></code> lost), <code>PRESENT</code> makes the next <code>null</code> swapchain present report <code><a href="device/device.html">device</a></code> loss. <code>ACQUIRE_OUT_OF_DATE</code> makes the next <code>null</code> swapchain acquire fail with <code>rhi_error::OUT_OF_DATE</code> and <code>PRESENT_FAILED</code> the next present with <code>rhi_error::PRESENT_FAILED</code>; neither marks the <code><a href="device/device.html">device</a></code> lost or advances the back-buffer index, the way a real swapchain refuses without <code><a href="device/device.html">device</a></code> loss.

## Forward Declarations

### `device::impl` {#symbol-device-impl}

```cpp
struct impl;
```

### `IDevice` {#symbol-idevice}

```cpp
class IDevice;
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
<a id="device"></a>
<a id="device_desc"></a>
<a id="device_descadapterindex"></a>
<a id="device_descbackend"></a>
<a id="device_descenabledebuglayer"></a>
<a id="device_descenablegpuvalidation"></a>
<a id="device_descprefersoftwareadapter"></a>
<a id="device_descwindow"></a>
<a id="devicebackend"></a>
<a id="devicecapabilities"></a>
<a id="devicecreate"></a>
<a id="devicecreate_tag"></a>
<a id="devicedesc"></a>
<a id="devicedevice"></a>
<a id="devicedevice_1"></a>
<a id="deviceimpl"></a>
<a id="deviceinject_null_fault"></a>
<a id="deviceis_lost"></a>
<a id="devicemimpl"></a>
<a id="devicenative_device"></a>
<a id="devicenull_stats"></a>
<a id="deviceoperator"></a>
<a id="devicereset_null_stats"></a>
<a id="devicerun_garbage_collection"></a>
<a id="devicesimulate_device_loss"></a>
<a id="deviceswapchain"></a>
<a id="devicewait_idle"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="idevice"></a>
<a id="internals-and-algorithms"></a>
<a id="null_fault"></a>
<a id="null_faultacquire_out_of_date"></a>
<a id="null_faultnone"></a>
<a id="null_faultpresent"></a>
<a id="null_faultpresent_failed"></a>
<a id="null_faultwait_idle"></a>
<a id="other-declarations"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
