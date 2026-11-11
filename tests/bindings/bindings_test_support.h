#pragma once

// Shared fixtures of the bindings tests (docs/design/15_bindings.md 7, 10): bare_fixture wires a world with
// the scene, physics and audio components, an asset manager over a memory source, a scene_manager with its
// four systems, the null window and an input accumulator, the engine's check registry and frame stats, a
// script_runtime over the null host and (on request) a physics world and a null-backend audio engine into a
// binding_context, fills a native table from it and wraps the slots with counting thunks; headless_app wraps
// engine::application with the bindings subsystem added. Everything is headless and deterministic.

#include "conjure/testing/doctest.h"

#include "conjure/testing/test_environment.h"

#include "conjure/assets/asset_importer.h"
#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_source.h"
#include "conjure/assets/asset_types.h"
#include "conjure/bindings/bindings_subsystem.h"
#include "conjure/bindings/call_counters.h"
#include "conjure/bindings/native_api_entries.h"
#include "conjure/core/jobs/job_system.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/scheduler.h"
#include "conjure/ecs/system_phase.h"
#include "conjure/ecs/world.h"
#include "conjure/engine/application.h"
#include "conjure/engine/diagnostics/check_registry.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_time.h"
#include "conjure/engine/frame_stats.h"
#include "conjure/platform/input.h"
#include "conjure/platform/window.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_manager.h"
#include "conjure/scripting/native_api.h"
#include "conjure/scripting/null_script_host.h"
#include "conjure/scripting/script_runtime.h"

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    #include "conjure/physics/physics_world.h"
#endif
#if defined(CONJURE_WITH_AUDIO) && CONJURE_WITH_AUDIO
    #include "conjure/audio/audio_engine.h"
