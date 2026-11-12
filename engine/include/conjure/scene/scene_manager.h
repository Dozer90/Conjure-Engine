#pragma once

#include "conjure/scene/scene.h"
#include "conjure/scene/scene_serializer.h"
#include "conjure/scene/transform_system.h"

#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_types.h"
#include "conjure/core/api.h"
#include "conjure/core/event.h"
#include "conjure/core/result.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/core/vfs/file_system.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/world.h"

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Scene residency (docs/design/13_scene.md 3.11, 4.7, S11): the one scene over the world, the loaded
// scene records with their asset_load_batch, synchronous and deferred loads (replace or additive),
// unloads that cascade through the scene and release the assets, the layer names and the transform
// system the engine registers. Main thread only.

namespace conjure::scene {

    /// <summary>
    /// Whether a load replaces the current content or adds to it.
    /// </summary>
    enum class load_mode : u8 {
        REPLACE,
        ADDITIVE
    };

    /// <summary>
    /// Handle of a deferred load request (0 is invalid).
    /// </summary>
    struct load_request_id {
        u32 value = 0;

        [[nodiscard]] constexpr bool is_valid() const noexcept {
            return value != 0;
        }

        [[nodiscard]] constexpr bool operator==(const load_request_id &) const noexcept = default;
    };

    /// <summary>
    /// One loaded scene asset.
    /// </summary>
    struct loaded_scene_info {
        scene_id id = {};
        assets::asset_id asset = {};
        std::string name = {};
        std::string path = {};                             // file path for load_file, empty for assets
        std::vector<ecs::entity> roots = {};               // the scene's current roots, in root order
    };

    /// <summary>
    /// Names of the 32 layers, filled by the engine from project.json.
    /// </summary>
    class CONJURE_API layer_table {
        public:
            static constexpr u32 LAYER_COUNT = 32;

        private:
            std::array<core::string_id, LAYER_COUNT> mNames = {};

        public:
            layer_table();
            ~layer_table();

        public:
            /// <summary>
            /// Names a layer; indices above 31 are ignored (asserted).
            /// </summary>
            void set_name(u8 index, core::string_id value) noexcept;

            /// <summary>
            /// The name of a layer (the null id when unnamed or out of range).
            /// </summary>
            [[nodiscard]] core::string_id name(u8 index) const noexcept;

            /// <summary>
            /// The index of the layer with this name, or nullopt.
            /// </summary>
            [[nodiscard]] std::optional<u8> find(core::string_id value) const noexcept;

            [[nodiscard]] std::span<const core::string_id> names() const noexcept {
                return mNames;
            }
    };

    class CONJURE_API scene_manager {
        private:
            struct loaded_record {
                loaded_scene_info info = {};
                assets::asset_load_batch batch = {};
                bool alive = false;
            };

            struct pending_load {
                load_request_id id = {};
                assets::asset_id asset = {};
                load_mode mode = load_mode::ADDITIVE;
                assets::asset_handle handle = {};
            };

            ecs::world *mWorld = nullptr;
            assets::asset_manager *mAssets = nullptr;
            scene mScene;                                  // no default: bound to the world in the constructor
            transform_system mTransforms;                  // no default: bound to mScene in the constructor
            layer_table mLayers = {};
            std::vector<loaded_record> mRecords = {};      // slots reused with a fresh generation
            std::vector<u32> mLoadOrder = {};              // record slots in load order
            std::vector<pending_load> mPending = {};
            std::vector<scene_id> mPendingUnloads = {};
            u32 mNextGeneration = 2;                       // 1 is SCENE_ID_AUTHORED
            u32 mNextRequest = 1;
            core::event<scene_id, std::span<const ecs::entity>> mOnBeforeUnload = {};
            core::event<scene_id> mOnLoaded = {};
            core::event<load_request_id, const core::error &> mOnLoadFailed = {};

        public:
            scene_manager(ecs::world &world, assets::asset_manager &assets);

            /// <summary>
            /// Drops the load records (and their asset handles); the scene's entities stay in the world.
            /// </summary>
            ~scene_manager();

            scene_manager(const scene_manager &) = delete;
            scene_manager &operator=(const scene_manager &) = delete;

        public:
            /// <summary>
            /// The one scene over the world.
            /// </summary>
            [[nodiscard]] scene &active() noexcept {
                return mScene;
            }

            [[nodiscard]] const scene &active() const noexcept {
                return mScene;
            }

            /// <summary>
            /// The propagation system register_systems adds to the scheduler (the engine toggles its
            /// interpolation from engine_time.interpolatePhysics).
            /// </summary>
            [[nodiscard]] transform_system &transforms() noexcept {
                return mTransforms;
            }

            /// <summary>
            /// The layer names, filled by the engine from project.json.
            /// </summary>
            [[nodiscard]] layer_table &layers() noexcept {
                return mLayers;
            }

            [[nodiscard]] const layer_table &layers() const noexcept {
                return mLayers;
            }

            /// <summary>
            /// The asset manager the scene binds through and loads scene assets from.
            /// </summary>
            [[nodiscard]] assets::asset_manager &assets() noexcept {
                return *mAssets;
            }

