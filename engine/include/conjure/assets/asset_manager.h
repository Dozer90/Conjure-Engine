#pragma once

#include "conjure/assets/asset_ref.h"
#include "conjure/assets/asset_slot.h"
#include "conjure/assets/asset_source.h"
#include "conjure/assets/asset_type_registry.h"
#include "conjure/assets/asset_types.h"

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

// The runtime asset manager (docs/design/05_assets.md 3.1-3.5, 4.5): slots, binding, the async
// load pipeline (dispatch, completions, budgeted finalisation, dependency gating, publish and
// retirement), explicit unloading, fallbacks, events and statistics. Every mutating call is
// main-thread only (D13); asset_ref / asset_handle reads are lock-free from any thread.

namespace conjure::assets {

    /// <summary>
    /// Construction parameters.
    /// </summary>
    struct asset_manager_desc {
        core::jobs::job_system *jobs = nullptr;                      // required; 0 workers = inline
        std::unique_ptr<asset_source> source = nullptr;              // the primary blob provider
        u32 maxJobsInFlight = DEFAULT_MAX_JOBS_IN_FLIGHT;
        std::chrono::microseconds finaliseBudget = std::chrono::microseconds(2000);   // per update()
        std::chrono::milliseconds blockingTimeout = std::chrono::milliseconds(30000); // load_blocking
    };

    /// <summary>
    /// What happened to a slot.
    /// </summary>
    enum class asset_event_kind : u8 {
        LOADED = 0,
        RELOADED,
        RELOAD_FAILED,
        FAILED,
        UNLOADED
    };

    /// <summary>
    /// Delivered to subscribers on the main thread at the end of update(), in order.
    /// </summary>
    struct asset_event {
        asset_event_kind kind = asset_event_kind::LOADED;
        asset_id id = {};
        asset_type_id type = asset_type_id::INVALID;
        u32 version = 0;
        const core::error *error = nullptr;   // FAILED and RELOAD_FAILED only
    };

    using asset_event_callback = std::function<void(const asset_event &event)>;

    /// <summary>
    /// Token returned by subscribe(); 0 is invalid.
    /// </summary>
    struct asset_subscription {
        u64 id = 0;

        [[nodiscard]] bool is_valid() const noexcept {
            return id != 0;
        }
    };

    /// <summary>
    /// The handles of one load_batch() call.
    /// </summary>
    class asset_load_batch {
        private:
            std::vector<asset_handle> mHandles = {};

        public:
            asset_load_batch();
            explicit asset_load_batch(std::vector<asset_handle> handles);
            ~asset_load_batch();

            asset_load_batch(const asset_load_batch &) = default;
            asset_load_batch &operator=(const asset_load_batch &) = default;
            asset_load_batch(asset_load_batch &&) noexcept = default;
            asset_load_batch &operator=(asset_load_batch &&) noexcept = default;

        public:
            /// <summary>
            /// True when every handle is terminal (an empty batch is complete).
            /// </summary>
            [[nodiscard]] bool is_complete() const noexcept;

            /// <summary>
            /// Terminal handles divided by total, 0..1 (1 for an empty batch).
            /// </summary>
            [[nodiscard]] f32 progress() const noexcept;

            /// <summary>
            /// Number of handles that are LOADED.
            /// </summary>
            [[nodiscard]] usize loaded_count() const noexcept;

            [[nodiscard]] std::span<const asset_handle> handles() const noexcept {
                return mHandles;
            }

            [[nodiscard]] usize size() const noexcept {
                return mHandles.size();
            }
    };

    /// <summary>
    /// Counters for tooling.
    /// </summary>
    struct asset_manager_stats {
        u32 slots = 0;
        u32 loaded = 0;
        u32 pending = 0;          // QUEUED + LOADING + FINALISING + RELOADING
        u32 failed = 0;
        u32 retired = 0;          // payloads awaiting destruction
        u32 jobsInFlight = 0;
        u64 bytesResident = 0;    // cooked payload bytes of LOADED slots
        u64 frameIndex = 0;
    };

    /// <summary>
    /// Owns every slot, payload and the load pipeline. Outlives every handle.
    /// </summary>
    class asset_manager {
        private:
            struct state;

            std::unique_ptr<state> mState = nullptr;

        public:
            explicit asset_manager(asset_manager_desc desc);

            /// <summary>
            /// Releases internal handles, destroys payloads and asserts that no external strong
            /// reference is left (logs and continues in Distribution).
            /// </summary>
            ~asset_manager();

            asset_manager(const asset_manager &) = delete;
            asset_manager &operator=(const asset_manager &) = delete;

        public:
            [[nodiscard]] asset_type_registry &types() noexcept;
            [[nodiscard]] const asset_type_registry &types() const noexcept;

            /// <summary>
            /// Adds a secondary source searched after the primary (plugin packs).
            /// </summary>
            void add_source(std::unique_ptr<asset_source> source);

            /// <summary>
            /// Replaces the primary source (editor: cache -> pack preview). Loaded payloads stay.
            /// Safe while loads are in flight: a job reading from the old source finishes its
            /// read first, and jobs that have not started resolve the new source.
            /// </summary>
            void swap_source(std::unique_ptr<asset_source> source);

            /*---------------------------------------------------------------*/
            // Binding (never loads)