#endif

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace bindings_test {

    /// <summary>
    /// A deterministic uuid for tests (never null).
    /// </summary>
    [[nodiscard]] conjure::core::uuid test_uuid(conjure::u64 index) noexcept;

    /// <summary>
    /// The conjure_uuid of a core::uuid.
    /// </summary>
    [[nodiscard]] conjure_uuid uuid_of(const conjure::core::uuid &value) noexcept;

    /// <summary>
    /// A UTF-8 string argument pair for the entries taking (const char *, int32_t).
    /// </summary>
    struct utf8_argument {
        const char *data = nullptr;
        std::int32_t length = 0;
    };

    [[nodiscard]] utf8_argument utf8(std::string_view text) noexcept;

    /// <summary>
    /// Reads a string through the needed-length protocol of an entry taking (char *, int32_t).
    /// </summary>
    template <typename Reader>
    [[nodiscard]] std::string read_string(Reader &&reader) {
        std::string text(256, '\0');
        const std::int32_t needed = reader(text.data(), static_cast<std::int32_t>(text.size()));
        if (needed < 0) {
            return {};
        }
        if (static_cast<std::size_t>(needed) > text.size()) {
            text.resize(static_cast<std::size_t>(needed));
            const std::int32_t again = reader(text.data(), needed);
            REQUIRE(again == needed);
        }
        text.resize(static_cast<std::size_t>(needed));
        return text;
    }

    /// <summary>
    /// Which optional services bare_fixture creates.
    /// </summary>
    struct bare_fixture_options {
        bool scripting = true;      // a script_runtime over the null host (deferred_commands, phases)
        bool physics = false;       // a physics_world attached to the world (CONJURE_WITH_PHYSICS)
        bool audio = false;         // a null-backend audio_engine (CONJURE_WITH_AUDIO)
        bool window = true;         // the null window and an input accumulator
        bool fill = true;           // fill the table and wrap it in the constructor
    };

    /// <summary>
    /// The 0.1 s mono 48 kHz WAV the audio cases cook into a clip asset.
    /// </summary>
    [[nodiscard]] std::vector<std::byte> make_test_wav();

    class bare_fixture {
        public:
            conjure::ecs::component_registry registry = {};
            conjure::ecs::world world;                                   // no default: built on the registry
            conjure::core::jobs::job_system jobs = {};                   // never started: everything inline
            conjure::assets::memory_asset_source *source = nullptr;      // owned by the manager
            conjure::assets::asset_manager assets;                       // no default: built from a desc
            conjure::assets::importer_registry importers = {};
            conjure::scene::scene_manager scenes;                        // no default: bound to world and assets
            conjure::ecs::scheduler scheduler;                           // no default: bound to the world
            conjure::ecs::frame_info frame = {};
            std::unique_ptr<conjure::platform::window> window = nullptr;
            conjure::platform::input input = {};
            conjure::engine::diagnostics::check_registry checks = {};
            conjure::engine::frame_stats stats = {};
            conjure::engine::engine_time time = {};
#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
            std::unique_ptr<conjure::physics::physics_world> physics = nullptr;
#endif
#if defined(CONJURE_WITH_AUDIO) && CONJURE_WITH_AUDIO
            std::unique_ptr<conjure::audio::audio_engine> audio = nullptr;
#endif
            std::unique_ptr<conjure::scripting::script_runtime> runtime = nullptr;
            conjure_native_api standaloneApi = {};                       // the table without a runtime
            conjure::bindings::script_residency residency = {};
            conjure::bindings::binding_context context = {};

        public:
            // Implicit on purpose: fixtures are written `bare_fixture f = {};`.
            bare_fixture(const bare_fixture_options &options = {});
            ~bare_fixture();

            bare_fixture(const bare_fixture &) = delete;
            bare_fixture &operator=(const bare_fixture &) = delete;

        public:
            /// <summary>
            /// The table the entries were installed into (the runtime's when scripting is on).
            /// </summary>
            [[nodiscard]] conjure_native_api &api() noexcept;

            [[nodiscard]] conjure::scene::scene &active() noexcept {
                return scenes.active();
            }

            /// <summary>
            /// The null host behind the runtime (REQUIREs scripting).
            /// </summary>
            [[nodiscard]] conjure::scripting::null_script_host &null_host() noexcept;

            /// <summary>
            /// fill_native_table + wrap_with_counters over 'context' and installs the residency.
            /// </summary>
            void fill();

            /// <summary>
            /// A scene entity (name interned) under 'parent'; REQUIREs success.
            /// </summary>
            [[nodiscard]] conjure::ecs::entity create(
              std::string_view name,
              conjure::ecs::entity parent = conjure::ecs::NULL_ENTITY
            );

            /// <summary>
            /// Runs FRAME_BEGIN, TRANSFORM and FRAME_END through the scene systems 'frames' times.
            /// </summary>
            void step(conjure::u32 frames = 1);

            /// <summary>
            /// Adds a cooked blob of 'type' with 'payload' bytes under an optional asset path.
            /// </summary>
            void add_blob(
              const conjure::assets::asset_id &id,
              conjure::assets::asset_type_id type,
              std::span<const std::byte> payload,
              std::string_view assetPath = {}
            );

            /// <summary>
            /// Registers the scene asset types and publishes a prefab built from a subtree; returns its uuid.
            /// </summary>
            [[nodiscard]] conjure::assets::asset_id publish_prefab(
              conjure::ecs::entity root,
              std::string_view name,
              conjure::u64 index,
              std::string_view assetPath = {}
            );

            /// <summary>
            /// Publishes a scene asset with 'rootCount' named roots; returns its uuid.
            /// </summary>
            [[nodiscard]] conjure::assets::asset_id publish_scene(
              std::string_view name,
              conjure::u32 rootCount,
              conjure::u64 index,
              std::string_view assetPath
            );

            /// <summary>
            /// Registers the audio clip asset type and publishes the test WAV as a clip; returns its uuid.
            /// </summary>
            [[nodiscard]] conjure::assets::asset_id publish_clip(conjure::u64 index, std::string_view assetPath = {});

            /// <summary>
            /// Loads an asset synchronously (REQUIREs LOADED).
            /// </summary>
            void load_now(const conjure::assets::asset_id &id);

            /// <summary>
            /// Registers the scene asset types once (scene, prefab, their importers).
            /// </summary>
            void register_scene_asset_types();

        private:
            bool mSceneTypesRegistered = false;
            bool mAudioTypesRegistered = false;

            [[nodiscard]] conjure::assets::asset_manager_desc make_asset_desc();
    };

    /// <summary>
    /// engine_config::headless_defaults() with physics and audio enabled, scripting on the null host, every
    /// job inline and no engine plugins.
    /// </summary>
    [[nodiscard]] conjure::engine::engine_config headless_config();

    /// <summary>
    /// An application over 'config' with a scripting_bindings_subsystem added; not initialised.
    /// </summary>
    [[nodiscard]] std::unique_ptr<conjure::engine::application> make_app(const conjure::engine::engine_config &config);

    /// <summary>
    /// The bindings subsystem of an initialised application, or nullptr.
    /// </summary>
    [[nodiscard]] conjure::bindings::scripting_bindings_subsystem *find_bindings(conjure::engine::application &app);

    /// <summary>
    /// Directory holding Conjure.Bootstrap.* and Conjure.Managed.dll; empty when the SDK was not built.
    /// </summary>
    [[nodiscard]] std::filesystem::path managed_directory();

    /// <summary>
    /// Conjure.TestScripts.dll of the v1 variant; empty when the SDK was not built.
    /// </summary>
    [[nodiscard]] std::filesystem::path test_scripts_assembly();

    /// <summary>
    /// docs/generated/native_api_manifest.json.
    /// </summary>
    [[nodiscard]] std::filesystem::path manifest_path();
}
