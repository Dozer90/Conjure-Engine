// The sample plugin of docs/design/08_testing.md 6.1 test_module_plugin: one reflected component
// (integration.plugin_counter) and one PARALLEL system that increments it every UPDATE, both registered through
// the module records of docs/design/01_ecs.md 4.7 so the engine's ecs_module_consumer turns them into a
// component type and a scheduled system (R2, R19). Built by tests/integration/CMakeLists.txt with
// conjure_add_game_module and loaded through a project's modules.search_paths.

#include "conjure/modules/modules.h"

#include "conjure/core/reflect/reflect.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/ecs/chunk_view.h"
#include "conjure/ecs/command_buffer.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/component_type_desc.h"
#include "conjure/ecs/system.h"
#include "conjure/ecs/system_phase.h"
#include "conjure/ecs/view.h"

#include <span>

namespace sample_plugin {

    /// <summary>
    /// Counts the frames the plugin's system has seen the entity.
    /// </summary>
    struct plugin_counter {
        conjure::u32 count = 0;
    };
}

CONJURE_REFLECT_STRUCT(sample_plugin::plugin_counter, "integration.plugin_counter", CONJURE_FIELD(count));
CONJURE_COMPONENT(sample_plugin::plugin_counter, "integration.plugin_counter");

namespace {
    using namespace conjure;
    using namespace conjure::modules;

    constexpr core::string_id COUNTER_NAME("integration.plugin_counter");
    const core::string_id COUNTER_WRITES[] = {COUNTER_NAME};

    void count_frames(ecs::system_context &context) {
        ecs::view<sample_plugin::plugin_counter> counters = context.view<sample_plugin::plugin_counter>();
        counters.parallel_for_each_chunk(context, [](ecs::chunk_view &chunk, ecs::command_buffer &) {
            for (sample_plugin::plugin_counter &counter : chunk.get<sample_plugin::plugin_counter>()) {
                ++counter.count;
            }
        });
    }

    module_entry_status plugin_entry(module_context &context) {
        module_registrar &registrar = context.registrar();
        registrar.register_extension(ecs::describe_component<sample_plugin::plugin_counter>());

        ecs::system_extension_desc system = {};
        system.name = "integration.plugin_count_frames";
        system.phase = ecs::system_phase::UPDATE;
        system.flags = ecs::system_flags::NONE;
        system.writes = COUNTER_WRITES;
        system.writeCount = 1;
        system.run = &count_frames;
        registrar.register_extension(system);
        return module_entry_status::OK;
    }
}

CONJURE_DECLARE_MODULE(plugin_entry)
