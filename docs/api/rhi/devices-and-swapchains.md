# Devices and swapchains

📘 **Public API wiki** · 🛠 Handbook: [RHI module](../../handbook/modules/rhi.md)

Create the graphics device, put frames on a window and know when the GPU has gone away. The device is in
`conjure/rhi/device.h`, the swapchain in `conjure/rhi/swapchain.h`; the smaller headers they build on are the
backend enumeration (`conjure/rhi/backend.h`), the opaque window handle (`conjure/rhi/native_window.h`), the
engine's texture formats (`conjure/rhi/texture_format.h`) and the null device's counters
(`conjure/rhi/null_device_stats.h`). Everything is in `namespace conjure::rhi`.

## At a glance

| You want to... | Use | Header |
|---|---|---|
| Pick the backend the engine would pick | `default_graphics_backend()` | `conjure/rhi/backend.h` |
| Ask whether a backend exists in this build | `is_backend_compiled(backend)` | `conjure/rhi/backend.h` |
| Read or print a backend name | `parse_graphics_backend("d3d12")`, `to_string(backend)` | `conjure/rhi/backend.h` |
| Create the device | `device::create(device_desc)` | `conjure/rhi/device.h` |
| Record GPU work (engine modules only) | `device.native_device()` | `conjure/rhi/device.h` |
| Describe the window to present to | `native_window`; a default one is headless | `conjure/rhi/native_window.h` |
| Show frames | `swapchain::create`, then `acquire()` / `present()` each frame | `conjure/rhi/swapchain.h` |
| React to a window resize | `swapchain.resize(width, height)` | `conjure/rhi/swapchain.h` |
| Detect a lost GPU | `device.is_lost()`, `rhi_error::DEVICE_LOST` | `conjure/rhi/device.h` |
| Size a texture or a row of pixels | `bytes_per_pixel(format)`, `row_pitch(format, width)` | `conjure/rhi/texture_format.h` |
| Assert on what a test drew | `device.null_stats()` | `conjure/rhi/null_device_stats.h` |

## Quick example

```cpp
#include "conjure/rhi/device.h"
#include "conjure/rhi/swapchain.h"

#include <memory>

using namespace conjure;
using namespace conjure::rhi;

// 'record' receives the back buffer to draw into through the device's NVRHI command lists.
core::status present_frames(u32 frameCount, void (*record)(device &gpu, nvrhi::ITexture *target)) {
    device_desc deviceDesc = {};
    deviceDesc.backend = graphics_backend::NULL_DEVICE;    // D3D12 + preferSoftwareAdapter runs this on WARP
    CONJURE_TRY_ASSIGN(std::unique_ptr<device> gpu, device::create(deviceDesc));

    swapchain_desc chainDesc = {};
    chainDesc.width = 256;
    chainDesc.height = 128;
    const native_window window = {};                       // kind NONE: headless, nothing is shown
    CONJURE_TRY_ASSIGN(std::unique_ptr<swapchain> chain, swapchain::create(*gpu, window, chainDesc));

    for (u32 frame = 0; frame < frameCount; ++frame) {
        CONJURE_TRY_ASSIGN(const u32 index, chain->acquire());
        record(*gpu, chain->back_buffer(index));
        CONJURE_TRY(chain->present());
        gpu->run_garbage_collection();                     // once per frame
    }

    chain.reset();                                         // the swapchain dies before its device
    return core::status::success();
}
```

## Choosing a backend

`graphics_backend` has three values: `NULL_DEVICE`, `D3D12` and `VULKAN`. `is_backend_compiled()` tells you
which ones this build contains (`NULL_DEVICE` always is) and `default_graphics_backend()` returns the first
compiled one in the order D3D12, Vulkan, null. `to_string()` gives the lower-case name (`"null"`, `"d3d12"`,
`"vulkan"`, and `""` for an out-of-range value) and `parse_graphics_backend()` reads one back, ignoring case
and surrounding whitespace; anything else is `core::error_code::INVALID_ARGUMENT`. That pair is what a
`--backend` command-line option or a settings file should use.

