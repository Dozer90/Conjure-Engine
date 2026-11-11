#pragma once

#include "conjure/engine/engine_context.h"
#include "conjure/engine/layer.h"

#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/platform/event.h"

#include <memory>
#include <span>
#include <vector>

// The layer stack (docs/design/06_engine_loop.md 2.8): owns the layers below mOverlayBegin and the
// overlays above it. While the stack is attached (after attach_all) or iterating, pushes and pops are
// deferred to apply_pending(), which the application calls at frame step 13; before attach_all they take
// effect at once so executables build the stack before initialise().

namespace conjure::engine {

    class CONJURE_API layer_stack {
        private:
            std::vector<std::unique_ptr<layer>> mLayers = {};
            usize mOverlayBegin = 0;                        // first overlay in mLayers
            std::vector<std::unique_ptr<layer>> mPendingPush = {};
            std::vector<bool> mPendingPushIsOverlay = {};
            std::vector<layer *> mPendingPop = {};
            bool mIterating = false;
            bool mAttached = false;

        public:
            layer_stack();

            /// <summary>
            /// Destroys the layers top first (overlays, then layers), without detaching them: the owner
            /// calls detach_all() while the context is still alive.
            /// </summary>
            ~layer_stack();

            layer_stack(const layer_stack &) = delete;
            layer_stack &operator=(const layer_stack &) = delete;

        public:
            /// <summary>
            /// Adds a layer below the overlays and returns it. Deferred to apply_pending() while attached or
            /// iterating (the reference stays valid either way). A null pointer asserts and is ignored.
            /// </summary>
            layer *push_layer(std::unique_ptr<layer> value);

            /// <summary>
            /// Adds an overlay above every layer and the earlier overlays.
            /// </summary>
            layer *push_overlay(std::unique_ptr<layer> value);

            /// <summary>
            /// Removes a layer or overlay. Deferred to apply_pending() while attached or iterating, where an
            /// attached layer detaches with the real context before it is destroyed; before attach_all the
            /// layer is destroyed at once. A layer still waiting in the push queue is forgotten. Unknown
            /// layers are ignored.
            /// </summary>
            void pop(layer &target);

            /// <summary>
            /// Delivers an event top-down; true when a layer consumed it.
            /// </summary>
            [[nodiscard]] bool dispatch_event(engine_context &context, const platform::event &event);

            /// <summary>
            /// Every layer bottom-up: layers first in push order, then overlays in push order.
            /// </summary>
            [[nodiscard]] std::span<const std::unique_ptr<layer>> layers() const noexcept {
                return mLayers;
            }

            /// <summary>
            /// Number of overlays (the tail of layers()).
            /// </summary>
            [[nodiscard]] usize overlay_count() const noexcept {
                return mLayers.size() - mOverlayBegin;
            }

            /// <summary>
            /// Pushes and pops waiting for apply_pending().
            /// </summary>
            [[nodiscard]] usize pending_count() const noexcept {
                return mPendingPush.size() + mPendingPop.size();
            }

            /// <summary>
            /// True between attach_all() and detach_all().
            /// </summary>
            [[nodiscard]] bool is_attached() const noexcept {
                return mAttached;
            }

            /// <summary>
            /// Startup step 19: on_attach bottom-up. The first failure detaches the layers attached so far
            /// and returns LAYER_ATTACH_FAILED naming the layer.
            /// </summary>
            [[nodiscard]] core::result<void> attach_all(engine_context &context);

            /// <summary>
            /// Shutdown step 3: on_detach top-down; pending pushes are discarded, pending pops forgotten.
            /// </summary>
            void detach_all(engine_context &context);

            /// <summary>
            /// Frame step 13: pops first (detach, erase), then pushes (insert, attach; a failed attach logs an
            /// ERROR and drops the layer).
            /// </summary>
            void apply_pending(engine_context &context);

            /*---------------------------------------------------------------*/
            // Frame hooks, bottom-up

            void frame_begin(engine_context &context);
            void update(engine_context &context);
            void imgui(engine_context &context);
            void imgui_end(engine_context &context);
            void frame_end(engine_context &context);

        private:
            layer *push(std::unique_ptr<layer> value, bool overlay);
            void insert(std::unique_ptr<layer> value, bool overlay);
            [[nodiscard]] bool forget_pending_push(layer &target);
            void erase(layer &target, engine_context *context);
            template <typename Callable>
            void for_each_bottom_up(Callable &&callable);
    };
}
