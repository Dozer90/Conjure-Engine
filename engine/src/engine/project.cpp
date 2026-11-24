#include "conjure/engine/project.h"

#include "conjure/engine/engine_error.h"
#include "conjure/engine/engine_version.h"
#include "conjure/engine/log_category.h"

#include "conjure/core/json/value.h"
#include "conjure/core/log.h"
#include "conjure/core/string_utils.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <system_error>
#include <utility>

using namespace conjure;
using namespace conjure::engine;

namespace {

    using core::json::value;

    /// <summary>
    /// The top-level keys the schema knows; everything else is preserved in extensions.
    /// </summary>
    constexpr std::array<std::string_view, 17> KNOWN_KEYS = {
        "schema_version", "name", "version", "engine_version", "assets_root", "cooked_root", "startup_scene",
        "cooked", "modules", "window", "time", "physics", "rendering", "audio", "assets", "scripting", "layers",
    };

    [[nodiscard]] core::error invalid(std::string_view pointer, std::string_view reason) {
        return make_error(engine_error::PROJECT_INVALID, std::format("project.json {}: {}", pointer, reason));
    }

    /// <summary>
    /// Reads an optional member into 'target'; a present member of the wrong type is PROJECT_INVALID.
    /// </summary>
    [[nodiscard]] core::result<void> read_string(const value &object, std::string_view key, std::string &target) {
        const value member = object[key];
        if (!member.is_valid()) {
            return core::result<void>::success();
        }
        if (!member.is_string()) {
            return invalid(key, "expected a string");
        }
        target = std::string(member.as_string());
        return core::result<void>::success();
    }

    [[nodiscard]] core::result<void> read_path(
      const value &object,
      std::string_view key,
      std::filesystem::path &target
    ) {
        std::string text = {};
        const value member = object[key];
        if (!member.is_valid()) {
            return core::result<void>::success();
        }
        CONJURE_TRY(read_string(object, key, text));
        target = std::filesystem::path(text);
        return core::result<void>::success();
    }

    [[nodiscard]] core::result<void> read_bool(const value &object, std::string_view key, bool &target) {
        const value member = object[key];
        if (!member.is_valid()) {
            return core::result<void>::success();
        }
        if (!member.is_bool()) {
            return invalid(key, "expected a boolean");
        }
        target = member.as_bool();
        return core::result<void>::success();
    }

    [[nodiscard]] core::result<void> read_u32(const value &object, std::string_view key, u32 &target) {
        const value member = object[key];
        if (!member.is_valid()) {
            return core::result<void>::success();
        }
        if (!member.is_number() || !member.is_integer()) {
            return invalid(key, "expected a non-negative integer");
        }
        const u64 wide = member.as_u64(std::numeric_limits<u64>::max());
        if (wide > std::numeric_limits<u32>::max()) {
            return invalid(key, "expected a non-negative integer that fits 32 bits");
        }
        target = static_cast<u32>(wide);
        return core::result<void>::success();
    }

    [[nodiscard]] core::result<void> read_f32(const value &object, std::string_view key, f32 &target) {
        const value member = object[key];
        if (!member.is_valid()) {
            return core::result<void>::success();
        }
        if (!member.is_number()) {
            return invalid(key, "expected a number");
        }
        const f32 parsed = member.as_f32(std::numeric_limits<f32>::quiet_NaN());
        if (!std::isfinite(parsed)) {
            return invalid(key, "expected a finite number");
        }
        target = parsed;
        return core::result<void>::success();
    }

    [[nodiscard]] core::result<void> read_vec3(const value &object, std::string_view key, math::vec3 &target) {
        const value member = object[key];
        if (!member.is_valid()) {
            return core::result<void>::success();
        }
        if (!member.is_array() || member.size() != 3) {
            return invalid(key, "expected an array of three numbers");
        }
        for (usize index = 0; index < 3; ++index) {
            const value component = member[index];
            if (!component.is_number()) {
                return invalid(key, "expected an array of three numbers");
            }
            // as_f32's fallback stands for a value outside the f32 range: refuse it, as read_f32 does.
            const f32 parsed = component.as_f32(std::numeric_limits<f32>::quiet_NaN());
            if (!std::isfinite(parsed)) {
                return invalid(key, "expected an array of three finite numbers");
            }
            target[static_cast<glm::length_t>(index)] = parsed;
        }
        return core::result<void>::success();
    }

