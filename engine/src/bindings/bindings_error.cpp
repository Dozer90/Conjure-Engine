#include "conjure/bindings/bindings_error.h"

#include "conjure/assets/assets_error.h"
#include "conjure/core/platform_detect.h"
#include "conjure/ecs/ecs_error.h"
#include "conjure/platform/platform_error.h"
#include "conjure/scene/scene_error.h"

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    #include "conjure/physics/physics_error.h"
#endif

#include <algorithm>
#include <array>
#include <cstring>
#include <format>

using namespace conjure;
using namespace conjure::bindings;

namespace {

    /// <summary>
    /// The last-error state (15_bindings.md 2.2): one fixed buffer and a code, main-thread only.
    /// </summary>
    struct last_error_state {
        std::array<char, LAST_ERROR_CAPACITY> text = {};
        usize length = 0;
        bindings_error code = bindings_error::NONE;
    };

    last_error_state sLastError = {};

    constexpr std::array<std::string_view, BINDINGS_ERROR_COUNT> STATUS_NAMES = {
#define CONJURE_BINDINGS_STATUS_NAME(name) #name,
        CONJURE_BINDINGS_FOR_EACH_STATUS(CONJURE_BINDINGS_STATUS_NAME)
#undef CONJURE_BINDINGS_STATUS_NAME
    };

    void record(bindings_error code, std::string_view entry, std::string_view detail) noexcept {
        sLastError.code = code;
        // format_to_n writes at most capacity - 1 bytes and reports the full length, so the message is
        // truncated at 511 bytes and stays terminated whatever the inputs were.
        const usize capacity = sLastError.text.size() - 1;
        const auto result = detail.empty()
          ? std::format_to_n(sLastError.text.data(), static_cast<isize>(capacity), "{}", entry)
          : std::format_to_n(sLastError.text.data(), static_cast<isize>(capacity), "{}: {}", entry, detail);
        sLastError.length = std::min(static_cast<usize>(result.size), capacity);
        sLastError.text[sLastError.length] = '\0';
    }