            /*---------------------------------------------------------------*/
            // Loading

            /// <summary>
            /// Loads a scene asset synchronously (load_blocking, dependencies included) and applies it.
            /// LOAD_FAILED when the asset is not a scene or failed to load; SCENE_NOT_LOADED when an
            /// on_structure_changed handler unloads the scene before its load finishes.
            /// </summary>
            [[nodiscard]] core::result<scene_id> load(const assets::asset_id &id, load_mode mode = load_mode::REPLACE);

            /// <summary>
            /// Parses a scene file from the file system, applies it and loads the referenced assets as a
            /// batch (the record keeps the handles). SCENE_NOT_LOADED as for load.
            /// </summary>
            [[nodiscard]] core::result<scene_id> load_file(
              core::vfs::file_system &fs,
              std::string_view path,
              load_mode mode = load_mode::REPLACE
            );

            /// <summary>
            /// Queues an asynchronous load applied by apply_pending_loads once the asset is resident.
            /// </summary>
            [[nodiscard]] load_request_id request_load(
              const assets::asset_id &id,
              load_mode mode = load_mode::ADDITIVE
            );

            /// <summary>
            /// Queues an unload applied at the next apply_pending_loads; unknown ids are ignored.
            /// </summary>
            void request_unload(scene_id id);

            /// <summary>
            /// Emits on_before_unload, destroys the scene's roots (reverse order), drops the asset
            /// handles and runs asset_manager::collect. SCENE_NOT_LOADED for an unknown id.
            /// </summary>
            [[nodiscard]] core::result<void> unload(scene_id id);

            /// <summary>
            /// Unloads every scene in reverse load order, then clears the remaining authored entities;
            /// unload_all fires on_before_unload once more with SCENE_ID_AUTHORED and the remaining roots when
            /// any entity is left. A scene an on_before_unload handler loads meanwhile is cleared and retired
            /// with the rest, so no scene is loaded afterwards.
            /// </summary>
            void unload_all();

            /// <summary>
            /// Body of scene.apply_pending_loads: pending unloads, then pending loads in request order
            /// (a FAILED asset drops its request with ERROR and on_load_failed; a load still in flight
            /// stops the pass).
            /// </summary>
            void apply_pending_loads();

            /*---------------------------------------------------------------*/
            // Queries

            /// <summary>
            /// True while the id names a loaded scene (unload and REPLACE loads retire ids for good).
            /// </summary>
            [[nodiscard]] bool is_loaded(scene_id id) const noexcept;

            /// <summary>
            /// The loaded scene created from an asset, or nullopt.
            /// </summary>
            [[nodiscard]] std::optional<scene_id> find_loaded(const assets::asset_id &asset) const noexcept;

            /// <summary>
            /// Every loaded scene in load order, with its current roots.
            /// </summary>
            [[nodiscard]] std::vector<loaded_scene_info> loaded_scenes() const;

            /// <summary>
            /// The record of a loaded scene with its current roots (gathered on each call), or nullopt.
            /// </summary>
            [[nodiscard]] std::optional<loaded_scene_info> info(scene_id id) const;

            /// <summary>
            /// Requests not yet applied.
            /// </summary>
            [[nodiscard]] u32 pending_count() const noexcept {
                return static_cast<u32>(mPending.size());
            }

            /// <summary>
            /// True while a request for the asset waits (R55).
            /// </summary>
            [[nodiscard]] bool is_load_pending(const assets::asset_id &id) const noexcept;

            /*---------------------------------------------------------------*/
            // Events (main thread)

            /// <summary>
            /// Fires before a scene's entities die with the roots about to be destroyed (scripting runs
            /// destroy_all here, R29); unload_all fires it once more with SCENE_ID_AUTHORED and the remaining
            /// roots when any entity is left. Handlers may load scenes; one loaded during unload_all is
            /// cleared and retired with the rest.
            /// </summary>
            [[nodiscard]] core::event<scene_id, std::span<const ecs::entity>> &on_before_unload() noexcept {
                return mOnBeforeUnload;
            }

            /// <summary>
            /// Fires once a load (synchronous or deferred) has instantiated and ordered its roots.
            /// </summary>
            [[nodiscard]] core::event<scene_id> &on_loaded() noexcept {
                return mOnLoaded;
            }

            /// <summary>
            /// Fires when a deferred request is dropped: the asset FAILED, is not a scene, or could not
            /// be applied.
            /// </summary>
            [[nodiscard]] core::event<load_request_id, const core::error &> &on_load_failed() noexcept {
                return mOnLoadFailed;
            }

        private:
            [[nodiscard]] core::result<scene_id> apply_document(
              const scene_document &document,
              load_mode mode,
              assets::asset_load_batch batch,
              const assets::asset_id &asset,
              std::string path
            );
            [[nodiscard]] loaded_record *find_record(scene_id id) noexcept;
            [[nodiscard]] const loaded_record *find_record(scene_id id) const noexcept;
            [[nodiscard]] u32 allocate_record();
            void collect_owned_roots(scene_id id, std::vector<ecs::entity> &out) const;
    };
}