After creation, `device.capabilities()` reports what the adapter can do:

| `device_capabilities` field | Meaning |
|---|---|
| `backend` | the backend the device runs on |
| `adapterName` | the adapter's name (`"Conjure Null Device"` on the null backend) |
| `maxTextureDimension2d` | largest 2D texture side; 16384 on every current backend |
| `dedicatedVideoMemoryBytes` | video memory the adapter owns; 0 for software adapters |
| `computeQueue`, `timerQueries` | whether an async compute queue and GPU timers exist |
| `softwareAdapter` | true for WARP, a CPU Vulkan driver and the null device |

## Creating a device

Fill a `device_desc` and call `device::create()`. The description is kept verbatim and readable through
`desc()`.

| `device_desc` field | Default | Meaning |
|---|---|---|
| `backend` | `NULL_DEVICE` | which backend to create |
| `enableDebugLayer` | `false` | the D3D12 debug layer or the Vulkan validation layers, plus NVRHI's own validation layer on every backend (the null device included) |
| `enableGpuValidation` | `false` | D3D12 GPU-based validation; needs `enableDebugLayer` or the description is `INVALID_DESC` |
| `adapterIndex` | `0` | index among the hardware adapters, fastest first; software adapters are not counted |
| `preferSoftwareAdapter` | `false` | WARP on D3D12, a CPU driver on Vulkan: deterministic, GPU-free, for tests and CI |
| `window` | headless | Vulkan only: picks the surface extensions for the window kind; ignored by D3D12 and the null device |

`create()` fails with `core::error_code::UNSUPPORTED` when the backend is not compiled in,
`rhi_error::INVALID_DESC` when the description contradicts itself (an out-of-range backend,
`enableGpuValidation` without the debug layer) and `rhi_error::DEVICE_CREATION_FAILED` when the API refuses;
the message quotes the API's reason, for example an adapter index past the last adapter.

Once you have a device: `run_garbage_collection()` once per frame releases what finished command lists no
longer need, `wait_idle()` blocks until the GPU is idle, the last present included (and marks the device lost
when the wait fails), and
`native_device()` returns the non-owning NVRHI device that engine modules record through.

> [!TIP]
> Turn `enableDebugLayer` on in Debug builds while developing GPU code. NVRHI's validation layer catches
> wrong resource states, missing bindings and mismatched descriptors before the driver does, and its messages
> arrive in your log under `LOG_RHI` at the matching level.

> [!WARNING]
> `device::create`, destruction, `run_garbage_collection()`, `wait_idle()` and everything on a swapchain
> belong to the render thread (the main thread today). Only `is_lost()` may be read from any thread. Destroy
> every swapchain and every NVRHI object before the device; the device waits for the GPU in its destructor,
> so order is the only thing you have to get right.

## Windows and headless mode

`native_window` carries the operating-system handles as opaque pointers: `kind` (`NONE`, `WINDOWS`, `COCOA`,
`X11`, `WAYLAND`), `window`, `display` and `x11Window`. The engine copies the platform window's native handle
into it field for field when it creates the swapchain; you only build one yourself when you are not using the
engine's window. A default-constructed handle has kind `NONE`, which `is_headless()` reports, and means:

| Backend | A `NONE` window |
|---|---|
| Null | ignored; the swapchain exists and cycles its buffers |
| D3D12 | a composition swapchain that is never attached to anything: nothing is shown, but acquire, clear, present and resize all run for real (this is how the GPU tests work on WARP) |
| Vulkan | `INVALID_DESC`: Vulkan has no windowless swapchain |

`to_string(native_window_kind)` returns the enumerator name, and `""` for an out-of-range value.

## Swapchains

A swapchain owns `bufferCount` back buffers of one format and presents them in turn. Create it with the
device, the window and a `swapchain_desc`:

