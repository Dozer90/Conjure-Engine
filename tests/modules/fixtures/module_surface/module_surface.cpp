// Fixture: a plugin that compiles and links a representative slice of the public engine surface
// through the import side of conjure_shared (CONJURE_SHARED_IMPORTS). Every declaration it touches is
// defined in a .cpp of an engine static library and reaches this binary only through a CONJURE_API
// export, so the fixture fails to link, independently of tools/check_exports.py, whenever an
// annotation goes missing (JSON iteration, bit_vector operators, slot_map's detail helpers,
// CONJURE_ASSERT's reporter, string_id's name table, reflection's description finisher, chunk_view's
// column access, material_desc's comparison).
//
// conjure_surface_checksum() runs the slice and packs one small result per area into two decimal
// digits each (area i occupies 100^i), so a wrong total names the area that went wrong:
//
//   json      11   array elements 1+2+3 plus object keys "a","b" (1+1) and values 1+2
//   bits      12   popcounts of &, |, ^ and ~ (1 + 3 + 2 + 6)
//   slots      8   size after one erase (1) plus the surviving element (7)
//   string_id 20   length of the interned name "module_surface.probe" looked up through view()
//   reflect    5   to_json_document(): member count (2) plus the "value" member (3)
//   ecs       10   chunk_view: rows (4) plus the sum of the component values 0+1+2+3 (6)
//   material   1   two default material_desc compare equal (CONJURE_TEST_SURFACE_RENDERER builds only)

#include "conjure/modules/modules.h"

#include "conjure/core/assert.h"
#include "conjure/core/bit_vector.h"
#include "conjure/core/containers/slot_map.h"
#include "conjure/core/json/json.h"
#include "conjure/core/log.h"
#include "conjure/core/reflect/reflect.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/ecs/chunk_view.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/query.h"
#include "conjure/ecs/world.h"
#if CONJURE_TEST_SURFACE_RENDERER
    #include "conjure/renderer/material_asset.h"
#endif

#include <array>
#include <cstdint>
#include <string_view>

namespace conjure::modules::fixtures::surface {

    struct probe {
        i32 value = 3;
        f32 weight = 0.5f;
    };

    struct counter_component {
        u32 value = 0;
    };
}

CONJURE_REFLECT_STRUCT(conjure::modules::fixtures::surface::probe, "module_surface.probe",
    CONJURE_FIELD(value),
    CONJURE_FIELD(weight));
CONJURE_REFLECT_STRUCT(conjure::modules::fixtures::surface::counter_component, "module_surface.counter",
    CONJURE_FIELD(value));
CONJURE_COMPONENT(conjure::modules::fixtures::surface::counter_component, "module_surface.counter");

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;
using namespace conjure::modules::fixtures::surface;

namespace {
    CONJURE_DEFINE_LOG_CATEGORY(LOG_MODULE_SURFACE, "module_surface");

    constexpr u64 AREA_RADIX = 100;

    u64 json_term() {
        result<json::document> array = json::parse("[1,2,3]");
        CONJURE_VERIFY(array.has_value(), "module_surface: the array literal must parse");
        u64 term = 0;
        for (const json::value item : array->root()) {
            term += item.as_u64();
        }

        result<json::document> object = json::parse(R"({"a":1,"b":2})");
        CONJURE_VERIFY(object.has_value(), "module_surface: the object literal must parse");
        for (const json::member member : object->root().members()) {
            term += member.key.size() + member.item.as_u64();
        }
        return term;
    }

    u64 bits_term() {
        bit_vector left(8);
        left.set(0);
        left.set(1);
        bit_vector right(8);
        right.set(1);
        right.set(2);
        return (left & right).count() + (left | right).count() + (left ^ right).count() + (~left).count();
    }

    u64 slots_term() {
        slot_map<u32> map = {};
        const slot_key first = map.insert(5u);
        const slot_key second = map.insert(7u);
        CONJURE_VERIFY(map.erase(first), "module_surface: the first key must still be live");
        CONJURE_ASSERT(!map.contains(first), "module_surface: an erased key must be stale");
        const u32 *survivor = map.find(second);
        return map.size() + (survivor == nullptr ? 0u : *survivor);
    }

    u64 string_id_term() {
        const string_id id("module_surface.probe");
        return id.view().size();
    }

    u64 reflect_term() {
        const probe value = {};
        result<json::document> document = reflect::to_json_document(value);
        CONJURE_VERIFY(document.has_value(), "module_surface: a reflected struct must serialise");
        return document->root().size() + document->root()["value"].as_u64();
    }

    u64 ecs_term() {
        ecs::component_registry registry = {};
        CONJURE_VERIFY(registry.register_component<counter_component>().has_value(),
            "module_surface: the component must register");
        ecs::world world(registry);

        ecs::component_mask mask = {};
        mask.set(registry.id_of<counter_component>());
        std::array<ecs::entity, 4> entities = {};
        CONJURE_VERIFY(world.create_entities(mask, entities).has_value(), "module_surface: entities must spawn");
        for (u32 index = 0; index < entities.size(); ++index) {
            CONJURE_VERIFY(world.set(entities[index], counter_component{index}).has_value(),
                "module_surface: the component must be set");
        }

        ecs::query_desc desc = {};
        desc.with = mask;
        ecs::query counters = world.create_query(desc);
        u64 term = 0;
        counters.for_each_chunk([&term](ecs::chunk_view &chunk) {
            term += chunk.count();
            for (const counter_component &counter : chunk.get<const counter_component>()) {
                term += counter.value;
            }
        });
        return term;
    }

    u64 material_term() {
#if CONJURE_TEST_SURFACE_RENDERER
        const renderer::material_desc left = {};
        const renderer::material_desc right = {};
        return left == right ? 1u : 0u;
#else
        return 0;
#endif
    }

    module_entry_status surface_entry(module_context &) {
        CONJURE_LOG_INFO(LOG_MODULE_SURFACE, "module_surface entry running");
        return module_entry_status::OK;
    }
}

extern "C" CONJURE_MODULE_EXPORT std::uint64_t conjure_surface_checksum(void) {
    const std::array<u64, 7> terms = {
        json_term(),
        bits_term(),
        slots_term(),
        string_id_term(),
        reflect_term(),
        ecs_term(),
        material_term(),
    };
    u64 checksum = 0;
    u64 scale = 1;
    for (const u64 term : terms) {
        CONJURE_ASSERT(term < AREA_RADIX, "module_surface: an area result must fit two decimal digits");
        checksum += term * scale;
        scale *= AREA_RADIX;
    }
    return checksum;
}

CONJURE_DECLARE_MODULE(surface_entry)