    [[nodiscard]] bindings_error map_core_code(core::error_code code) noexcept {
        switch (code) {
            case core::error_code::INVALID_ARGUMENT:
            case core::error_code::OUT_OF_RANGE:
                return bindings_error::INVALID_ARGUMENT;
            case core::error_code::NOT_FOUND:
                return bindings_error::NOT_FOUND;
            case core::error_code::UNSUPPORTED:
            case core::error_code::NOT_IMPLEMENTED:
                return bindings_error::NOT_SUPPORTED;
            default:
                return bindings_error::INTERNAL;
        }
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Names and mapping

std::string_view conjure::bindings::bindings_error_name(bindings_error code) noexcept {
    const u32 index = static_cast<u32>(code);
    if (index >= BINDINGS_ERROR_COUNT) {
        return "";
    }
    return STATUS_NAMES[index];
}

bindings_error conjure::bindings::map_error(const core::error &failure) noexcept {
    if (failure.is_category<bindings_error>()) {
        // Our own helpers (marshal::read_utf8) already speak in status codes.
        return from_status(-static_cast<int32_t>(failure.code()));
    }
    if (failure.is(ecs::ecs_error::ENTITY_NOT_ALIVE)) {
        return bindings_error::ENTITY_NOT_ALIVE;
    }
    if (failure.is(ecs::ecs_error::STRUCTURE_LOCKED)) {
        return bindings_error::STRUCTURE_LOCKED;
    }
    if (failure.is(ecs::ecs_error::WRONG_THREAD)) {
        return bindings_error::WRONG_THREAD;
    }
    if (failure.is(ecs::ecs_error::COMPONENT_NOT_PRESENT)) {
        return bindings_error::COMPONENT_MISSING;
    }
    if (failure.is(assets::asset_error_code::NOT_FOUND)) {
        return bindings_error::NOT_FOUND;
    }
    if (failure.is(assets::asset_error_code::TYPE_MISMATCH)) {
        return bindings_error::ASSET_TYPE_MISMATCH;
    }
    if (failure.is(scene::scene_error::INVALID_ENTITY)) {
        return bindings_error::ENTITY_NOT_ALIVE;
    }
    if (failure.is(scene::scene_error::NO_TRANSFORM)) {
        return bindings_error::COMPONENT_MISSING;
    }
    if (failure.is(scene::scene_error::SCENE_NOT_LOADED)) {
        return bindings_error::SCENE_NOT_FOUND;
    }
    if (failure.is(scene::scene_error::PREFAB_NOT_LOADED)) {
        return bindings_error::ASSET_NOT_LOADED;
    }
    if (failure.is(scene::scene_error::GUID_NOT_FOUND)) {
        return bindings_error::NOT_FOUND;
    }
    if (failure.is(scene::scene_error::PREFAB_MULTIPLE_ROOTS) || failure.is(scene::scene_error::BAD_FORMAT)) {
        return bindings_error::PREFAB_INVALID;
    }
    // A hierarchy request refused for its arguments (self, a descendant, too deep, a bad sibling index) is
    // the caller's mistake: ArgumentException in C#, not a generic ConjureNativeException (6).
    const bool badHierarchyArgument = failure.is(scene::scene_error::SELF_PARENT)
      || failure.is(scene::scene_error::HIERARCHY_CYCLE) || failure.is(scene::scene_error::HIERARCHY_TOO_DEEP)
      || failure.is(scene::scene_error::INVALID_SIBLING_INDEX);
    if (badHierarchyArgument) {
        return bindings_error::INVALID_ARGUMENT;
    }
    if (failure.is(platform::platform_error::CLIPBOARD_UNAVAILABLE)) {
        return bindings_error::NOT_FOUND;
    }
    if (failure.is(platform::platform_error::FEATURE_UNAVAILABLE)) {
        return bindings_error::NOT_SUPPORTED;
    }
#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    if (failure.is(physics::physics_error::NO_BODY) || failure.is(physics::physics_error::NOT_A_CHARACTER)) {
        return bindings_error::COMPONENT_MISSING;
    }
    // A body command the entity's kind of body cannot take (a force on a static body, move_kinematic on a
    // dynamic one) is refused for what the caller passed: ArgumentException in C#, like the hierarchy requests.
    if (failure.is(physics::physics_error::WRONG_BODY_KIND)) {
        return bindings_error::INVALID_ARGUMENT;
    }
    if (failure.is(physics::physics_error::WRONG_THREAD)) {
        return bindings_error::WRONG_THREAD;
    }
    if (failure.is(physics::physics_error::UNKNOWN_LAYER)) {
        return bindings_error::NOT_FOUND;
    }
#endif
    if (failure.is_category<core::error_code>()) {
        return map_core_code(failure.generic_code());
    }
    return bindings_error::INTERNAL;
}


/*===========================================================================*/
/*===========================================================================*/
// Recording and reading

int32_t conjure::bindings::fail(bindings_error code, std::string_view entry, std::string_view detail) noexcept {
    record(code, entry, detail);
    return to_status(code);
}

int32_t conjure::bindings::fail(const core::error &failure, std::string_view entry) noexcept {
    const bindings_error code = map_error(failure);
    // The message is built here so the hot path never allocates: to_string() would, so the category and
    // code names are written straight into the buffer and the module's message follows.
    const std::string &message = failure.message();
    const usize capacity = sLastError.text.size() - 1;
    const auto result = message.empty()
      ? std::format_to_n(
          sLastError.text.data(),
          static_cast<isize>(capacity),
          "{}: {}.{}",
          entry,
          failure.category_name(),
          failure.code_name()
        )
      : std::format_to_n(
          sLastError.text.data(),
          static_cast<isize>(capacity),
          "{}: {}.{}: {}",
          entry,
          failure.category_name(),
          failure.code_name(),
          message
        );
    sLastError.code = code;
    sLastError.length = std::min(static_cast<usize>(result.size), capacity);
    sLastError.text[sLastError.length] = '\0';
    return to_status(code);
}

bindings_error conjure::bindings::last_error_code() noexcept {
    return sLastError.code;
}

std::string_view conjure::bindings::last_error_message() noexcept {
    return std::string_view(sLastError.text.data(), sLastError.length);
}

void conjure::bindings::clear_last_error() noexcept {
    sLastError.code = bindings_error::NONE;
    sLastError.length = 0;
    sLastError.text[0] = '\0';
}
