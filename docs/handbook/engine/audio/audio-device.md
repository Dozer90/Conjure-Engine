---
kind: header
---

[Engine](../../engine.md) → [Audio](../audio.md)

# audio_device.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/audio/audio_device.h` |
| API ↗ | [Playing sounds](../../../api/audio/playing-sounds.md) |

<p>Device descriptions shared by every backend.</p>

## Structs

| File | Description |
|---|---|
| [`audio_device_info`](audio-device/audio-device-info.md) | One playback device as reported by <code>enumerate_devices()</code>. |
| [`backend_desc`](audio-device/backend-desc.md) | What <code>audio_backend::start()</code> needs: the requested mix format (channels fixed at <code>MIX_CHANNELS</code>), the period size (480 = 10 ms at 48 kHz; miniaudio may round), the device name (empty = default) and the callback the backend drives. |

## Type Aliases

### `mix_callback` {#symbol-mix-callback}

```cpp
using mix_callback = void (*)(void *user, f32 *output, u32 frameCount) noexcept;
```

The function a backend drives to pull mixed audio: interleaved <code><a href="../core/types.html#symbol-f32">f32</a></code> frames of <code>MIX_CHANNELS</code> channels at the format negotiated by <code>start()</code>. Called on the device thread (miniaudio) or on the main thread (<code>null</code>); it must not allocate, lock or log.

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-NULL_DEVICE_NAME"></a><code>NULL_DEVICE_NAME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <span class="k">const</span> <span class="k">char</span> *</code> | <code class="signature"><span class="s">"Null Output"</span></code> | Name the <code>null</code> backend reports for its single virtual device. |

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="audio_device_info"></a>
<a id="audio_device_infoisdefault"></a>
<a id="audio_device_infomaxchannels"></a>
<a id="audio_device_infominchannels"></a>
<a id="audio_device_infoname"></a>
<a id="audio_device_infooperator"></a>
<a id="audio_device_infosamplerates"></a>
<a id="backend_desc"></a>
<a id="backend_desccallback"></a>
<a id="backend_descdevicename"></a>
<a id="backend_descformat"></a>
<a id="backend_descperiodframes"></a>
<a id="backend_descuser"></a>
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="mix_callback"></a>
<a id="null_device_name"></a>
<a id="other-declarations"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
