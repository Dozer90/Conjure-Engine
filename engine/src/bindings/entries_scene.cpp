#include "bindings/entry_support.h"

#include "conjure/assets/asset_database.h"
#include "conjure/assets/asset_manager.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_manager.h"

#include <optional>
#include <string>
#include <vector>

// scene_* (docs/design/15_bindings.md 2.3): loads and unloads are requests the scene manager applies at
// FRAME_BEGIN (never immediate), paths resolve through asset_manager::find and the loaded_scene_info path
// (or the database's asset path for scenes loaded by uuid) is what the string entries return.

using namespace conjure;
using namespace conjure::bindings;
using namespace conjure::bindings::detail;

namespace {

    [[nodiscard]] scene::scene_manager *scenes() noexcept {
        return active_context().scenes;
    }

    /// <summary>
    /// The asset behind a path argument; records INVALID_ARGUMENT / SCENE_NOT_FOUND / NOT_SUPPORTED.
    /// </summary>
    [[nodiscard]] std::optional<assets::asset_id> resolve_path(
      native_function_id id,
      const char *utf8,
      int32_t length,
      int32_t &status
    ) noexcept {
        const core::result<std::string_view> text = marshal::read_utf8(utf8, length);
        if (!text.has_value()) {
            status = fail_entry(id, text.get_error());
            return std::nullopt;
        }
        if (scenes() == nullptr) {
            status = unavailable(id, "scene manager");
            return std::nullopt;
        }
        const std::optional<assets::asset_id> asset = scenes()->assets().find(*text);
        if (!asset.has_value()) {
            status = fail_entry(id, bindings_error::SCENE_NOT_FOUND, *text);
            return std::nullopt;
        }
        return asset;
    }

    /// <summary>
    /// The path a loaded scene reports: the file path of a load_file, else the database's asset path
    /// of its uuid, else the uuid itself (cooked projects without a path table).
    /// </summary>
    [[nodiscard]] std::string path_of(const scene::loaded_scene_info &info) {
        if (!info.path.empty()) {
            return info.path;
        }
        if (active_context().assetDatabase != nullptr) {
            const std::optional<std::string> assetPath = active_context().assetDatabase->asset_path_of(info.asset);
            if (assetPath.has_value()) {
                return *assetPath;
            }
        }
        return info.asset.to_string();
    }
}

int32_t conjure::bindings::entries::scene_request_load(const char *utf8Path, int32_t length, int32_t additive) {
    constexpr native_function_id ID = native_function_id::scene_request_load;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    int32_t status = 0;
    const std::optional<assets::asset_id> asset = resolve_path(ID, utf8Path, length, status);
    if (!asset.has_value()) {
        return status;
    }
    const scene::load_request_id request =
      scenes()->request_load(*asset, additive != 0 ? scene::load_mode::ADDITIVE : scene::load_mode::REPLACE);
    return request.is_valid() ? 0 : fail_entry(ID, bindings_error::INTERNAL, "request refused");
}

int32_t conjure::bindings::entries::scene_request_load_asset(const conjure_uuid *scene, int32_t additive) {
    constexpr native_function_id ID = native_function_id::scene_request_load_asset;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (scene == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "scene");
    }
    if (scenes() == nullptr) {
        return unavailable(ID, "scene manager");
    }
    const assets::asset_id asset = marshal::to_uuid(*scene);
    if (asset.is_null()) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "null scene uuid");
    }
    const scene::load_request_id request =
      scenes()->request_load(asset, additive != 0 ? scene::load_mode::ADDITIVE : scene::load_mode::REPLACE);
    return request.is_valid() ? 0 : fail_entry(ID, bindings_error::INTERNAL, "request refused");
}

int32_t conjure::bindings::entries::scene_request_unload(const char *utf8Path, int32_t length) {
    constexpr native_function_id ID = native_function_id::scene_request_unload;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    int32_t status = 0;
    const std::optional<assets::asset_id> asset = resolve_path(ID, utf8Path, length, status);
    if (!asset.has_value()) {
        return status;
    }
    const std::optional<scene::scene_id> loaded = scenes()->find_loaded(*asset);
    if (!loaded.has_value()) {
        // A scene that is not loaded has nothing to unload: a no-op, not an error.
        return 0;
    }
    scenes()->request_unload(*loaded);
    return 0;
}

