# Graphics device layer (RHI)
<!-- doc-template:wiki-index -->
[API](../index.md)

<!-- doc-content:purpose:start -->
The RHI (render hardware interface) is the thin layer between the engine and the graphics API. It owns one
graphics **device** per process, the **swapchain** that puts finished frames on a window, and the **shader
library** that hands compiled shaders to whoever records GPU work. Everything lives in `namespace conjure::rhi`
under `conjure/rhi/`. The module wraps NVRHI; NVRHI's own types never appear in a public header, so including
the RHI costs nothing beyond the engine's core headers.
<!-- doc-content:purpose:end -->

<!-- doc-content:body:start -->
## Who needs it

| You are... | You need the RHI for... | Start with |
|---|---|---|
| Making a game in C# or with the renderer's components | nothing: the engine creates the device and swapchain for you | the renderer pages |
| Writing a renderer or a rendering pass | the device to record into, the swapchain's back buffers, compiled shaders | [Devices and swapchains](devices-and-swapchains.md), then [Shaders](shaders.md) |
| Writing a module that does its own GPU work (compute particles, a GPU importer) | `device::native_device()` and a `shader_library` for your module's shaders | [Shaders](shaders.md) |
| Writing tests for any of the above | the null device and its statistics, which run without a GPU | [Devices and swapchains](devices-and-swapchains.md#testing-without-a-gpu) |

## Backends

A device runs on one of three backends; the engine picks the first compiled one in this order.

| Backend | `graphics_backend` | Compiled when | Use |
|---|---|---|---|
| Direct3D 12 | `D3D12` | Windows, `CONJURE_RHI_D3D12` (on by default) | shipping on Windows; `preferSoftwareAdapter` gives WARP for tests |
| Vulkan | `VULKAN` | `CONJURE_RHI_VULKAN` (on by default off Windows) | Linux, macOS through MoltenVK |
| Null | `NULL_DEVICE` | always | tests and headless tools: a complete device that records what you asked of it and draws nothing |

Asking for a backend that is not compiled in fails with `core::error_code::UNSUPPORTED`; everything else the
module reports comes from the `rhi_error` category (`DEVICE_CREATION_FAILED`, `DEVICE_LOST`, `OUT_OF_DATE`,
`SWAPCHAIN_CREATION_FAILED`, `INVALID_DESC`, `PRESENT_FAILED`). Errors arrive through `result<T>` like
everywhere else in the engine ([Errors and results](../core/errors-and-results.md)).

> [!TIP]
> Write your tests against `NULL_DEVICE`. Every renderer and module test in the engine runs on it in CI; the
> real backends are exercised by a handful of `[gpu]` cases that skip on machines without a software adapter.

## When to use the renderer instead

The RHI gives you a device and raw back buffers; it does not draw. If you want meshes, lights, sprites, a
camera or post-processing, use the renderer: it owns the render graph, the GPU copies of assets and
the pass shaders, and it talks to the RHI for you. Reach for the RHI directly only when you record NVRHI
command lists yourself, which means you are inside the engine or writing an engine module. Game code never
sees `native_device()`.

## Pages

| Page | Covers |
|---|---|
| [Devices and swapchains](devices-and-swapchains.md) | `graphics_backend`, `device_desc`, `device::create`, capabilities, `native_window`, `swapchain` create / acquire / present / resize, device loss, `texture_format`, `null_device_stats` |
| [Shaders](shaders.md) | `shader_library`, `shader_key`, `shaders/index.json`, `conjure_add_shaders()` for a module with HLSL sources, DXIL versus SPIR-V, hot reload |

> [!WARNING]
> Everything in this module belongs to the render thread (the main thread today): creating and destroying
> the device, every swapchain call and every shader-library call. Only `device::is_lost()` may be read from
> any thread.

## See also

- 📘 API: [Errors and results](../core/errors-and-results.md) for `result<T>`, `CONJURE_TRY` and error categories
- 📘 API: [Logging and assertions](../core/logging-and-assertions.md); the module logs under `LOG_RHI`
- 📘 API: [Glossary](../glossary.md)
- 🛠 Handbook: [RHI module](../../handbook/engine/rhi.md) for the backends' internals and the shader pipeline

## Supporting headers

| Include path | Purpose |
|---|---|
| `conjure/rhi/log_category.h` | Declares the module log category used to filter its diagnostics through core logging. |
| `conjure/rhi/rhi_error.h` | Declares `rhi_error`, the module failure codes carried by `core::error` and `core::result<T>`. |
<!-- doc-content:body:end -->
