# rhi

The graphics device layer: one `nvrhi::IDevice` per process behind `rhi::device`, the swapchain
that presents to the window, and the `shader_library` that hands compiled DXIL / SPIR-V blobs to
the renderer. Backends: `null` (always), `d3d12` (`CONJURE_RHI_D3D12`, Windows) and `vulkan`
(`CONJURE_RHI_VULKAN`). Design: `docs/design/02_renderer.md` sections 1, 3.12, 4.1, 7 and 9
(steps 1-4); `docs/design/08_testing.md` 8.4. Static library `conjure::rhi`; depends publicly on
`conjure::core` and privately on `conjure::thirdparty::nvrhi`.

NVRHI never appears in a public header (R15): `device.h` and `swapchain.h` forward-declare
`nvrhi::IDevice` / `nvrhi::ITexture` and expose non-owning `native_device()` / `back_buffer()`
accessors that `conjure_renderer` records through.

## Public headers (`conjure/rhi/`)

| Header | Contents |
|---|---|
| `log_category.h` | `LOG_RHI` |
| `rhi_error.h` | `rhi_error` (`DEVICE_CREATION_FAILED`, `DEVICE_LOST`, `OUT_OF_DATE`, `SWAPCHAIN_CREATION_FAILED`, `INVALID_DESC`, `PRESENT_FAILED`), bound to the category `"rhi"`. Generic conditions use `core::error_code`: `UNSUPPORTED` (backend not compiled), `NOT_FOUND` / `PARSE_FAILURE` / `IO_FAILURE` (shader library) |
| `backend.h` | `graphics_backend { NULL_DEVICE, D3D12, VULKAN }`, `device_capabilities`, `to_string`, `parse_graphics_backend`, `is_backend_compiled`, `default_graphics_backend` |
| `texture_format.h` | `texture_format` (the 21 engine formats), constexpr `bytes_per_pixel`, `bytes_per_block`, `block_size`, `row_pitch`, `is_depth`, `is_srgb`, `is_compressed`, `to_string`, `parse_texture_format` |
| `native_window.h` | `native_window_kind`, `native_window` (opaque OS handles; mirrors `platform::native_window_handle` field for field) |
| `null_device_stats.h` | `null_device_stats`: resource creations by type, command-list lifecycle, recorded draws / dispatches / clears / writes / copies, debug marker names, swapchain acquires / presents / resizes |
| `device.h` | `null_fault`, `device_desc`, `device` (`create`, `backend`, `desc`, `capabilities`, `is_lost`, `native_device`, `run_garbage_collection`, `wait_idle`, `null_stats`, `reset_null_stats`, `simulate_device_loss`, `inject_null_fault`) |
| `swapchain.h` | `swapchain_desc`, `swapchain` (`create`, `acquire`, `present`, `resize`, `back_buffer`, `buffer_count`, `current_index`, `desc`, `owner`) |
| `shader_library.h` | `shader_stage`, `shader_key`, `shader_blob`, `shader_library` (`open`, `get`, `contains`, `reload`, `entry_count`, `loaded_blob_count`, `backend`, `directory`) |

## Layout

```
engine/src/rhi/
  device.cpp, device_impl.h        public device over a device_backend; validation wrapper when enableDebugLayer
  device_backend.h/.cpp            the private interfaces every backend implements + swapchain_desc validation
  swapchain.cpp                    public swapchain over a swapchain_backend
  shader_library.cpp               index.json parsing, lazy blob loading, reload
  message_callback.h/.cpp          nvrhi::IMessageCallback -> LOG_RHI, Fatal -> lost flag
  texture_format.cpp, texture_format_mapping.h   names, parsing and the constexpr NVRHI mapping
  backends/null/                   null_device, null_resources, null_command_list, null_swapchain
  backends/d3d12/                  device_d3d12, swapchain_d3d12 (DXGI + nvrhi::d3d12)
  backends/vulkan/                 device_vulkan, swapchain_vulkan, vulkan_loader (compile-only on Windows CI)
engine/shaders/                    fullscreen_triangle.hlsl, shared/common.hlsli, CMakeLists.txt (conjure_shaders; added by
                                   engine/CMakeLists.txt when CONJURE_WITH_RENDERER, independent of this module)
cmake/ConjureShaders.cmake         conjure_add_shaders(); cmake/ConjureShaderIndex.cmake writes index.json at build time
```

## Behaviour worth knowing

