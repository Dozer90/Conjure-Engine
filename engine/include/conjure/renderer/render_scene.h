#pragma once

#include "conjure/renderer/camera.h"
#include "conjure/renderer/debug_draw.h"
#include "conjure/renderer/gpu_ids.h"
#include "conjure/renderer/light.h"
#include "conjure/renderer/limits.h"
#include "conjure/renderer/render_settings.h"
#include "conjure/renderer/sprite_batch.h"

#include "conjure/core/api.h"
#include "conjure/core/enum_flags.h"
#include "conjure/core/math.h"
#include "conjure/core/types.h"

#include <array>
#include <span>
#include <thread>
#include <vector>

// The extraction contract (docs/design/02_renderer.md 2.3, R4, R8): the engine's EXTRACT systems
// fill a render_scene, the renderer renders it. The renderer owns a render_scene_pair; begin_extract
// clears and returns the write slot, end_extract swaps, render_frame renders the read slot. The pair
// is not internally synchronised: the engine loop guarantees extraction of frame N and rendering of
// frame N-1 never touch the same slot (asserted by owner thread in Debug).

namespace conjure::renderer {

    struct mesh_instance {
        gpu_mesh_id mesh = {};
        std::array<gpu_material_id, MAX_MATERIAL_SLOTS> materials = {};
        math::mat4 worldFromLocal = math::mat4(1.0f);
        math::aabb worldBounds = {};                    // computed by extraction from the mesh bounds
        u32 entityId = 0;                               // debug visualisation only (R11)
        u32 layerMask = 1;
        bool castsShadows = true;
        bool receivesShadows = true;
    };

    enum class view_flags : u8 {
        NONE = 0,
        DRAW_DEBUG = 1 << 0,
        NO_POST_PROCESS = 1 << 1,
        SKIP_SHADOWS = 1 << 2
    };
    CONJURE_ENUM_FLAGS(view_flags);

    /// <summary>
    /// A debug visualisation that replaces a view's output. A property of the view only (editor views,
    /// screenshots and tests), never stored in a scene or project.
    /// </summary>
    enum class view_debug_mode : u8 {
        NONE = 0,
        AMBIENT_OCCLUSION                               // the SSAO term as grey, white = unoccluded
    };

    struct render_view {
        camera cameraData = {};
        math::mat4 viewFromWorld = math::mat4(1.0f);
        math::vec3 cameraPosition = math::vec3(0.0f);
        math::urect viewport = {};                      // pixels inside the target; empty = whole target
        render_target_id target = {};                   // invalid = swapchain
        view_flags flags = view_flags::NONE;
        u32 layerMask = 0xFFFFFFFFu;
        // The groups of the two members below that replace the scene's for this view
        // (effective_view_settings); a group not named here is ignored.
        view_settings_override_flags settingsOverride = view_settings_override_flags::NONE;
        // Not NONE: the renderer also draws the view without shadows and leaves it out of the shadow atlas
        // casters, as SKIP_SHADOWS does.
        view_debug_mode debugMode = view_debug_mode::NONE;
        post_process_settings postProcess = {};
        ssao_settings ssao = {};
    };

    /// <summary>
    /// The post-process and SSAO groups one view renders with.
    /// </summary>
    struct view_render_settings {
        post_process_settings postProcess = {};
        ssao_settings ssao = {};

        [[nodiscard]] bool operator==(const view_render_settings &other) const noexcept = default;
    };

    /// <summary>
    /// The given scene groups with each group the view names in settingsOverride replaced by the view's own,
    /// unsanitised. Turning post-processing off (view_flags::NO_POST_PROCESS) and debugMode are not applied
    /// here: the result is the configured values.
    /// </summary>
    [[nodiscard]] CONJURE_API view_render_settings effective_view_settings(
      const post_process_settings &scenePostProcess,
      const ssao_settings &sceneSsao,
      const render_view &view
    ) noexcept;

    struct CONJURE_API render_scene {
        u64 frameIndex = 0;
        f32 deltaSeconds = 0.0f;
        std::vector<render_view> views = {};
        std::vector<mesh_instance> meshes = {};
        std::vector<light> lights = {};
        std::vector<sprite> sprites = {};
        environment_settings environment = {};
        // The scene-wide post-process, shadow and SSAO groups, shared by every view (engine.render_extract writes
        // them every frame: project.json rendering.defaults with each group that a render_settings component
        // overrides replaced by that group's winner, renderer::resolve_scene_settings; a hand-built scene sets
        // them). A view's settingsOverride groups replace postProcess and ssao for that view. The renderer clamps
        // them into their ranges (sanitise) on its own copy, so values out of range render clamped and never abort
        // a frame.
        post_process_settings postProcess = {};
        shadow_settings shadows = {};
        ssao_settings ssao = {};
        debug_draw_buffer debugDraw = {};

        /// <summary>
        /// Resizes the mesh array once and returns the whole span so jobs can fill disjoint ranges.
        /// Every element is default-initialised.
        /// </summary>
        [[nodiscard]] std::span<mesh_instance> allocate_meshes(u32 count);

        /// <summary>
        /// Clears every array but keeps capacity so steady-state frames do not allocate. Settings
        /// return to their defaults.
        /// </summary>
        void clear() noexcept;
    };

    /// <summary>
    /// Two scenes: one being written by extraction, one being read by the renderer.
    /// </summary>
    class CONJURE_API render_scene_pair {
        private:
            std::array<render_scene, 2> mScenes = {};
            u32 mWriteIndex = 0;
            bool mExtracting = false;
            std::thread::id mOwner = {};

        public:
            render_scene_pair();
            ~render_scene_pair();

            render_scene_pair(const render_scene_pair &) = delete;
            render_scene_pair &operator=(const render_scene_pair &) = delete;

        public:
            /// <summary>
            /// Clears the write slot and returns it. Asserts when extraction is already open.
            /// </summary>
            [[nodiscard]] render_scene &begin_extract();

            /// <summary>
            /// Swaps the slots; what was written becomes read(). Asserts without a matching
            /// begin_extract.
            /// </summary>
            void end_extract();

            /// <summary>
            /// The slot the renderer reads. Stable between end_extract calls.
            /// </summary>
            [[nodiscard]] const render_scene &read() const noexcept;

            /// <summary>
            /// The slot extraction writes (valid between begin_extract and end_extract).
            /// </summary>
            [[nodiscard]] render_scene &write() noexcept;

            [[nodiscard]] bool is_extracting() const noexcept {
                return mExtracting;
            }

            /// <summary>
            /// Index of the write slot (0 or 1); flips on every end_extract.
            /// </summary>
            [[nodiscard]] u32 write_index() const noexcept {
                return mWriteIndex;
            }
    };
}
