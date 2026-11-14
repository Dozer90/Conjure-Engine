# RHI module

🛠 **Engineering handbook** · 📘 API: [Graphics device layer](../../api/rhi/index.md)

`conjure::rhi` is the graphics device layer: one `nvrhi::IDevice` per process behind `rhi::device`, a
`swapchain` per window, and the `shader_library` that serves the blobs `conjure_add_shaders()` compiled.
This chapter is for people changing the module, adding a backend or touching the shader pipeline. It covers
how the public classes sit over the private backend interfaces, what each backend does differently, how the
null device records enough to test a whole renderer, how shaders get built and indexed, and how the module is
tested with and without a GPU. The binding design is [`../../design/02_renderer.md`](../../design/02_renderer.md)
sections 1, 3.12, 4.1 and 7; the module README is `engine/src/rhi/README.md`.

## Contents

- [1. Purpose and position](#1-purpose-and-position)
- [2. Public headers](#2-public-headers)
- [3. Internals](#3-internals)
- [4. Swapchains per backend](#4-swapchains-per-backend)
- [5. Shader pipeline](#5-shader-pipeline)
- [6. Threading](#6-threading)
- [7. Extension points](#7-extension-points)
- [8. Failure handling](#8-failure-handling)
- [9. Tests](#9-tests)
- [10. Open items](#10-open-items)
- [Related](#related)

## 1. Purpose and position

Wave 2 module; static library `conjure::rhi` built by `conjure_add_module(rhi ...)` in
`engine/src/rhi/CMakeLists.txt` when `CONJURE_WITH_RENDERER` is on. It depends publicly on `conjure::core`
and privately on `conjure::thirdparty::nvrhi` (plus the Vulkan headers with `VK_NO_PROTOTYPES` when
`CONJURE_RHI_VULKAN`). It does not link `conjure::platform`: `rhi::native_window` mirrors
`platform::native_window_handle` field for field and the engine copies one into the other. Above it sit the
renderer (wave 3), which records into `device::native_device()`, and the engine (wave 4), which owns the
device, swapchain and shader library and destroys them in reverse order.

| Option | Default | Effect |
|---|---|---|
| `CONJURE_WITH_RENDERER` | ON | builds this module, `engine/shaders` and (wave 3) the renderer |
| `CONJURE_RHI_D3D12` | ON on Windows, forced OFF elsewhere | compiles `backends/d3d12/` and the DXIL column of every shader |
| `CONJURE_RHI_VULKAN` | OFF on Windows, ON elsewhere | compiles `backends/vulkan/` and the SPIR-V column |
| `CONJURE_SPIRV_COMPILER` | `glslang` | `dxc` switches the SPIR-V step to `dxc -spirv` |

At least one real backend must be on when the renderer is built; the null backend is always compiled. A
backend that is off never reaches the compiler, and `device::create` answers `UNSUPPORTED` for it.

## 2. Public headers

| Header (`conjure/rhi/`) | Contents | Wiki page |
|---|---|---|
| `backend.h` | `graphics_backend`, `device_capabilities`, `to_string`, `parse_graphics_backend`, `is_backend_compiled`, `default_graphics_backend` | [Devices and swapchains](../../api/rhi/devices-and-swapchains.md) |
| `device.h` | `null_fault`, `device_desc`, `device` | same |
| `swapchain.h` | `swapchain_desc`, `swapchain` | same |
| `native_window.h` | `native_window_kind`, `native_window` | same |
| `texture_format.h` | `texture_format` and its `constexpr` helpers | same |
| `null_device_stats.h` | `null_device_stats` | same |
| `shader_library.h` | `shader_stage`, `shader_key`, `shader_blob`, `shader_library` | [Shaders](../../api/rhi/shaders.md) |
| `rhi_error.h` | `rhi_error`, category `"rhi"` | errors are met through `result<T>` |
| `log_category.h` | `LOG_RHI` | - |

NVRHI never appears in a public header (R15): `device.h` and `swapchain.h` forward-declare `nvrhi::IDevice`
and `nvrhi::ITexture` and expose non-owning `native_device()` / `back_buffer()` accessors.

## 3. Internals

### 3.1 Layout

```
engine/src/rhi/
  device.cpp, device_impl.h        public device over a device_backend; validation wrapper when enableDebugLayer
  device_backend.h/.cpp            device_backend + swapchain_backend interfaces; validate_swapchain_desc
  swapchain.cpp                    public swapchain over a swapchain_backend
  shader_library.cpp               index.json parsing, lazy blob loading, reload
  message_callback.h/.cpp          nvrhi::IMessageCallback -> LOG_RHI; Fatal -> lost flag
  texture_format.cpp, texture_format_mapping.h   names, parsing, constexpr NVRHI mapping (static_assert round trip)
  backends/null/                   null_device, null_resources, null_command_list, null_swapchain
  backends/d3d12/                  device_d3d12, swapchain_d3d12 (DXGI + nvrhi::d3d12)
  backends/vulkan/                 device_vulkan, swapchain_vulkan, vulkan_loader, vulkan_common.h
```

### 3.2 Device over `device_backend`

`device::impl` (`device_impl.h`) holds the `device_desc`, a `message_callback`, the `device_backend` and the
`nvrhi::DeviceHandle` the renderer sees, declared in reverse destruction order: the validation wrapper
releases the backend's device first, the backend then destroys the API objects, and the callback outlives
both because NVRHI may still report during teardown. `swapchain.cpp` is a friend so it can reach
`impl->backend->create_swapchain()` without the public header exposing `device_backend`.

`device::create` validates the description (out-of-range backend, `enableGpuValidation` without
`enableDebugLayer`), refuses an uncompiled backend, builds the backend through a `switch` guarded by the
`CONJURE_RHI_*` macros, and wraps the backend's device in `nvrhi::validation::createValidationLayer` when
`enableDebugLayer` is set, on every backend including the null one. `is_lost()` is the message callback's
atomic flag or, failing that, a cheap `poll_device_removed()` on the backend (`GetDeviceRemovedReason`,
a `VK_ERROR_DEVICE_LOST` seen by a queue operation; never on the null device) whose positive answer is
latched into the flag. Both destructors (`~device`, `~swapchain`) ask that polling `is_lost()` before
`waitForIdle` / `runGarbageCollection`, because a removed device must not be waited on.

| `device_backend` method | Contract |
|---|---|
| `nvrhi_device()` | the device built over the API objects, never the validation wrapper |
| `capabilities()` | discovered once at creation |
| `create_swapchain(window, desc)` | the description was already validated by `validate_swapchain_desc` |
| `poll_device_removed()` | ask the API; the null backend returns false |
| `as_null()` | the null device or nullptr; drives `null_stats`, `reset_null_stats`, `inject_null_fault` |

### 3.3 Message callback and validation layer

`message_callback` is one `nvrhi::IMessageCallback` per device. `Info`, `Warning` and `Error` go to
`LOG_RHI` at the same level prefixed `nvrhi:`; `Fatal` is logged as an error and sets the lost flag
(`mark_lost()`, release/acquire atomics, safe from any thread). `device::simulate_device_loss()` simply routes
a `Fatal` message through it, which is why it works on every backend. Backends call `mark_lost()` when a
present or resize sees `DXGI_ERROR_DEVICE_REMOVED` / `VK_ERROR_DEVICE_LOST`, and `device::wait_idle()` calls
it when `waitForIdle()` returns false.

### 3.4 Null backend

`null_device` (`backends/null/null_device.h`) is a complete `nvrhi::RefCounter<nvrhi::IDevice>`:

| Area | Behaviour |
|---|---|
| Resources | every `create*` stores the descriptor and returns a live handle; invalid descriptors (zero-sized texture, empty shader blob, pipeline without a vertex shader) return `nullptr` and an `Error` message like a real backend. `null_backend::make_resource` (`null_resources.h`) is the single `new` site, because `RefCounter::Release` deletes the object itself |
| Buffers and staging textures | buffers own zero-filled CPU storage so `mapBuffer` / `writeBuffer` / `copyBuffer` round-trip for buffers created with CPU access; an out-of-range `writeBuffer` / `copyBuffer`, or a `copyBuffer` between overlapping ranges of one buffer, reports an `Error` and stores nothing (`bufferWriteBytes` counts only stored bytes); mapping a GPU-only buffer or staging texture (`CpuAccessMode::None`) returns `nullptr` with an `Error`, as on D3D12, while a different map mode or a second map stays legal. Staging textures map to zero-filled memory with a tight row pitch, so readback is zero |
| Command lists | count draws, instances, dispatches, clears, writes, copies and keep `beginMarker` names in recording order (once armed, see Statistics); do not check for misuse (recording on a closed list, a second `open()` and a draw without a graphics state all succeed); that is NVRHI's validation layer's job: tests that want misuse caught create the null device with `enableDebugLayer`, as the renderer fixtures do; track the explicitly set states (`beginTracking*State`, `set*State`, `setPermanent*State`) so `getTextureSubresourceState` / `getBufferState` report them. The implicit transitions of automatic barriers (`setGraphicsState`, clears, copies, writes) are not modelled; an untracked resource reports its `initialState` with `keepInitialState`, `Unknown` otherwise. Per-list tracking resets on `open()`; a permanent state lives on the `null_texture` / `null_buffer` itself, visible to every list and gone with the resource. `executeCommandLists` returns an increasing fence, refuses an open list and drops the references the recording held (the null GPU has finished a list the moment it is executed, which is when NVRHI's queue lets go of what the list used) |
| Format support | `queryFormatSupport` answers like a D3D12 device: every known format is `Texture` and `ShaderLoad`; non-integer formats are `ShaderSample`; depth formats add `DepthStencil` only; uncompressed formats add `RenderTarget`, `Buffer` and `VertexBuffer` (plus UAV load/store unless sRGB); `Blendable` excludes integer and compressed formats |
| Features | `queryFeatureSupport` is true for `ComputeQueue`, `CopyQueue`, `ConstantBufferRanges`, `DeferredCommandLists`, `VirtualResources`, `WaveLaneCountMinMax` (32 lanes); every GPU-only feature is false. `getGraphicsAPI()` reports `D3D12` so API-dependent code takes the zero-based `SV_InstanceID` path |
| Statistics | `record(fn)` applies `fn(null_device_stats &)` under `mStatsMutex`; `stats()` snapshots; `reset_stats()` clears. Marker names are stored only after the first `reset_stats()` armed the recording and at most `MAX_RECORDED_MARKER_NAMES` (4096) per reset, the rest counted in `markerNamesDropped`, so a headless run that never resets allocates nothing per frame (the renderer's allocation gates measure through this device) |
| Faults | `inject_fault` arms one `null_fault`; `consume_fault(fault)` returns true once for the armed one and disarms. `waitForIdle` consumes `WAIT_IDLE`, the null swapchain's present consumes `PRESENT` |

Capabilities report `"Conjure Null Device"`, 16384, 0 bytes of video memory, compute queue and timer queries
true, `softwareAdapter` true. `create_null_backend` never fails.

### 3.5 D3D12 backend

`device_d3d12.cpp` creates a DXGI factory (with `DXGI_CREATE_FACTORY_DEBUG` when the debug layer is on and
available; a missing debug layer is a warning, not a failure), enumerates hardware adapters by GPU
preference skipping software adapters so `adapterIndex` counts GPUs, takes WARP through `EnumWarpAdapter`
when `preferSoftwareAdapter`, creates a feature level 11_0 device with one DIRECT, COMPUTE and COPY queue
each and hands them to `nvrhi::d3d12::createDevice`. `poll_device_removed()` is `GetDeviceRemovedReason() != S_OK`.

`flush_queues()` (a `device_backend` virtual that only D3D12 implements) signals a private fence on the graphics
queue and waits for it. NVRHI's `waitForIdle` stops at the last command list NVRHI executed and never sees the DXGI
present queued behind it, so `device::wait_idle()`, `~device`, `~d3d12_swapchain` and `d3d12_swapchain::resize` all
call it before a back buffer may be released; without it the debug layer reports
`OBJECT_DELETED_WHILE_STILL_IN_USE` and breaks with exception `0x87D` (code sweep F011, F071). The wait polls for
device removal every 100 ms and never blocks on a removed device.

The debug layer is process-wide: once a device enabled it, every later D3D12 device of the process runs under it
(`debug_layer_inherited()`, shown in the "created d3d12 device" line). While it is on, its messages are routed to
`LOG_RHI` through `ID3D12InfoQueue1` (Windows 11): errors and corruption as `ERROR`, warnings as `WARN`. A
validation error therefore fails every test case that holds a `log_capture`.

### 3.6 Vulkan backend

`vulkan_loader` loads `vulkan-1.dll` / `libvulkan.so.1` / `libvulkan.1.dylib` or `libMoltenVK.dylib` through
`core::dynamic_library` and initialises vulkan.hpp's dynamic dispatcher in three stages (global, instance,
device), so nothing links against the Vulkan loader and a machine without it fails `device::create` with
`DEVICE_CREATION_FAILED` naming the libraries tried. `device_vulkan.cpp` builds instance (surface extensions
chosen from `device_desc::window.kind`), physical device, device and queues over `nvrhi::vulkan::createDevice`.

### 3.7 Shader library

`shader_library.cpp` parses `index.json` with `core::json`: `version` must be 1, `shaders` an array, every
entry an object with string `name`, `entry` and `stage` (`defines`, `dxil` and `spirv` default to empty); an
unknown stage or a duplicate `name/entry/defines` is `PARSE_FAILURE` naming the entry. `open` picks the
column for the device's backend (DXIL on D3D12, SPIR-V on Vulkan; the null device takes DXIL and falls back
to SPIR-V). `get` loads a blob once into a cache keyed by entry and returns a span into it; the file is read
in one call into a buffer sized by `std::filesystem::file_size`, and an empty file is `IO_FAILURE`. `reload`
parses into a fresh table and swaps it in only on success, dropping the cache. `sourceHash` is ignored (it
serves the build's incremental rebuilds). The index's UTF-8 blob paths become paths through a local
`path_from_utf8()` (`std::u8string`), and paths in messages and log lines go back through `display_path()`
(`path::u8string()`): on Windows a narrow string is decoded in the ANSI code page, and `path::string()` throws
for a character outside it.

## 4. Swapchains per backend

`swapchain::create` runs `validate_swapchain_desc` (non-zero size, 2..16 buffers, a presentable format:
`RGBA8_UNORM`, `RGBA8_SRGB`, `BGRA8_SRGB`, `RGBA16_FLOAT`), checks `is_lost()` and asks the backend. The
public wrapper adds the `DEVICE_LOST` short-circuit on `acquire`, `present` and `resize`, logs a present that
observed loss, and rejects a zero resize before the backend sees it. On every backend `buffer_count()` is the
number of back buffers that exist now, 0 after a failed resize until one succeeds; while it is, `acquire` and
`present` return `OUT_OF_DATE`.

| | Null | D3D12 | Vulkan |
|---|---|---|---|
| API object | `bufferCount` null textures in `Present` state with `keepInitialState` | flip-discard `IDXGISwapChain3`, buffers wrapped with `createHandleForNativeTexture` | `VkSurfaceKHR` + `VkSwapchainKHR`, images wrapped the same way; binary semaphores through `queueWaitForSemaphore` / `queueSignalSemaphore` |
| `NONE` window | ignored | composition swapchain never attached to a visual: the real present path without a window (the `[gpu]` tests) | `INVALID_DESC` (also `COCOA` until MoltenVK surfaces land) |
| Format and vsync | mapped directly | flip model forbids sRGB buffers: created `UNORM`, the NVRHI wrapper carries the sRGB format | the requested format must be offered with the sRGB non-linear colour space or creation is `SWAPCHAIN_CREATION_FAILED`; present mode FIFO with vsync, else mailbox, then immediate |
| `acquire` | returns the current index | `GetCurrentBackBufferIndex`; `OUT_OF_DATE` while a failed resize left no back buffers | `vkAcquireNextImageKHR` with the frame slot's semaphore; `OUT_OF_DATE` when no usable swapchain exists or the surface changed, `VK_ERROR_DEVICE_LOST` marks lost, anything else is `PRESENT_FAILED` |
| `present` | advances the index (`0, 1, 2, 0, ...`); consumes a `PRESENT` fault as `DEVICE_LOST` | `Present`, tearing only with `vsync = false` and DXGI support; `DXGI_ERROR_DEVICE_REMOVED` marks lost, `DXGI_ERROR_INVALID_CALL` is `PRESENT_FAILED`; `OUT_OF_DATE` while a failed resize left no back buffers | `vkQueuePresentKHR`; `VK_ERROR_OUT_OF_DATE_KHR` / `VK_ERROR_SURFACE_LOST_KHR` are `OUT_OF_DATE`, `VK_SUBOPTIMAL_KHR` is success, `VK_ERROR_DEVICE_LOST` marks lost, anything else is `PRESENT_FAILED` |
| `resize` | refuses with `SWAPCHAIN_CREATION_FAILED`, changing nothing, while a back buffer is referenced outside the swapchain (as `ResizeBuffers` does); otherwise recreates buffers, restarts at 0 | waits idle, `flush_queues()` for the last present, releases wrappers, `ResizeBuffers` (refused while the caller still holds a framebuffer over a back buffer), re-wraps | flushes a pending acquire (its semaphore wait is submitted empty), queries the surface and returns `OUT_OF_DATE` for a zero extent without touching the old swapchain, passes the old `VkSwapchainKHR` as `oldSwapchain` and destroys it exactly once; new semaphores are built before the old set is destroyed |
| Back-buffer usage | - | render target | `COLOR_ATTACHMENT` guaranteed; `TRANSFER_DST` only when the surface offers it, otherwise a WARN says `clearTextureFloat` cannot be used on a back buffer and the renderer must clear through a render pass |

## 5. Shader pipeline

### 5.1 `cmake/ConjureShaders.cmake`

`conjure_add_shaders(TARGET OUTPUT_DIR [SOURCE_DIR] [INCLUDE_DIRS] [DEPENDENCIES] SHADERS ...)` expands each
`source;entry;stage;defines` entry with `conjure_shader_permutations` (cartesian product of `NAME=v1,v2`
axes, axis order preserved; an empty define string is carried as `__NONE__` because a CMake list cannot hold
an empty element) and registers one custom command per blob. The blob stem is
`<name>.<entry>[.<NAME_value>...]`.

| Column | Compiler and flags |
|---|---|
| DXIL (`CONJURE_RHI_D3D12`) | `dxc -T <vs|ps|cs>_6_5 -E <entry> -HV 2018 -Zpr -WX -D CONJURE_D3D12=1 -D<defines> -I <dirs> -Fo <out>.dxil`; `-Zi -Qembed_debug` outside Distribution, `-O3` in it |
| SPIR-V, glslang (default) | `glslang -D -S <vert|frag|comp> -e <entry> -V --target-env vulkan1.2 --hlsl-iomap --auto-map-locations --shift-texture-binding 0 --shift-sampler-binding 128 --shift-ubo-binding 256 --shift-uav-binding 384 --shift-ssbo-binding 0 -DCONJURE_VULKAN=1 ... -o <out>.spv`; `-g` in Debug, `-Os` otherwise |
| SPIR-V, `CONJURE_SPIRV_COMPILER=dxc` | `dxc -spirv -fspv-target-env=vulkan1.2 -fvk-t-shift 0 all -fvk-s-shift 128 all -fvk-b-shift 256 all -fvk-u-shift 384 all ...` |

The shifts equal NVRHI's default `VulkanBindingOffsets`; read-only `StructuredBuffer` (SSBO class in
glslang) shares offset 0 with textures, which is where NVRHI binds `StructuredBuffer_SRV`. Configuration
flags are one generator expression per flag with `COMMAND_EXPAND_LISTS`, because a `;` inside one expression
would be split before evaluation.

Compilers: `dxc` through `CONJURE_DXC_EXECUTABLE`, then the newest Windows SDK (`Windows Kits/10/bin/<version>/x64`
from the registry root, Program Files and `CMAKE_WINDOWS_KITS_10_DIR`), the Vulkan SDK and `PATH`; `glslang`
through the fetched `glslang-standalone` target, `CONJURE_GLSLANG_EXECUTABLE` or `PATH`. A compiler an
enabled backend needs but nobody can find is a configure error. `OUTPUT_DIR` must lie inside the build
directory, and `-Fo` / `-o` paths are relative to `PROJECT_BINARY_DIR` (the custom commands' working
directory) so depfiles contain the paths Ninja expects.

### 5.2 Depfiles and rebuild triggers

Under Ninja and Makefile generators every custom command carries `DEPFILE <out>.d`. glslang writes it in the
same run (`--depfile`). dxc 1.8 treats `-MF` / `-MD` as dependency-only modes that compile nothing, so the
DXIL command runs dxc twice: once to compile, once with `-MF <out>.dxil.d`. Other generators (the
`windows-vs` preset) rebuild only on `DEPENDS "${sourcePath}" ${ARG_DEPENDENCIES}`, which is why
`engine/shaders/CMakeLists.txt` lists every shared `.hlsli` under `DEPENDENCIES`.

### 5.3 `cmake/ConjureShaderIndex.cmake`

`conjure_add_shaders` writes one tab-separated line per permutation (`name entry stage defines dxil spirv
sourcePath [dependencyPath ...]`) into `<TARGET>_entries.txt` through a `configure_file(... COPYONLY)`
staging copy, so an unchanged list does not rebuild the index. A custom command runs
`cmake -D CONJURE_SHADER_ENTRIES=... -D CONJURE_SHADER_INDEX=... -P ConjureShaderIndex.cmake` at build time
with `DEPENDS` on the entries file, the script, every source and every dependency. The script reads the
entries as UTF-8 (a non-ASCII checkout path would otherwise be truncated), splits by regex rather than
`list()` so empty fields and paths with `;` survive, and computes `sourceHash` as the SHA-256 over the source
file's SHA-256 followed by the SHA-256 of every `DEPENDENCIES` file in order. A missing source or dependency
is a build error, never an empty hash; headers found only through depfiles are not part of the hash. The
index is written only when its content changed. Blob paths are relative to `OUTPUT_DIR`, empty for a backend
that is off. The `TARGET` gets the property `CONJURE_SHADER_OUTPUT_DIR`, which `tests/rhi/CMakeLists.txt`
reads into the `CONJURE_TEST_SHADER_DIR` compile definition.

`engine/CMakeLists.txt` includes `ConjureShaders` and adds `engine/shaders` when `CONJURE_WITH_RENDERER`;
the target `conjure_shaders` compiles `fullscreen_triangle.hlsl` (`vs_main`, `ps_main`) with
`shared/common.hlsli` as its dependency. It is independent of the module directories; `conjure_rhi_tests`
and (wave 3) `conjure_renderer` depend on it.

## 6. Threading

Everything belongs to the render thread (the main thread in v1): `device::create` and destruction,
`run_garbage_collection`, `wait_idle`, every swapchain call and every `shader_library` call.
`device::is_lost()` is an atomic flag plus a cheap API poll and may be read from any thread. The null
device's statistics are mutex-protected, so several command lists may record from different threads at once
(`test_null_device.cpp` `[parallel]` case) and `null_stats()` may be read anywhere. The message callback is
safe from any thread because NVRHI's validation layer and the drivers call it from wherever they like.

## 7. Extension points

| To add... | Do |
|---|---|
| A backend | implement `device_backend` and `swapchain_backend` under `backends/<name>/`, add a `graphics_backend` enumerator before `COUNT`, extend `to_string` / `parse_graphics_backend` / `is_backend_compiled` in `backend.cpp`, the `switch` in `device.cpp`, a `CONJURE_RHI_<NAME>` option and the source list in `CMakeLists.txt`, and a shader column in `ConjureShaders.cmake`, `ConjureShaderIndex.cmake` and `shader_library.cpp` |
| A texture format | add the enumerator before `COUNT`, a row in every `constexpr` helper of `texture_format.h`, `to_string` / parsing in `texture_format.cpp`, the NVRHI mapping (the `static_assert` round trip fails until it is complete) and the expectation row in `test_texture_format.cpp` |
| A shader stage | extend `shader_stage`, `to_string` / `parse_shader_stage`, the `stage` branch of `conjure_add_shaders` (dxc profile and glslang stage) |
| A null-device fault | add a `null_fault` enumerator and a `consume_fault()` call at the failure site |

## 8. Failure handling

| Condition | Code | Where |
|---|---|---|
| Backend not compiled | `core::error_code::UNSUPPORTED` | `device::create` |
| Out-of-range backend, `enableGpuValidation` without debug layer, zero swapchain size, buffer count outside 2..16, non-presentable format, window kind the backend cannot present to | `rhi_error::INVALID_DESC` (message names the field) | `device::create`, `swapchain::create`, `resize` |
| API refuses the device (no adapter at `adapterIndex`, no Vulkan loader) | `rhi_error::DEVICE_CREATION_FAILED` with the API's text | `device::create` |
| API refuses to create or resize the swapchain | `rhi_error::SWAPCHAIN_CREATION_FAILED` | `swapchain::create`, `resize` |
| Device removed, `Fatal` message, failed `waitForIdle` | `rhi_error::DEVICE_LOST`; `is_lost()` sticky | every swapchain call afterwards |
| Surface changed or empty (Vulkan), or a failed resize left no back buffers (every backend) | `rhi_error::OUT_OF_DATE` | `acquire`, `present`, `resize` |
| Acquire or present refused for another reason (`DXGI_ERROR_INVALID_CALL`, unexpected `VkResult`) | `rhi_error::PRESENT_FAILED` | `present`, `acquire` |
| Missing index, unreadable index, corrupt index | `NOT_FOUND`, `IO_FAILURE`, `PARSE_FAILURE` | `shader_library::open`, `reload` (old table kept) |
| Unknown key or no blob for this backend, missing or empty blob | `NOT_FOUND`, `IO_FAILURE` | `shader_library::get` |

Programmer errors (a null pointer, use after `reload()`) assert; nothing throws.

## 9. Tests

`tests/rhi/` builds `conjure_rhi_tests` with `conjure_add_test_module(rhi ... DEPS conjure::thirdparty::nvrhi)`:
one `test_<header>.cpp` per public header plus the topic files the design names (`test_null_device.cpp`,
`test_device_lost.cpp`, `test_null_swapchain.cpp`, `test_shader_library.cpp`). `rhi_test_support.h` provides
`null_device_desc()`, `software_device_desc()` (D3D12 + `preferSoftwareAdapter`), `make_null_device()`,
`make_software_device()`, `small_swapchain_desc()` and `headless_window()`.

| Aspect | How |
|---|---|
| GPU-free default | every case runs on the null device; `conjure_rhi_tests` is green on every CI platform |
| `[gpu]` cases | start with `CONJURE_TEST_REQUIRE(precondition::SOFTWARE_GPU_ADAPTER)`; `rhi_test_support.cpp` installs the probe from a static initialiser (tries WARP once per process, console muted) so they skip with exit 77 where no software adapter or no D3D12 backend exists ([Testing](../testing.md) section 6). They create a WARP device, clear and present a headless composition swapchain for six frames, resize it, force a failed resize (32768 x 32768, past the D3D12 limit) to check `OUT_OF_DATE` until the next resize succeeds, and open the built index to check the DXIL column is selected |
| Shader pipeline | `add_dependencies(conjure_rhi_tests conjure_shaders)`; `test_shader_library.cpp` opens the real `index.json` from `CONJURE_TEST_SHADER_DIR` and checks the blob magic (`DXBC` or `0x07230203`); every other case writes its own index into a `temp_directory` |
| NVRHI in tests | `test_null_device.cpp`, `test_null_swapchain.cpp`, `test_device_lost.cpp` and `test_swapchain.cpp` include `<nvrhi/nvrhi.h>` because the null device is an `nvrhi::IDevice` reachable only through `native_device()`: the sanctioned exception to "no third-party headers in tests", to be listed in `tools/style_exceptions.json` once a `third_party_in_tests` section exists |
| Format mapping | proven by `static_assert`s in `texture_format.cpp`, not by a test |
| Leaks | `memory_guard` around device, swapchain and library lifetimes with `LOG_RHI` muted (captured log lines allocate) |
| Regressions | `TEST_CASE("regression - review F<n> ...")` for review findings and `TEST_CASE("regression - code sweep F<n> ...")` for code-sweep findings, next to the behaviour they pin |

```
python tools/build.py --build-dir build/mod_rhi --target conjure_rhi_tests --test -R rhi
build/mod_rhi/bin/conjure_rhi_tests
```

To compile-check the Vulkan backend on Windows configure a second build directory with
`-DCONJURE_RHI_VULKAN=ON` and build `conjure_rhi` (glslang is fetched for the SPIR-V step; linking needs no
Vulkan SDK because every entry point is loaded at runtime).

> [!NOTE]
> `tests/rhi/CMakeLists.txt` returns early when `conjure::rhi` does not exist, so a build with
> `CONJURE_WITH_RENDERER=OFF` has no rhi tests rather than a configure error.

## 10. Open items

| Item | State |
|---|---|
| Vulkan at run time | compile-checked only (Windows CI builds with `CONJURE_RHI_VULKAN=OFF`); unverified until it runs on lavapipe in CI. Treat every Vulkan behaviour in section 4 as the intended contract, not a proven one |
| macOS surfaces | `COCOA` handles are `INVALID_DESC` on Vulkan until a `VK_EXT_metal_surface` / MoltenVK path exists |
| Device-loss recovery | v1 marks the device lost and the engine quits; recreating device, swapchain and GPU mirrors is a v2 item |
| Block-compressed formats | `BC1_SRGB`, `BC5_UNORM`, `BC7_SRGB` are reserved; nothing produces them until the cook step |
| Style exceptions | the NVRHI header includes in the four null-device tests need a `third_party_in_tests` section in `tools/style_exceptions.json` |
| glslang HLSL front-end | deprecated upstream; the pinned tag keeps it, `CONJURE_SPIRV_COMPILER=dxc` is the exit |
| Skip accounting | `tools/ctest_check_skips.py` (08_testing D12) flags a *Not Run* outside the allow-list in CI; the `SOFTWARE_GPU_ADAPTER` cases carry no label, so a machine that silently skips every `[gpu]` case is flagged only through the JUnit report |

## Related

- design records: [`../../design/02_renderer.md`](../../design/02_renderer.md) sections 1 (R14, R15, R17), 3.12, 4.1, 6, 7 and 9 steps 1-4;
  [`../../design/08_testing.md`](../../design/08_testing.md) 3.2 and 8.4 (the `SOFTWARE_GPU_ADAPTER` probe, null backends);
  [`../../design/11_platform.md`](../../design/11_platform.md) 2.4 (`native_window_handle`)
- module README: `engine/src/rhi/README.md`
- handbook: [Build pipeline](../build-pipeline.md) (options, third-party pinning), [Testing](../testing.md) (preconditions and skipping), [Architecture](../architecture.md) (wave plan)
- 📘 API: [Graphics device layer](../../api/rhi/index.md), [Devices and swapchains](../../api/rhi/devices-and-swapchains.md), [Shaders](../../api/rhi/shaders.md)
