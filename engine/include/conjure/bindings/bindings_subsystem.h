#pragma once

#include "conjure/bindings/native_api_entries.h"

#include "conjure/assets/asset_ref.h"
#include "conjure/assets/asset_types.h"
#include "conjure/core/api.h"
#include "conjure/core/containers/flat_hash_map.h"
#include "conjure/core/event.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"
#include "conjure/engine/engine_context.h"
#include "conjure/engine/engine_subsystem.h"

#include <string_view>

// Startup step 16 (docs/design/15_bindings.md 3.1, 4.2, B9): the subsystem that fills the bindings-owned
// native entries before scripting's post_startup runs the handshake, wraps the table in counting thunks
// outside Distribution, owns the asset residency scripts request through asset_load / asset_release and
// clears it when a scene unloads. The executables add it with
// application.add_subsystem(std::make_unique<scripting_bindings_subsystem>()) before initialise().

namespace conjure::bindings {

    /// <summary>
    /// The asset handles scripts hold through asset_load, keyed by uuid (B9). Repeat holds of one uuid are
    /// no-ops; release drops the handle (the slot stays, 05_assets.md). Main thread only.
    /// </summary>
    class CONJURE_API script_residency {
        private:
            core::flat_hash_map<core::uuid, assets::asset_handle> mHandles = {};

        public:
            script_residency();
            ~script_residency();

            script_residency(const script_residency &) = delete;
            script_residency &operator=(const script_residency &) = delete;

        public:
            /// <summary>
            /// Keeps the handle under its uuid; a uuid already held keeps its first handle. False for an
            /// unbound handle or the null uuid.
            /// </summary>
            bool hold(const core::uuid &id, assets::asset_handle handle);

            /// <summary>
            /// Drops the handle of a uuid. False when none was held.
            /// </summary>
            bool release(const core::uuid &id) noexcept;

            /// <summary>
            /// True while a handle is held for the uuid.
            /// </summary>
            [[nodiscard]] bool holds(const core::uuid &id) const noexcept;

            /// <summary>
            /// Drops every handle.
            /// </summary>
            void clear() noexcept;

            /// <summary>
            /// Handles held.
            /// </summary>
            [[nodiscard]] usize size() const noexcept;
    };

    class CONJURE_API scripting_bindings_subsystem final : public engine::engine_subsystem {
        private:
            binding_context mContext = {};
            script_residency mResidency = {};
            core::event_subscription mUnloadSubscription = {};
            bool mFilled = false;

        public:
            scripting_bindings_subsystem();
            ~scripting_bindings_subsystem() override;

        public:
            /// <summary>
            /// "bindings".
            /// </summary>
            [[nodiscard]] std::string_view name() const noexcept override;

            /// <summary>
            /// Step 16 (3.1): nothing to do without a script runtime; otherwise builds the binding_context from
            /// the engine_context, fills the owned slots, wraps every slot with counting thunks outside
            /// Distribution, verifies that no slot is null (TABLE_INCOMPLETE unwinds the startup) and
            /// subscribes to scene_manager::on_before_unload to clear the residency.
            /// </summary>
            [[nodiscard]] core::result<void> startup(engine::engine_context &context) override;

            /// <summary>
            /// Step 17, after scripting's post_startup filled the core entries: wraps them in counting thunks too
            /// (idempotent for the slots already wrapped at step 16).
            /// </summary>
            [[nodiscard]] core::result<void> post_startup(engine::engine_context &context) override;

            /// <summary>
            /// Re-installs the counting thunks and the query_end hook when a re-initialised script runtime
            /// dropped them (is_scope_hook_installed), clears the last-error buffer and the per-frame
            /// rate-limit bits of the entry failure logs, and applies the scene fixups a force-closed scope
            /// left behind.
            /// </summary>
            void begin_frame(engine::engine_context &context) override;

            /// <summary>
            /// Drops the residency and the subscription and forgets the active context. Safe after a failed
            /// or never attempted startup and when called twice.
            /// </summary>
            void shutdown(engine::engine_context &context) override;

            /// <summary>
            /// The context the table was filled from (every pointer null before startup).
            /// </summary>
            [[nodiscard]] const binding_context &context() const noexcept {
                return mContext;
            }

            /// <summary>
            /// The handles scripts hold through asset_load.
            /// </summary>
            [[nodiscard]] script_residency &residency() noexcept {
                return mResidency;
            }

            /// <summary>
            /// True once startup filled the table (false when scripting was disabled).
            /// </summary>
            [[nodiscard]] bool is_filled() const noexcept {
                return mFilled;
            }

            /// <summary>
            /// The residency the active entries use (asset_load / asset_release), or nullptr when no
            /// subsystem started. Tests without an application install one with set_active_residency.
            /// </summary>
            [[nodiscard]] static script_residency *active_residency() noexcept;

            /// <summary>
            /// Installs (or, with nullptr, removes) the residency the entries use.
            /// </summary>
            static void set_active_residency(script_residency *residency) noexcept;
    };
}
