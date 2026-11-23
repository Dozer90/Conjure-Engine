#include "conjure/testing/doctest.h"

#include "conjure/testing/test_environment.h"
#include "scripting_test_support.h"

#include "conjure/core/hash.h"
#include "conjure/core/log.h"
#include "conjure/scripting/script_fields.h"

#include <filesystem>
#include <utility>

using namespace conjure;
using namespace conjure::scripting;
using namespace conjure::testing;

#ifndef CONJURE_SCRIPTING_HAS_DOTNET
    #define CONJURE_SCRIPTING_HAS_DOTNET 0
#endif
#ifndef CONJURE_SCRIPTING_MANAGED_DIR
    #define CONJURE_SCRIPTING_MANAGED_DIR ""
#endif
#ifndef CONJURE_SCRIPTING_TEST_SCRIPTS_DIR
    #define CONJURE_SCRIPTING_TEST_SCRIPTS_DIR ""
#endif
#ifndef CONJURE_SCRIPTING_TEST_SCRIPTS_PROJECT
    #define CONJURE_SCRIPTING_TEST_SCRIPTS_PROJECT ""
#endif
#ifndef CONJURE_SCRIPTING_BOOTSTRAP_PROJECT
    #define CONJURE_SCRIPTING_BOOTSTRAP_PROJECT ""
#endif
#ifndef CONJURE_SCRIPTING_MANIFEST_PATH
    #define CONJURE_SCRIPTING_MANIFEST_PATH ""
#endif

namespace {

    /// <summary>
    /// The DOTNET_RUNTIME precondition: a runtime is installed and the SDK was built with this tree. The
    /// probe never starts the CLR (it only asks nethost for hostfxr).
    /// </summary>
    bool dotnet_runtime_available() noexcept {
        static const bool available = []() noexcept {
            if (!scripting_test::has_managed_sdk()) {
                return false;
            }
            std::error_code probe = {};
            const bool sdkPresent = std::filesystem::is_regular_file(
              scripting_test::managed_directory() / "Conjure.Managed.dll",
              probe
            );
            return sdkPresent && is_runtime_available();
        }();
        return available;
    }

    struct probe_installer {
        probe_installer() {
            set_precondition_probe(precondition::DOTNET_RUNTIME, &dotnet_runtime_available);
        }
    };

    const probe_installer sProbeInstaller = {};
}

std::filesystem::path scripting_test::managed_directory() {
    return std::filesystem::path(CONJURE_SCRIPTING_MANAGED_DIR);
}

std::filesystem::path scripting_test::test_scripts_assembly(std::string_view variant) {
    const std::filesystem::path root(CONJURE_SCRIPTING_TEST_SCRIPTS_DIR);
    if (root.empty()) {
        return {};
    }
    return root / std::filesystem::path(variant) / "Conjure.TestScripts.dll";
}

std::filesystem::path scripting_test::test_scripts_project() {
    return std::filesystem::path(CONJURE_SCRIPTING_TEST_SCRIPTS_PROJECT);
}

std::filesystem::path scripting_test::bootstrap_project() {
    return std::filesystem::path(CONJURE_SCRIPTING_BOOTSTRAP_PROJECT);
}

std::filesystem::path scripting_test::manifest_path() {
    return std::filesystem::path(CONJURE_SCRIPTING_MANIFEST_PATH);
}

bool scripting_test::has_managed_sdk() noexcept {
    return CONJURE_SCRIPTING_HAS_DOTNET != 0;
}

void scripting_test::register_test_components(ecs::component_registry &registry) {
    CONJURE_REQUIRE_OK(register_script_component(registry));
    CONJURE_REQUIRE_OK(registry.register_component<test_position>());
    CONJURE_REQUIRE_OK(registry.register_component<test_velocity>());
    CONJURE_REQUIRE_OK(registry.register_component<test_flags>());
}

script_field_info scripting_test::make_field(
  std::string_view name,
  script_field_kind kind,
  std::string_view defaultJson,
  script_field_kind elementKind,
  std::string_view metaJson
) {
    script_field_info field = {};
    field.name = std::string(name);
    field.hash = hash_of(name);
    field.kind = kind;
    field.elementKind = elementKind;
    if (!defaultJson.empty()) {
        field.defaultValue = json(defaultJson);
    }
    if (!metaJson.empty()) {
        field.meta = json(metaJson);
    }
    return field;
}

script_class_info scripting_test::make_class(
  std::string_view fullName,
  std::initializer_list<script_callback> callbacks,
  std::vector<script_field_info> fields
) {
    script_class_info info = {};
    info.fullName = std::string(fullName);
    info.hash = hash_of(fullName);
    for (const script_callback callback : callbacks) {
        info.callbackMask |= callback_bit(callback);
    }
    info.fields = std::move(fields);
    info.sort_fields();
    return info;
}