| `swapchain_desc` field | Default | Accepted |
|---|---|---|
| `width`, `height` | `0` | the window's framebuffer size in pixels; zero is `INVALID_DESC` |
| `bufferCount` | `3` | 2 to 16 |
| `format` | `RGBA8_SRGB` | `RGBA8_UNORM`, `RGBA8_SRGB`, `BGRA8_SRGB` or `RGBA16_FLOAT`; anything else "cannot be presented" |
| `vsync` | `true` | `false` lets the backend tear when the display allows it |

Each frame, `acquire()` returns the index of the back buffer to draw into and `present()` shows it.
`back_buffer(index)` is the non-owning NVRHI texture behind that index (nullptr for `index >= buffer_count()`),
`current_index()` repeats the last `acquire()` result and `desc()` reports the current configuration: the size
follows `resize()`, the format and buffer count are as created. `owner()` is the device.

`resize(width, height)` waits for the GPU (the last present included), recreates the back buffers and discards
an image that was acquired but not presented. Release everything you hold over a back buffer first (a
framebuffer, a texture handle): while a reference is left, D3D12 and the null backend refuse with
`SWAPCHAIN_CREATION_FAILED`. With a renderer, call `renderer::resize_swapchain(size)` instead, which drops the
renderer's own framebuffers and then resizes. Zero is `INVALID_DESC`; `OUT_OF_DATE` on Vulkan means the surface is currently empty (a
minimised window), so keep the old size and try again later. A resize the API refuses
(`SWAPCHAIN_CREATION_FAILED`) can leave the swapchain without back buffers; on every backend `acquire()` and
`present()` then return `OUT_OF_DATE` until a later resize succeeds.

> [!WARNING]
> Every `back_buffer()` pointer is invalid after `resize()`; fetch it again after each `acquire()` rather
> than caching it. After a failed resize `buffer_count()` may be 0 until a later resize succeeds, and
> `back_buffer(i)` is non-null exactly for `i < buffer_count()`.

> [!TIP]
> Treat `OUT_OF_DATE` from `acquire()` or `present()` as "resize and draw again", not as an error to report:
> it is the normal signal that the window changed under you (Vulkan) or that the last resize did not
> complete (every backend). `PRESENT_FAILED` is the real failure (the API refused the acquire or present for
> another reason) and is worth logging.

## Device loss

A GPU can be removed, reset by the driver or hit a fatal error. From then on `device.is_lost()` is true and
every `swapchain::create`, `acquire()`, `present()` and `resize()` returns `rhi_error::DEVICE_LOST`; the
back buffers stay readable so you can tear down without dereferencing null, and destroying the swapchain and
device is safe. Recovery (recreating everything) is the engine's job; today it logs and quits.

```cpp
#include "conjure/rhi/device.h"
#include "conjure/rhi/rhi_error.h"
#include "conjure/rhi/swapchain.h"

#include "conjure/core/log.h"

using namespace conjure;
using namespace conjure::rhi;

// Presents the frame; resizes and reports OUT_OF_DATE as success, reports every other failure.
core::status present_or_resize(device &gpu, swapchain &chain, u32 width, u32 height) {
    const core::status presented = chain.present();
    if (presented) {
        return presented;
    }
    if (presented.get_error().is(rhi_error::OUT_OF_DATE)) {
        return chain.resize(width, height);
    }
    if (gpu.is_lost()) {
        CONJURE_LOG_ERROR(LOG_RHI, "device lost, shutting the renderer down: {}", presented.get_error());
    }
    return presented;
}
```

`simulate_device_loss()` makes any device behave as if the API had removed it, so your own device-loss path
can be tested on the null device: after the call `is_lost()` is true and the next present fails with
`DEVICE_LOST`.

## Texture formats

`texture_format` is the short list of formats the engine uses for render targets, vertex streams and asset
payloads: `R8_UNORM`, `RG8_UNORM`, `RGBA8_UNORM`, `RGBA8_SRGB`, `BGRA8_SRGB`, `RG16_SNORM`, `RGBA16_SNORM`,
`R16_FLOAT`, `RG16_FLOAT`, `RGBA16_FLOAT`, `R32_FLOAT`, `RG32_FLOAT`, `RGB32_FLOAT`, `RGBA32_FLOAT`,
`R32_UINT`, `R11G11B10_FLOAT`, `D32_FLOAT` and the reserved block-compressed `BC1_SRGB`, `BC5_UNORM`,
`BC7_SRGB`. The helpers are `constexpr`, so they work in `static_assert` and array sizes:

