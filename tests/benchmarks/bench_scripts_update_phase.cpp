// Scripting scenarios (docs/design/08_testing.md 7.1, 03_scripting.md 10) on the real CLR: the batched
// managed UPDATE of 10,000 Conjure.TestScripts.Mover instances, the Span<T> variant (one PositionWriter
// walking 10,000 native positions through a DOD query scope) and the native-call overhead of one
// NativeCallProbe making 100,000 Entity.IsAlive calls per update (the design row's Entity.IsValid; IsAlive
// is the API's spelling of the native liveness check). All three skip (exit 77 under CTest) when the
// managed SDK was not built or no .NET runtime is installed.

#include "bench_support.h"

#include "conjure/bench/benchmark.h"
#include "conjure/core/json/document.h"
#include "conjure/core/reflect/reflect.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/world.h"
#include "conjure/scripting/native_api.h"
#include "conjure/scripting/script_class_registry.h"
#include "conjure/scripting/script_component.h"
#include "conjure/scripting/script_host.h"
#include "conjure/scripting/script_runtime.h"

#include <string>
#include <string_view>
#include <vector>

using namespace conjure;
using namespace conjure::bench;
using namespace conjure::bench::support;

namespace bench_scripting {

    /// <summary>
    /// Mirrored by Conjure.TestScripts.TestPosition ("test.scripting.position"): what PositionWriter walks.
    /// </summary>
    struct test_position {
        f32 x = 0.0f;
        f32 y = 0.0f;
        f32 z = 0.0f;
    };
}

CONJURE_REFLECT_STRUCT(bench_scripting::test_position, "test.scripting.position",
    CONJURE_FIELD(x),
    CONJURE_FIELD(y),
    CONJURE_FIELD(z));
CONJURE_COMPONENT(bench_scripting::test_position, "test.scripting.position");

namespace {

    constexpr std::string_view MOVER_CLASS = "Conjure.TestScripts.Mover";
    constexpr std::string_view POSITION_WRITER_CLASS = "Conjure.TestScripts.PositionWriter";
    constexpr std::string_view NATIVE_CALL_PROBE_CLASS = "Conjure.TestScripts.NativeCallProbe";
    constexpr u32 NATIVE_CALLS_PER_UPDATE = 100000;     // NativeCallProbe.CallsPerUpdate
    constexpr f32 STEP_SECONDS = 1.0f / 60.0f;

    /// <summary>
    /// Registry (script_component + the position mirror), world and a script_runtime over the real CLR
    /// with Conjure.TestScripts v1 loaded in PLAY mode. The runtime is declared after the world so it
    /// detaches first.
    /// </summary>
    class dotnet_fixture {
        public:
            ecs::component_registry registry = {};
            ecs::world world;                       // no default: built on the registry
            scripting::script_runtime runtime;      // no default: built on the host
            std::string failure = {};

        public:
            dotnet_fixture() :
                world((register_all(registry), registry)),
                runtime(scripting::create_script_host(scripting::script_backend::DOTNET)) {}

            ~dotnet_fixture() {
                runtime.shutdown();
            }

            dotnet_fixture(const dotnet_fixture &) = delete;
            dotnet_fixture &operator=(const dotnet_fixture &) = delete;

        public:
            static void register_all(ecs::component_registry &target) {
                CONJURE_UNUSED(scripting::register_script_component(target));
                CONJURE_UNUSED(target.register_component<bench_scripting::test_position>());
            }

            /// <summary>
            /// Starts the CLR and loads the test scripts; 'failure' names the problem when false.
            /// </summary>
            [[nodiscard]] bool initialise() {
                runtime.attach_registry(&registry);
                runtime.attach_world(&world);
                scripting::script_runtime_config config = {};
                config.host.backend = scripting::script_backend::DOTNET;
                config.host.managedDirectory = managed_directory();
                config.gameAssemblyPath = test_scripts_assembly();
                config.initialMode = scripting::script_mode::PLAY;
                const core::result<void> initialised = runtime.initialise(config);
                if (!initialised.has_value()) {
                    failure = "script runtime: " + initialised.get_error().to_string();
                    return false;
                }
                if (runtime.status() != scripting::script_host_status::READY) {
                    failure = "script runtime is not READY after initialisation";
                    return false;
                }
                return true;
            }