u64 scripting_test::hash_of(std::string_view name) noexcept {
    return core::hash::fnv1a64(name);
}

core::json::document scripting_test::json(std::string_view text) {
    core::result<core::json::document> parsed = core::json::document::parse(text);
    REQUIRE_MESSAGE(parsed.has_value(), describe_error(parsed));
    return std::move(parsed).value();
}


/*===========================================================================*/
/*===========================================================================*/
// scripting_fixture

scripting_test::scripting_fixture::scripting_fixture() :
    world((register_test_components(registry), registry)),
    runtime(create_script_host(script_backend::NULL_HOST)) {
    runtime.attach_registry(&registry);
    runtime.attach_world(&world);
    script_runtime_config config = {};
    config.host.backend = script_backend::NULL_HOST;
    CONJURE_REQUIRE_OK(runtime.initialise(config));
    REQUIRE(runtime.status() == script_host_status::READY);
}

scripting_test::scripting_fixture::~scripting_fixture() {
    runtime.destroy_all(false);
    runtime.attach_world(nullptr);
}

null_script_host &scripting_test::scripting_fixture::null_host() noexcept {
    return static_cast<null_script_host &>(runtime.host());
}

void scripting_test::scripting_fixture::load_fake_classes(std::vector<script_class_info> classes) {
    for (script_class_info &info : classes) {
        null_host().add_fake_class(std::move(info));
    }
    CONJURE_REQUIRE_OK(runtime.load_game_assembly({}));
}

ecs::entity scripting_test::scripting_fixture::create_entity() {
    core::result<ecs::entity> created = world.create_entity();
    REQUIRE(created.has_value());
    return created.value();
}

core::json::document scripting_test::scripting_fixture::fields_of(script_entry_id id) {
    core::result<core::json::document> collected = runtime.collect_entry_fields(id);
    if (!collected.has_value()) {
        return core::json::document::object();
    }
    return std::move(collected).value();
}


/*===========================================================================*/
/*===========================================================================*/
// dotnet_fixture

scripting_test::dotnet_fixture::dotnet_fixture(std::string_view variant, bool playMode) :
    world((register_test_components(registry), registry)),
    runtime(create_script_host(script_backend::DOTNET)) {
    runtime.attach_registry(&registry);
    runtime.attach_world(&world);
    script_runtime_config config = {};
    config.host.backend = script_backend::DOTNET;
    config.host.managedDirectory = managed_directory();
    config.gameAssemblyPath = test_scripts_assembly(variant);
    config.initialMode = playMode ? script_mode::PLAY : script_mode::EDIT;
    initialised = runtime.initialise(config);
    REQUIRE_MESSAGE(initialised.has_value(), describe_error(initialised));
    REQUIRE(runtime.status() == script_host_status::READY);
}

scripting_test::dotnet_fixture::~dotnet_fixture() {
    runtime.destroy_all(false);
    runtime.shutdown();
    runtime.attach_world(nullptr);
}

ecs::entity scripting_test::dotnet_fixture::create_entity() {
    core::result<ecs::entity> created = world.create_entity();
    REQUIRE(created.has_value());
    return created.value();
}

script_entry_id scripting_test::dotnet_fixture::add_script(
  ecs::entity entity,
  std::string_view className,
  std::string_view fieldsJson
) {
    const core::json::document fields = json(fieldsJson);
    const script_entry_id id = runtime.add_script(entity, hash_of(className), fields.root());
    REQUIRE(id.is_valid());
    runtime.flush_pending();
    return id;
}

void scripting_test::dotnet_fixture::run_frame(f32 deltaTime) {
    runtime.run_phase(script_phase::FIXED_UPDATE, deltaTime);
    runtime.run_phase(script_phase::UPDATE, deltaTime);
    runtime.run_phase(script_phase::LATE_UPDATE, deltaTime);
}

core::json::document scripting_test::dotnet_fixture::fields_of(script_entry_id id) {
    core::result<core::json::document> collected = runtime.collect_entry_fields(id);
    if (!collected.has_value()) {
        return core::json::document::object();
    }
    return std::move(collected).value();
}

i64 scripting_test::dotnet_fixture::field_i64(script_entry_id id, std::string_view name) {
    return fields_of(id).root()[name].as_i64(-1);
}

std::string scripting_test::dotnet_fixture::field_string(script_entry_id id, std::string_view name) {
    return std::string(fields_of(id).root()[name].as_string("<missing>"));
}

f64 scripting_test::dotnet_fixture::field_f64(script_entry_id id, std::string_view name) {
    return fields_of(id).root()[name].as_f64(-1.0);
}