- **Creation.** `device::create(desc)` validates the description (an out-of-range `backend` and
  `enableGpuValidation` without `enableDebugLayer` are `INVALID_DESC`), refuses a backend that is
  not compiled with `UNSUPPORTED`, builds the backend, and wraps the NVRHI device in
  `nvrhi::validation::createValidationLayer` when `enableDebugLayer` is set (on every backend, the
  null device included). Messages from NVRHI go to `LOG_RHI` at their severity; a `Fatal` message,
  `DXGI_ERROR_DEVICE_REMOVED` seen by a present or resize, `GetDeviceRemovedReason() != S_OK`, or a
  failed `waitForIdle` set `is_lost()`, after which `swapchain::create`, `acquire`, `present` and
  `resize` return `rhi_error::DEVICE_LOST`. Both teardown paths (`~device`, `~swapchain` and the
  backend destructors) ask the same polling `is_lost()` before waiting for the GPU. Recovery is the
  engine's job (v1 quits, R8). `simulate_device_loss()` exists for those tests on every backend;
  `inject_null_fault(null_fault::WAIT_IDLE | PRESENT)` arms one failure on the null backend so the
  "failed waitForIdle marks the device lost" and "present reports DEVICE_LOST" paths run in CTest.
- **Present failures.** An acquire or present the API refuses for a reason other than removal or an
  out-of-date surface (`DXGI_ERROR_INVALID_CALL`, an unexpected `VkResult`) is
  `rhi_error::PRESENT_FAILED`; `SWAPCHAIN_CREATION_FAILED` is reserved for creation and resize.
