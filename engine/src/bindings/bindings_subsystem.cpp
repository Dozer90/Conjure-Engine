#include "conjure/bindings/bindings_subsystem.h"

#include "bindings/entry_support.h"

#include "conjure/bindings/call_counters.h"
#include "conjure/bindings/log_category.h"
#include "conjure/bindings/marshal.h"

#include "conjure/core/log.h"
#include "conjure/scene/scene_manager.h"
#include "conjure/scripting/native_api.h"
#include "conjure/scripting/script_runtime.h"

#include <span>
#include <utility>

using namespace conjure;
using namespace conjure::bindings;

namespace {

    script_residency *sActiveResidency = nullptr;
}

/*===========================================================================*/
/*===========================================================================*/
// script_residency

script_residency::script_residency() {}

script_residency::~script_residency() {}

bool script_residency::hold(const core::uuid &id, assets::asset_handle handle) {
    if (id.is_null() || !handle.is_bound()) {
        return false;
    }
    if (mHandles.contains(id)) {
        return true;
    }
    mHandles.emplace(id, std::move(handle));
    return true;
}

bool script_residency::release(const core::uuid &id) noexcept {
    return mHandles.erase(id);
}

bool script_residency::holds(const core::uuid &id) const noexcept {
    return mHandles.contains(id);
}

void script_residency::clear() noexcept {
    mHandles.clear();
}

usize script_residency::size() const noexcept {
    return mHandles.size();
}


/*===========================================================================*/
/*===========================================================================*/
// scripting_bindings_subsystem

scripting_bindings_subsystem::scripting_bindings_subsystem() {}

scripting_bindings_subsystem::~scripting_bindings_subsystem() {
    if (sActiveResidency == &mResidency) {
        sActiveResidency = nullptr;
    }
}

std::string_view scripting_bindings_subsystem::name() const noexcept {
    return "bindings";
}

core::result<void> scripting_bindings_subsystem::startup(engine::engine_context &context) {
    mFilled = false;
    if (context.scripting == nullptr) {
        CONJURE_LOG_INFO(LOG_BINDINGS, "scripting disabled: no native table to fill");
        return core::result<void>::success();
    }
    if (!marshal::quat_storage_is_xyzw()) {
        return core::make_error(
          bindings_error::INTERNAL,
          "math::quat is not stored x, y, z, w; transforms cannot be marshalled by memcpy (R58)"
        );
    }

    mContext = binding_context::from(context);
    conjure_native_api &api = context.scripting->native_api();
    CONJURE_TRY(fill_native_table(api, mContext));
    wrap_with_counters(api);

    // Step 5: no slot the bindings own may be null now; the core ones are scripting's business at step 17.
    std::span<void *const> slots = scripting::native_api_slots(api);
    std::string missing = {};
    for (u32 index = static_cast<u32>(CONJURE_NATIVE_API_CORE_ENTRY_COUNT); index < NATIVE_FUNCTION_COUNT; ++index) {
        if (slots[index] == nullptr) {
            if (!missing.empty()) {
                missing += ", ";
            }
            missing += function_name(static_cast<native_function_id>(index));
        }
    }
    if (!missing.empty()) {
        return core::make_error(bindings_error::TABLE_INCOMPLETE, "null bindings slots after startup: {}", missing);
    }

    // Residency scripts hold (B9) ends with the scene that asked for it.
    script_residency *residency = &mResidency;
    if (context.scenes != nullptr) {
        mUnloadSubscription = context.scenes->on_before_unload().subscribe(
          [residency](scene::scene_id, std::span<const ecs::entity>) {
              residency->clear();
          }
        );
    }
    sActiveResidency = &mResidency;
    mFilled = true;
    CONJURE_LOG_INFO(
      LOG_BINDINGS, "{} native entries filled ({} bindings-owned)", NATIVE_FUNCTION_COUNT, BINDINGS_ENTRY_COUNT
    );
    return core::result<void>::success();
}

core::result<void> scripting_bindings_subsystem::post_startup(engine::engine_context &context) {
    if (!mFilled || context.scripting == nullptr) {
        return core::result<void>::success();
    }
    // The core entries exist only once script_runtime::initialise ran (scripting's post_startup, step 17,
    // which precedes ours: user subsystems come after the built-in ones).
    wrap_with_counters(context.scripting->native_api());
    return core::result<void>::success();
}

void scripting_bindings_subsystem::begin_frame(engine::engine_context &context) {
    if (!mFilled) {
        return;
    }
    if (context.scripting != nullptr && !is_scope_hook_installed(context.scripting->native_api())) {
        // script_runtime::initialise refills the core slots: a runtime re-initialised after shutdown() has
        // lost the query_end hook and the core counting thunks until they are installed again here.
        wrap_with_counters(context.scripting->native_api());
    }
    clear_last_error();
    detail::clear_frame_failure_log();
    static_cast<void>(detail::apply_pending_fixups());
}

void scripting_bindings_subsystem::shutdown(engine::engine_context &context) {
    CONJURE_UNUSED(context);
    mUnloadSubscription = {};
    mResidency.clear();
    if (sActiveResidency == &mResidency) {
        sActiveResidency = nullptr;
    }
    if (mFilled) {
        clear_active_context();
    }
    mContext = {};
    mFilled = false;
}

script_residency *scripting_bindings_subsystem::active_residency() noexcept {
    return sActiveResidency;
}

void scripting_bindings_subsystem::set_active_residency(script_residency *residency) noexcept {
    sActiveResidency = residency;
}
