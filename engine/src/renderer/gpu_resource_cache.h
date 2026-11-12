#pragma once

#include "renderer/binding_layouts.h"
#include "renderer/gpu_types.h"

#include "conjure/renderer/environment_asset.h"
#include "conjure/renderer/gpu_ids.h"
#include "conjure/renderer/material_asset.h"
#include "conjure/renderer/mesh_asset.h"
#include "conjure/renderer/texture_asset.h"

#include "conjure/assets/asset_types.h"
#include "conjure/core/containers/flat_hash_map.h"
#include "conjure/core/containers/slot_map.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <nvrhi/nvrhi.h>

#include <array>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <variant>
#include <vector>

// GPU mirrors of the renderer asset types (docs/design/02_renderer.md 2.2, 2.6, R10, 5). enqueue_*
// and release_* may be called from any thread: the payload is copied into an upload record under the
// mutex and the slot is reserved at once, so the returned id is valid immediately (resolving to the
// fallback until the record is flushed at the start of a render). flush_uploads and collect belong to
// the render thread. resolve_* lock internally and return references into heap-allocated entries:
// an entry released from any thread is retired, not destroyed, until the next collect(), so a
// reference obtained during a frame stays valid for that frame even while jobs release ids. A
// released mirror's GPU objects stay alive bufferCount + 1 frames so in-flight command lists never
// lose their resources. No NVRHI object is created outside flush_uploads (R5).

namespace conjure::renderer {

    struct gpu_mesh {
        nvrhi::BufferHandle stream0 = nullptr;
        nvrhi::BufferHandle stream1 = nullptr;
        nvrhi::BufferHandle indices = nullptr;
        std::vector<submesh> submeshes = {};
        math::aabb bounds = {};
        u32 indexCount = 0;
        u32 vertexCount = 0;
    };

    struct gpu_texture {
        nvrhi::TextureHandle texture = nullptr;
        rhi::texture_format format = rhi::texture_format::RGBA8_SRGB;
        u32 width = 0;
        u32 height = 0;
        u32 mipCount = 0;
        sampler_filter filter = sampler_filter::LINEAR;
        texture_wrap wrap = texture_wrap::REPEAT;
    };

    struct gpu_material {
        nvrhi::BufferHandle constants = nullptr;
        nvrhi::BindingSetHandle bindingSet = nullptr;
        gpu::material_constants data = {};
        alpha_mode alphaMode = alpha_mode::SOLID;
        bool doubleSided = false;
        bool unlit = false;
        bool constantsDirty = false;    // data differs from the GPU constant buffer; written by the next flush
        bool rebindPending = false;     // a referenced texture was released; rebound by the next flush
        std::array<assets::asset_id, MATERIAL_TEXTURE_SLOTS> textureAssets = {};
        std::array<gpu_texture_id, MATERIAL_TEXTURE_SLOTS> textures = {};
    };

    struct gpu_environment {
        nvrhi::TextureHandle sky = nullptr;
        nvrhi::TextureHandle prefiltered = nullptr;
        sh9 irradiance = {};
        u32 prefilteredMipCount = 1;
    };

    /// <summary>
    /// What one flush did.
    /// </summary>
    struct upload_report {
        u32 completed = 0;              // records flushed, built or not (they count against the byte budget)
        u32 failed = 0;                 // completed records the device could not build; the fallback serves them
        u32 pending = 0;
        u64 bytes = 0;
        u32 materialsRebound = 0;       // binding sets rebuilt because a texture arrived or was released
        u32 constantWrites = 0;         // material constant buffers written
    };

    class gpu_resource_cache {
        private:
            struct create_tag {};

            template <typename Mirror>
            struct entry {
                Mirror mirror = {};
                assets::asset_id asset = {};
                bool ready = false;             // false until an upload flushed and built
                bool pendingReported = false;   // the "not ready" DEBUG line was logged
            };

            template <typename Mirror>
            using entry_ptr = std::unique_ptr<entry<Mirror>>;

            struct upload_record {
                std::variant<std::monostate, mesh_asset, texture_asset, material_asset, environment_asset> payload = {};
                u64 index = 0;              // slot index of the target entry
                u16 generation = 0;
                u64 bytes = 0;

                /// <summary>
                /// Identity of the target entry (payload kind, slot, generation): records with equal keys
                /// must flush in queue order.
                /// </summary>
                [[nodiscard]] u64 key() const noexcept;
            };

            nvrhi::IDevice *mDevice = nullptr;
            const binding_layouts *mLayouts = nullptr;
            u32 mReleaseDelayFrames = 4;