- **Null device** (`backends/null/`). A complete `nvrhi::IDevice`: every `create*` stores the
  descriptor and returns a live handle (invalid descriptors such as a zero-sized texture, an empty
  shader blob or a pipeline without a vertex shader return `nullptr` and an `Error` message, like a
  real backend). Buffers own zero-filled CPU storage so `mapBuffer` / `writeBuffer` / `copyBuffer`
  round-trip for buffers created with CPU access; an out-of-range `writeBuffer` / `copyBuffer`, or a
  `copyBuffer` whose source and destination ranges overlap in one buffer, reports an `Error` and
  stores nothing (`bufferWriteBytes` counts only stored bytes), and mapping a buffer or staging
  texture created with `CpuAccessMode::None` returns `nullptr` with an `Error`, as on D3D12 (a map
  with a different mode than the creation one, or a second map, is legal there and here). Staging
  textures map to zero-filled memory (readback is zero, 02_renderer.md 6); `queryFormatSupport`
  answers like a D3D12 device (integer formats are not `ShaderSample`, block-compressed formats are
  not `RenderTarget`). Command lists count draws, instances, dispatches, clears, writes, copies and
  keep `beginMarker` names in recording order; they do not check for misuse (recording on a closed
  list, a second `open()` and a draw without a graphics state all succeed): that is NVRHI's
  validation layer's job, so tests that want misuse caught create the null device with
  `enableDebugLayer` (the renderer fixtures do). They also track the states set explicitly
  (`beginTracking*State`, `set*State`, `setPermanent*State`) so `getTextureSubresourceState` /
  `getBufferState` report them. The implicit transitions of automatic barriers (`setGraphicsState`,
  clears, copies, writes) are not modelled; an untracked resource reports its `initialState` when
  `keepInitialState` is set, `Unknown` otherwise; per-list tracking resets on `open()`; a permanent
  state is stored on the `null_texture` / `null_buffer` itself, so it is visible to every command
  list and dies with the resource instead of surviving in a pointer-keyed map. The device is the
  instrument the renderer's allocation gates measure through (`renderer.null_frame_10k`, allocs 0,
  counts device-side allocations too), so its bookkeeping is allocation-free in steady state: the
  referenced-resource list and the state trackers (`core::flat_hash_map`, `clear()` keeps the table)
  keep their storage across `open()`, the counters are integers, and marker names, the one growing
  record, are stored only after a consumer asked for them with `device::reset_null_stats()` and at
  most `MAX_RECORDED_MARKER_NAMES` (4096) per reset; the rest are counted in
  `null_device_stats::markerNamesDropped`. A headless run that never resets the stats stores no
  names and never grows (`test_null_device.cpp` "open/close 100 command lists after warm-up
  allocates nothing"). `executeCommandLists` returns an increasing fence and refuses a list that is
  still open. `queryFeatureSupport` is true for `ComputeQueue`, `CopyQueue`, `ConstantBufferRanges`,
  `DeferredCommandLists`, `VirtualResources` and `WaveLaneCountMinMax` (32 lanes); every GPU-only
  feature is false. `getGraphicsAPI()` reports `D3D12` so code that branches on the API takes the
  zero-based `SV_InstanceID` path the design assumes. The statistics are read with
  `device::null_stats()` (a snapshot) and cleared with `reset_null_stats()`, which is also what
  starts the marker-name recording: a test that asserts on the pass stream resets first.
- **Null swapchain.** `bufferCount` textures in the `Present` state with `keepInitialState`;
  `acquire()` returns the current index and `present()` advances it, so acquire/present pairs
  cycle `0, 1, 2, 0, ...`; `resize` recreates the buffers and restarts at 0. Zero sizes are
  `INVALID_DESC`. The window handle is ignored. On every backend `buffer_count()` is the number of
  back buffers that exist right now (`back_buffer(i)` is non-null exactly for `i < buffer_count()`),
  which is 0 after a failed resize until a later one succeeds.
- **D3D12.** DXGI factory (with `DXGI_CREATE_FACTORY_DEBUG` when the debug layer is on and
  available; a missing debug layer is a warning, not a failure), hardware adapters enumerated by
  GPU preference with software adapters skipped so `adapterIndex` counts GPUs, WARP through
  `EnumWarpAdapter` when `preferSoftwareAdapter`, feature level 11_0, one DIRECT / COMPUTE / COPY
  queue each, `nvrhi::d3d12::createDevice`. The swapchain is flip-discard; flip model forbids sRGB
  buffer formats, so sRGB chains are created UNORM and the NVRHI wrapper (`createHandleForNativeTexture`,
  `Present` state, `keepInitialState`) carries the sRGB format. A `native_window` of kind `NONE`
  creates a composition swapchain that is never attached to a visual: nothing is shown, but the real
  acquire / clear / present / resize path runs, which is how the `[gpu]` tests work on WARP without a
  window. Tearing is used only with `vsync = false` when DXGI reports support. A resize that fails in
  `ResizeBuffers` or while wrapping leaves no back buffers (a wrap that fails half-way drops the ones it
  made), and `acquire()` / `present()` return `OUT_OF_DATE` until a resize succeeds, as on Vulkan.
- **Vulkan.** Instance, surface, physical device, device and `VkSwapchainKHR` over
  `nvrhi::vulkan::createDevice`, with every entry point loaded at runtime through
  `core::dynamic_library` (no link dependency on the loader; a machine without `vulkan-1` fails
  `device::create` with `DEVICE_CREATION_FAILED`). Swapchain lifetime rules: `resize()` first
  flushes a pending acquire (its semaphore wait is submitted empty so NVRHI never waits on a
  destroyed semaphore), queries the surface and returns `OUT_OF_DATE` for a zero extent (minimised
  window) without touching the old swapchain, then hands the old `VkSwapchainKHR` to
  `vkCreateSwapchainKHR` as `oldSwapchain` and destroys it exactly once after the call; the new
  semaphore set is built before the old one is destroyed. After a failed resize `mSwapchain` is null
  and `acquire()` / `present()` return `OUT_OF_DATE` until a resize succeeds. The swapchain images
  request `TRANSFER_DST` only when `VkSurfaceCapabilitiesKHR::supportedUsageFlags` offers it (only
  `COLOR_ATTACHMENT` is guaranteed); without it NVRHI's `clearTextureFloat` cannot be used on a back
  buffer and the renderer must clear through a render pass (a WARN says so at creation). The backend
  is compile-checked only until it runs on lavapipe in CI: treat it as unverified. Compile-only on
  the Windows CI runner, where `CONJURE_RHI_VULKAN` is off; see "Testing".
- **Shader library.** `conjure_add_shaders()` writes `<output>/index.json` (`version` 1, one
  `{ name, entry, stage, defines, dxil, spirv, sourceHash }` per permutation). `open` selects the
  blob column for the device's backend (DXIL on D3D12, SPIR-V on Vulkan; the null device takes DXIL
  and falls back to SPIR-V because it only checks that a blob exists), `get` loads a blob once and
  caches it (a returned span is valid until `reload()`), `reload` re-parses the index and drops the
  cache, keeping the old table when the new index does not parse. Keys match the index exactly:
  `defines` is the axis string in CMake order (`"ALPHA_MASK=1|FOO=2"`), never reordered.
  `shader_library` ignores `sourceHash`; it is there for the build's incremental rebuilds. A blob is
  read in one call into a buffer sized by `std::filesystem::file_size`. The index's blob paths are
  UTF-8 and become paths through `std::u8string`, never as a narrow string, which Windows decodes in
  the ANSI code page. Paths in messages and log lines are formatted as UTF-8 (`path::u8string()`),
  never through `path::string()`, which throws on Windows for a character outside that code page.
