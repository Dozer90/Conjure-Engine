#pragma once

#include "conjure/renderer/camera.h"
#include "conjure/renderer/debug_draw.h"
#include "conjure/renderer/gpu_ids.h"
#include "conjure/renderer/light.h"
#include "conjure/renderer/limits.h"
#include "conjure/renderer/render_settings.h"
#include "conjure/renderer/sprite_batch.h"

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

    struct render_view {
        camera cameraData = {};
        math::mat4 viewFromWorld = math::mat4(1.0f);
        math::vec3 cameraPosition = math::vec3(0.0f);
        math::urect viewport = {};                      // pixels inside the target; empty = whole target
        render_target_id target = {};                   // invalid = swapchain
        view_flags flags = view_flags::NONE;
        u32 layerMask = 0xFFFFFFFFu;
    };

    struct render_scene {
        u64 frameIndex = 0;
        f32 deltaSeconds = 0.0f;
        std::vector<render_view> views = {};
        std::vector<mesh_instance> meshes = {};
        std::vector<light> lights = {};
        std::vector<sprite> sprites = {};
        environment_settings environment = {};
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
    class render_scene_pair {
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
