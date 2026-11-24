#pragma once

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"

#include "conjure/renderer/camera.h"
#include "conjure/renderer/mesh_asset.h"
#include "conjure/renderer/render_scene.h"
#include "conjure/renderer/render_target.h"
#include "conjure/renderer/renderer.h"

#include "conjure/assets/asset_types.h"
#include "conjure/core/image.h"
#include "conjure/core/jobs/job_system.h"
#include "conjure/core/math.h"
#include "conjure/core/types.h"
#include "conjure/rhi/device.h"
#include "conjure/rhi/shader_library.h"
#include "conjure/rhi/texture_format.h"

#include <filesystem>
#include <memory>
#include <span>
#include <string_view>

// Helpers shared by the renderer tests.
//
//   headless_renderer fixture = make_headless_renderer();          // validated null device, real shader index
//   CONJURE_TEST_REQUIRE(precondition::SOFTWARE_GPU_ADAPTER);       // [gpu] cases, WARP through software_device_desc()
//   headless_renderer gpu = make_headless_renderer(true);           // the WARP device
//   headless_renderer gate = make_raw_headless_renderer();          // allocation gates: no validation layer
//   const render_target_id target = make_target(*fixture, math::uvec2(64, 64), false);   // a target without depth
//   open_written_shaders(device, directory, stages);                // a hand-written index: missing shaders
//   CHECK(warning_count(logs, "exceed MAX_VIEWS") == 1);            // how often one warning was logged
//   CONJURE_CHECK_APPROX(unpacked_tangent(vertex), math::vec4(1.0f, 0.0f, 0.0f, 1.0f));   // a packed vertex
//
// The null device runs under NVRHI's validation layer (enableDebugLayer), so command-list misuse such
// as recording on a closed list or drawing without a graphics state logs an ERROR, which fails every
// case holding a log_capture (sweep-2026-11-12 F171). The allocation gates use the raw device instead:
// they measure the renderer, not the validation layer's own bookkeeping.
//
// renderer_test_support.cpp installs the SOFTWARE_GPU_ADAPTER probe (08_testing.md 3.2) from a
// static initialiser, the same way tests/rhi does, so the [gpu] cases skip cleanly where no software
// adapter exists.

namespace conjure::testing::renderer_support {

    /// <summary>
    /// The renderer class, named without the namespace / class ambiguity the using-directives create.
    /// </summary>
    using renderer_type = conjure::renderer::renderer;

    /// <summary>
    /// Where conjure_shaders wrote index.json (CONJURE_TEST_SHADER_DIR).
    /// </summary>
    [[nodiscard]] std::filesystem::path shader_directory();

    /// <summary>
    /// The engine/shaders source tree (CONJURE_TEST_SHADER_SOURCE_DIR).
    /// </summary>
    [[nodiscard]] std::filesystem::path shader_source_directory();

    /// <summary>
    /// Description of a null device, optionally wrapped in NVRHI's validation layer.
    /// </summary>
    [[nodiscard]] rhi::device_desc null_device_desc(bool enableDebugLayer = false);

    /// <summary>
    /// Description of the software (WARP) D3D12 device the [gpu] cases use.
    /// </summary>
    [[nodiscard]] rhi::device_desc software_device_desc();

    /// <summary>
    /// Creates a null device under NVRHI's validation layer, or the WARP device when 'software' is set,
    /// or fails the case.
    /// </summary>
    [[nodiscard]] std::unique_ptr<rhi::device> make_device(bool software = false);

    /// <summary>
    /// Creates a null device without NVRHI's validation layer or fails the case; for the allocation
    /// gates only.
    /// </summary>
    [[nodiscard]] std::unique_ptr<rhi::device> make_raw_device();

    /// <summary>
    /// Opens the shader index conjure_shaders built, or fails the case.
    /// </summary>
    [[nodiscard]] std::unique_ptr<rhi::shader_library> open_shaders(rhi::device &device);

    /// <summary>
    /// One shader of a hand-written index: 'stage' is "vertex", "pixel" or "compute". With 'writeBlob'
    /// unset the index still lists the blob file but the file does not exist.
    /// </summary>
    struct index_stage {
        std::string_view name = "";
        std::string_view entry = "";
        std::string_view stage = "";
        std::string_view defines = "";
        bool writeBlob = true;
    };

    /// <summary>
    /// Writes index.json into 'directory' listing exactly 'stages' (none gives the empty index) and
    /// opens it on 'device', or fails the case. Every stage names one blob file of placeholder bytes in
    /// both the DXIL and the SPIR-V column, so the library finds it whichever column the device's
    /// backend selects; only the null device accepts such bytes as a shader.
    /// </summary>
    [[nodiscard]] std::unique_ptr<rhi::shader_library> open_written_shaders(
      rhi::device &device,
      const temp_directory &directory,
      std::span<const index_stage> stages = {}
    );