| Helper | Returns |
|---|---|
| `bytes_per_pixel(format)` | bytes of one pixel; 0 for `UNKNOWN` and the compressed formats |
| `block_size(format)` | pixels per side of a storage block: 4 for BC formats, 1 otherwise |
| `bytes_per_block(format)` | bytes of one block: 8 for BC1, 16 for BC5 and BC7, `bytes_per_pixel` otherwise |
| `row_pitch(format, width)` | bytes of a tightly packed row, rounded up to whole blocks (`row_pitch(RGBA8_UNORM, 3) == 12`, `row_pitch(BC1_SRGB, 5) == 16`) |
| `is_depth`, `is_srgb`, `is_compressed` | format classification (`D32_FLOAT` is the only depth format) |
| `to_string(format)`, `parse_texture_format(name)` | the enumerator name and back, case-insensitive |

> [!NOTE]
> The compressed formats are reserved for a later asset cook step; nothing produces them yet, and a swapchain
> never accepts them.

## Testing without a GPU

The null device is a complete device: every resource you create is a live handle, buffers keep real CPU
storage so writes and copies round-trip, and command lists count what they record. Like D3D12, it refuses
three kinds of misuse with an error in the log: a buffer write or copy past the end of a buffer and a copy
between overlapping ranges of one buffer store nothing, and a buffer or staging texture created without CPU
access does not map. `device.null_stats()` returns a snapshot of the counters (`std::nullopt` on D3D12 and
Vulkan) and `reset_null_stats()` clears them. The most useful fields:

| `null_device_stats` field | Counts |
|---|---|
| `texturesCreated`, `buffersCreated`, `shadersCreated`, `graphicsPipelinesCreated`, ... | resource creation by type |
| `commandListsOpened`, `commandListsClosed`, `commandListsSubmitted` | command list lifecycle |
| `draws`, `instancesDrawn`, `dispatches`, `textureClears`, `bufferWrites`, `bufferWriteBytes`, `textureCopies` | recorded work |
| `markersBegun`, `markerNames`, `markerNamesDropped` | debug markers: `markersBegun` counts every `beginMarker`; `markerNames` holds the names in recording order (your pass names), stored only after your first `reset_null_stats()` and at most `MAX_RECORDED_MARKER_NAMES` (4096) per reset, the rest counted in `markerNamesDropped` |
| `garbageCollections`, `waitsForIdle`, `acquires`, `presents`, `resizes` | device and swapchain calls |

```cpp
#include "conjure/testing/doctest.h"

#include "conjure/rhi/device.h"
#include "conjure/rhi/null_device_stats.h"

#include <memory>
#include <optional>

using namespace conjure;
using namespace conjure::rhi;

TEST_CASE("my pass - records one draw under its marker") {
    device_desc desc = {};
    desc.backend = graphics_backend::NULL_DEVICE;
    core::result<std::unique_ptr<device>> created = device::create(desc);
    CONJURE_REQUIRE_OK(created);
    device &gpu = *created.value();

    gpu.reset_null_stats();
    gpu.run_garbage_collection();                  // your pass would record and submit here

    const std::optional<null_device_stats> stats = gpu.null_stats();
    REQUIRE(stats.has_value());
    CHECK(stats->garbageCollections == 1);
    CHECK(stats->draws == 0);
}
```

`inject_null_fault(null_fault::WAIT_IDLE)` makes the next `wait_idle()` fail and mark the device lost;
`inject_null_fault(null_fault::PRESENT)` makes the next null present report `DEVICE_LOST`.
`null_fault::ACQUIRE_OUT_OF_DATE` makes the next `acquire()` fail with `OUT_OF_DATE` and
`null_fault::PRESENT_FAILED` the next `present()` with `PRESENT_FAILED`; those two leave the device healthy and the
back-buffer index where it was, as a real swapchain refuses without device loss. Each fault fires once,
`null_fault::NONE` disarms, and the calls do nothing on a real backend. That is how the engine's own error paths
run in CTest.