    [[nodiscard]] core::result<value> read_object(const value &parent, std::string_view key) {
        const value member = parent[key];
        if (member.is_valid() && !member.is_object()) {
            return invalid(key, "expected an object");
        }
        return member;
    }

    [[nodiscard]] core::result<void> read_strings(
      const value &object,
      std::string_view key,
      std::vector<std::string> &target
    ) {
        const value member = object[key];
        if (!member.is_valid()) {
            return core::result<void>::success();
        }
        if (!member.is_array()) {
            return invalid(key, "expected an array of strings");
        }
        target.clear();
        for (const value item : member) {
            if (!item.is_string()) {
                return invalid(key, "expected an array of strings");
            }
            target.emplace_back(item.as_string());
        }
        return core::result<void>::success();
    }

    [[nodiscard]] core::result<void> read_modules(const value &root, modules_settings &settings) {
        CONJURE_TRY_ASSIGN(const value section, read_object(root, "modules"));
        if (!section.is_valid()) {
            return core::result<void>::success();
        }
        std::vector<std::string> paths = {};
        const value searchPaths = section["search_paths"];
        if (searchPaths.is_valid()) {
            CONJURE_TRY(read_strings(section, "search_paths", paths));
            settings.searchPaths.clear();
            for (const std::string &text : paths) {
                settings.searchPaths.emplace_back(text);
            }
        }
        CONJURE_TRY(read_bool(section, "engine_plugins", settings.enginePlugins));
        CONJURE_TRY(read_strings(section, "required", settings.required));
        return core::result<void>::success();
    }

    [[nodiscard]] core::result<void> read_window(const value &root, window_settings &settings) {
        CONJURE_TRY_ASSIGN(const value section, read_object(root, "window"));
        if (!section.is_valid()) {
            return core::result<void>::success();
        }
        CONJURE_TRY(read_string(section, "title", settings.title));
        CONJURE_TRY(read_u32(section, "width", settings.width));
        CONJURE_TRY(read_u32(section, "height", settings.height));
        CONJURE_TRY(read_bool(section, "fullscreen", settings.fullscreen));
        CONJURE_TRY(read_bool(section, "resizable", settings.resizable));
        CONJURE_TRY(read_bool(section, "vsync", settings.vsync));
        return core::result<void>::success();
    }

    [[nodiscard]] core::result<void> read_time(const value &root, time_settings &settings) {
        CONJURE_TRY_ASSIGN(const value section, read_object(root, "time"));
        if (!section.is_valid()) {
            return core::result<void>::success();
        }
        const value fixedHz = section["fixed_hz"];
        if (fixedHz.is_valid()) {
            if (!fixedHz.is_number() || fixedHz.as_f64() <= 0.0) {
                return invalid("time/fixed_hz", "expected a positive number");
            }
            settings.fixedDeltaSeconds = static_cast<f32>(1.0 / fixedHz.as_f64());
        }
        CONJURE_TRY(read_f32(section, "max_frame_delta_seconds", settings.maxFrameDeltaSeconds));
        CONJURE_TRY(read_u32(section, "max_fixed_steps_per_frame", settings.maxFixedStepsPerFrame));
        CONJURE_TRY(read_f32(section, "time_scale", settings.timeScale));
        CONJURE_TRY(read_f32(section, "forced_frame_delta_seconds", settings.forcedFrameDeltaSeconds));
        CONJURE_TRY(read_bool(section, "interpolate_physics", settings.interpolatePhysics));
        return core::result<void>::success();
    }

    [[nodiscard]] core::result<void> read_physics(const value &root, project_description &description) {
        CONJURE_TRY_ASSIGN(const value section, read_object(root, "physics"));
        if (!section.is_valid()) {
            return core::result<void>::success();
        }
        description.physicsSection = core::json::document::from_value(section);
        CONJURE_TRY(read_bool(section, "enabled", description.physics.enabled));
        CONJURE_TRY(read_vec3(section, "gravity", description.physics.gravity));
        CONJURE_TRY(read_u32(section, "max_bodies", description.physics.maxBodies));
        return core::result<void>::success();
    }

