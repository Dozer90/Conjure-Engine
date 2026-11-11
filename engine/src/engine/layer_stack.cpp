#include "conjure/engine/layer_stack.h"

#include "conjure/engine/engine_error.h"
#include "conjure/engine/log_category.h"

#include "conjure/core/assert.h"
#include "conjure/core/log.h"

#include <algorithm>
#include <format>
#include <utility>

using namespace conjure;
using namespace conjure::engine;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

layer_stack::layer_stack() {}

layer_stack::~layer_stack() {
    // Top first: the overlays, then the layers, each in reverse push order (destruction_reverse_order).
    while (!mLayers.empty()) {
        mLayers.pop_back();
    }
    mPendingPush.clear();
    mPendingPushIsOverlay.clear();
    mPendingPop.clear();
}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

layer *layer_stack::push_layer(std::unique_ptr<layer> value) {
    return push(std::move(value), false);
}

layer *layer_stack::push_overlay(std::unique_ptr<layer> value) {
    return push(std::move(value), true);
}

void layer_stack::pop(layer &target) {
    // A layer still waiting in the push queue never attached: forgetting it is safe at any time.
    if (forget_pending_push(target)) {
        return;
    }
    // Attached layers detach with the real context in apply_pending(); erase(target, nullptr) would destroy
    // them without on_detach (regression: pop outside an iteration while attached).
    if (mIterating || mAttached) {
        if (std::find(mPendingPop.begin(), mPendingPop.end(), &target) == mPendingPop.end()) {
            mPendingPop.push_back(&target);
        }
        return;
    }
    erase(target, nullptr);
}

bool layer_stack::dispatch_event(engine_context &context, const platform::event &event) {
    const bool wasIterating = mIterating;
    mIterating = true;
    bool consumed = false;
    for (usize index = mLayers.size(); index > 0; --index) {
        layer *current = mLayers[index - 1].get();
        if (std::find(mPendingPop.begin(), mPendingPop.end(), current) != mPendingPop.end()) {
            continue;
        }
        if (current->on_event(context, event)) {
            consumed = true;
            break;
        }
    }
    mIterating = wasIterating;
    return consumed;
}

core::result<void> layer_stack::attach_all(engine_context &context) {
    CONJURE_ASSERT(!mAttached, "layer_stack::attach_all called twice");
    if (mAttached) {
        return core::result<void>::success();
    }

    for (usize index = 0; index < mLayers.size(); ++index) {
        core::result<void> attached = mLayers[index]->on_attach(context);
        if (attached.has_value()) {
            continue;
        }
        // Unwind: the layers attached so far detach top-down, the failing one never attached.
        for (usize undo = index; undo > 0; --undo) {
            mLayers[undo - 1]->on_detach(context);
        }
        return make_error(
          engine_error::LAYER_ATTACH_FAILED,
          std::format("layer '{}' failed to attach: {}", mLayers[index]->name(), attached.get_error().to_string())
        );
    }
    mAttached = true;
    return core::result<void>::success();
}

void layer_stack::detach_all(engine_context &context) {
    mPendingPush.clear();
    mPendingPushIsOverlay.clear();
    mPendingPop.clear();
    if (!mAttached) {
        return;
    }
    for (usize index = mLayers.size(); index > 0; --index) {
        mLayers[index - 1]->on_detach(context);
    }
    mAttached = false;
}

