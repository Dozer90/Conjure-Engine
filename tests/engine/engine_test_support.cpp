#include "engine_test_support.h"

#include "conjure/core/string_id.h"
#include "conjure/ecs/system.h"
#include "conjure/engine/engine_error.h"
#include "conjure/engine/engine_phase.h"

#include <format>
#include <fstream>
#include <system_error>
#include <utility>

#ifndef CONJURE_TEST_SHADER_DIR
    #define CONJURE_TEST_SHADER_DIR ""
#endif

using namespace conjure;
using namespace conjure::engine;

/*===========================================================================*/
/*===========================================================================*/
// Free functions

std::filesystem::path engine_test::shader_directory() {
    return std::filesystem::path(CONJURE_TEST_SHADER_DIR);
}

engine_config engine_test::headless_config() {
    engine_config config = engine_config::headless_defaults();
    config.rendering.shaderDirectory = shader_directory();
    config.logLevel = core::log_level::WARN;
    config.audio.enabled = true;
    config.physics.enabled = true;
    config.scripting.enabled = false;
    config.modules.enginePlugins = false;
    return config;
}

engine_config engine_test::inline_config() {
    engine_config config = headless_config();
    config.jobs.workerCount = 0;
    return config;
}

void engine_test::enable_null_rendering(engine_config &config) {
    config.rendering.enabled = true;
    config.rendering.backend = rhi_backend_choice::NULL_DEVICE;
    config.rendering.gpuValidation = false;
}

application_desc engine_test::make_desc(const engine_config &config) {
    application_desc desc = {};
    desc.config = config;
    return desc;
}

project engine_test::write_project(
  const std::filesystem::path &directory,
  std::string_view name,
  std::string_view startupScene,
  std::string_view extraJson,
  const std::filesystem::path &assetsRoot
) {
    std::error_code ignored = {};
    std::filesystem::create_directories(assetsRoot.is_absolute() ? assetsRoot : directory / assetsRoot, ignored);
    std::string text = std::format(
      "{{\n  \"schema_version\": 1,\n  \"name\": \"{}\",\n  \"assets_root\": \"{}\",\n  \"startup_scene\": \"{}\"",
      name,
      assetsRoot.generic_string(),
      startupScene
    );
    if (!extraJson.empty()) {
        text += ",\n  ";
        text += extraJson;
    }
    text += "\n}\n";
    {
        std::filesystem::create_directories(directory, ignored);
        std::ofstream stream(directory / "project.json", std::ios::binary | std::ios::trunc);
        REQUIRE(stream.is_open());
        stream << text;
    }
    core::result<project> loaded = project::load(directory);
    CONJURE_REQUIRE_OK(loaded);
    return std::move(loaded).value();
}

void engine_test::add_phase_probes(
  ecs::scheduler &scheduler,
  std::vector<std::string> &log,
  std::string_view label,
  bool pausedProbes
) {
    for (u32 index = 0; index < ecs::SYSTEM_PHASE_COUNT; ++index) {
        const ecs::system_phase phase = static_cast<ecs::system_phase>(index);
        const std::string_view phaseName = conjure::engine::to_string(phase);
        const std::string entry = label.empty() ? std::string(phaseName) : std::format("{}:{}", phaseName, label);
        ecs::system_desc desc = {};
        const std::string group = label.empty() ? "default" : std::string(label);
        desc.name = core::string_id::intern(std::format("probe.{}.{}", group, phaseName));
        desc.phase = phase;
        desc.flags = ecs::system_flags::EXCLUSIVE;
        if (pausedProbes && (phase == ecs::system_phase::UPDATE || phase == ecs::system_phase::LATE_UPDATE)) {
            desc.flags |= ecs::system_flags::RUNS_WHILE_PAUSED;
        }
        std::vector<std::string> *target = &log;
        desc.run = [target, entry](ecs::system_context &) {
            target->push_back(entry);
        };
        CONJURE_REQUIRE_OK(scheduler.add_system(std::move(desc)));
    }
}

std::vector<std::string> engine_test::filter_log(const std::vector<std::string> &log, std::string_view prefix) {
    std::vector<std::string> filtered = {};
    for (const std::string &entry : log) {
        if (entry.starts_with(prefix)) {
            filtered.push_back(entry);
        }
    }
    return filtered;
}


