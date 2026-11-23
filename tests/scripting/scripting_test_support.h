#pragma once

// Shared fixtures of the scripting tests (docs/design/03_scripting.md 10): a registry with
// script_component and two test component types, a world on it and a script_runtime over the null
// host (scripting_fixture) or over the real CLR (dotnet_fixture, guarded by precondition::DOTNET_RUNTIME,
// whose probe this test module installs). Paths of the managed build outputs come from the compile
// definitions tests/scripting/CMakeLists.txt sets.

#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/test_environment.h"

#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/reflect/reflect.h"
#include "conjure/core/types.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/world.h"
#include "conjure/scripting/native_api.h"
#include "conjure/scripting/null_script_host.h"
#include "conjure/scripting/script_class_registry.h"
#include "conjure/scripting/script_component.h"
#include "conjure/scripting/script_host.h"
#include "conjure/scripting/script_runtime.h"

#include <filesystem>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace scripting_test {

    /// <summary>
    /// Mirrored by Conjure.TestScripts.TestPosition ("test.scripting.position").
    /// </summary>
    struct test_position {
        conjure::f32 x = 0.0f;
        conjure::f32 y = 0.0f;
        conjure::f32 z = 0.0f;

        [[nodiscard]] bool operator==(const test_position &) const = default;
    };

    /// <summary>
    /// A second component so queries over two types and tag-like membership can be exercised.
    /// </summary>
    struct test_velocity {
        conjure::f32 x = 0.0f;
        conjure::f32 y = 0.0f;

        [[nodiscard]] bool operator==(const test_velocity &) const = default;
    };

    /// <summary>
    /// Mirrored by Conjure.TestScripts.TestFlags ("test.scripting.flags"): a bool and sub-word fields, whose
    /// managed layout (8 bytes) differs from the marshalled one (12 bytes).
    /// </summary>
    struct test_flags {
        bool active = false;
        conjure::u8 layer = 0;
        conjure::u16 mask = 0;
        conjure::f32 weight = 0.0f;

        [[nodiscard]] bool operator==(const test_flags &) const = default;
    };
}

CONJURE_REFLECT_STRUCT(scripting_test::test_position, "test.scripting.position",
    CONJURE_FIELD(x),
    CONJURE_FIELD(y),
    CONJURE_FIELD(z));
CONJURE_REFLECT_STRUCT(scripting_test::test_velocity, "test.scripting.velocity", CONJURE_FIELD(x), CONJURE_FIELD(y));
CONJURE_REFLECT_STRUCT(scripting_test::test_flags, "test.scripting.flags",
    CONJURE_FIELD(active),
    CONJURE_FIELD(layer),
    CONJURE_FIELD(mask),
    CONJURE_FIELD(weight));

CONJURE_COMPONENT(scripting_test::test_position, "test.scripting.position");
CONJURE_COMPONENT(scripting_test::test_velocity, "test.scripting.velocity");
CONJURE_COMPONENT(scripting_test::test_flags, "test.scripting.flags");

namespace scripting_test {

    /// <summary>
    /// Directory holding Conjure.Bootstrap.* and Conjure.Managed.dll; empty when the SDK was not built.
    /// </summary>
    [[nodiscard]] std::filesystem::path managed_directory();

    /// <summary>
    /// Conjure.TestScripts.dll of a variant ("v1", "v2", "v3"); empty when the SDK was not built.
    /// </summary>
    [[nodiscard]] std::filesystem::path test_scripts_assembly(std::string_view variant);

    /// <summary>
    /// The Conjure.TestScripts project file (for the managed build tests).
    /// </summary>
    [[nodiscard]] std::filesystem::path test_scripts_project();

    /// <summary>
    /// The Conjure.Bootstrap project file (for the self-contained publish test).
    /// </summary>
    [[nodiscard]] std::filesystem::path bootstrap_project();

    /// <summary>
    /// docs/generated/native_api_manifest.json.
    /// </summary>
    [[nodiscard]] std::filesystem::path manifest_path();

    /// <summary>
    /// True when the SDK was built with this build (CONJURE_SCRIPTING_HAS_DOTNET).
    /// </summary>
    [[nodiscard]] bool has_managed_sdk() noexcept;

    /// <summary>
    /// Registers script_component and the three test components (position, velocity, flags in that order).
    /// </summary>
    void register_test_components(conjure::ecs::component_registry &registry);

