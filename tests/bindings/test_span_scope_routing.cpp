#include "conjure/testing/doctest.h"

#include "conjure/testing/allocation_scope.h"
#include "conjure/testing/approx.h"
#include "conjure/testing/log_capture.h"
#include "bindings_test_support.h"

#include "conjure/bindings/bindings_error.h"
#include "conjure/bindings/marshal.h"
#include "conjure/ecs/command_buffer.h"
#include "conjure/scene/components.h"
#include "conjure/scene/scene.h"
#include "conjure/scripting/native_api.h"
#include "conjure/scripting/script_runtime.h"
#include "bindings/entry_support.h"

#include <array>
#include <utility>
#include <vector>

// The R5 routing (docs/design/15_bindings.md 3.3, B7): structural requests made inside a DOD query scope land
// in script_runtime::deferred_commands(), the world does not change until query_end flushes them, the spans
// handed to managed code stay valid meanwhile and non-structural writes land at once.

using namespace conjure;
using namespace conjure::bindings;
using namespace conjure::testing;
using namespace bindings_test;

namespace {

    struct routing_fixture {
        bare_fixture base = {};
        std::array<u32, 1> transformIds = {};
        assets::asset_id prefab = {};

        routing_fixture() {
            transformIds[0] = base.registry.id_of<scene::local_transform>().value;
            const ecs::entity source = base.create("prefab_root");
            static_cast<void>(base.create("prefab_child", source));
            prefab = base.publish_prefab(source, "lamp", 1);
            base.load_now(prefab);
            CONJURE_REQUIRE_OK(base.active().destroy_now(source));
        }

        /// <summary>
        /// Runs 'body' inside UPDATE with a query scope over local_transform open; closes it unless told not to.
        /// </summary>
        template <typename Body>
        void inside_scope(Body &&body, bool close = true) {
            conjure_native_api &api = base.api();
            base.null_host().set_phase_hook([&](scripting::script_phase, f32) {
                conjure_query_scope scope = {};
                REQUIRE(api.query_begin(transformIds.data(), 1, &scope) == 0);
                REQUIRE(base.world.is_locked());
                body(scope);
                if (close) {
                    api.query_end(scope.scope_id);
                    CHECK_FALSE(base.world.is_locked());
                }
            });
            base.runtime->run_phase(scripting::script_phase::UPDATE, 1.0f / 60.0f);
            base.null_host().set_phase_hook({});
        }
    };
}