> [!WARNING]
> `null_stats()` is a copy taken under a lock, so it is safe to read from any thread and never changes after
> it is returned. Marker names are stored only once you have asked for them: call `reset_null_stats()` before
> the frames you want to assert on; without it `markerNames` stays empty and every `beginMarker` is counted in
> `markerNamesDropped` instead. Each reset keeps at most `MAX_RECORDED_MARKER_NAMES` (4096) names, so reset
> again between long runs of frames.

## API summary

| Signature (short) | Purpose |
|---|---|
| `enum class graphics_backend : u8 { NULL_DEVICE, D3D12, VULKAN }` | the backends |
| `std::string_view to_string(graphics_backend)` / `result<graphics_backend> parse_graphics_backend(std::string_view)` | names |
| `bool is_backend_compiled(graphics_backend)` / `graphics_backend default_graphics_backend()` | availability |
| `struct device_capabilities { backend, adapterName, maxTextureDimension2d, dedicatedVideoMemoryBytes, computeQueue, timerQueries, softwareAdapter }` | what the adapter can do |
| `struct device_desc { backend, enableDebugLayer, enableGpuValidation, adapterIndex, preferSoftwareAdapter, window }` | how to create the device |
| `static result<std::unique_ptr<device>> device::create(const device_desc &)` | create |
| `graphics_backend backend()` / `const device_desc &desc()` / `const device_capabilities &capabilities()` | inspect |
| `bool is_lost()` / `nvrhi::IDevice *native_device()` | state and the NVRHI device |
| `void run_garbage_collection()` / `void wait_idle()` | per-frame and shutdown |
| `std::optional<null_device_stats> null_stats()` / `void reset_null_stats()` | null device counters |
| `void simulate_device_loss()` / `void inject_null_fault(null_fault)` | test hooks |
| `enum class native_window_kind : u8 { NONE, WINDOWS, COCOA, X11, WAYLAND }` | window handle kinds |
| `struct native_window { kind, window, display, x11Window; bool is_headless() }` | the opaque window |
| `struct swapchain_desc { width, height, bufferCount = 3, format = RGBA8_SRGB, vsync = true }` | how to create the swapchain |
| `static result<std::unique_ptr<swapchain>> swapchain::create(device &, const native_window &, const swapchain_desc &)` | create |
| `result<u32> acquire()` / `status present()` / `status resize(u32, u32)` | the frame |
| `nvrhi::ITexture *back_buffer(u32)` / `u32 buffer_count()` / `u32 current_index()` / `const swapchain_desc &desc()` / `device &owner()` | inspect |
| `enum class texture_format : u8` (21 formats) | the engine's formats |
| `constexpr u32 bytes_per_pixel(f)` / `block_size(f)` / `bytes_per_block(f)` / `constexpr u64 row_pitch(f, width)` | sizes |
| `constexpr bool is_depth(f)` / `is_srgb(f)` / `is_compressed(f)` | classification |
| `std::string_view to_string(texture_format)` / `result<texture_format> parse_texture_format(std::string_view)` | names |
| `struct null_device_stats { ... u32 counters, std::vector<std::string> markerNames, u32 markerNamesDropped; void reset() }` | what the null device recorded |
| `inline constexpr u32 MAX_RECORDED_MARKER_NAMES = 4096` | marker names kept per `reset_null_stats()`; the rest count in `markerNamesDropped` |

## See also

- 📘 API: [Shaders](shaders.md) for loading the compiled shaders a device needs
- 📘 API: [RHI overview](index.md) for the backends and when to use the renderer instead
- 📘 API: [Errors and results](../core/errors-and-results.md) for `result<T>`, `status` and `CONJURE_TRY`
- 📘 API: [Glossary](../glossary.md)
- 🛠 Handbook: [RHI module](../../handbook/modules/rhi.md) for how each backend implements the swapchain
