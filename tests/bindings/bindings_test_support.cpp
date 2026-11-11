#include "bindings_test_support.h"

#include "conjure/assets/cooked_blob.h"
#include "conjure/bindings/bindings_error.h"
#include "conjure/bindings/marshal.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/string_id.h"
#include "conjure/scene/components.h"
#include "conjure/scene/prefab.h"
#include "conjure/scene/register.h"
#include "conjure/scene/scene_serializer.h"
#include "conjure/scripting/script_component.h"
#include "conjure/scripting/script_host.h"
#include "engine/subsystems/audio_components.h"

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    #include "conjure/physics/physics_settings.h"
    #include "conjure/physics/register.h"
#endif
#if defined(CONJURE_WITH_AUDIO) && CONJURE_WITH_AUDIO
    #include "conjure/audio/clip_asset.h"
#endif

#include <cstring>
#include <utility>

#ifndef CONJURE_BINDINGS_HAS_DOTNET
    #define CONJURE_BINDINGS_HAS_DOTNET 0
#endif
#ifndef CONJURE_BINDINGS_MANAGED_DIR
    #define CONJURE_BINDINGS_MANAGED_DIR ""
#endif
#ifndef CONJURE_BINDINGS_TEST_SCRIPTS_DIR
    #define CONJURE_BINDINGS_TEST_SCRIPTS_DIR ""
#endif
#ifndef CONJURE_BINDINGS_MANIFEST_PATH
    #define CONJURE_BINDINGS_MANIFEST_PATH ""
#endif

using namespace conjure;
using namespace conjure::bindings;

namespace {

    /// <summary>
    /// The DOTNET_RUNTIME precondition of this executable: the SDK was built with this tree and nethost
    /// finds a runtime (the probe never starts the CLR).
    /// </summary>
    bool dotnet_runtime_available() noexcept {
        static const bool available = []() noexcept {
            if (CONJURE_BINDINGS_HAS_DOTNET == 0) {
                return false;
            }
            std::error_code probe = {};
            const bool sdkPresent =
              std::filesystem::is_regular_file(bindings_test::managed_directory() / "Conjure.Managed.dll", probe);
            const bool scriptsPresent = std::filesystem::is_regular_file(bindings_test::test_scripts_assembly(), probe);
            return sdkPresent && scriptsPresent && scripting::is_runtime_available();
        }();
        return available;
    }

    struct probe_installer {
        probe_installer() {
            testing::set_precondition_probe(testing::precondition::DOTNET_RUNTIME, &dotnet_runtime_available);
        }
    };

    const probe_installer sProbeInstaller = {};

    void append_u32(std::vector<std::byte> &out, u32 value) {
        for (u32 shift = 0; shift < 32; shift += 8) {
            out.push_back(static_cast<std::byte>((value >> shift) & 0xFFu));
        }
    }

    void append_u16(std::vector<std::byte> &out, u16 value) {
        out.push_back(static_cast<std::byte>(value & 0xFFu));
        out.push_back(static_cast<std::byte>((value >> 8) & 0xFFu));
    }