            /// <summary>
            /// Finds or creates the slot and binds a typed reference. When the slot's type is not
            /// yet known it is taken from the source, else from AssetType; a recorded type that
            /// differs from AssetType logs an ERROR once per uuid and yields an unbound reference.
            /// </summary>
            template <declared_asset_type AssetType>
            [[nodiscard]] asset_ref<AssetType> bind(const asset_id &id) {
                const asset_slot *slot = bind_typed(id, asset_type_traits<AssetType>::id());
                if (slot == nullptr) {
                    return asset_ref<AssetType>(id);
                }
                return asset_ref<AssetType>(id, slot);
            }

            /// <summary>
            /// Untyped bind for the scene bind pass (R25). Null for the null uuid.
            /// </summary>
            [[nodiscard]] const asset_slot *bind_slot(const asset_id &id);

            /// <summary>
            /// An owning handle on the slot; binds, never loads. Unbound for the null uuid.
            /// </summary>
            [[nodiscard]] asset_handle make_handle(const asset_id &id);

            /// <summary>
            /// Resolves "mount://path#sub" through the sources.
            /// </summary>
            [[nodiscard]] std::optional<asset_id> find(std::string_view assetPath) const;

            /*---------------------------------------------------------------*/
            // Loading

            /// <summary>
            /// Queues a load (UNLOADED -> QUEUED) and returns a handle. Other states are left alone
            /// (FAILED needs retry_failed()). Unbound handle for the null uuid.
            /// </summary>
            [[nodiscard]] asset_handle load(
              const asset_id &id,
              asset_load_priority priority = asset_load_priority::NORMAL
            );

            [[nodiscard]] asset_load_batch load_batch(
              std::span<const asset_id> ids,
              asset_load_priority priority = asset_load_priority::NORMAL
            );

            /// <summary>
            /// Main thread. Runs the whole pipeline inline, dependencies first, and returns the
            /// LOADED handle or the failure (the slot is then FAILED). Waits for an in-flight job
            /// up to blockingTimeout (TIMEOUT). Inside a frame it logs one WARN per frame (R23).
            /// </summary>
            [[nodiscard]] core::result<asset_handle> load_blocking(const asset_id &id);

            /// <summary>
            /// LOADED -> RELOADING through the normal pipeline; the old payload stays published
            /// until the new one finalises. Ignored in other states.
            /// </summary>
            void reload(const asset_id &id);

            /// <summary>
            /// FAILED -> QUEUED. Ignored in other states.
            /// </summary>
            void retry_failed(const asset_id &id);

            /// <summary>
            /// A pinned slot is never unloaded by collect().
            /// </summary>
            void pin(const asset_id &id, bool pinned);

            [[nodiscard]] bool is_pinned(const asset_id &id) const noexcept;

            /*---------------------------------------------------------------*/
            // Fallbacks and queries

            /// <summary>
            /// The type's fallback payload, created lazily from its loader; nullptr when the type
            /// is unregistered or has no fallback. Pinned for the manager's lifetime.
            /// </summary>
            template <declared_asset_type AssetType>
            [[nodiscard]] AssetType *fallback() noexcept {
                return static_cast<AssetType *>(fallback_payload(asset_type_traits<AssetType>::id()));
            }

            /// <summary>
            /// ref.get(), or the type's fallback while the asset is not LOADED.
            /// </summary>
            template <declared_asset_type AssetType>
            [[nodiscard]] AssetType *get_or_fallback(const asset_ref<AssetType> &ref) noexcept {
                AssetType *loaded = ref.get();
                if (loaded != nullptr) {
                    return loaded;
                }
                return fallback<AssetType>();
            }

            /// <summary>
            /// The error of a FAILED slot (or of the last failed reload), else nullptr. Valid until
            /// the slot changes state.
            /// </summary>
            [[nodiscard]] const core::error *last_error(const asset_id &id) const noexcept;

            /// <summary>
            /// UNLOADED for unknown uuids.
            /// </summary>
            [[nodiscard]] asset_state state_of(const asset_id &id) const noexcept;

            /// <summary>
            /// Slots that are FAILED with the given code (the hot reload controller repairs
            /// CACHE_MISS).
            /// </summary>
            [[nodiscard]] std::vector<asset_id> failed_ids(asset_error_code code) const;

            /*---------------------------------------------------------------*/
            // Frame

            void begin_frame(u64 frameIndex);

            /// <summary>
            /// Dispatches queued loads, drains completions, finalises under the budget (at least
            /// one slot per call), destroys retired payloads and fires events.
            /// </summary>
            void update();

            void end_frame();

            /// <summary>
            /// Unloads every LOADED slot with zero strong references that is not pinned, releasing
            /// its dependency handles, until nothing changes. Returns how many were unloaded.
            /// </summary>
            u32 collect();

            /// <summary>
            /// Runs update() until every queued load is terminal.
            /// </summary>
            void flush();

            /// <summary>
            /// Refuses new work, waits for in-flight jobs, discards their completions and returns
            /// queued slots to UNLOADED.
            /// </summary>
            void shutdown();

            [[nodiscard]] bool is_shut_down() const noexcept;

            /*---------------------------------------------------------------*/
            // Events and statistics

            [[nodiscard]] asset_subscription subscribe(asset_event_callback callback);
            void unsubscribe(asset_subscription subscription);
            [[nodiscard]] asset_manager_stats stats() const noexcept;
            [[nodiscard]] u64 frame_index() const noexcept;

        private:
            [[nodiscard]] const asset_slot *bind_typed(const asset_id &id, asset_type_id type);
            [[nodiscard]] void *fallback_payload(asset_type_id type) noexcept;
    };
}
