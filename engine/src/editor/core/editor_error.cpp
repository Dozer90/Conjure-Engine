#include "conjure/editor/editor_error.h"

#include "conjure/assets/assets_error.h"
#include "conjure/ecs/ecs_error.h"
#include "conjure/engine/engine_error.h"
#include "conjure/scene/scene_error.h"

#include <array>

namespace conjure::editor {

    namespace {
        constexpr std::array<std::string_view, static_cast<usize>(protocol_error_code::COUNT)> NAMES = {
            "NONE",
            "PARSE_ERROR",
            "INVALID_REQUEST",
            "MESSAGE_TOO_LARGE",
            "UNKNOWN_COMMAND",
            "INVALID_ARGS",
            "ENTITY_NOT_FOUND",
            "COMPONENT_NOT_FOUND",
            "TYPE_NOT_FOUND",
            "PATH_NOT_FOUND",
            "ASSET_NOT_FOUND",
            "PROJECT_NOT_OPEN",
            "SCENE_NOT_OPEN",
            "WRONG_PLAY_STATE",
            "NOT_AVAILABLE",
            "IO_ERROR",
            "COMPILE_FAILED",
            "BUSY",
            "TIMEOUT",
            "CANCELLED",
            "UNAUTHORISED",
            "INTERNAL",
        };

        constexpr std::array<protocol_error_code, static_cast<usize>(protocol_error_code::COUNT) - 1> ALL_CODES = {
            protocol_error_code::PARSE_ERROR,
            protocol_error_code::INVALID_REQUEST,
            protocol_error_code::MESSAGE_TOO_LARGE,
            protocol_error_code::UNKNOWN_COMMAND,
            protocol_error_code::INVALID_ARGS,
            protocol_error_code::ENTITY_NOT_FOUND,
            protocol_error_code::COMPONENT_NOT_FOUND,
            protocol_error_code::TYPE_NOT_FOUND,
            protocol_error_code::PATH_NOT_FOUND,
            protocol_error_code::ASSET_NOT_FOUND,
            protocol_error_code::PROJECT_NOT_OPEN,
            protocol_error_code::SCENE_NOT_OPEN,
            protocol_error_code::WRONG_PLAY_STATE,
            protocol_error_code::NOT_AVAILABLE,
            protocol_error_code::IO_ERROR,
            protocol_error_code::COMPILE_FAILED,
            protocol_error_code::BUSY,
            protocol_error_code::TIMEOUT,
            protocol_error_code::CANCELLED,
            protocol_error_code::UNAUTHORISED,
            protocol_error_code::INTERNAL,
        };
    }

    std::string_view to_string(protocol_error_code code) noexcept {
        const usize index = static_cast<usize>(code);
        if (index >= NAMES.size()) {
            return "";
        }
        return NAMES[index];
    }

    std::string_view editor_error_name(protocol_error_code code) noexcept {
        return to_string(code);
    }

    std::optional<protocol_error_code> parse_error_code(std::string_view text) noexcept {
        for (usize index = 1; index < NAMES.size(); ++index) {
            if (NAMES[index] == text) {
                return static_cast<protocol_error_code>(index);
            }
        }
        return std::nullopt;
    }

    std::span<const protocol_error_code> all_error_codes() noexcept {
        return ALL_CODES;
    }