    void append_tag(std::vector<std::byte> &out, std::string_view tag) {
        for (const char character : tag) {
            out.push_back(static_cast<std::byte>(character));
        }
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Free functions

core::uuid bindings_test::test_uuid(u64 index) noexcept {
    return core::uuid(0xB1AD1A6500000000ull + index, 0x2000 + index);
}

conjure_uuid bindings_test::uuid_of(const core::uuid &value) noexcept {
    return marshal::from_uuid(value);
}

bindings_test::utf8_argument bindings_test::utf8(std::string_view text) noexcept {
    utf8_argument argument = {};
    argument.data = text.data();
    argument.length = static_cast<int32_t>(text.size());
    return argument;
}

std::vector<std::byte> bindings_test::make_test_wav() {
    // RIFF + fmt + data, 16-bit PCM, mono, 48 kHz, 4800 frames (0.1 s) of a 440 Hz sine.
    constexpr u32 SAMPLE_RATE = 48000;
    constexpr u32 FRAMES = 4800;
    constexpr u32 DATA_BYTES = FRAMES * 2;
    std::vector<std::byte> bytes = {};
    bytes.reserve(44 + DATA_BYTES);
    append_tag(bytes, "RIFF");
    append_u32(bytes, 36 + DATA_BYTES);
    append_tag(bytes, "WAVE");
    append_tag(bytes, "fmt ");
    append_u32(bytes, 16);
    append_u16(bytes, 1);                      // PCM
    append_u16(bytes, 1);                      // mono
    append_u32(bytes, SAMPLE_RATE);
    append_u32(bytes, SAMPLE_RATE * 2);        // byte rate
    append_u16(bytes, 2);                      // block align
    append_u16(bytes, 16);                     // bits per sample
    append_tag(bytes, "data");
    append_u32(bytes, DATA_BYTES);
    for (u32 frame = 0; frame < FRAMES; ++frame) {
        const f64 phase = static_cast<f64>(frame) * 440.0 * 2.0 * 3.14159265358979323846 / SAMPLE_RATE;
        const i16 sample = static_cast<i16>(std::sin(phase) * 16000.0);
        append_u16(bytes, static_cast<u16>(sample));
    }
    return bytes;
}

engine::engine_config bindings_test::headless_config() {
    engine::engine_config config = engine::engine_config::headless_defaults();
    config.logLevel = core::log_level::WARN;
    config.audio.enabled = true;
    config.physics.enabled = true;
    config.scripting.enabled = true;
    config.scripting.required = true;
    config.scripting.useNullHost = true;
    config.scripting.hotReload = false;
    config.modules.enginePlugins = false;
    config.jobs.workerCount = 0;
    return config;
}

std::unique_ptr<engine::application> bindings_test::make_app(const engine::engine_config &config) {
    engine::application_desc desc = {};
    desc.config = config;
    std::unique_ptr<engine::application> app = std::make_unique<engine::application>(std::move(desc));
    app->add_subsystem(std::make_unique<scripting_bindings_subsystem>());
    return app;
}

scripting_bindings_subsystem *bindings_test::find_bindings(engine::application &app) {
    for (const std::unique_ptr<engine::engine_subsystem> &subsystem : app.subsystems()) {
        if (subsystem->name() == "bindings") {
            return dynamic_cast<scripting_bindings_subsystem *>(subsystem.get());
        }
    }
    return nullptr;
}

std::filesystem::path bindings_test::managed_directory() {
    return std::filesystem::path(CONJURE_BINDINGS_MANAGED_DIR);
}

std::filesystem::path bindings_test::test_scripts_assembly() {
    const std::filesystem::path root(CONJURE_BINDINGS_TEST_SCRIPTS_DIR);
    if (root.empty()) {
        return {};
    }
    return root / "v1" / "Conjure.TestScripts.dll";
}

std::filesystem::path bindings_test::manifest_path() {
    return std::filesystem::path(CONJURE_BINDINGS_MANIFEST_PATH);
}


/*===========================================================================*/
/*===========================================================================*/
// bare_fixture

namespace {

    const ecs::component_registry &prepare_registry(
      ecs::component_registry &registry,
      const bindings_test::bare_fixture_options &options
    ) {
        CONJURE_REQUIRE_OK(scene::register_components(registry));
#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
        CONJURE_REQUIRE_OK(physics::register_components(registry));
#else
        CONJURE_UNUSED(options);
#endif
#if defined(CONJURE_WITH_AUDIO) && CONJURE_WITH_AUDIO
        CONJURE_REQUIRE_OK(registry.register_component<audio::audio_source>());
        CONJURE_REQUIRE_OK(registry.register_component<audio::audio_listener>());
#endif
        if (options.scripting) {
            CONJURE_REQUIRE_OK(scripting::register_script_component(registry));
        }
        return registry;
    }

    [[nodiscard]] ecs::scheduler_desc make_scheduler_desc() noexcept {
        ecs::scheduler_desc desc = {};
        desc.parallelSystems = false;
        desc.parallelChunks = false;
        return desc;
    }
}

bindings_test::bare_fixture::bare_fixture(const bare_fixture_options &options) :
    world(prepare_registry(registry, options)),
    assets(make_asset_desc()),
    scenes(world, assets),
    scheduler(world, nullptr, make_scheduler_desc()) {
    // Counters, the last error and the rate-limit bits are process-wide: every case starts from zero.
    reset_call_counters();
    clear_last_error();
    bindings::detail::clear_frame_failure_log();
    CONJURE_REQUIRE_OK(scene::register_systems(scheduler, scenes));
    CONJURE_REQUIRE_OK(scheduler.finalise());
    frame.deltaTime = 1.0f / 60.0f;
    frame.fixedDeltaTime = 1.0f / 60.0f;
    standaloneApi = scripting::make_native_api(nullptr);

    if (options.window) {
        platform::window_desc desc = {};
        desc.backend = platform::backend_kind::NULL_BACKEND;
        desc.width = 1280;
        desc.height = 720;
        core::result<std::unique_ptr<platform::window>> created = platform::create_window(desc);
        CONJURE_REQUIRE_OK(created);
        window = std::move(created).value();
    }
#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    if (options.physics) {
        physics::physics_settings settings = {};
        settings.maxBodies = 1024;
        settings.maxBodyPairs = 4096;
        settings.maxContactConstraints = 2048;
        settings.tempAllocatorBytes = 4 * MIB;
        settings.maxJobs = 256;
        settings.maxContactEventsPerStep = 1024;
        settings.maxQueuedCommands = 256;
        settings.maxCharacters = 8;
        const core::result<u8> player = settings.layers.add_layer(core::string_id::intern("player"));
        CONJURE_REQUIRE_OK(player);
        settings.layers.set_collides(player.value(), player.value(), false);
        physics = std::make_unique<physics::physics_world>(settings, jobs);
        CONJURE_REQUIRE_OK(physics->attach(world));
    }
#endif
#if defined(CONJURE_WITH_AUDIO) && CONJURE_WITH_AUDIO
    if (options.audio) {
        audio::audio_settings settings = {};
        settings.backend = audio::audio_backend_kind::NULL_BACKEND;
        settings.sampleRate = 48000;
        audio::group_desc music = {};
        music.name = core::string_id::intern("music");
        music.gain = 0.5f;
        settings.groups.push_back(music);
        audio = std::make_unique<audio::audio_engine>(settings);
    }
#endif
    if (options.scripting) {
        runtime = std::make_unique<scripting::script_runtime>(
          scripting::create_script_host(scripting::script_backend::NULL_HOST)
        );
        runtime->attach_registry(&registry);
        runtime->attach_world(&world);
    }

    context.world = &world;
    context.components = &registry;
    context.scenes = &scenes;
    context.assets = &assets;
    context.input = options.window ? &input : nullptr;
    context.window = window.get();
    context.checks = &checks;
    context.stats = &stats;
    context.time = &time;
    context.scripting = runtime.get();
#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    context.physics = physics.get();
#endif
#if defined(CONJURE_WITH_AUDIO) && CONJURE_WITH_AUDIO
    context.audio = audio.get();
#endif

    if (options.fill) {
        fill();
    }
    if (runtime != nullptr) {
        scripting::script_runtime_config config = {};
        config.host.backend = scripting::script_backend::NULL_HOST;
        CONJURE_REQUIRE_OK(runtime->initialize(config));
        runtime->set_mode(scripting::script_mode::PLAY);
        if (options.fill) {
            // The core entries exist only after initialize: wrap them too so every count is observable.
            wrap_with_counters(runtime->native_api());
        }
    }
}

bindings_test::bare_fixture::~bare_fixture() {
    scripting_bindings_subsystem::set_active_residency(nullptr);
    clear_active_context();
    if (runtime != nullptr) {
        runtime->destroy_all(false);
        runtime->shutdown();
        runtime->attach_world(nullptr);
    }
#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    if (physics != nullptr) {
        physics->detach();
    }
#endif
    residency.clear();
    CONJURE_CHECK_OK(world.validate());
}