/*===========================================================================*/
/*===========================================================================*/
// probe_subsystem

engine_test::probe_subsystem::probe_subsystem(
  std::string name,
  std::vector<std::string> &log,
  bool failStartup,
  bool failPostStartup
) :
    mName(std::move(name)),
    mLog(&log),
    mFailStartup(failStartup),
    mFailPostStartup(failPostStartup) {}

engine_test::probe_subsystem::~probe_subsystem() {
    mLog->push_back(mName + ".destroyed");
}

std::string_view engine_test::probe_subsystem::name() const noexcept {
    return mName;
}

core::result<void> engine_test::probe_subsystem::startup(engine_context &context) {
    CONJURE_UNUSED(context);
    mLog->push_back(mName + ".startup");
    if (mFailStartup) {
        return make_error(engine_error::LAYER_ATTACH_FAILED, mName + " refused to start");
    }
    mStarted = true;
    return core::result<void>::success();
}

core::result<void> engine_test::probe_subsystem::post_startup(engine_context &context) {
    CONJURE_UNUSED(context);
    mLog->push_back(mName + ".post_startup");
    if (mFailPostStartup) {
        return make_error(engine_error::SCRIPT_RUNTIME_NOT_FOUND, mName + " refused post_startup");
    }
    return core::result<void>::success();
}

void engine_test::probe_subsystem::register_systems(ecs::scheduler &scheduler, engine_context &context) {
    CONJURE_UNUSED(scheduler);
    CONJURE_UNUSED(context);
    mLog->push_back(mName + ".register_systems");
}

void engine_test::probe_subsystem::begin_frame(engine_context &context) {
    CONJURE_UNUSED(context);
    mLog->push_back(mName + ".begin_frame");
}

void engine_test::probe_subsystem::end_frame(engine_context &context) {
    CONJURE_UNUSED(context);
    mLog->push_back(mName + ".end_frame");
}

void engine_test::probe_subsystem::shutdown(engine_context &context) {
    CONJURE_UNUSED(context);
    mLog->push_back(mName + ".shutdown");
    mStarted = false;
}


/*===========================================================================*/
/*===========================================================================*/
// probe_layer

engine_test::probe_layer::probe_layer(
  std::string name,
  std::vector<std::string> &log,
  bool consumeEvents,
  bool failAttach
) :
    layer(std::move(name)),
    mLog(&log),
    mConsumeEvents(consumeEvents),
    mFailAttach(failAttach) {}

engine_test::probe_layer::~probe_layer() {
    mLog->push_back(std::string(name()) + ".destroyed");
}

core::result<void> engine_test::probe_layer::on_attach(engine_context &context) {
    CONJURE_UNUSED(context);
    mLog->push_back(std::string(name()) + ".attach");
    if (mFailAttach) {
        return core::error(core::error_code::INVALID_STATE, std::string(name()) + " refused to attach");
    }
    return core::result<void>::success();
}

void engine_test::probe_layer::on_detach(engine_context &context) {
    CONJURE_UNUSED(context);
    mLog->push_back(std::string(name()) + ".detach");
}

void engine_test::probe_layer::on_frame_begin(engine_context &context) {
    CONJURE_UNUSED(context);
    mLog->push_back(std::string(name()) + ".frame_begin");
}

void engine_test::probe_layer::on_update(engine_context &context) {
    CONJURE_UNUSED(context);
    ++mUpdates;
    mLog->push_back(std::string(name()) + ".update");
}

void engine_test::probe_layer::on_imgui(engine_context &context) {
    CONJURE_UNUSED(context);
    mLog->push_back(std::string(name()) + ".imgui");
}

void engine_test::probe_layer::on_imgui_end(engine_context &context) {
    CONJURE_UNUSED(context);
    mLog->push_back(std::string(name()) + ".imgui_end");
}

void engine_test::probe_layer::on_frame_end(engine_context &context) {
    CONJURE_UNUSED(context);
    mLog->push_back(std::string(name()) + ".frame_end");
}

bool engine_test::probe_layer::on_event(engine_context &context, const platform::event &event) {
    CONJURE_UNUSED(context);
    CONJURE_UNUSED(event);
    mLog->push_back(std::string(name()) + ".event");
    return mConsumeEvents;
}