            mutable std::mutex mMutex = {};
            core::slot_map<entry_ptr<gpu_mesh>, gpu_mesh_id> mMeshes = {};
            core::slot_map<entry_ptr<gpu_texture>, gpu_texture_id> mTextures = {};
            core::slot_map<entry_ptr<gpu_material>, gpu_material_id> mMaterials = {};
            core::slot_map<entry_ptr<gpu_environment>, gpu_environment_id> mEnvironments = {};
            core::flat_hash_map<assets::asset_id, gpu_mesh_id> mMeshByAsset = {};
            core::flat_hash_map<assets::asset_id, gpu_texture_id> mTextureByAsset = {};
            core::flat_hash_map<assets::asset_id, gpu_material_id> mMaterialByAsset = {};
            core::flat_hash_map<assets::asset_id, gpu_environment_id> mEnvironmentByAsset = {};
            std::vector<upload_record> mPending = {};
            std::vector<upload_record> mDeferred = {};
            std::unordered_set<u64> mDeferredKeys = {};
            // Entries released since the previous collect(): their memory stays valid until then.
            std::vector<entry_ptr<gpu_mesh>> mRetiredMeshes = {};
            std::vector<entry_ptr<gpu_texture>> mRetiredTextures = {};
            std::vector<entry_ptr<gpu_material>> mRetiredMaterials = {};
            std::vector<entry_ptr<gpu_environment>> mRetiredEnvironments = {};
            using buried_handle = std::variant<nvrhi::BufferHandle, nvrhi::TextureHandle, nvrhi::BindingSetHandle>;
            std::vector<buried_handle> mGraveyard = {};
            std::vector<u64> mGraveyardFrames = {};

            gpu_mesh mFallbackMesh = {};
            std::array<gpu_texture, 4> mFallbackTextures = {};   // white, flat normal, black, magenta checker
            gpu_material mFallbackMaterial = {};
            gpu_environment mFallbackEnvironment = {};
            std::array<nvrhi::SamplerHandle, 6> mSamplers = {};  // [filter][wrap]
            u32 mFallbacksUsed = 0;
            std::unordered_set<u64> mStaleReported = {};         // (kind, id) pairs already warned about

        public:
            explicit gpu_resource_cache(create_tag);
            ~gpu_resource_cache();

            gpu_resource_cache(const gpu_resource_cache &) = delete;
            gpu_resource_cache &operator=(const gpu_resource_cache &) = delete;

        public:
            /// <summary>
            /// Creates the samplers and the fallback resources (uploaded through 'commandList', which
            /// must be open). 'releaseDelayFrames' is the swapchain's bufferCount + 1. Fails with
            /// renderer_error::RESOURCE_CREATION_FAILED.
            /// </summary>
            [[nodiscard]] static core::result<std::unique_ptr<gpu_resource_cache>> create(
              nvrhi::IDevice &device,
              const binding_layouts &layouts,
              nvrhi::ICommandList &commandList,
              u32 releaseDelayFrames
            );

            // Any thread.
            [[nodiscard]] gpu_mesh_id enqueue_mesh(const assets::asset_id &asset, const mesh_asset &mesh);
            [[nodiscard]] gpu_texture_id enqueue_texture(const assets::asset_id &asset, const texture_asset &texture);
            [[nodiscard]] gpu_material_id enqueue_material(
              const assets::asset_id &asset,
              const material_asset &material
            );
            [[nodiscard]] gpu_environment_id enqueue_environment(
              const assets::asset_id &asset,
              const environment_asset &environment
            );

            /// <summary>
            /// Makes the id stale at once; the GPU objects die 'releaseDelayFrames' after 'frameIndex'
            /// and the entry itself at the next collect(). Releasing a texture only marks the materials
            /// using it for a rebind by the next flush (no NVRHI object is created here).
            /// </summary>
            void release_mesh(gpu_mesh_id id, u64 frameIndex);
            void release_texture(gpu_texture_id id, u64 frameIndex);
            void release_material(gpu_material_id id, u64 frameIndex);
            void release_environment(gpu_environment_id id, u64 frameIndex);
            [[nodiscard]] u32 pending_count() const;

            [[nodiscard]] gpu_mesh_id find_mesh(const assets::asset_id &asset) const noexcept;
            [[nodiscard]] gpu_texture_id find_texture(const assets::asset_id &asset) const noexcept;
            [[nodiscard]] gpu_material_id find_material(const assets::asset_id &asset) const noexcept;
            [[nodiscard]] gpu_environment_id find_environment(const assets::asset_id &asset) const noexcept;

            // Render thread.

            /// <summary>
            /// Creates the GPU objects of the queued uploads in queue order, up to 'budgetBytes' of
            /// payload (at least one record always runs); once a record is deferred, every later record
            /// of the same id is deferred too so a newer payload never flushes before an older one.
            /// A record the device cannot build logs an ERROR, counts in upload_report::failed and
            /// leaves its id on the fallback. Materials whose textures arrived, failed or were released
            /// are rebound, and only the materials whose constants changed are written.
            /// </summary>
            upload_report flush_uploads(nvrhi::ICommandList &commandList, u64 frameIndex, u64 budgetBytes);