            /// <summary>
            /// A live entity, NULL_ENTITY when the world refuses.
            /// </summary>
            [[nodiscard]] ecs::entity create_entity() {
                const core::result<ecs::entity> created = world.create_entity();
                return created.has_value() ? created.value() : ecs::NULL_ENTITY;
            }

            /// <summary>
            /// Adds a script by class name with default fields; false when the runtime refused.
            /// </summary>
            [[nodiscard]] bool add_script(ecs::entity entity, std::string_view className) {
                const core::json::document fields = core::json::document::object();
                const u64 classHash = scripting::script_name_hash(className);
                return runtime.add_script(entity, classHash, fields.root()).is_valid();
            }
    };

    /// <summary>
    /// Skips the scenario unless the CLR can be started; true when it can.
    /// </summary>
    [[nodiscard]] bool require_dotnet(bench_context &context) {
        std::string reason = {};
        if (!dotnet_available(reason)) {
            context.skip(reason);
            return false;
        }
        return true;
    }
}

CONJURE_BENCHMARK(scripting.update_10k) {
    if (!require_dotnet(context)) {
        return;
    }
    dotnet_fixture fixture = {};
    if (!fixture.initialise()) {
        context.skip(fixture.failure);
        return;
    }
    const u32 count = context.entity_count();
    for (u32 index = 0; index < count; ++index) {
        const ecs::entity entity = fixture.create_entity();
        if (entity == ecs::NULL_ENTITY || !fixture.add_script(entity, MOVER_CLASS)) {
            context.skip("the runtime refused a Mover script");
            return;
        }
    }
    fixture.runtime.flush_pending();
    if (fixture.runtime.stats().liveInstances != count) {
        context.skip("not every Mover instance was created");
        return;
    }

    context.set_items(count);
    context.measure([&]() {
        fixture.runtime.run_phase(scripting::script_phase::UPDATE, STEP_SECONDS);
    });
}

CONJURE_BENCHMARK(scripting.update_10k_dod) {
    if (!require_dotnet(context)) {
        return;
    }
    dotnet_fixture fixture = {};
    if (!fixture.initialise()) {
        context.skip(fixture.failure);
        return;
    }
    const u32 count = context.entity_count();
    for (u32 index = 0; index < count; ++index) {
        const ecs::entity entity = fixture.create_entity();
        if (entity == ecs::NULL_ENTITY || !fixture.world.add<bench_scripting::test_position>(entity).has_value()) {
            context.skip("the world refused a position entity");
            return;
        }
    }
    const ecs::entity writer = fixture.create_entity();
    if (writer == ecs::NULL_ENTITY || !fixture.add_script(writer, POSITION_WRITER_CLASS)) {
        context.skip("the runtime refused the PositionWriter script");
        return;
    }
    fixture.runtime.flush_pending();
    if (fixture.runtime.stats().liveInstances != 1) {
        context.skip("the PositionWriter instance was not created");
        return;
    }

    context.set_items(count);
    context.measure([&]() {
        fixture.runtime.run_phase(scripting::script_phase::UPDATE, STEP_SECONDS);
    });
}

CONJURE_BENCHMARK(scripting.native_call_100k) {
    if (!require_dotnet(context)) {
        return;
    }
    dotnet_fixture fixture = {};
    if (!fixture.initialise()) {
        context.skip(fixture.failure);
        return;
    }
    const ecs::entity probe = fixture.create_entity();
    if (probe == ecs::NULL_ENTITY || !fixture.add_script(probe, NATIVE_CALL_PROBE_CLASS)) {
        context.skip("the runtime refused the NativeCallProbe script");
        return;
    }
    fixture.runtime.flush_pending();
    if (fixture.runtime.stats().liveInstances != 1) {
        context.skip("the NativeCallProbe instance was not created");
        return;
    }

    // One UPDATE is the probe's 100,000 native calls: the per-call overhead of the managed -> native bridge.
    context.set_items(NATIVE_CALLS_PER_UPDATE);
    context.measure([&]() {
        fixture.runtime.run_phase(scripting::script_phase::UPDATE, STEP_SECONDS);
    });
}
