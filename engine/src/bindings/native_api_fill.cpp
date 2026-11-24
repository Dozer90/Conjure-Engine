#include "conjure/bindings/native_api_entries.h"

#include "bindings/entry_support.h"

#include "conjure/bindings/log_category.h"

#include "conjure/core/assert.h"
#include "conjure/core/jobs/main_thread.h"
#include "conjure/core/log.h"
#include "conjure/engine/engine_context.h"
#include "conjure/modules/module_manager.h"

#include <algorithm>
#include <array>
#include <bitset>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Filling the table (docs/design/15_bindings.md 3.1 steps 3-5, B3, B6): the active context, the per-group
// real-or-stub decision, the counting-thunk wrap and the slot inspection helpers. Everything is generated
// over CONJURE_BINDINGS_FOR_EACH_ENTRY (owned entries) and CONJURE_NATIVE_API_FOR_EACH_ENTRY_TYPED (every
// entry) so a new .inl line is handled without a code change here.

using namespace conjure;
using namespace conjure::bindings;

namespace {

    binding_context sActiveContext = {};
    std::bitset<NATIVE_FUNCTION_COUNT> sLoggedThisFrame = {};
    std::vector<ecs::entity> sScratchEntities = {};
    std::vector<detail::scene_fixup> sPendingFixups = {};
    std::vector<detail::scene_fixup> sApplyingScratch = {};     // empty outside apply_pending_fixups
    bool sApplyingFixups = false;
    void (*sQueryEndTarget)(uint32_t) = nullptr;

    /// <summary>
    /// Installed over the query_end slot by wrap_with_counters: once the scope closed and the runtime flushed
    /// deferred_commands(), the scene-level fixups the locked entries queued are applied (3.3). sQueryEndTarget
    /// is the slot it covers (the raw entry, or its counting thunk outside Distribution).
    /// </summary>
    void query_end_interposer(uint32_t scopeId) {
        if (sQueryEndTarget != nullptr) {
            sQueryEndTarget(scopeId);
        }
        static_cast<void>(detail::apply_pending_fixups());
    }

    struct fill_report {
        u32 overridden = 0;
        u32 kept = 0;
        std::string missing = {};
    };

    /// <summary>
    /// Installs the real implementation or the stub into one slot (3.1 step 3). A foreign pointer (a
    /// plugin's native_binding_desc) is overridden with a WARN when the group is compiled in and kept
    /// when it is compiled out; our own pointers (real, stub, or a thunk over either) count as "ours".
    /// </summary>
    template <native_function_id Id, typename Ret, typename... Args>
    void fill_slot(
      Ret (*&slot)(Args...),
      Ret (*real)(Args...),
      bool available,
      bool compiled,
      fill_report &report
    ) noexcept {
        using stub = stubs::stub_thunk<Id, Ret(Args...)>;
        using thunk = counting_thunk<Id, Ret(Args...)>;
        Ret (*const target)(Args...) = (available && real != nullptr) ? real : &stub::invoke;
        Ret (*current)(Args...) = slot;
        if (current == &thunk::invoke) {
            current = thunk::sTarget;
        }
        const bool ours = current == nullptr || current == real || current == &stub::invoke;
        if (!ours) {
            if (!compiled) {
                ++report.kept;
                return;
            }
            CONJURE_LOG_WARN(
              LOG_BINDINGS,
              "slot '{}' was already filled (plugin native_binding_desc); the bindings override it (B3)",
              function_name(Id)
            );
            ++report.overridden;
        }
        slot = target;
    }

    template <native_function_id Id, typename Ret, typename... Args>
    void wrap_slot(Ret (*&slot)(Args...)) noexcept {
        using thunk = counting_thunk<Id, Ret(Args...)>;
        if (slot == nullptr || slot == &thunk::invoke) {
            return;
        }
        thunk::sTarget = slot;
        slot = &thunk::invoke;
    }

    template <native_function_id Id, typename Ret, typename... Args>
    [[nodiscard]] Ret (*unwrapped(Ret (*slot)(Args...)) noexcept)(Args...) {
        using thunk = counting_thunk<Id, Ret(Args...)>;
        return slot == &thunk::invoke ? thunk::sTarget : slot;
    }