int32_t conjure::bindings::entries::scene_is_loaded(const char *utf8Path, int32_t length) {
    constexpr native_function_id ID = native_function_id::scene_is_loaded;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    const core::result<std::string_view> text = marshal::read_utf8(utf8Path, length);
    if (!text.has_value()) {
        return fail_entry(ID, text.get_error());
    }
    if (scenes() == nullptr) {
        return unavailable(ID, "scene manager");
    }
    const std::optional<assets::asset_id> asset = scenes()->assets().find(*text);
    if (!asset.has_value()) {
        return 0;
    }
    return scenes()->find_loaded(*asset).has_value() ? 1 : 0;
}

int32_t conjure::bindings::entries::scene_is_load_pending(const char *utf8Path, int32_t length) {
    constexpr native_function_id ID = native_function_id::scene_is_load_pending;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    const core::result<std::string_view> text = marshal::read_utf8(utf8Path, length);
    if (!text.has_value()) {
        return fail_entry(ID, text.get_error());
    }
    if (scenes() == nullptr) {
        return unavailable(ID, "scene manager");
    }
    const std::optional<assets::asset_id> asset = scenes()->assets().find(*text);
    if (!asset.has_value()) {
        return 0;
    }
    return scenes()->is_load_pending(*asset) ? 1 : 0;
}

int32_t conjure::bindings::entries::scene_active_path(char *utf8, int32_t capacity) {
    constexpr native_function_id ID = native_function_id::scene_active_path;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (capacity < 0) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "capacity");
    }
    if (scenes() == nullptr) {
        return unavailable(ID, "scene manager");
    }
    const std::vector<scene::loaded_scene_info> loaded = scenes()->loaded_scenes();
    if (loaded.empty()) {
        return 0;
    }
    // The active scene is the first loaded one, Unity's meaning: a REPLACE load unloads everything before it, so
    // this is the scene of the last non-additive load (or the oldest when every load was additive). Additive loads
    // never change it; when it is unloaded the oldest remaining scene takes over.
    return marshal::write_utf8(path_of(loaded.front()), utf8, capacity);
}

int32_t conjure::bindings::entries::scene_loaded_count(void) {
    constexpr native_function_id ID = native_function_id::scene_loaded_count;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (scenes() == nullptr) {
        return unavailable(ID, "scene manager");
    }
    return static_cast<int32_t>(scenes()->loaded_scenes().size());
}

int32_t conjure::bindings::entries::scene_loaded_path(int32_t index, char *utf8, int32_t capacity) {
    constexpr native_function_id ID = native_function_id::scene_loaded_path;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (capacity < 0) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "capacity");
    }
    if (scenes() == nullptr) {
        return unavailable(ID, "scene manager");
    }
    const std::vector<scene::loaded_scene_info> loaded = scenes()->loaded_scenes();
    if (index < 0 || static_cast<usize>(index) >= loaded.size()) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "index");
    }
    return marshal::write_utf8(path_of(loaded[static_cast<usize>(index)]), utf8, capacity);
}

int32_t conjure::bindings::entries::scene_entity_count(void) {
    constexpr native_function_id ID = native_function_id::scene_entity_count;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (active_world() == nullptr) {
        return unavailable(ID, "world");
    }
    return static_cast<int32_t>(active_world()->stats().aliveEntities);
}

uint64_t conjure::bindings::entries::scene_structure_version(void) {
    constexpr native_function_id ID = native_function_id::scene_structure_version;
    CONJURE_BINDINGS_GUARD_ENTITY(ID);
    if (active_scene() == nullptr) {
        static_cast<void>(unavailable(ID, "scene"));
        return 0;
    }
    return active_scene()->structure_version();
}

int32_t conjure::bindings::entries::scene_roots(uint64_t *out, int32_t capacity) {
    constexpr native_function_id ID = native_function_id::scene_roots;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (capacity < 0) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "capacity");
    }
    if (active_scene() == nullptr) {
        return unavailable(ID, "scene");
    }
    std::vector<ecs::entity> &roots = scratch_entities();
    for (const scene::root_entry &root : active_scene()->roots()) {
        roots.push_back(root.entity);
    }
    return marshal::write_entities(roots, out, capacity);
}