            /// <summary>
            /// Destroys the GPU objects released more than the delay ago and frees the entries retired
            /// since the previous collect (references returned by resolve_* before this call die here).
            /// </summary>
            void collect(u64 frameIndex);

            /// <summary>
            /// Device loss: drops every mirror and queued upload; ids become stale.
            /// </summary>
            void drop_all();

            /// <summary>
            /// The mirror, or the fallback for a stale, unknown or not ready id (a stale or unknown id
            /// WARNs once, a pending or failed one logs DEBUG once; fallbacks_used() is incremented).
            /// Locks internally; the reference is valid until the next collect().
            /// </summary>
            [[nodiscard]] const gpu_mesh &resolve_mesh(gpu_mesh_id id);
            [[nodiscard]] const gpu_texture &resolve_texture(gpu_texture_id id);
            [[nodiscard]] const gpu_material &resolve_material(gpu_material_id id);
            [[nodiscard]] const gpu_environment &resolve_environment(gpu_environment_id id);

            /// <summary>
            /// True when the id is live and its upload has flushed.
            /// </summary>
            [[nodiscard]] bool is_mesh_ready(gpu_mesh_id id) const noexcept;
            [[nodiscard]] bool is_texture_ready(gpu_texture_id id) const noexcept;
            [[nodiscard]] bool is_material_ready(gpu_material_id id) const noexcept;
            [[nodiscard]] bool is_environment_ready(gpu_environment_id id) const noexcept;

            /// <summary>
            /// Registers an externally owned texture (a render target's colour texture) so it has a
            /// gpu_texture_id; the cache never destroys it.
            /// </summary>
            [[nodiscard]] gpu_texture_id register_external_texture(
              nvrhi::ITexture *texture,
              rhi::texture_format format,
              u32 width,
              u32 height
            );
            void unregister_external_texture(gpu_texture_id id);

            [[nodiscard]] const gpu_mesh &fallback_mesh() const noexcept {
                return mFallbackMesh;
            }

            [[nodiscard]] const gpu_texture &fallback_texture(material_texture slot) const noexcept;

            [[nodiscard]] const gpu_material &fallback_material() const noexcept {
                return mFallbackMaterial;
            }

            [[nodiscard]] const gpu_environment &fallback_environment() const noexcept {
                return mFallbackEnvironment;
            }

            [[nodiscard]] nvrhi::ISampler *sampler(sampler_filter filter, texture_wrap wrap) const noexcept;

            /// <summary>
            /// Fallback resolutions since the last reset_counters().
            /// </summary>
            [[nodiscard]] u32 fallbacks_used() const noexcept;

            void reset_counters() noexcept;

            [[nodiscard]] usize mesh_count() const noexcept;
            [[nodiscard]] usize texture_count() const noexcept;
            [[nodiscard]] usize material_count() const noexcept;
            [[nodiscard]] usize environment_count() const noexcept;

            /// <summary>
            /// GPU objects waiting in the graveyard for their delay to pass.
            /// </summary>
            [[nodiscard]] usize graveyard_count() const noexcept;

            /// <summary>
            /// Entries released since the previous collect() whose memory is still alive.
            /// </summary>
            [[nodiscard]] usize retired_count() const noexcept;

        private:
            [[nodiscard]] core::result<void> create_fallbacks(nvrhi::ICommandList &commandList);
            [[nodiscard]] bool build_mesh(
              gpu_mesh &mirror,
              const mesh_asset &mesh,
              nvrhi::ICommandList &commandList,
              const char *name
            );
            [[nodiscard]] bool build_texture(
              gpu_texture &mirror,
              const texture_asset &texture,
              nvrhi::ICommandList &commandList,
              const char *name
            );
            [[nodiscard]] bool build_material(
              gpu_material &mirror,
              const material_asset &material,
              nvrhi::ICommandList &commandList,
              const char *name
            );
            [[nodiscard]] bool build_environment(
              gpu_environment &mirror,
              const environment_asset &environment,
              nvrhi::ICommandList &commandList,
              const char *name
            );
            [[nodiscard]] bool upload_cubemap(
              nvrhi::TextureHandle &target,
              const cubemap_data &data,
              nvrhi::ICommandList &commandList,
              const char *name
            );
            void rebind_material(gpu_material &mirror);
            [[nodiscard]] u32 rebind_materials_using(const assets::asset_id &textureAsset, u64 frameIndex);
            void mark_materials_using(const assets::asset_id &textureAsset);
            void bury(const gpu_mesh &mirror, u64 frameIndex);
            void bury(const gpu_texture &mirror, u64 frameIndex);
            void bury(const gpu_material &mirror, u64 frameIndex);
            void bury(const gpu_environment &mirror, u64 frameIndex);
            void warn_stale(u8 kindIndex, const char *kind, u64 packedId, u32 index);
    };
}