conjure_native_api &bindings_test::bare_fixture::api() noexcept {
    if (runtime != nullptr) {
        return runtime->native_api();
    }
    return standaloneApi;
}

scripting::null_script_host &bindings_test::bare_fixture::null_host() noexcept {
    REQUIRE(runtime != nullptr);
    return static_cast<scripting::null_script_host &>(runtime->host());
}

void bindings_test::bare_fixture::fill() {
    CONJURE_REQUIRE_OK(fill_native_table(api(), context));
    wrap_with_counters(api());
    scripting_bindings_subsystem::set_active_residency(&residency);
}

ecs::entity bindings_test::bare_fixture::create(std::string_view name, ecs::entity parent) {
    const core::result<ecs::entity> created = scenes.active().create_entity(core::string_id(name), parent);
    REQUIRE(created.has_value());
    return created.value();
}

void bindings_test::bare_fixture::step(u32 frames) {
    for (u32 index = 0; index < frames; ++index) {
        ++frame.frameIndex;
        ++time.frameIndex;
        scheduler.run_phase(ecs::system_phase::FRAME_BEGIN, frame);
        scheduler.run_phase(ecs::system_phase::TRANSFORM, frame);
        scheduler.run_phase(ecs::system_phase::FRAME_END, frame);
    }
}

