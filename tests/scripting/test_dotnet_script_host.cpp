#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/test_environment.h"
#include "scripting_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/scripting/dotnet_script_host.h"
#include "conjure/scripting/native_api.h"
#include "conjure/scripting/script_host.h"
#include "conjure/scripting/script_runtime.h"
#include "conjure/scripting/scripting_error.h"

#include <format>
#include <string>

using namespace conjure;
using namespace conjure::scripting;
using namespace conjure::testing;
using namespace scripting_test;

TEST_SUITE("scripting/dotnet_script_host") {

    TEST_CASE(
      "dotnet_script_host - initialises the real runtime and completes the handshake [dotnet]" * doctest::timeout(60)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_script_host host = {};
        conjure_native_api api = make_native_api(nullptr);
        fill_core_native_entries(api);
        script_host_config config = {};
        config.managedDirectory = managed_directory();
        CONJURE_REQUIRE_OK(host.initialise(config, api));
        CHECK(host.status() == script_host_status::READY);
        CHECK(host.backend() == script_backend::DOTNET);
        CHECK(dotnet_script_host::is_runtime_started());
        CHECK(host.mode() == host_mode::FRAMEWORK_DEPENDENT);
        CONJURE_CHECK_OK(validate_managed_api(host.managed_api()));
        CHECK(host.managed_api().layout_hash == CONJURE_NATIVE_API_HASH);
        CHECK(host.managed_api().size == sizeof(conjure_managed_api));
        CHECK(host.managed_api().collect_garbage() == 0);
        host.shutdown();
        CHECK(host.status() == script_host_status::UNINITIALISED);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("dotnet_script_host - the managed SDK is built for the .NET major global.json pins [dotnet]") {
        // Code sweep F778: the runtime config the build writes beside Conjure.Bootstrap.dll decides which runtime
        // hostfxr starts, so it must name the framework of managed/Directory.Build.props, not an older one.
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        const i32 major = dotnet_major();
        CAPTURE(major);
        const core::result<core::json::document> parsed =
          core::json::document::parse_file(managed_directory() / "Conjure.Bootstrap.runtimeconfig.json");
        CONJURE_REQUIRE_OK(parsed);
        const core::json::value options = parsed.value().root()["runtimeOptions"];
        REQUIRE(options.is_object());
        CHECK(options["tfm"].as_string() == std::format("net{}.0", major));
        CHECK(options["rollForward"].as_string() == "Major");
        const core::json::value framework = options["framework"];
        CHECK(framework["name"].as_string() == "Microsoft.NETCore.App");
        CHECK(framework["version"].as_string().starts_with(std::format("{}.0.", major)));
    }

    TEST_CASE("dotnet_script_host - a second host reuses the process runtime [dotnet]" * doctest::timeout(60)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        conjure_native_api api = make_native_api(nullptr);
        fill_core_native_entries(api);
        script_host_config config = {};
        config.managedDirectory = managed_directory();
        {
            dotnet_script_host first = {};
            CONJURE_REQUIRE_OK(first.initialise(config, api));
        }
        dotnet_script_host second = {};
        CONJURE_REQUIRE_OK(second.initialise(config, api));
        CHECK(second.status() == script_host_status::READY);
        CHECK(second.managed_directory() == managed_directory());
        CHECK(second.managed_api().run_phase != nullptr);
        CHECK(second.managed_api().run_phase(static_cast<uint8_t>(script_phase::UPDATE), 0.0f) == 0);
    }

    TEST_CASE(
      "dotnet_script_host - an ABI mismatch is detected when the hash differs [dotnet]" * doctest::timeout(60)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        conjure_native_api api = make_native_api(nullptr);
        fill_core_native_entries(api);
        script_host_config config = {};
        config.managedDirectory = managed_directory();
        dotnet_script_host host = {};
        SUBCASE("hash") {
            api.layout_hash ^= 0xFF;
        }
        SUBCASE("size") {
            api.size = 8;
        }
        SUBCASE("version") {
            api.version = CONJURE_NATIVE_API_VERSION + 7;
        }
        const core::result<void> initialised = host.initialise(config, api);
        CONJURE_CHECK_ERROR(initialised, script_error::ABI_MISMATCH);
        CHECK(host.status() == script_host_status::DISABLED);
        // The message carries both hashes so a stale Conjure.Managed.dll is diagnosable.
        CHECK(initialised.get_error().message().find("managed hash") != std::string::npos);
    }

    TEST_CASE("dotnet_script_host - a runtime over the host runs phases [dotnet]" * doctest::timeout(60)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture("v1", false);
        CHECK(fixture.runtime.status() == script_host_status::READY);
        CHECK(fixture.runtime.is_assembly_loaded());
        fixture.run_frame();
        CHECK(fixture.runtime.stats().liveInstances == 0);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE(
      "dotnet_script_host - loading the test scripts registers classes and fields [dotnet]" * doctest::timeout(60)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        dotnet_fixture fixture("v1", false);
        const script_class_registry &classes = fixture.runtime.classes();
        CHECK(classes.size() >= 10);

        const script_class_info *probe = classes.find("Conjure.TestScripts.LifecycleProbe");
        REQUIRE(probe != nullptr);
        CHECK(probe->has_callback(script_callback::CREATE));
        CHECK(probe->has_callback(script_callback::START));
        CHECK(probe->has_callback(script_callback::FIXED_UPDATE));
        CHECK(probe->has_callback(script_callback::UPDATE));
        CHECK(probe->has_callback(script_callback::LATE_UPDATE));
        CHECK(probe->has_callback(script_callback::DESTROY));
        CHECK_FALSE(probe->has_callback(script_callback::COLLISION_ENTER));
        CHECK_FALSE(probe->has_callback(script_callback::RELOAD));
        REQUIRE(probe->find_field("Updates") != nullptr);
        CHECK(probe->find_field("Updates")->kind == script_field_kind::INT32);
        CHECK(probe->find_field("Updates")->defaultValue.root().as_i64() == 0);

        // OnReload is detected like the other callbacks (code sweep OWN-RELOAD).
        const script_class_info *reloadProbe = classes.find("Conjure.TestScripts.ReloadProbe");
        REQUIRE(reloadProbe != nullptr);
        CHECK(reloadProbe->has_callback(script_callback::RELOAD));

        const script_class_info *mover = classes.find("Conjure.TestScripts.Mover");
        REQUIRE(mover != nullptr);
        CHECK(mover->has_callback(script_callback::UPDATE));
        CHECK_FALSE(mover->has_callback(script_callback::CREATE));
        const script_field_info *speed = mover->find_field("_speed");      // [SerializeField] private
        REQUIRE(speed != nullptr);
        CHECK(speed->kind == script_field_kind::FLOAT);
        CHECK(speed->defaultValue.root().as_f64() == doctest::Approx(1.5));
        const script_field_info *velocity = mover->find_field("Velocity");
        REQUIRE(velocity != nullptr);
        CHECK(velocity->kind == script_field_kind::VEC3);
        CHECK(velocity->defaultValue.root()[0].as_f64() == doctest::Approx(1.0));
        CHECK(mover->find_field("_privateCounter") == nullptr);          // private, not serialised

        const script_class_info *kinds = classes.find("Conjure.TestScripts.FieldKinds");
        REQUIRE(kinds != nullptr);
        CHECK(kinds->find_field("Flag")->kind == script_field_kind::BOOL);
        CHECK(kinds->find_field("Big")->kind == script_field_kind::INT64);
        CHECK(kinds->find_field("Precise")->kind == script_field_kind::DOUBLE);
        CHECK(kinds->find_field("Label")->kind == script_field_kind::STRING);
        CHECK(kinds->find_field("Planar")->kind == script_field_kind::VEC2);
        CHECK(kinds->find_field("Homogeneous")->kind == script_field_kind::VEC4);
        CHECK(kinds->find_field("Orientation")->kind == script_field_kind::QUAT);
        CHECK(kinds->find_field("Tint")->kind == script_field_kind::COLOR);
        CHECK(kinds->find_field("Link")->kind == script_field_kind::ENTITY);
        CHECK(kinds->find_field("Texture")->kind == script_field_kind::ASSET);
        CHECK(kinds->find_field("Current")->kind == script_field_kind::ENUM);
        CHECK(kinds->find_field("Current")->defaultValue.root().as_i64() == 1);
        REQUIRE(kinds->find_field("Ints") != nullptr);
        CHECK(kinds->find_field("Ints")->kind == script_field_kind::LIST);
        CHECK(kinds->find_field("Ints")->elementKind == script_field_kind::INT32);
        CHECK(kinds->find_field("Ints")->defaultValue.root().size() == 3);
        CHECK(kinds->find_field("Floats")->elementKind == script_field_kind::FLOAT);
        CHECK(kinds->find_field("Names")->elementKind == script_field_kind::STRING);
        CHECK(kinds->find_field("Links")->elementKind == script_field_kind::ENTITY);
        CHECK(kinds->find_field("Hidden")->meta.root()["hidden"].as_bool() == true);
        // TestScripts.TextureAsset is a plain class, not an IAssetType tag: the reference is untyped.
        CHECK_FALSE(kinds->find_field("Texture")->meta.root().contains("assetType"));
        CHECK(kinds->find_field("Current")->meta.root()["enumNames"].size() == 3);
        CHECK(kinds->find_field("Current")->meta.root()["enumValues"].size() == 3);
    }

    TEST_CASE("dotnet_script_host - the test scripts run code that uses C# 14 [dotnet]" * doctest::timeout(60)) {
        // managed/Directory.Build.props pins LangVersion 14 for the SDK and the test scripts, and LanguageVersionProbe
        // uses features of C# 14: under a lower pin the test scripts do not build. Each public field holds what one
        // feature computed in OnCreate. The backing field the field keyword declares is compiler generated, so it is
        // not serialised, and the types the compiler emits for extension blocks leave discovery undisturbed.
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        const script_class_info *probe = fixture.runtime.classes().find("Conjure.TestScripts.LanguageVersionProbe");
        REQUIRE(probe != nullptr);
        CHECK(probe->has_callback(script_callback::CREATE));
        CHECK(probe->fields.size() == 7);
        const script_entry_id id = fixture.add_script(
          fixture.create_entity(),
          "Conjure.TestScripts.LanguageVersionProbe"
        );
        REQUIRE(fixture.runtime.entry(id)->is_live());
        CHECK(fixture.field_i64(id, "ClampedHealth") == 100);                  // the field keyword
        CHECK(fixture.field_i64(id, "ExtensionSquare") == 49);                 // an extension property
        CHECK(fixture.field_i64(id, "SpanTotal") == 10);                       // first-class spans
        CHECK(fixture.field_i64(id, "AssignedValue") == 42);                   // null-conditional assignment
        CHECK(fixture.field_i64(id, "RightSideEvaluations") == 1);
        CHECK(fixture.field_string(id, "UnboundGenericName") == "Dictionary"); // nameof of an unbound generic
        CHECK(fixture.field_i64(id, "ParsedByLambda") == 42);                  // simple lambda parameter modifiers
        CHECK(logs.count(core::log_level::ERR) == 0);
        CHECK(logs.count(core::log_level::WARN) == 0);
    }

    TEST_CASE(
      "regression - review 17 a field redeclared by a derived class keeps both under distinct names [dotnet]" *
      doctest::timeout(60)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        const script_class_info *child = fixture.runtime.classes().find("Conjure.TestScripts.ShadowChild");
        REQUIRE(child != nullptr);
        // _hidden, Shared, ChildView, BaseView, ShadowBase._hidden, ShadowBase.Shared
        CHECK(child->fields.size() == 6);
        REQUIRE(child->find_field("_hidden") != nullptr);
        CHECK(child->find_field("_hidden")->defaultValue.root().as_i64() == 2);
        REQUIRE(child->find_field("Shared") != nullptr);
        CHECK(child->find_field("Shared")->defaultValue.root().as_i64() == 2);
        REQUIRE(child->find_field("ShadowBase._hidden") != nullptr);
        CHECK(child->find_field("ShadowBase._hidden")->defaultValue.root().as_i64() == 1);
        REQUIRE(child->find_field("ShadowBase.Shared") != nullptr);
        CHECK(child->find_field("ShadowBase.Shared")->defaultValue.root().as_i64() == 1);
        CHECK(child->find_field("BaseView") != nullptr);
        CHECK(logs.contains(core::log_level::INFO, "is hidden by a derived field"));
        const script_class_info *base = fixture.runtime.classes().find("Conjure.TestScripts.ShadowBase");
        REQUIRE(base != nullptr);
        CHECK(base->fields.size() == 3);
        CHECK(base->find_field("_hidden")->defaultValue.root().as_i64() == 1);

        // Values reach the field they name: the bare names the derived fields, the qualified ones the base.
        const script_entry_id id = fixture.add_script(
          fixture.create_entity(),
          "Conjure.TestScripts.ShadowChild",
          R"({"_hidden": 20, "Shared": 10, "ShadowBase._hidden": 300, "ShadowBase.Shared": 100})"
        );
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        CHECK(fixture.field_i64(id, "ChildView") == 30);
        CHECK(fixture.field_i64(id, "BaseView") == 400);
        CHECK(fixture.field_i64(id, "ShadowBase.Shared") == 100);
        CHECK(logs.count(core::log_level::ERR) == 0);
        CHECK(logs.count(core::log_level::WARN) == 0);
    }

    TEST_CASE("dotnet_script_host - Conjure.Managed is shared with the game context [dotnet]" * doctest::timeout(60)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        // Discovery resolves the game's ScriptBehaviour base against the SDK loaded in the default
        // context: a second copy in the game context would make every subclass invisible.
        dotnet_fixture fixture("v1", true);
        REQUIRE(fixture.runtime.classes().find("Conjure.TestScripts.LifecycleProbe") != nullptr);
        const ecs::entity entity = fixture.create_entity();
        const script_entry_id id = fixture.add_script(entity, "Conjure.TestScripts.LifecycleProbe");
        CHECK(fixture.runtime.entry(id)->is_live());
        CHECK(fixture.field_i64(id, "Created") == 1);
    }
}