TEST_SUITE("bindings/span_scope_routing") {

    TEST_CASE("span routing - structural requests inside a scope land in the deferred commands") {
        log_capture logs = {};
        routing_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const ecs::entity parent = fixture.base.create("parent");
        const ecs::entity child = fixture.base.create("child");
        const ecs::entity toggled = fixture.base.create("toggled");
        const conjure_uuid prefab = uuid_of(fixture.prefab);
        const u32 entitiesBefore = fixture.base.world.entity_count();
        uint64_t root = 0;
        std::array<uint64_t, 3> created = {};
        fixture.inside_scope([&](conjure_query_scope &) {
            ecs::command_buffer &commands = fixture.base.runtime->deferred_commands();
            CHECK(commands.command_count() == 0);
            CHECK(api.prefab_instantiate(&prefab, parent.to_u64(), nullptr, &root) == 0);
            CHECK(root != 0);
            CHECK(commands.command_count() > 0);
            const u32 afterPrefab = commands.command_count();
            // Capturing a subtree is structural: a duplicate has no command form inside a scope (B7).
            CHECK(api.entity_duplicate(child.to_u64()) == 0);
            CHECK(last_error_code() == bindings_error::STRUCTURE_LOCKED);
            CHECK(commands.command_count() == afterPrefab);
            CHECK(api.entity_create_many(3, parent.to_u64(), created.data()) == 3);
            CHECK(api.transform_set_parent(child.to_u64(), parent.to_u64()) == 0);
            CHECK(api.entity_set_enabled(toggled.to_u64(), 0) == 0);
            const utf8_argument tag = utf8("Deferred");
            CHECK(api.entity_set_tag(toggled.to_u64(), tag.data, tag.length) == 0);
            CHECK(api.entity_set_layer(toggled.to_u64(), 4) == 0);
            // The world is untouched while the scope is open.
            CHECK(fixture.base.world.entity_count() == entitiesBefore);
            CHECK(fixture.base.active().tree().parent_of(child).is_null());
            CHECK(fixture.base.active().is_enabled(toggled));
            CHECK_FALSE(fixture.base.world.has<scene::tag>(toggled));
            CHECK(commands.command_count() >= 7);
            CHECK(bindings::detail::pending_fixup_count() == 6);        // root, three children, reparent, enabled
        });
        // query_end flushed the commands and applied the scene fixups: instance, three children, reparent, tag.
        CHECK(bindings::detail::pending_fixup_count() == 0);
        CHECK(fixture.base.world.entity_count() == entitiesBefore + 2 + 3);
        CHECK(fixture.base.world.is_alive(marshal::to_entity(root)));
        for (const uint64_t packed : created) {
            CHECK(fixture.base.world.is_alive(marshal::to_entity(packed)));
            CHECK(fixture.base.active().tree().parent_of(marshal::to_entity(packed)) == parent);
        }
        CHECK(fixture.base.active().tree().parent_of(child) == parent);
        CHECK(fixture.base.active().tree().parent_of(marshal::to_entity(root)) == parent);
        CHECK(fixture.base.active().tree().children_of(parent).size() == 5);
        CHECK_FALSE(fixture.base.active().is_enabled(toggled));
        CHECK(fixture.base.active().tag_of(toggled) == core::string_id("Deferred"));
        CHECK(fixture.base.active().layer_of(toggled) == 4);
        CHECK(fixture.base.world.has<scene::prefab_instance>(marshal::to_entity(root)));
        // The TRANSFORM pass links the instance's inner entities (their archive parents) the same frame.
        fixture.base.step();
        CHECK(fixture.base.active().tree().children_of(marshal::to_entity(root)).size() == 1);
        CONJURE_CHECK_OK(fixture.base.active().validate());
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("span routing - a span over 1000 transforms stays valid until query_end") {
        routing_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        std::vector<ecs::entity> movers = {};
        for (u32 index = 0; index < 1000; ++index) {
            movers.push_back(fixture.base.create("mover"));
        }
        const conjure_uuid prefab = uuid_of(fixture.prefab);
        std::array<uint64_t, 4> created = {};
        fixture.inside_scope([&](conjure_query_scope &scope) {
            REQUIRE(scope.chunk_count > 0);
            void *const column = scope.chunks[0].columns[0];
            const void *const firstRow =
              fixture.base.world.get_raw(movers[0], fixture.base.registry.id_of<scene::local_transform>());
            REQUIRE(column != nullptr);
            u32 rows = 0;
            for (int32_t chunk = 0; chunk < scope.chunk_count; ++chunk) {
                rows += static_cast<u32>(scope.chunks[static_cast<usize>(chunk)].count);
            }
            CHECK(rows >= 1000);
            uint64_t root = 0;
            CHECK(api.prefab_instantiate(&prefab, 0, nullptr, &root) == 0);
            CHECK(api.entity_create_many(4, 0, created.data()) == 4);
            CHECK(api.entity_duplicate(movers[5].to_u64()) == 0);           // no command form: refused
            // Nothing moved: the first chunk's column and the first entity's row are where they were.
            CHECK(scope.chunks[0].columns[0] == column);
            CHECK(
              fixture.base.world.get_raw(movers[0], fixture.base.registry.id_of<scene::local_transform>()) == firstRow
            );
            CHECK(fixture.base.world.is_alive(movers[999]));
        });
        for (const uint64_t packed : created) {
            CHECK(fixture.base.world.is_alive(marshal::to_entity(packed)));
        }
        fixture.base.step();
        CONJURE_CHECK_OK(fixture.base.active().validate());
    }

    TEST_CASE("span routing - query_end flushes the requests in order") {
        routing_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const usize rootsBefore = fixture.base.active().roots().size();
        std::array<uint64_t, 3> first = {};
        std::array<uint64_t, 2> second = {};
        fixture.inside_scope([&](conjure_query_scope &) {
            CHECK(api.entity_create_many(3, 0, first.data()) == 3);
            CHECK(api.entity_create_many(2, 0, second.data()) == 2);
            // A reserved id is not alive until the flush, yet the entity setters record on it behind the create,
            // as the core component_* / script_add / entity_destroy entries do (code sweep OWN-P2).
            const utf8_argument tag = utf8("First");
            CHECK(api.entity_set_tag(first[0], tag.data, tag.length) == 0);
            CHECK(api.entity_get_tag(first[0]) == 0);
        });
        CHECK(fixture.base.active().tag_of(marshal::to_entity(first[0])) == core::string_id("First"));
        const std::span<const scene::root_entry> roots = fixture.base.active().roots();
        REQUIRE(roots.size() == rootsBefore + 5);
        CHECK(roots[rootsBefore].entity == marshal::to_entity(first[0]));
        CHECK(roots[rootsBefore + 2].entity == marshal::to_entity(first[2]));
        CHECK(roots[rootsBefore + 3].entity == marshal::to_entity(second[0]));
        CHECK(roots[rootsBefore + 4].entity == marshal::to_entity(second[1]));
        fixture.base.step();
        CONJURE_CHECK_OK(fixture.base.active().validate());
    }

    TEST_CASE("span routing - non-structural writes apply immediately inside a scope") {
        routing_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const ecs::entity mover = fixture.base.create("mover");
        const utf8_argument tag = utf8("Tagged");
        REQUIRE(api.entity_set_tag(mover.to_u64(), tag.data, tag.length) == 0);
        fixture.inside_scope([&](conjure_query_scope &) {
            conjure_transform local = {};
            REQUIRE(api.transform_get_local(mover.to_u64(), &local) == 0);
            local.position.x = 5.0f;
            CHECK(api.transform_set_local(mover.to_u64(), &local) == 0);
            CHECK(fixture.base.world.get<scene::local_transform>(mover)->position.x == 5.0f);
            const utf8_argument retagged = utf8("Retagged");
            CHECK(api.entity_set_tag(mover.to_u64(), retagged.data, retagged.length) == 0);   // present: in place
            CHECK(fixture.base.active().tag_of(mover) == core::string_id("Retagged"));
            CHECK(api.entity_set_layer(mover.to_u64(), 3) == 0);                               // absent: deferred
            CHECK_FALSE(fixture.base.world.has<scene::layer>(mover));
            CHECK(fixture.base.runtime->deferred_commands().command_count() == 1);
        });
        CHECK(fixture.base.active().layer_of(mover) == 3);
    }

    TEST_CASE("span routing - instantiate inside a scope returns a real id that is alive after the flush") {
        routing_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const conjure_uuid prefab = uuid_of(fixture.prefab);
        uint64_t root = 0;
        bool reservedInside = false;
        fixture.inside_scope([&](conjure_query_scope &) {
            conjure_transform local = {};
            local.position = {1.0f, 2.0f, 3.0f};
            local.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
            local.scale = {1.0f, 1.0f, 1.0f};
            CHECK(api.prefab_instantiate(&prefab, 0, &local, &root) == 0);
            REQUIRE(root != 0);
            // Reserved, not yet alive: the id is real and may be stored, the components appear at query_end.
            reservedInside = !fixture.base.world.is_alive(marshal::to_entity(root));
        });
        CHECK(reservedInside);
        const ecs::entity instance = marshal::to_entity(root);
        REQUIRE(fixture.base.world.is_alive(instance));
        CHECK(fixture.base.active().tree().parent_of(instance).is_null());
        CHECK(fixture.base.active().root_owner(instance).has_value());        // adopted as a scene root
        CHECK(api.prefab_is_instance_root(root) == 1);
        conjure_uuid source = {};
        CHECK(api.prefab_source_of(root, &source) == 0);
        CHECK(marshal::to_uuid(source) == fixture.prefab);
        conjure_transform read = {};
        CHECK(api.transform_get_local(root, &read) == 0);
        CHECK(read.position.z == 3.0f);
        fixture.base.step();
        CHECK(fixture.base.active().tree().children_of(instance).size() == 1);
    }

    TEST_CASE("regression - code sweep OWN-P2 ids create_many and instantiate reserve take the core requests") {
        // The bindings reserve ids in the runtime's deferred buffer too: the core component_* / script_add /
        // entity_destroy entries record their requests on them behind the create, as on an Entity.Create id.
        log_capture logs = {};
        routing_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const u32 layerType = fixture.base.registry.id_of<scene::layer>().value;
        const u32 tagType = fixture.base.registry.id_of<scene::tag>().value;
        const u32 transformType = fixture.transformIds[0];
        const scene::layer third = {3};
        const scene::layer fifth = {5};
        const u64 scriptClass = 0x5C21u;

        SUBCASE("create_many ids") {
            std::array<uint64_t, 2> created = {};
            fixture.inside_scope([&](conjure_query_scope &) {
                REQUIRE(api.entity_create_many(2, 0, created.data()) == 2);
                CHECK(fixture.base.runtime->is_deferred_reserved(marshal::to_entity(created[0])));
                CHECK(api.component_add(created[0], layerType, &third) == 1);
                CHECK(api.script_add(created[0], scriptClass) == 0);
                CHECK(api.component_remove(created[1], layerType) == 0);        // nothing recorded to remove
                CHECK(api.component_add(created[1], layerType, &fifth) == 1);
                CHECK(api.component_remove(created[1], layerType) == 1);
                CHECK(api.component_remove(created[1], transformType) == 1);    // added by create_many itself
                CHECK(api.component_add(created[1], transformType, nullptr) == 1);
                CHECK(api.entity_destroy(created[1]) == 1);
            });
            const ecs::entity kept = marshal::to_entity(created[0]);
            REQUIRE(fixture.base.world.is_alive(kept));
            REQUIRE(fixture.base.world.has<scene::layer>(kept));
            CHECK(*std::as_const(fixture.base.world).get<scene::layer>(kept) == third);
            CHECK(fixture.base.runtime->scripts_of(kept).size() == 1);
            CHECK(fixture.base.active().root_owner(kept).has_value());           // the ATTACH fixup still applied
            CHECK_FALSE(fixture.base.world.is_alive(marshal::to_entity(created[1])));
            CHECK(bindings::detail::pending_fixup_count() == 0);
        }
        SUBCASE("a deferred instantiation root") {
            const conjure_uuid prefab = uuid_of(fixture.prefab);
            uint64_t root = 0;
            conjure_transform moved = {};
            moved.position = {4.0f, 5.0f, 6.0f};
            moved.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
            moved.scale = {1.0f, 1.0f, 1.0f};
            const scene::local_transform movedLocal =
              scene::local_transform::from_transform(marshal::to_transform(moved));
            fixture.inside_scope([&](conjure_query_scope &) {
                REQUIRE(api.prefab_instantiate(&prefab, 0, nullptr, &root) == 0);
                CHECK(fixture.base.runtime->is_deferred_reserved(marshal::to_entity(root)));
                CHECK(api.component_add(root, layerType, &fifth) == 1);
                CHECK(api.component_set(root, transformType, &movedLocal) == 1);
                CHECK(api.component_remove(root, tagType) == 1);                // the archive's components are unknown
                CHECK(api.script_add(root, scriptClass) == 0);
            });
            const ecs::entity instance = marshal::to_entity(root);
            REQUIRE(fixture.base.world.is_alive(instance));
            REQUIRE(fixture.base.world.has<scene::layer>(instance));
            CHECK(*std::as_const(fixture.base.world).get<scene::layer>(instance) == fifth);
            conjure_transform read = {};
            CHECK(api.transform_get_local(root, &read) == 0);
            CHECK(read.position.y == 5.0f);
            CHECK(fixture.base.runtime->scripts_of(instance).size() == 1);
            CHECK(api.prefab_is_instance_root(root) == 1);
        }
        SUBCASE("the name, tag, layer and enabled setters record on a reserved id") {
            std::array<uint64_t, 2> created = {};
            const utf8_argument name = utf8("spawned_name");
            const utf8_argument tag = utf8("Spawned");
            const utf8_argument doomedName = utf8("doomed_name");
            const usize namesBefore = fixture.base.runtime->entity_name_count();
            fixture.inside_scope([&](conjure_query_scope &) {
                REQUIRE(api.entity_create_many(2, 0, created.data()) == 2);
                CHECK(api.entity_set_name(created[0], name.data, name.length) == 0);
                CHECK(api.entity_set_tag(created[0], tag.data, tag.length) == 0);
                CHECK(api.entity_set_layer(created[0], 4) == 0);
                CHECK(api.entity_set_enabled(created[0], 0) == 0);
                CHECK(api.entity_set_layer(created[0], 32) == to_status(bindings_error::INVALID_ARGUMENT));
                // Reads keep answering as for a dead entity until the flush.
                CHECK(api.entity_get_tag(created[0]) == 0);
                CHECK(api.entity_get_layer(created[0]) == to_status(bindings_error::ENTITY_NOT_ALIVE));
                // A destroy recorded later wins over the setters, and the name goes with the id.
                CHECK(api.entity_set_name(created[1], doomedName.data, doomedName.length) == 0);
                CHECK(api.entity_set_enabled(created[1], 0) == 0);
                CHECK(api.entity_destroy(created[1]) == 1);
            });
            const ecs::entity kept = marshal::to_entity(created[0]);
            REQUIRE(fixture.base.world.is_alive(kept));
            REQUIRE(fixture.base.world.has<scene::name>(kept));
            CHECK(std::as_const(fixture.base.world).get<scene::name>(kept)->value == core::string_id("spawned_name"));
            CHECK(fixture.base.runtime->entity_name(kept) == "spawned_name");
            CHECK(fixture.base.active().tag_of(kept) == core::string_id("Spawned"));
            CHECK(fixture.base.active().layer_of(kept) == 4);
            CHECK_FALSE(fixture.base.active().is_enabled(kept));
            CHECK_FALSE(fixture.base.world.is_alive(marshal::to_entity(created[1])));
            CHECK(fixture.base.runtime->entity_name_count() == namesBefore + 1);
            CHECK(bindings::detail::pending_fixup_count() == 0);
            CONJURE_CHECK_OK(fixture.base.active().validate());
        }
        SUBCASE("a deferred instantiation root takes a destroy in call order: no part of the instance is created") {
            // Owner ruling 2026-11-24: the destroy is recorded behind the instantiation and the requests on the
            // root, and the flush cancels the copy, so the child does not outlive its root (the scene links the
            // inner entities to the root only after the flush) and the root's ATTACH fixup finds nothing to adopt.
            const conjure_uuid prefab = uuid_of(fixture.prefab);
            const u32 entitiesBefore = fixture.base.world.entity_count();
            const usize rootsBefore = fixture.base.active().roots().size();
            uint64_t root = 0;
            uint64_t kept = 0;
            fixture.inside_scope([&](conjure_query_scope &) {
                REQUIRE(api.prefab_instantiate(&prefab, 0, nullptr, &root) == 0);
                CHECK(fixture.base.runtime->is_deferred_instance(marshal::to_entity(root)));
                CHECK(api.component_add(root, layerType, &fifth) == 1);
                CHECK(api.entity_destroy(root) == 1);
                REQUIRE(api.prefab_instantiate(&prefab, 0, nullptr, &kept) == 0);
            });
            CHECK_FALSE(fixture.base.world.is_alive(marshal::to_entity(root)));
            REQUIRE(fixture.base.world.is_alive(marshal::to_entity(kept)));
            CHECK(fixture.base.world.entity_count() == entitiesBefore + 2);       // the kept instance only
            CHECK(fixture.base.active().roots().size() == rootsBefore + 1);
            CHECK(bindings::detail::pending_fixup_count() == 0);
            fixture.base.step();
            CHECK(fixture.base.active().tree().children_of(marshal::to_entity(kept)).size() == 1);
            CONJURE_CHECK_OK(fixture.base.active().validate());
        }
        CHECK(logs.count(core::log_level::ERR) == 0);
        CONJURE_CHECK_OK(fixture.base.world.validate());
    }

    TEST_CASE("span routing - a request on an entity dead at the flush is skipped") {
        log_capture logs = {};
        routing_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const ecs::entity doomed = fixture.base.create("doomed");
        const u32 before = fixture.base.world.entity_count();
        fixture.inside_scope([&](conjure_query_scope &) {
            CHECK(api.entity_set_layer(doomed.to_u64(), 2) == 0);           // deferred add
            CHECK(api.entity_set_enabled(doomed.to_u64(), 0) == 0);         // deferred add
            CHECK(api.entity_destroy(doomed.to_u64()) == 1);                // deferred destroy wins
            CHECK(fixture.base.world.is_alive(doomed));
        });
        CHECK_FALSE(fixture.base.world.is_alive(doomed));
        CHECK(fixture.base.world.entity_count() == before - 1);
        CHECK(bindings::detail::pending_fixup_count() == 0);               // the dead entity's fixup was dropped
        CHECK(logs.count(core::log_level::ERR) == 0);
        CONJURE_CHECK_OK(fixture.base.world.validate());
    }

    TEST_CASE("span routing - a scope left open at phase end is force-closed and flushed") {
        log_capture logs = {};
        routing_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const u32 before = fixture.base.world.entity_count();
        std::array<uint64_t, 2> created = {};
        fixture.inside_scope(
          [&](conjure_query_scope &) {
              CHECK(api.entity_create_many(2, 0, created.data()) == 2);
          },
          false
        );
        CHECK_FALSE(fixture.base.world.is_locked());
        CHECK(fixture.base.runtime->open_query_scope_count() == 0);
        CHECK(fixture.base.world.entity_count() == before + 2);
        CHECK(fixture.base.world.is_alive(marshal::to_entity(created[1])));
        CHECK(logs.contains(core::log_level::WARN, "force-closed"));
        // No query_end ran: the scene fixups wait for the next entry call (or begin_frame) to adopt the roots.
        CHECK(bindings::detail::pending_fixup_count() == 2);
        CHECK(api.entity_get_parent(created[0]) == 0);
        CHECK(bindings::detail::pending_fixup_count() == 0);
        CHECK(fixture.base.active().root_owner(marshal::to_entity(created[0])).has_value());
    }

    TEST_CASE("span routing - disable then enable inside a scope leaves the entity enabled") {
        log_capture logs = {};
        routing_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const ecs::entity lamp = fixture.base.create("lamp");
        const ecs::entity bulb = fixture.base.create("bulb", lamp);
        const u32 warningsBefore = logs.count(core::log_level::WARN);
        fixture.inside_scope([&](conjure_query_scope &) {
            CHECK(api.entity_set_enabled(lamp.to_u64(), 0) == 0);
            CHECK(api.entity_set_enabled(lamp.to_u64(), 1) == 0);
            // Both are queued: the live state cannot change while the scope holds the world.
            CHECK(bindings::detail::pending_fixup_count() == 2);
            CHECK(api.entity_is_enabled(lamp.to_u64()) == 1);                // the pre-flush state
        });
        CHECK(bindings::detail::pending_fixup_count() == 0);
        CHECK(fixture.base.active().is_enabled(lamp));
        CHECK_FALSE(fixture.base.world.has<scene::inactive>(bulb));
        CONJURE_CHECK_OK(fixture.base.active().validate());
        CHECK(logs.count(core::log_level::WARN) == warningsBefore);           // neither fixup was refused
    }

    TEST_CASE("span routing - enable then disable inside a scope leaves the entity disabled") {
        routing_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const ecs::entity lamp = fixture.base.create("lamp");
        const ecs::entity bulb = fixture.base.create("bulb", lamp);
        REQUIRE(api.entity_set_enabled(lamp.to_u64(), 0) == 0);
        REQUIRE_FALSE(fixture.base.active().is_enabled(lamp));
        fixture.inside_scope([&](conjure_query_scope &) {
            CHECK(api.entity_set_enabled(lamp.to_u64(), 1) == 0);
            CHECK(api.entity_set_enabled(lamp.to_u64(), 0) == 0);
            CHECK(bindings::detail::pending_fixup_count() == 2);
            CHECK(api.entity_is_enabled(lamp.to_u64()) == 0);
        });
        CHECK(bindings::detail::pending_fixup_count() == 0);
        CHECK_FALSE(fixture.base.active().is_enabled(lamp));
        CHECK(fixture.base.world.has<scene::inactive>(bulb));
        CONJURE_CHECK_OK(fixture.base.active().validate());
    }

    TEST_CASE("span routing - a deferred self-parent is refused and a refused reparent is logged at WARN") {
        log_capture logs = {};
        routing_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const ecs::entity parent = fixture.base.create("parent");
        const ecs::entity child = fixture.base.create("child", parent);
        const u32 warningsBefore = logs.count(core::log_level::WARN);
        fixture.inside_scope([&](conjure_query_scope &) {
            // Wrong whatever the tree looks like at the flush: refused at the call, nothing queued.
            CHECK(
              api.transform_set_parent(child.to_u64(), child.to_u64()) == to_status(bindings_error::INVALID_ARGUMENT)
            );
            CHECK(bindings::detail::pending_fixup_count() == 0);
            // A cycle depends on the tree at the flush: queued, and refused by the scene when applied.
            CHECK(api.transform_set_parent(parent.to_u64(), child.to_u64()) == 0);
            CHECK(bindings::detail::pending_fixup_count() == 1);
        });
        CHECK(bindings::detail::pending_fixup_count() == 0);
        CHECK(fixture.base.active().tree().parent_of(child) == parent);
        CHECK(fixture.base.active().tree().parent_of(parent).is_null());
        CHECK(logs.count(core::log_level::WARN) == warningsBefore + 1);
        CHECK(logs.contains(core::log_level::WARN, "deferred scene fixup"));
        CONJURE_CHECK_OK(fixture.base.active().validate());
    }

    TEST_CASE("span routing - a deferred reparent keeps the world or the local pose as asked") {
        routing_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const auto placeAt = [&](ecs::entity entity, f32 x) {
            conjure_transform local = {};
            REQUIRE(api.transform_get_local(entity.to_u64(), &local) == 0);
            local.position.x = x;
            REQUIRE(api.transform_set_local(entity.to_u64(), &local) == 0);
        };
        const auto localX = [&](ecs::entity entity) {
            conjure_transform local = {};
            REQUIRE(api.transform_get_local(entity.to_u64(), &local) == 0);
            return local.position.x;
        };
        // A live new parent at x 10; two roots at x 1 move under it.
        const ecs::entity parent = fixture.base.create("parent");
        const ecs::entity keepsWorld = fixture.base.create("keeps_world");
        const ecs::entity keepsLocal = fixture.base.create("keeps_local");
        placeAt(parent, 10.0f);
        placeAt(keepsWorld, 1.0f);
        placeAt(keepsLocal, 1.0f);
        // A parent destroyed before the flush sends the child to the root: two children of a holder at x 10,
        // local x 1 (world x 11), ask for it.
        const ecs::entity holder = fixture.base.create("holder");
        const ecs::entity doomed = fixture.base.create("doomed");
        const ecs::entity orphanKeepsWorld = fixture.base.create("orphan_keeps_world", holder);
        const ecs::entity orphanKeepsLocal = fixture.base.create("orphan_keeps_local", holder);
        placeAt(holder, 10.0f);
        placeAt(orphanKeepsWorld, 1.0f);
        placeAt(orphanKeepsLocal, 1.0f);
        fixture.inside_scope([&](conjure_query_scope &) {
            CHECK(api.transform_set_parent_ex(keepsWorld.to_u64(), parent.to_u64(), 1, -1) == 0);
            CHECK(api.transform_set_parent_ex(keepsLocal.to_u64(), parent.to_u64(), 0, -1) == 0);
            CHECK(api.transform_set_parent_ex(orphanKeepsWorld.to_u64(), doomed.to_u64(), 1, -1) == 0);
            CHECK(api.transform_set_parent_ex(orphanKeepsLocal.to_u64(), doomed.to_u64(), 0, -1) == 0);
            CHECK(api.entity_destroy(doomed.to_u64()) == 1);                // deferred: dead at the flush
            CHECK(bindings::detail::pending_fixup_count() == 4);
        });
        CHECK(bindings::detail::pending_fixup_count() == 0);
        REQUIRE_FALSE(fixture.base.world.is_alive(doomed));
        CHECK(fixture.base.active().tree().parent_of(keepsWorld) == parent);
        CHECK(fixture.base.active().tree().parent_of(keepsLocal) == parent);
        CONJURE_CHECK_APPROX(localX(keepsWorld), -9.0f);                     // keepWorld 1: world x stays 1
        CONJURE_CHECK_APPROX(localX(keepsLocal), 1.0f);                      // keepWorld 0: local x stays 1
        CHECK(fixture.base.active().tree().parent_of(orphanKeepsWorld).is_null());
        CHECK(fixture.base.active().tree().parent_of(orphanKeepsLocal).is_null());
        CONJURE_CHECK_APPROX(localX(orphanKeepsWorld), 11.0f);               // the root: local = the old world
        CONJURE_CHECK_APPROX(localX(orphanKeepsLocal), 1.0f);
        CONJURE_CHECK_OK(fixture.base.active().validate());
    }

    TEST_CASE("regression - applying fixups reuses the queue storage") {
        routing_fixture fixture = {};
        const ecs::entity steady = fixture.base.create("steady");
        // SET_ENABLED to the current state: scene::set_enabled returns early, so the apply itself allocates nothing.
        bindings::detail::scene_fixup fixup = {};
        fixup.operation = bindings::detail::scene_fixup::kind::SET_ENABLED;
        fixup.entity = steady;
        fixup.enabled = true;
        // Two warm-up rounds give both the queue and the scratch it is swapped with a capacity.
        for (u32 round = 0; round < 2; ++round) {
            CAPTURE(round);
            bindings::detail::queue_fixup(fixup);
            CHECK(bindings::detail::apply_pending_fixups() == 1);
        }
        u32 applied = 0;
        u64 allocations = 0;
        {
            const allocation_scope scope = {};
            bindings::detail::queue_fixup(fixup);
            applied = bindings::detail::apply_pending_fixups();
            allocations = scope.allocation_count();
        }
        CHECK(applied == 1);
        CHECK(allocations == 0);
        CHECK(bindings::detail::pending_fixup_count() == 0);
        CHECK(fixture.base.active().is_enabled(steady));
    }

    TEST_CASE("span routing - a locked world without a runtime refuses with STRUCTURE_LOCKED") {
        bare_fixture_options options = {};
        options.scripting = false;
        bare_fixture fixture(options);
        conjure_native_api &api = fixture.api();
        const ecs::entity parent = fixture.create("parent");
        const ecs::entity child = fixture.create("child");
        std::array<uint64_t, 1> created = {};
        {
            const ecs::iteration_scope lock = fixture.world.lock_iteration();
            CHECK(
              api.transform_set_parent(child.to_u64(), parent.to_u64()) == to_status(bindings_error::STRUCTURE_LOCKED)
            );
            CHECK(api.entity_duplicate(child.to_u64()) == 0);
            CHECK(last_error_code() == bindings_error::STRUCTURE_LOCKED);
            CHECK(api.entity_create_many(1, 0, created.data()) == to_status(bindings_error::STRUCTURE_LOCKED));
            CHECK(api.entity_set_enabled(child.to_u64(), 0) == to_status(bindings_error::STRUCTURE_LOCKED));
            CHECK(api.entity_set_enabled(child.to_u64(), 1) == 0);         // no change requested: nothing to defer
            const utf8_argument tag = utf8("x");
            CHECK(
              api.entity_set_tag(child.to_u64(), tag.data, tag.length) == to_status(bindings_error::STRUCTURE_LOCKED)
            );
            CHECK(api.entity_set_layer(child.to_u64(), 1) == to_status(bindings_error::STRUCTURE_LOCKED));
        }
        CHECK(api.transform_set_parent(child.to_u64(), parent.to_u64()) == 0);
    }
}