- **Shader build.** `conjure_add_shaders(TARGET OUTPUT_DIR [SOURCE_DIR] [INCLUDE_DIRS] [DEPENDENCIES] SHADERS ...)`
  expands each `source;entry;stage;defines` entry into its permutations and runs `dxc` (DXIL, `-T xx_6_5 -HV 2018 -Zpr -WX`,
  `-Zi -Qembed_debug` outside Distribution, `-O3` in it) and `glslang` (SPIR-V, `vulkan1.2`, the NVRHI
  binding shifts t:0 s:128 b:256 u:384, SSBO 0; `-g` in Debug, `-Os` otherwise) or `dxc -spirv` when
  `CONJURE_SPIRV_COMPILER=dxc`. Depfiles drive include tracking under Ninja and Makefile generators
  (glslang's `--depfile` compiles and writes deps in one run; dxc 1.8 treats `-MF` as a dependency-only
  mode, so the DXIL step runs dxc twice); other generators (the `windows-vs` preset) rebuild on the
  listed `DEPENDENCIES`, which is why `engine/shaders/CMakeLists.txt` lists every shared `.hlsli`
  there. `sourceHash` in `index.json` is the SHA-256 over the source file's SHA-256 followed by the
  SHA-256 of every `DEPENDENCIES` file, so it changes with the source or a listed header (headers
  found only through depfiles are not part of it); the index reads its entries as UTF-8 and a
  missing source or dependency fails the build instead of writing an empty hash. `dxc` is found through
  `CONJURE_DXC_EXECUTABLE`, the newest Windows SDK (`Windows Kits/10/bin/<version>/x64`), the Vulkan SDK
  and `PATH`; `glslang` through the fetched `glslang-standalone` target, `CONJURE_GLSLANG_EXECUTABLE` or `PATH`.
- **`new` in the null backend.** NVRHI resources are intrusively counted and `RefCounter::Release`
  deletes the object itself, so creating one needs a matching plain `new`. `null_backend::make_resource`
  is the single place that does it (`null_resources.h`).
- **Platform coupling.** `rhi::native_window` mirrors `platform::native_window_handle`
  (`11_platform.md` 2.4) field for field; the engine copies `window.native_handle()` into it when it
  creates the swapchain. This module does not link `conjure::platform`, so it builds and tests
  while the platform module is being brought up; the Vulkan backend enables the surface instance
  extensions for the handle kind itself and creates the `VkSurfaceKHR` from the native handle.

## Threading

Everything in this module belongs to the render thread (the main thread in v1): `device::create`
and destruction, `run_garbage_collection`, `wait_idle`, every swapchain call and every
`shader_library` call. `device::is_lost()` is an atomic flag plus a cheap API poll and may be read
from any thread. The null device's statistics are mutex-protected, so several command lists may
record from different threads at once (08_testing.md 8.1) and `null_stats()` may be read anywhere.

## Testing

`tests/rhi/` builds `conjure_rhi_tests`: one `test_<header>.cpp` per public header plus the topic
files the design names (`test_null_device.cpp`, `test_device_lost.cpp`, `test_null_swapchain.cpp`,
`test_shader_library.cpp`). Every case runs on the null device; the `[gpu]` cases create a WARP
D3D12 device (`precondition::SOFTWARE_GPU_ADAPTER`, probe installed by `rhi_test_support.cpp`)
and skip with exit 77 where no software adapter exists. `test_shader_library.cpp` reads the
`index.json` that `conjure_shaders` built (`CONJURE_TEST_SHADER_DIR`), so the shader pipeline is
proven on every CI platform; its `[gpu]` case opens the same index on the WARP device and checks
that the DXIL column is selected. The null device is an `nvrhi::IDevice` reachable only through
`device::native_device()`, so `test_null_device.cpp`, `test_null_swapchain.cpp`,
`test_device_lost.cpp` and `test_swapchain.cpp` include `<nvrhi/nvrhi.h>`: NVRHI is the API under
test there, not an implementation detail (CONVENTIONS.md 6 forbids third-party headers in tests;
this is the sanctioned exception for the rhi null-device tests and belongs in
`tools/style_exceptions.json` once a `third_party_in_tests` section exists there). The
texture-format <-> NVRHI mapping is proven by `static_assert`s in `texture_format.cpp` instead.
Review regressions are `TEST_CASE("regression - review F<n> ...")` cases next to the behaviour
they pin.

```
python tools/build.py --build-dir build/mod_rhi --target conjure_rhi_tests --test -R rhi
build/mod_rhi/bin/conjure_rhi_tests
```

To compile-check the Vulkan backend on Windows configure a second build directory with
`-DCONJURE_RHI_VULKAN=ON` and build the `conjure_rhi` target (glslang is fetched for the SPIR-V
step; linking needs no Vulkan SDK because every entry point is loaded at runtime).