    /// <summary>
    /// Reads "gpu_validation": "off", "debug_layer" or "gpu_based". A boolean is the format before code sweep
    /// F458, when true turned on GPU-based validation and false nothing: true reads as GPU_BASED and false as OFF.
    /// </summary>
    [[nodiscard]] core::result<void> read_gpu_validation(const value &section, gpu_validation_level &target) {
        const value member = section["gpu_validation"];
        if (!member.is_valid()) {
            return core::result<void>::success();
        }
        if (member.is_bool()) {
            target = member.as_bool() ? gpu_validation_level::GPU_BASED : gpu_validation_level::OFF;
            return core::result<void>::success();
        }
        if (!member.is_string()) {
            return invalid("rendering/gpu_validation", "expected \"off\", \"debug_layer\" or \"gpu_based\"");
        }
        core::result<gpu_validation_level> parsed = parse_gpu_validation_level(member.as_string());
        if (!parsed.has_value()) {
            return invalid("rendering/gpu_validation", parsed.get_error().message());
        }
        target = parsed.value();
        return core::result<void>::success();
    }

    [[nodiscard]] core::result<void> read_rendering(const value &root, rendering_settings &settings) {
        CONJURE_TRY_ASSIGN(const value section, read_object(root, "rendering"));
        if (!section.is_valid()) {
            return core::result<void>::success();
        }
        CONJURE_TRY(read_bool(section, "enabled", settings.enabled));
        std::string backend = {};
        CONJURE_TRY(read_string(section, "backend", backend));
        if (!backend.empty()) {
            core::result<rhi_backend_choice> parsed = parse_rhi_backend_choice(backend);
            if (!parsed.has_value()) {
                return invalid("rendering/backend", parsed.get_error().message());
            }
            settings.backend = parsed.value();
        }
        CONJURE_TRY(read_gpu_validation(section, settings.gpuValidation));
        return core::result<void>::success();
    }

    [[nodiscard]] core::result<void> read_audio(const value &root, audio_config &settings) {
        CONJURE_TRY_ASSIGN(const value section, read_object(root, "audio"));
        if (!section.is_valid()) {
            return core::result<void>::success();
        }
        CONJURE_TRY(read_bool(section, "enabled", settings.enabled));
        std::string backend = {};
        CONJURE_TRY(read_string(section, "backend", backend));
        if (!backend.empty()) {
            core::result<audio_backend_choice> parsed = parse_audio_backend_choice(backend);
            if (!parsed.has_value()) {
                return invalid("audio/backend", parsed.get_error().message());
            }
            settings.backend = parsed.value();
        }
        CONJURE_TRY(read_u32(section, "sample_rate", settings.sampleRate));
        CONJURE_TRY(read_string(section, "device", settings.deviceName));
        CONJURE_TRY(read_f32(section, "master_gain", settings.masterGain));
        CONJURE_TRY(read_f32(section, "doppler_factor", settings.dopplerFactor));
        CONJURE_TRY(read_f32(section, "speed_of_sound", settings.speedOfSound));
        const value groups = section["groups"];
        if (groups.is_valid()) {
            if (!groups.is_array()) {
                return invalid("audio/groups", "expected an array of objects");
            }
            settings.groups.clear();
            for (const value item : groups) {
                if (!item.is_object()) {
                    return invalid("audio/groups", "expected an array of objects");
                }
                audio_group_setting group = {};
                CONJURE_TRY(read_string(item, "name", group.name));
                CONJURE_TRY(read_string(item, "parent", group.parent));
                CONJURE_TRY(read_f32(item, "gain", group.gain));
                if (group.name.empty()) {
                    return invalid("audio/groups", "every group needs a name");
                }
                settings.groups.push_back(std::move(group));
            }
        }
        return core::result<void>::success();
    }

    [[nodiscard]] core::result<void> read_assets(const value &root, assets_settings &settings) {
        CONJURE_TRY_ASSIGN(const value section, read_object(root, "assets"));
        if (!section.is_valid()) {
            return core::result<void>::success();
        }
        CONJURE_TRY(read_bool(section, "hot_reload", settings.hotReload));
        return core::result<void>::success();
    }