    protocol_error_code classify_error(const core::error &failure) noexcept {
        if (failure.is_category<protocol_error_code>()) {
            const protocol_error_code code = static_cast<protocol_error_code>(failure.code());
            if (static_cast<usize>(code) < static_cast<usize>(protocol_error_code::COUNT) &&
              code != protocol_error_code::NONE) {
                return code;
            }
            return protocol_error_code::INTERNAL;
        }
        if (failure.is_category<scene::scene_error>()) {
            if (failure.is(scene::scene_error::GUID_NOT_FOUND) || failure.is(scene::scene_error::INVALID_ENTITY)) {
                return protocol_error_code::ENTITY_NOT_FOUND;
            }
            if (failure.is(scene::scene_error::PREFAB_NOT_LOADED)) {
                return protocol_error_code::ASSET_NOT_FOUND;
            }
            if (failure.is(scene::scene_error::SELF_PARENT) || failure.is(scene::scene_error::HIERARCHY_CYCLE) ||
              failure.is(scene::scene_error::HIERARCHY_TOO_DEEP) || failure.is(scene::scene_error::NO_TRANSFORM) ||
              failure.is(scene::scene_error::DUPLICATE_GUID) || failure.is(scene::scene_error::BAD_FORMAT) ||
              failure.is(scene::scene_error::NOT_A_PREFAB_INSTANCE) ||
              failure.is(scene::scene_error::INVALID_SIBLING_INDEX) ||
              failure.is(scene::scene_error::PREFAB_MULTIPLE_ROOTS) ||
              failure.is(scene::scene_error::UNSUPPORTED_VERSION)) {
                return protocol_error_code::INVALID_ARGS;
            }
            if (failure.is(scene::scene_error::LOAD_FAILED) || failure.is(scene::scene_error::SCENE_NOT_LOADED)) {
                return protocol_error_code::IO_ERROR;
            }
            return protocol_error_code::INTERNAL;
        }
        if (failure.is_category<ecs::ecs_error>()) {
            if (failure.is(ecs::ecs_error::ENTITY_NOT_ALIVE)) {
                return protocol_error_code::ENTITY_NOT_FOUND;
            }
            if (failure.is(ecs::ecs_error::COMPONENT_NOT_PRESENT)) {
                return protocol_error_code::COMPONENT_NOT_FOUND;
            }
            if (failure.is(ecs::ecs_error::UNKNOWN_COMPONENT)) {
                return protocol_error_code::TYPE_NOT_FOUND;
            }
            if (failure.is(ecs::ecs_error::COMPONENT_ALREADY_PRESENT) ||
              failure.is(ecs::ecs_error::SERIALIZATION_FAILED) || failure.is(ecs::ecs_error::ARCHIVE_CORRUPT)) {
                return protocol_error_code::INVALID_ARGS;
            }
            return protocol_error_code::INTERNAL;
        }
        if (failure.is_category<assets::asset_error_code>()) {
            if (failure.is(assets::asset_error_code::NOT_FOUND) || failure.is(assets::asset_error_code::CACHE_MISS)) {
                return protocol_error_code::ASSET_NOT_FOUND;
            }
            if (failure.is(assets::asset_error_code::IO_ERROR)) {
                return protocol_error_code::IO_ERROR;
            }
            if (
              failure.is(assets::asset_error_code::NO_IMPORTER) || failure.is(assets::asset_error_code::TYPE_UNKNOWN)
            ) {
                return protocol_error_code::TYPE_NOT_FOUND;
            }
            if (failure.is(assets::asset_error_code::IMPORT_FAILED) ||
              failure.is(assets::asset_error_code::META_INVALID) ||
              failure.is(assets::asset_error_code::TYPE_MISMATCH) ||
              failure.is(assets::asset_error_code::LOAD_FAILED)) {
                return protocol_error_code::INVALID_ARGS;
            }
            if (failure.is(assets::asset_error_code::TIMEOUT)) {
                return protocol_error_code::TIMEOUT;
            }
            return protocol_error_code::INTERNAL;
        }
        if (failure.is_category<engine::engine_error>()) {
            if (failure.is(engine::engine_error::PROJECT_NOT_FOUND)) {
                return protocol_error_code::ASSET_NOT_FOUND;
            }
            if (failure.is(engine::engine_error::PROJECT_PARSE_FAILED) ||
              failure.is(engine::engine_error::PROJECT_INVALID) ||
              failure.is(engine::engine_error::PROJECT_SCHEMA_TOO_NEW) ||
              failure.is(engine::engine_error::BAD_ARGUMENTS) || failure.is(engine::engine_error::INVALID_CONFIG)) {
                return protocol_error_code::INVALID_ARGS;
            }
            if (failure.is(engine::engine_error::EXPORT_FAILED)) {
                return protocol_error_code::IO_ERROR;
            }
            return protocol_error_code::INTERNAL;
        }
        switch (failure.generic_code()) {
            case core::error_code::INVALID_ARGUMENT:
            case core::error_code::OUT_OF_RANGE:
            case core::error_code::PARSE_FAILURE:
            case core::error_code::ALREADY_EXISTS:
                return protocol_error_code::INVALID_ARGS;
            case core::error_code::NOT_FOUND:
                return protocol_error_code::PATH_NOT_FOUND;
            case core::error_code::IO_FAILURE:
            case core::error_code::PERMISSION_DENIED:
                return protocol_error_code::IO_ERROR;
            case core::error_code::UNSUPPORTED:
            case core::error_code::NOT_IMPLEMENTED:
            case core::error_code::BACKEND_FAILURE:
                return protocol_error_code::NOT_AVAILABLE;
            case core::error_code::TIMEOUT:
                return protocol_error_code::TIMEOUT;
            case core::error_code::CANCELLED:
                return protocol_error_code::CANCELLED;
            case core::error_code::INVALID_STATE:
            case core::error_code::NONE:
            case core::error_code::UNKNOWN:
            case core::error_code::OUT_OF_MEMORY:
            case core::error_code::DEPENDENCY_FAILURE:
            case core::error_code::COUNT:
                break;
        }
        return protocol_error_code::INTERNAL;
    }
}