    /// <summary>
    /// A script_field_info built from its parts (default and meta as JSON text).
    /// </summary>
    [[nodiscard]] conjure::scripting::script_field_info make_field(
      std::string_view name,
      conjure::scripting::script_field_kind kind,
      std::string_view defaultJson = {},
      conjure::scripting::script_field_kind elementKind = conjure::scripting::script_field_kind::INT32,
      std::string_view metaJson = {}
    );

    /// <summary>
    /// A script_class_info with the given callbacks and fields (fields are sorted).
    /// </summary>
    [[nodiscard]] conjure::scripting::script_class_info make_class(
      std::string_view fullName,
      std::initializer_list<conjure::scripting::script_callback> callbacks,
      std::vector<conjure::scripting::script_field_info> fields = {}
    );

    /// <summary>
    /// fnv1a64 of a class or field name.
    /// </summary>
    [[nodiscard]] conjure::u64 hash_of(std::string_view name) noexcept;

    /// <summary>
    /// A parsed JSON document (REQUIREs valid text).
    /// </summary>
    [[nodiscard]] conjure::core::json::document json(std::string_view text);

    /// <summary>
    /// Registry + world + runtime over the null host, initialised and READY in EDIT mode with the world
    /// and registry attached. The runtime is declared last so it detaches before the world dies.
    /// </summary>
    struct scripting_fixture {
        conjure::ecs::component_registry registry = {};
        conjure::ecs::world world;          // no default: built on the registry in the constructor
        conjure::scripting::script_runtime runtime;    // no default: built on the host in the constructor

        scripting_fixture();
        ~scripting_fixture();

        scripting_fixture(const scripting_fixture &) = delete;
        scripting_fixture &operator=(const scripting_fixture &) = delete;

        /// <summary>
        /// The null host behind the runtime.
        /// </summary>
        [[nodiscard]] conjure::scripting::null_script_host &null_host() noexcept;

        /// <summary>
        /// Registers a fake class on the null host and loads the (empty) assembly so it becomes active.
        /// </summary>
        void load_fake_classes(std::vector<conjure::scripting::script_class_info> classes);

        /// <summary>
        /// A live entity.
        /// </summary>
        [[nodiscard]] conjure::ecs::entity create_entity();

        /// <summary>
        /// The value of one field of an entry's collected JSON (invalid when absent).
        /// </summary>
        [[nodiscard]] conjure::core::json::document fields_of(conjure::scripting::script_entry_id id);
    };

    /// <summary>
    /// Registry + world + runtime over the real CLR with Conjure.TestScripts v1 loaded in PLAY mode. Use
    /// CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME) before constructing one.
    /// </summary>
    struct dotnet_fixture {
        conjure::ecs::component_registry registry = {};
        conjure::ecs::world world;          // no default: built on the registry in the constructor
        conjure::scripting::script_runtime runtime;    // no default: built on the host in the constructor
        conjure::core::result<void> initialised = conjure::core::result<void>::success();

        dotnet_fixture(std::string_view variant = "v1", bool playMode = true);   // default-constructible fixture
        ~dotnet_fixture();

        dotnet_fixture(const dotnet_fixture &) = delete;
        dotnet_fixture &operator=(const dotnet_fixture &) = delete;

        [[nodiscard]] conjure::ecs::entity create_entity();

        /// <summary>
        /// Adds a script by class name with optional initial fields and flushes so the instance exists.
        /// </summary>
        [[nodiscard]] conjure::scripting::script_entry_id add_script(
          conjure::ecs::entity entity,
          std::string_view className,
          std::string_view fieldsJson = "{}"
        );

        /// <summary>
        /// Runs FIXED_UPDATE, UPDATE and LATE_UPDATE once (one frame of script phases).
        /// </summary>
        void run_frame(conjure::f32 deltaTime = 1.0f / 60.0f);

        /// <summary>
        /// The collected fields of an entry as a document ({} when the entry is unknown).
        /// </summary>
        [[nodiscard]] conjure::core::json::document fields_of(conjure::scripting::script_entry_id id);

        /// <summary>
        /// One field of an entry as i64 (fallback -1).
        /// </summary>
        [[nodiscard]] conjure::i64 field_i64(conjure::scripting::script_entry_id id, std::string_view name);

        [[nodiscard]] std::string field_string(conjure::scripting::script_entry_id id, std::string_view name);

        [[nodiscard]] conjure::f64 field_f64(conjure::scripting::script_entry_id id, std::string_view name);
    };
}