void layer_stack::apply_pending(engine_context &context) {
    CONJURE_ASSERT(!mIterating, "layer_stack::apply_pending inside an iteration");
    if (mIterating) {
        return;
    }
    if (mPendingPop.empty() && mPendingPush.empty()) {
        // The common frame: nothing queued, and the moves below would cost three blocks under MSVC's debug
        // iterators (a moved-to vector allocates its container proxy).
        return;
    }

    std::vector<layer *> pops = std::move(mPendingPop);
    mPendingPop.clear();
    for (layer *target : pops) {
        erase(*target, mAttached ? &context : nullptr);
    }

    std::vector<std::unique_ptr<layer>> pushes = std::move(mPendingPush);
    std::vector<bool> overlays = std::move(mPendingPushIsOverlay);
    mPendingPush.clear();
    mPendingPushIsOverlay.clear();
    for (usize index = 0; index < pushes.size(); ++index) {
        std::unique_ptr<layer> value = std::move(pushes[index]);
        if (mAttached) {
            core::result<void> attached = value->on_attach(context);
            if (!attached.has_value()) {
                CONJURE_LOG_ERROR(
                  LOG_ENGINE,
                  "layer '{}' failed to attach and was dropped: {}",
                  value->name(),
                  attached.get_error().to_string()
                );
                continue;
            }
        }
        insert(std::move(value), overlays[index]);
    }
}

void layer_stack::frame_begin(engine_context &context) {
    for_each_bottom_up([&context](layer &current) {
        current.on_frame_begin(context);
    });
}

void layer_stack::update(engine_context &context) {
    for_each_bottom_up([&context](layer &current) {
        current.on_update(context);
    });
}

void layer_stack::imgui(engine_context &context) {
    for_each_bottom_up([&context](layer &current) {
        current.on_imgui(context);
    });
}

void layer_stack::imgui_end(engine_context &context) {
    for_each_bottom_up([&context](layer &current) {
        current.on_imgui_end(context);
    });
}

void layer_stack::frame_end(engine_context &context) {
    for_each_bottom_up([&context](layer &current) {
        current.on_frame_end(context);
    });
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

layer *layer_stack::push(std::unique_ptr<layer> value, bool overlay) {
    CONJURE_ASSERT(value != nullptr, "layer_stack: pushing a null layer");
    if (value == nullptr) {
        return nullptr;
    }
    layer *raw = value.get();
    if (mIterating || mAttached) {
        mPendingPush.push_back(std::move(value));
        mPendingPushIsOverlay.push_back(overlay);
        return raw;
    }
    insert(std::move(value), overlay);
    return raw;
}

void layer_stack::insert(std::unique_ptr<layer> value, bool overlay) {
    if (overlay) {
        mLayers.push_back(std::move(value));
        return;
    }
    mLayers.insert(mLayers.begin() + static_cast<isize>(mOverlayBegin), std::move(value));
    ++mOverlayBegin;
}

bool layer_stack::forget_pending_push(layer &target) {
    for (usize index = 0; index < mPendingPush.size(); ++index) {
        if (mPendingPush[index].get() == &target) {
            mPendingPush.erase(mPendingPush.begin() + static_cast<isize>(index));
            mPendingPushIsOverlay.erase(mPendingPushIsOverlay.begin() + static_cast<isize>(index));
            return true;
        }
    }
    return false;
}

void layer_stack::erase(layer &target, engine_context *context) {
    const auto found = std::find_if(mLayers.begin(), mLayers.end(), [&target](const std::unique_ptr<layer> &entry) {
        return entry.get() == &target;
    });
    if (found == mLayers.end()) {
        // Unknown layers (or a pending push that was forgotten meanwhile) are ignored.
        return;
    }
    const usize position = static_cast<usize>(found - mLayers.begin());
    if (mAttached && context != nullptr) {
        target.on_detach(*context);
    }
    mLayers.erase(found);
    if (position < mOverlayBegin) {
        --mOverlayBegin;
    }
}

template <typename Callable>
void layer_stack::for_each_bottom_up(Callable &&callable) {
    const bool wasIterating = mIterating;
    mIterating = true;
    for (usize index = 0; index < mLayers.size(); ++index) {
        layer *current = mLayers[index].get();
        if (std::find(mPendingPop.begin(), mPendingPop.end(), current) != mPendingPop.end()) {
            continue;
        }
        callable(*current);
    }
    mIterating = wasIterating;
}