void bindings_test::bare_fixture::add_blob(
  const assets::asset_id &id,
  assets::asset_type_id type,
  std::span<const std::byte> payload,
  std::string_view assetPath
) {
    assets::cooked_blob_header header = {};
    header.type = type;
    header.typeVersion = 1;
    header.importerVersion = 1;
    header.id = id;
    source->add(
      assets::make_cooked_blob(header, {}, std::vector<std::byte>(payload.begin(), payload.end())),
      assetPath
    );
}

void bindings_test::bare_fixture::register_scene_asset_types() {
    if (mSceneTypesRegistered) {
        return;
    }
    CONJURE_REQUIRE_OK(scene::register_asset_types(assets.types(), importers, registry));
    mSceneTypesRegistered = true;
}

assets::asset_id bindings_test::bare_fixture::publish_prefab(
  ecs::entity root,
  std::string_view name,
  u64 index,
  std::string_view assetPath
) {
    register_scene_asset_types();
    const core::result<scene::prefab_document> document = scene::prefab::build_document(scenes.active(), root, name);
    CONJURE_REQUIRE_OK(document);
    core::json::writer out = {};
    CONJURE_REQUIRE_OK(scene::scene_serializer::write_prefab(document.value(), out));
    const std::string text = out.finish();
    const assets::asset_id id = test_uuid(index);
    add_blob(
      id,
      assets::asset_type_traits<scene::prefab_asset>::id(),
      std::as_bytes(std::span<const char>(text.data(), text.size())),
      assetPath
    );
    return id;
}

assets::asset_id bindings_test::bare_fixture::publish_scene(
  std::string_view name,
  u32 rootCount,
  u64 index,
  std::string_view assetPath
) {
    register_scene_asset_types();
    std::string roots = {};
    std::string entities = {};
    for (u32 root = 0; root < rootCount; ++root) {
        const std::string guid = test_uuid(index * 100 + root + 1).to_string();
        if (root > 0) {
            roots += ",";
            entities += ",";
        }
        roots += "\"" + guid + "\"";
        entities += "{\"components\":{\"conjure.scene.name\":{\"value\":\"" + std::string(name) + "_"
          + std::to_string(root) + "\"},\"conjure.scene.guid\":{\"value\":\"" + guid + "\"}}}";
    }
    const std::string text = "{\"format\":\"conjure.scene\",\"version\":1,\"name\":\"" + std::string(name)
      + "\",\"settings\":{},\"roots\":[" + roots + "],\"archive\":{\"entities\":[" + entities + "]}}";
    const assets::asset_id id = test_uuid(index);
    add_blob(
      id,
      assets::asset_type_traits<scene::scene_asset>::id(),
      std::as_bytes(std::span<const char>(text.data(), text.size())),
      assetPath
    );
    return id;
}

assets::asset_id bindings_test::bare_fixture::publish_clip(u64 index, std::string_view assetPath) {
#if defined(CONJURE_WITH_AUDIO) && CONJURE_WITH_AUDIO
    if (!mAudioTypesRegistered) {
        CONJURE_REQUIRE_OK(audio::register_asset_types(assets.types(), importers));
        mAudioTypesRegistered = true;
    }
    const std::vector<std::byte> wav = make_test_wav();
    const core::result<std::vector<std::byte>> cooked = audio::cook_clip(wav, audio::clip_import_settings{});
    CONJURE_REQUIRE_OK(cooked);
    const assets::asset_id id = test_uuid(index);
    add_blob(id, assets::asset_type_traits<audio::audio_clip>::id(), cooked.value(), assetPath);
    return id;
#else
    CONJURE_UNUSED(assetPath);
    return test_uuid(index);
#endif
}

void bindings_test::bare_fixture::load_now(const assets::asset_id &id) {
    const core::result<assets::asset_handle> loaded = assets.load_blocking(id);
    CONJURE_REQUIRE_OK(loaded);
    REQUIRE(loaded.value().state() == assets::asset_state::LOADED);
    // The handle is dropped on purpose: residency for the test comes from the fixture's pin.
    assets.pin(id, true);
}

assets::asset_manager_desc bindings_test::bare_fixture::make_asset_desc() {
    assets::asset_manager_desc desc = {};
    desc.jobs = &jobs;
    std::unique_ptr<assets::memory_asset_source> owned = std::make_unique<assets::memory_asset_source>();
    source = owned.get();
    desc.source = std::move(owned);
    return desc;
}