    [[nodiscard]] bool pointers_needed(std::string_view group, const binding_context &context) noexcept {
        if (group == "transform" || group == "entity" || group == "scene" || group == "prefab") {
            return context.world != nullptr && context.scenes != nullptr;
        }
        if (group == "camera") {
            // No GPU and no renderer instance: the camera entries read components and the screen.
            return context.world != nullptr && context.scenes != nullptr;
        }
        if (group == "physics") {
            return context.physics != nullptr && context.world != nullptr;
        }
        if (group == "audio") {
            return context.audio != nullptr && context.world != nullptr;
        }
        if (group == "input") {
            return context.input != nullptr || context.window != nullptr;
        }
        if (group == "asset") {
            return context.assets != nullptr;
        }
        if (group == "diagnostics") {
            return true;
        }
        if (group == "modules") {
            return context.modules != nullptr;
        }
        return false;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Context

binding_context binding_context::from(const engine::engine_context &context) noexcept {
    binding_context result = {};
    result.world = context.world;
    result.components = context.components;
    result.scenes = context.scenes;
    result.physics = context.physics;
    result.audio = context.audio;
    result.assets = context.assets;
    result.assetDatabase = context.assetDatabase;
    result.input = context.input;
    result.window = context.window;
    result.renderer = context.renderer;
    result.checks = context.checks;
    result.modules = context.modules != nullptr ? &context.modules->registry() : nullptr;
    result.scripting = context.scripting;
    result.stats = context.stats;
    result.time = context.time;
    return result;
}

const binding_context &conjure::bindings::active_context() noexcept {
    return sActiveContext;
}

void conjure::bindings::clear_active_context() noexcept {
    sActiveContext = {};
    sPendingFixups.clear();
}

bool conjure::bindings::group_available(std::string_view group, const binding_context &context) noexcept {
    return group_compiled(group) && pointers_needed(group, context);
}

bool conjure::bindings::group_compiled(std::string_view group) noexcept {
    if (group == "physics") {
        return CONJURE_BINDINGS_GROUP_COMPILED(physics) == 1;
    }
    if (group == "audio") {
        return CONJURE_BINDINGS_GROUP_COMPILED(audio) == 1;
    }
    if (group == "camera") {
        return CONJURE_BINDINGS_GROUP_COMPILED(camera) == 1;
    }
    return group == "transform" || group == "entity" || group == "scene" || group == "prefab" || group == "input"
      || group == "asset" || group == "diagnostics" || group == "modules";
}


/*===========================================================================*/
/*===========================================================================*/
// Filling and wrapping

core::result<void> conjure::bindings::fill_native_table(conjure_native_api &api, const binding_context &context) {
    sActiveContext = context;
    fill_report report = {};

// The compiled-out groups never name their entries:: functions (the file that defines them is not built).
#define CONJURE_BINDINGS_CONCAT_(a, b) a##b
#define CONJURE_BINDINGS_CONCAT(a, b) CONJURE_BINDINGS_CONCAT_(a, b)
#define CONJURE_BINDINGS_FILL_1(group, name, ret, args)                                                   \
    fill_slot<native_function_id::group##_##name>(                                                        \
      api.group##_##name,                                                                                 \
      &entries::group##_##name,                                                                           \
      group_available(#group, context),                                                                   \
      true,                                                                                               \
      report                                                                                              \
    );
#define CONJURE_BINDINGS_FILL_0(group, name, ret, args)                                                   \
    fill_slot<native_function_id::group##_##name>(api.group##_##name, nullptr, false, false, report);
#define CONJURE_BINDINGS_FILL(group, name, ret, args)                                                     \
    CONJURE_BINDINGS_CONCAT(CONJURE_BINDINGS_FILL_, CONJURE_BINDINGS_GROUP_COMPILED(group))(group, name, ret, args)
    CONJURE_BINDINGS_FOR_EACH_ENTRY(CONJURE_BINDINGS_FILL)
#undef CONJURE_BINDINGS_FILL
#undef CONJURE_BINDINGS_FILL_0
#undef CONJURE_BINDINGS_FILL_1
#undef CONJURE_BINDINGS_CONCAT
#undef CONJURE_BINDINGS_CONCAT_

    // Step 5: every owned slot is filled. Only a broken build (an entries_<group>.cpp left out of the
    // module) can leave one null; the message names each missing entry.
#define CONJURE_BINDINGS_VERIFY(group, name, ret, args)                                                   \
    if (api.group##_##name == nullptr) {                                                                  \
        if (!report.missing.empty()) {                                                                    \
            report.missing += ", ";                                                                       \
        }                                                                                                 \
        report.missing += #group "_" #name;                                                               \
    }
    CONJURE_BINDINGS_FOR_EACH_ENTRY(CONJURE_BINDINGS_VERIFY)
#undef CONJURE_BINDINGS_VERIFY

    if (!report.missing.empty()) {
        return core::make_error(
          bindings_error::TABLE_INCOMPLETE,
          "bindings-owned slots left null after fill: {}",
          report.missing
        );
    }
    CONJURE_LOG_DEBUG(
      LOG_BINDINGS,
      "native table filled: {} owned entries ({} plugin slots overridden, {} kept)",
      BINDINGS_ENTRY_COUNT,
      report.overridden,
      report.kept
    );
    return core::result<void>::success();
}

void conjure::bindings::wrap_with_counters(conjure_native_api &api) noexcept {
    // The interposer is peeled off query_end first so the generic pass wraps the slot it covers (the raw entry
    // or its thunk) and never the interposer itself: wrapping the interposer would make the thunk and the
    // interposer forward to each other and the first query_end call recurse without end.
    if (api.query_end == &query_end_interposer) {
        api.query_end = sQueryEndTarget;
    }
#if !CONJURE_DISTRIBUTION
#define CONJURE_BINDINGS_WRAP(group, name, ret, args) wrap_slot<native_function_id::group##_##name>(api.group##_##name);
    CONJURE_NATIVE_API_FOR_EACH_ENTRY_TYPED(CONJURE_BINDINGS_WRAP)
#undef CONJURE_BINDINGS_WRAP
#endif
    // The scope-end hook (every configuration): the locked entries' scene fixups run when query_end unlocks.
    if (api.query_end != nullptr) {
        sQueryEndTarget = api.query_end;
        api.query_end = &query_end_interposer;
    }
}

bool conjure::bindings::is_scope_hook_installed(const conjure_native_api &api) noexcept {
    return api.query_end == &query_end_interposer;
}


/*===========================================================================*/
/*===========================================================================*/
// Slot inspection

bool conjure::bindings::is_stub_installed(const conjure_native_api &api, native_function_id id) noexcept {
    switch (id) {
#define CONJURE_BINDINGS_CASE(group, name, ret, args)                                                     \
        case native_function_id::group##_##name:                                                          \
            return unwrapped<native_function_id::group##_##name>(api.group##_##name)                      \
              == &stubs::stub_thunk<native_function_id::group##_##name, ret args>::invoke;
        CONJURE_BINDINGS_FOR_EACH_ENTRY(CONJURE_BINDINGS_CASE)
#undef CONJURE_BINDINGS_CASE
        default:
            return false;
    }
}

bool conjure::bindings::is_implementation_installed(const conjure_native_api &api, native_function_id id) noexcept {
    switch (id) {
#define CONJURE_BINDINGS_CONCAT_(a, b) a##b
#define CONJURE_BINDINGS_CONCAT(a, b) CONJURE_BINDINGS_CONCAT_(a, b)
#define CONJURE_BINDINGS_CASE_1(group, name, ret, args)                                                   \
        case native_function_id::group##_##name:                                                          \
            return unwrapped<native_function_id::group##_##name>(api.group##_##name) == &entries::group##_##name;
#define CONJURE_BINDINGS_CASE_0(group, name, ret, args)                                                   \
        case native_function_id::group##_##name:                                                          \
            return false;
#define CONJURE_BINDINGS_CASE(group, name, ret, args)                                                     \
    CONJURE_BINDINGS_CONCAT(CONJURE_BINDINGS_CASE_, CONJURE_BINDINGS_GROUP_COMPILED(group))(group, name, ret, args)
        CONJURE_BINDINGS_FOR_EACH_ENTRY(CONJURE_BINDINGS_CASE)
#undef CONJURE_BINDINGS_CASE
#undef CONJURE_BINDINGS_CASE_0
#undef CONJURE_BINDINGS_CASE_1
#undef CONJURE_BINDINGS_CONCAT
#undef CONJURE_BINDINGS_CONCAT_
        default:
            return false;
    }
}

bool conjure::bindings::is_counting_installed(const conjure_native_api &api, native_function_id id) noexcept {
    if (id == native_function_id::query_end && api.query_end == &query_end_interposer) {
        return sQueryEndTarget == &counting_thunk<native_function_id::query_end, void(uint32_t)>::invoke;
    }
    switch (id) {
#define CONJURE_BINDINGS_CASE(group, name, ret, args)                                                     \
        case native_function_id::group##_##name:                                                          \
            return api.group##_##name == &counting_thunk<native_function_id::group##_##name, ret args>::invoke;
        CONJURE_NATIVE_API_FOR_EACH_ENTRY_TYPED(CONJURE_BINDINGS_CASE)
#undef CONJURE_BINDINGS_CASE
        default:
            return false;
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Stubs and failure reporting

int32_t conjure::bindings::stubs::report_not_supported(native_function_id id) noexcept {
    return detail::fail_entry(id, bindings_error::NOT_SUPPORTED, "subsystem unavailable");
}

int32_t conjure::bindings::detail::fail_entry(
  native_function_id id,
  bindings_error code,
  std::string_view detail
) noexcept {
    const int32_t status = fail(code, function_name(id), detail);
#if !CONJURE_DISTRIBUTION
    const u32 index = static_cast<u32>(id);
    if (index < NATIVE_FUNCTION_COUNT && !sLoggedThisFrame.test(index)) {
        sLoggedThisFrame.set(index);
        CONJURE_LOG_DEBUG(LOG_BINDINGS, "{} (first failure of this entry this frame)", last_error_message());
    }
#endif
    return status;
}

int32_t conjure::bindings::detail::fail_entry(native_function_id id, const core::error &failure) noexcept {
    const int32_t status = fail(failure, function_name(id));
#if !CONJURE_DISTRIBUTION
    const u32 index = static_cast<u32>(id);
    if (index < NATIVE_FUNCTION_COUNT && !sLoggedThisFrame.test(index)) {
        sLoggedThisFrame.set(index);
        CONJURE_LOG_DEBUG(LOG_BINDINGS, "{} (first failure of this entry this frame)", last_error_message());
    }
#endif
    return status;
}

void conjure::bindings::detail::clear_frame_failure_log() noexcept {
    sLoggedThisFrame.reset();
}

bool conjure::bindings::detail::failure_logged_this_frame(native_function_id id) noexcept {
    const u32 index = static_cast<u32>(id);
    return index < NATIVE_FUNCTION_COUNT && sLoggedThisFrame.test(index);
}


/*===========================================================================*/
/*===========================================================================*/
// Entry support (entry_support.h)

bool conjure::bindings::detail::check_main_thread(native_function_id id) noexcept {
    if (core::jobs::is_main_thread()) {
        // A force-closed scope flushed without passing query_end: catch up before the entry runs.
        if (!sPendingFixups.empty()) {
            static_cast<void>(apply_pending_fixups());
        }
        return true;
    }
    // The last-error buffer and the per-frame log bits are plain statics the main thread owns (5): the one path
    // that runs elsewhere must not touch them, so the violation is only asserted and logged (the log is
    // thread-safe) and the guard macro returns the WRONG_THREAD status by itself.
    CONJURE_ASSERT(false, "native entry '{}' called off the main thread", function_name(id));
    CONJURE_LOG_ERROR(LOG_BINDINGS, "native entry '{}' called off the main thread (WRONG_THREAD)", function_name(id));
    return false;
}

scene::scene *conjure::bindings::detail::active_scene() noexcept {
    if (sActiveContext.scenes == nullptr) {
        return nullptr;
    }
    return &sActiveContext.scenes->active();
}

ecs::world *conjure::bindings::detail::active_world() noexcept {
    return sActiveContext.world;
}

ecs::entity conjure::bindings::detail::resolve_alive(native_function_id id, uint64_t packed, int32_t &status) noexcept {
    if (sActiveContext.world == nullptr) {
        status = unavailable(id, "world");
        return ecs::NULL_ENTITY;
    }
    const ecs::entity entity = marshal::to_entity(packed);
    if (entity.is_null() || !sActiveContext.world->is_alive(entity)) {
        status = fail_entry(id, bindings_error::ENTITY_NOT_ALIVE);
        return ecs::NULL_ENTITY;
    }
    return entity;
}

ecs::component_id conjure::bindings::detail::component_id_of(std::string_view registeredName) noexcept {
    if (sActiveContext.world == nullptr) {
        return ecs::component_id{};
    }
    // from_hash: a constexpr-hashed id never touches the interning table on the hot path.
    return sActiveContext.world->registry().find(core::string_id::from_hash(core::hash::fnv1a64(registeredName)));
}

std::vector<ecs::entity> &conjure::bindings::detail::scratch_entities() noexcept {
    sScratchEntities.clear();
    return sScratchEntities;
}

marshal::structural_guard conjure::bindings::detail::structural() noexcept {
    marshal::structural_guard guard = {};
    guard.world = sActiveContext.world;
    guard.runtime = sActiveContext.scripting;
    return guard;
}

void conjure::bindings::detail::queue_fixup(const scene_fixup &fixup) {
    sPendingFixups.push_back(fixup);
}

u32 conjure::bindings::detail::apply_pending_fixups() noexcept {
    const bool blocked = sApplyingFixups || sPendingFixups.empty() || sActiveContext.world == nullptr
      || sActiveContext.world->is_locked() || sActiveContext.scenes == nullptr;
    if (blocked) {
        return 0;
    }
    sApplyingFixups = true;
    scene::scene &active = sActiveContext.scenes->active();
    ecs::world &world = *sActiveContext.world;
    u32 applied = 0;
    // The queue is swapped with a scratch queue first: a scene operation may fire observers that reach back into
    // the bindings and queue more. Swapping (instead of moving into a local) keeps both capacities between scopes.
    std::vector<scene_fixup> &pending = sApplyingScratch;
    pending.clear();
    std::swap(pending, sPendingFixups);
    for (const scene_fixup &fixup : pending) {
        if (!world.is_alive(fixup.entity)) {
            CONJURE_LOG_DEBUG(
              LOG_BINDINGS, "deferred scene fixup skipped: entity {} died before the flush", fixup.entity
            );
            continue;
        }
        core::result<void> outcome = core::result<void>::success();
        switch (fixup.operation) {
            case scene_fixup::kind::ATTACH: {
                const scene::reparent_mode mode =
                  fixup.keepWorld ? scene::reparent_mode::KEEP_WORLD : scene::reparent_mode::KEEP_LOCAL;
                if (!fixup.parent.is_null() && !world.is_alive(fixup.parent)) {
                    outcome = active.set_parent(fixup.entity, ecs::NULL_ENTITY, mode, fixup.index);
                } else {
                    outcome = active.set_parent(fixup.entity, fixup.parent, mode, fixup.index);
                }
                break;
            }
            case scene_fixup::kind::SET_ENABLED:
                outcome = active.set_enabled(fixup.entity, fixup.enabled);
                break;
        }
        if (!outcome.has_value()) {
            // The request returned success when it was queued, so the refusal (a cycle, the depth limit) is the
            // only trace the script gets: visible at WARN, unlike the dead-entity skip above.
            CONJURE_LOG_WARN(
              LOG_BINDINGS,
              "deferred scene fixup on entity {} refused by the scene: {} (the request already returned success)",
              fixup.entity,
              outcome.get_error().to_string()
            );
            continue;
        }
        ++applied;
    }
    pending.clear();
    sApplyingFixups = false;
    return applied;
}

usize conjure::bindings::detail::pending_fixup_count() noexcept {
    return sPendingFixups.size();
}

int32_t conjure::bindings::detail::unavailable(native_function_id id, std::string_view subsystem) noexcept {
    // The message names the subsystem; the stub form says "subsystem unavailable" so both read alike.
    std::array<char, 64> buffer = {};
    const auto result =
      std::format_to_n(buffer.data(), static_cast<isize>(buffer.size()) - 1, "{} unavailable", subsystem);
    const usize length = std::min(static_cast<usize>(result.size), buffer.size() - 1);
    return fail_entry(id, bindings_error::NOT_SUPPORTED, std::string_view(buffer.data(), length));
}