    [[nodiscard]] core::result<void> read_scripting(const value &root, scripting_settings &settings) {
        CONJURE_TRY_ASSIGN(const value section, read_object(root, "scripting"));
        if (!section.is_valid()) {
            return core::result<void>::success();
        }
        CONJURE_TRY(read_bool(section, "enabled", settings.enabled));
        CONJURE_TRY(read_bool(section, "required", settings.required));
        CONJURE_TRY(read_path(section, "assembly", settings.gameAssembly));
        CONJURE_TRY(read_bool(section, "hot_reload", settings.hotReload));
        std::string mode = {};
        CONJURE_TRY(read_string(section, "export_mode", mode));
        if (!mode.empty()) {
            core::result<script_export_mode> parsed = parse_script_export_mode(mode);
            if (!parsed.has_value()) {
                return invalid("scripting/export_mode", parsed.get_error().message());
            }
            settings.exportMode = parsed.value();
        }
        CONJURE_TRY(read_bool(section, "debug_symbols", settings.exportDebugSymbols));
        return core::result<void>::success();
    }

    [[nodiscard]] std::string path_text(const std::filesystem::path &path) {
        return path.generic_string();
    }

    /// <summary>
    /// A float as the JSON number project.json stores: the shortest decimal that reads back as the same f32,
    /// widened through text so 0.1f is written as 0.1 rather than 0.10000000149011612.
    /// </summary>
    [[nodiscard]] f64 number(f32 value) {
        const std::optional<f64> shortest = core::str::parse_double(std::format("{}", value));
        return shortest.value_or(static_cast<f64>(value));
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

project::project() {}

project::~project() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

core::result<project> project::load(const std::filesystem::path &directory) {
    std::error_code ignored = {};
    const std::filesystem::path file = directory / std::filesystem::path(PROJECT_FILE_NAME);
    if (!std::filesystem::is_regular_file(file, ignored)) {
        return make_error(
          engine_error::PROJECT_NOT_FOUND,
          std::format("no {} in '{}'", PROJECT_FILE_NAME, directory.generic_string())
        );
    }

    std::ifstream stream(file, std::ios::binary);
    if (!stream.is_open()) {
        return make_error(engine_error::PROJECT_NOT_FOUND, std::format("cannot open '{}'", file.generic_string()));
    }
    std::ostringstream buffer = {};
    buffer << stream.rdbuf();
    std::filesystem::path root = std::filesystem::absolute(directory, ignored);
    if (root.empty()) {
        root = directory;
    }
    return parse(buffer.str(), root.lexically_normal());
}

core::result<project> project::parse(std::string_view text, std::filesystem::path rootDirectory) {
    core::result<core::json::document> parsed = core::json::document::parse(text);
    if (!parsed.has_value()) {
        return make_error(engine_error::PROJECT_PARSE_FAILED, parsed.get_error().message());
    }
    const core::json::document &document = parsed.value();
    const value root = document.root();
    if (!root.is_object()) {
        return invalid("/", "the root must be an object");
    }

    // A root given with a trailing separator ("C:/games/mygame/") has an empty filename(); drop it so the game
    // module and root_directory() are named after the folder. A bare root ("C:/", "/") stays as it is.
    if (!rootDirectory.has_filename() && rootDirectory.has_relative_path()) {
        rootDirectory = rootDirectory.parent_path();
    }
    project result = {};
    result.mRootDirectory = std::move(rootDirectory);
    project_description &description = result.mDescription;

    CONJURE_TRY(read_u32(root, "schema_version", description.schemaVersion));
    if (description.schemaVersion > PROJECT_SCHEMA_VERSION) {
        return make_error(
          engine_error::PROJECT_SCHEMA_TOO_NEW,
          std::format(
            "schema_version {} is newer than the supported {}",
            description.schemaVersion,
            PROJECT_SCHEMA_VERSION
          )
        );
    }
    CONJURE_TRY(read_string(root, "name", description.name));
    if (description.name.empty()) {
        return invalid("name", "a non-empty project name is required");
    }
    CONJURE_TRY(read_string(root, "version", description.version));
    CONJURE_TRY(read_string(root, "engine_version", description.engineVersion));
    CONJURE_TRY(read_path(root, "assets_root", description.assetsRoot));
    CONJURE_TRY(read_path(root, "cooked_root", description.cookedRoot));
    CONJURE_TRY(read_path(root, "startup_scene", description.startupScene));
    CONJURE_TRY(read_bool(root, "cooked", description.cooked));
    CONJURE_TRY(read_modules(root, description.modules));
    CONJURE_TRY(read_window(root, description.window));
    CONJURE_TRY(read_time(root, description.time));
    CONJURE_TRY(read_physics(root, description));
    CONJURE_TRY(read_rendering(root, description.rendering));
    CONJURE_TRY(read_audio(root, description.audio));
    CONJURE_TRY(read_assets(root, description.assets));
    CONJURE_TRY(read_scripting(root, description.scripting));
    CONJURE_TRY(read_strings(root, "layers", description.layers));
    if (description.layers.size() > 32) {
        return invalid("layers", "at most 32 layer names");
    }

    CONJURE_TRY_ASSIGN(const value extensions, read_object(root, "extensions"));
    description.extensions = core::json::document::object();
    if (extensions.is_valid()) {
        description.extensions = core::json::document::from_value(extensions);
    }

    // Unknown top-level keys: one WARN each, preserved inside extensions so save() keeps them (2.10).
    for (const core::json::member entry : root.members()) {
        if (entry.key == "extensions") {
            continue;
        }
        const bool known = std::find(KNOWN_KEYS.begin(), KNOWN_KEYS.end(), entry.key) != KNOWN_KEYS.end();
        if (known) {
            continue;
        }
        // The explicit extensions member is the canonical place (06 2.10): a stray top-level copy never replaces it.
        if (description.extensions.root()[entry.key].is_valid()) {
            CONJURE_LOG_WARN(
              LOG_ENGINE,
              "project.json: unknown key '{}' is also a member of extensions; the extensions member is kept",
              entry.key
            );
            continue;
        }
        CONJURE_LOG_WARN(LOG_ENGINE, "project.json: unknown key '{}' preserved in extensions", entry.key);
        description.extensions.set(entry.key, entry.item);
    }
    return result;
}

core::result<void> project::save() const {
    const core::json::document document = to_json();
    return document.write_file(file_path(), true);
}

core::json::document project::to_json() const {
    const project_description &desc = mDescription;
    core::json::document root = core::json::document::object();
    root.set("schema_version", desc.schemaVersion);
    root.set("name", desc.name);
    root.set("version", desc.version);
    root.set("engine_version", desc.engineVersion.empty() ? std::string(engine_version_string()) : desc.engineVersion);
    root.set("assets_root", path_text(desc.assetsRoot));
    root.set("cooked_root", path_text(desc.cookedRoot));
    root.set("startup_scene", path_text(desc.startupScene));
    root.set("cooked", desc.cooked);

    core::json::document modules = core::json::document::object();
    core::json::document searchPaths = core::json::document::array();
    for (const std::filesystem::path &path : desc.modules.searchPaths) {
        searchPaths.push_back(path_text(path));
    }
    modules.set("search_paths", searchPaths);
    modules.set("engine_plugins", desc.modules.enginePlugins);
    core::json::document required = core::json::document::array();
    for (const std::string &name : desc.modules.required) {
        required.push_back(name);
    }
    modules.set("required", required);
    root.set("modules", modules);

    core::json::document window = core::json::document::object();
    window.set("title", desc.window.title);
    window.set("width", desc.window.width);
    window.set("height", desc.window.height);
    window.set("fullscreen", desc.window.fullscreen);
    window.set("resizable", desc.window.resizable);
    window.set("vsync", desc.window.vsync);
    root.set("window", window);

    core::json::document time = core::json::document::object();
    time.set("fixed_hz", std::round(1.0 / number(desc.time.fixedDeltaSeconds) * 1000.0) / 1000.0);
    time.set("max_frame_delta_seconds", number(desc.time.maxFrameDeltaSeconds));
    time.set("max_fixed_steps_per_frame", desc.time.maxFixedStepsPerFrame);
    time.set("time_scale", number(desc.time.timeScale));
    time.set("forced_frame_delta_seconds", number(desc.time.forcedFrameDeltaSeconds));
    time.set("interpolate_physics", desc.time.interpolatePhysics);
    root.set("time", time);

    // The raw physics section carries every key physics::physics_settings knows (R53); the engine's
    // summary fields are written over it so the two never disagree.
    core::json::document physics = desc.physicsSection;
    if (!physics.root().is_object()) {
        physics = core::json::document::object();
    }
    physics.set("enabled", desc.physics.enabled);
    core::json::document gravity = core::json::document::array();
    gravity.push_back(number(desc.physics.gravity.x));
    gravity.push_back(number(desc.physics.gravity.y));
    gravity.push_back(number(desc.physics.gravity.z));
    physics.set("gravity", gravity);
    physics.set("max_bodies", desc.physics.maxBodies);
    root.set("physics", physics);

    core::json::document rendering = core::json::document::object();
    rendering.set("enabled", desc.rendering.enabled);
    rendering.set("backend", to_string(desc.rendering.backend));
    rendering.set("gpu_validation", to_string(desc.rendering.gpuValidation));
    root.set("rendering", rendering);

    core::json::document audio = core::json::document::object();
    audio.set("enabled", desc.audio.enabled);
    audio.set("backend", to_string(desc.audio.backend));
    audio.set("sample_rate", desc.audio.sampleRate);
    audio.set("device", desc.audio.deviceName);
    audio.set("master_gain", number(desc.audio.masterGain));
    audio.set("doppler_factor", number(desc.audio.dopplerFactor));
    audio.set("speed_of_sound", number(desc.audio.speedOfSound));
    core::json::document groups = core::json::document::array();
    for (const audio_group_setting &group : desc.audio.groups) {
        core::json::document item = core::json::document::object();
        item.set("name", group.name);
        item.set("parent", group.parent);
        item.set("gain", number(group.gain));
        groups.push_back(item);
    }
    audio.set("groups", groups);
    root.set("audio", audio);

    core::json::document assets = core::json::document::object();
    assets.set("hot_reload", desc.assets.hotReload);
    root.set("assets", assets);

    core::json::document scripting = core::json::document::object();
    scripting.set("enabled", desc.scripting.enabled);
    scripting.set("required", desc.scripting.required);
    scripting.set("assembly", path_text(desc.scripting.gameAssembly));
    scripting.set("hot_reload", desc.scripting.hotReload);
    scripting.set("export_mode", to_string(desc.scripting.exportMode));
    scripting.set("debug_symbols", desc.scripting.exportDebugSymbols);
    root.set("scripting", scripting);

    core::json::document layers = core::json::document::array();
    for (const std::string &name : desc.layers) {
        layers.push_back(name);
    }
    root.set("layers", layers);

    if (desc.extensions.root().is_object()) {
        root.set("extensions", desc.extensions);
    } else {
        root.set("extensions", core::json::document::object());
    }
    return root;
}

std::filesystem::path project::file_path() const {
    return mRootDirectory / std::filesystem::path(PROJECT_FILE_NAME);
}

std::filesystem::path project::assets_directory() const {
    if (mDescription.assetsRoot.is_absolute()) {
        return mDescription.assetsRoot.lexically_normal();
    }
    return (mRootDirectory / mDescription.assetsRoot).lexically_normal();
}

std::filesystem::path project::cooked_directory() const {
    if (mDescription.cookedRoot.is_absolute()) {
        return mDescription.cookedRoot.lexically_normal();
    }
    return (mRootDirectory / mDescription.cookedRoot).lexically_normal();
}

void project::apply_to(engine_config &config) const {
    const project_description &desc = mDescription;
    const bool headless = config.headless;

    config.modules = desc.modules;
    config.time = desc.time;
    config.physics = desc.physics;
    config.scripting.required = desc.scripting.required;
    config.scripting.gameAssembly = desc.scripting.gameAssembly;
    config.scripting.hotReload = desc.scripting.hotReload;
    config.scripting.exportMode = desc.scripting.exportMode;
    config.scripting.exportDebugSymbols = desc.scripting.exportDebugSymbols;
    config.audio.sampleRate = desc.audio.sampleRate;
    config.audio.deviceName = desc.audio.deviceName;
    config.audio.masterGain = desc.audio.masterGain;
    config.audio.dopplerFactor = desc.audio.dopplerFactor;
    config.audio.speedOfSound = desc.audio.speedOfSound;
    config.audio.groups = desc.audio.groups;
    config.audio.enabled = desc.audio.enabled;
    // The project only raises the level: to_json always writes the key, so most saved projects name OFF, and
    // reading that as a choice would switch off the debug layer a Debug build starts with. --gpu-validation,
    // applied after this merge (D9), sets any level for one run (code sweep F458).
    config.rendering.gpuValidation = std::max(desc.rendering.gpuValidation, config.rendering.gpuValidation);
    config.window.title = desc.window.title;
    config.window.fullscreen = desc.window.fullscreen;
    config.window.resizable = desc.window.resizable;
    config.window.vsync = desc.window.vsync;

    if (!headless) {
        // Headless keeps its null devices, its window size, its disabled scripting (--scripting turns it on
        // after this merge, D12) and its disabled asset hot reload (D8; the watcher's stat passes would run on
        // job threads during a headless frame).
        config.window.width = desc.window.width;
        config.window.height = desc.window.height;
        config.rendering.enabled = desc.rendering.enabled;
        config.rendering.backend = desc.rendering.backend;
        config.audio.backend = desc.audio.backend;
        config.assets.hotReload = desc.assets.hotReload;
        config.scripting.enabled = desc.scripting.enabled;
    } else {
        // A headless time setting keeps its forced delta: the project's wall-clock preference is for
        // interactive runs. It stays within the project's frame clamp, which validate() requires and which
        // would have shortened it to the same value anyway.
        const engine_config headlessDefaults = engine_config::headless_defaults();
        config.time.forcedFrameDeltaSeconds =
          std::min(headlessDefaults.time.forcedFrameDeltaSeconds, config.time.maxFrameDeltaSeconds);
    }

    if (config.startupSceneOverride.empty() && !desc.startupScene.empty()) {
        config.startupSceneOverride = desc.startupScene;
    }
}

std::vector<project_module_entry> project::module_manifests() const {
    std::vector<project_module_entry> entries = {};
    std::error_code ignored = {};

    const std::filesystem::path gameManifest = mRootDirectory / "module.json";
    if (std::filesystem::is_regular_file(gameManifest, ignored)) {
        project_module_entry entry = {};
        entry.name = mRootDirectory.filename().generic_string();
        entry.manifestPath = gameManifest;
        entry.required = true;
        entries.push_back(std::move(entry));
    }

    for (const std::filesystem::path &searchPath : mDescription.modules.searchPaths) {
        const std::filesystem::path directory =
          searchPath.is_absolute() ? searchPath : (mRootDirectory / searchPath).lexically_normal();
        if (!std::filesystem::is_directory(directory, ignored)) {
            continue;
        }
        // An explicit iterator: the range-for increments with the throwing operator++ (CONVENTIONS 3), so an
        // unreadable entry would throw out of a function that returns no error.
        std::vector<std::filesystem::path> children = {};
        std::filesystem::directory_iterator walker(directory, ignored);
        const std::filesystem::directory_iterator end = {};
        while (walker != end) {
            std::error_code status = {};
            if (walker->is_directory(status)) {
                children.push_back(walker->path());
            }
            walker.increment(ignored);
            if (ignored) {
                break;
            }
        }
        std::sort(children.begin(), children.end());
        for (const std::filesystem::path &child : children) {
            const std::filesystem::path manifest = child / "module.json";
            if (!std::filesystem::is_regular_file(manifest, ignored)) {
                continue;
            }
            project_module_entry entry = {};
            entry.name = child.filename().generic_string();
            entry.manifestPath = manifest;
            const std::vector<std::string> &required = mDescription.modules.required;
            entry.required = std::find(required.begin(), required.end(), entry.name) != required.end();
            entries.push_back(std::move(entry));
        }
    }
    return entries;
}