    /// <summary>
    /// A renderer description with small shadow resolutions and no swapchain.
    /// </summary>
    [[nodiscard]] renderer::renderer_desc headless_desc(
      rhi::device &device,
      rhi::shader_library &shaders,
      core::jobs::job_system *jobs = nullptr
    );

    /// <summary>
    /// A device, its shader library and a headless renderer, destroyed in reverse order.
    /// </summary>
    struct headless_renderer {
        std::unique_ptr<rhi::device> device = nullptr;
        std::unique_ptr<rhi::shader_library> shaders = nullptr;
        std::unique_ptr<renderer::renderer> instance = nullptr;

        headless_renderer();
        ~headless_renderer();

        headless_renderer(const headless_renderer &) = delete;
        headless_renderer &operator=(const headless_renderer &) = delete;
        headless_renderer(headless_renderer &&) noexcept = default;
        headless_renderer &operator=(headless_renderer &&) noexcept = default;

        [[nodiscard]] renderer::renderer &operator*() const noexcept {
            return *instance;
        }

        [[nodiscard]] renderer::renderer *operator->() const noexcept {
            return instance.get();
        }
    };

    /// <summary>
    /// Creates the three objects or fails the case. 'software' selects WARP (after CONJURE_TEST_REQUIRE).
    /// </summary>
    [[nodiscard]] headless_renderer make_headless_renderer(
      bool software = false,
      core::jobs::job_system *jobs = nullptr
    );

    /// <summary>
    /// A headless renderer on make_raw_device() or fails the case; for the allocation gates only.
    /// </summary>
    [[nodiscard]] headless_renderer make_raw_headless_renderer(core::jobs::job_system *jobs = nullptr);

    /// <summary>
    /// Creates an off-screen target of 'size' on 'instance' or fails the case. 'withDepth' keeps the scene
    /// depth in the target (render_target_desc::withDepth); without it every view renders with a transient
    /// depth buffer of the frame graph.
    /// </summary>
    [[nodiscard]] renderer::render_target_id make_target(
      renderer_type &instance,
      math::uvec2 size,
      bool withDepth = true,
      rhi::texture_format format = rhi::texture_format::RGBA8_SRGB
    );

    /// <summary>
    /// Number of WARN messages 'logs' captured so far whose text contains 'text'; for the cases that
    /// count how often one warning of the frame was logged while others are logged around it.
    /// </summary>
    [[nodiscard]] u32 warning_count(const log_capture &logs, std::string_view text);

    /// <summary>
    /// A deterministic asset id for tests: {0x7E57, index}.
    /// </summary>
    [[nodiscard]] assets::asset_id test_asset_id(u64 index) noexcept;

    /// <summary>
    /// The normal a packed vertex holds (stream 1 of mesh_asset.h), unpacked from its 16-bit form.
    /// </summary>
    [[nodiscard]] math::vec3 unpacked_normal(const renderer::packed_vertex_attributes &attributes) noexcept;

    /// <summary>
    /// The tangent a packed vertex holds, with the handedness the vertex keeps in its normal's w as w.
    /// </summary>
    [[nodiscard]] math::vec4 unpacked_tangent(const renderer::packed_vertex_attributes &attributes) noexcept;

    /// <summary>
    /// A perspective view looking from 'eye' at 'target' (y up) into the given render target.
    /// </summary>
    [[nodiscard]] renderer::render_view look_at_view(
      const math::vec3 &eye,
      const math::vec3 &target,
      renderer::render_target_id renderTarget,
      f32 verticalFovRadians = 1.0472f
    );

    /// <summary>
    /// An instance of a mesh at a position with the bounds the mesh's local bounds transform to.
    /// </summary>
    [[nodiscard]] renderer::mesh_instance instance_at(
      renderer::gpu_mesh_id mesh,
      renderer::gpu_material_id material,
      const math::vec3 &position,
      const math::aabb &localBounds,
      f32 scale = 1.0f
    );

    /// <summary>
    /// The pixel a world position projects to for a clip-from-world matrix and a target size, or
    /// (UINT32_MAX, UINT32_MAX) when it is outside the viewport.
    /// </summary>
    [[nodiscard]] math::uvec2 project_to_pixel(
      const math::mat4 &clipFromWorld,
      const math::vec3 &world,
      math::uvec2 size
    ) noexcept;

    /// <summary>
    /// sRGB-encoded 8-bit channel to linear.
    /// </summary>
    [[nodiscard]] f32 srgb_to_linear(u8 encoded) noexcept;

    /// <summary>
    /// Rec. 709 luminance of a read-back pixel, in linear light.
    /// </summary>
    [[nodiscard]] f32 linear_luminance(core::rgba8 pixel) noexcept;
}
