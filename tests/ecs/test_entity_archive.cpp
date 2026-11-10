#include "conjure/testing/doctest.h"

#include "ecs_test_fixture.h"

#include "conjure/testing/memory_guard.h"

#include "conjure/ecs/command_buffer.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/component_type_desc.h"
#include "conjure/ecs/ecs_error.h"
#include "conjure/ecs/entity_archive.h"
#include "conjure/ecs/serialization.h"
#include "conjure/ecs/world.h"

#include <array>
#include <cstring>
#include <format>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::ecs;
using namespace ecs_test;

namespace {
    /// <summary>
    /// Three linked entities: root -> child -> grandchild, plus a link from the child to an entity
    /// outside the captured set.
    /// </summary>
    struct linked_set {
        entity root = {};
        entity child = {};
        entity grandchild = {};
        entity outsider = {};
        std::array<entity, 3> captured = {};
    };

    linked_set make_linked_set(ecs_fixture &fixture) {
        world &world = fixture.world;
        linked_set set = {};
        const core::result<entity> outsider = world.create_entity();
        const core::result<entity> root = world.create_entity(fixture.mask<link, position, name_component>());
        const core::result<entity> child = world.create_entity(fixture.mask<link, position>());
        const core::result<entity> grandchild = world.create_entity(fixture.mask<link, player_tag>());
        REQUIRE(outsider.has_value());
        REQUIRE(root.has_value());
        REQUIRE(child.has_value());
        REQUIRE(grandchild.has_value());
        set.outsider = outsider.value();
        set.root = root.value();
        set.child = child.value();
        set.grandchild = grandchild.value();
        set.captured = {set.root, set.child, set.grandchild};
        REQUIRE(world.set(set.root, link{set.child, NULL_ENTITY}).has_value());
        REQUIRE(world.set(set.root, position{1.0f, 2.0f, 3.0f}).has_value());
        REQUIRE(world.set(set.root, name_component{"root"}).has_value());
        REQUIRE(world.set(set.child, link{set.grandchild, set.outsider}).has_value());
        REQUIRE(world.set(set.child, position{4.0f, 5.0f, 6.0f}).has_value());
        REQUIRE(world.set(set.grandchild, link{set.root, NULL_ENTITY}).has_value());
        return set;
    }

    entity read_entity(std::span<const std::byte> bytes, u32 offset) {
        entity value = {};
        std::memcpy(&value, bytes.data() + offset, sizeof(entity));
        return value;
    }
}

TEST_SUITE("ecs/entity_archive") {

    TEST_CASE("entity_archive - capture three linked entities") {
        ecs_fixture fixture = {};
        const linked_set set = make_linked_set(fixture);
        const core::result<entity_archive> captured = entity_archive::capture(fixture.world, set.captured);
        CONJURE_REQUIRE_OK(captured);
        const entity_archive &archive = captured.value();
        CHECK(&archive.registry() == &fixture.registry);
        REQUIRE(archive.entity_count() == 3);
        CONJURE_CHECK_OK(archive.validate());

        const std::span<const archive_entry> entries = archive.entries();
        CHECK(entries[0].mask == fixture.mask<link, position, name_component>());
        CHECK(entries[1].mask == fixture.mask<link, position>());
        CHECK(entries[2].mask == fixture.mask<link, player_tag>());
        CHECK(entries[0].components.size() == 3);
        CHECK(entries[2].components.size() == 2);

        // Internal references became local indices, the external one null.
        const std::span<const std::byte> rootLink = archive.component_bytes(0, fixture.id<link>());
        REQUIRE(rootLink.size() == sizeof(link));
        CHECK(read_entity(rootLink, 0) == entity::archive_local(1));
        CHECK(read_entity(rootLink, sizeof(entity)) == NULL_ENTITY);
        const std::span<const std::byte> childLink = archive.component_bytes(1, fixture.id<link>());
        CHECK(read_entity(childLink, 0) == entity::archive_local(2));
        CHECK(read_entity(childLink, sizeof(entity)) == NULL_ENTITY);
        const std::span<const std::byte> grandchildLink = archive.component_bytes(2, fixture.id<link>());
        CHECK(read_entity(grandchildLink, 0) == entity::archive_local(0));

        // Values are copied, tags carry no bytes, missing components yield empty spans.
        const std::span<const std::byte> rootPosition = archive.component_bytes(0, fixture.id<position>());
        position rootValue = {};
        std::memcpy(&rootValue, rootPosition.data(), sizeof(rootValue));
        CHECK(rootValue == position{1.0f, 2.0f, 3.0f});
        CHECK(archive.component_bytes(2, fixture.id<player_tag>()).empty());
        CHECK(archive.component_bytes(2, fixture.id<position>()).empty());
        CHECK(archive.component_bytes(9, fixture.id<position>()).empty());
        const std::span<const std::byte> nameBytes = archive.component_bytes(0, fixture.id<name_component>());
        const name_component &rootName = *reinterpret_cast<const name_component *>(nameBytes.data());
        CHECK(rootName.value == "root");

        // The world is untouched.
        CHECK(fixture.world.entity_count() == 4);
        CHECK(fixture.world.get<link>(set.child)->other == set.outsider);
    }

    TEST_CASE("entity_archive - capture failures") {
        ecs_fixture fixture = {};
        const linked_set set = make_linked_set(fixture);
        const std::array<entity, 2> withDead = {set.root, entity{77, 9}};
        CONJURE_CHECK_ERROR(entity_archive::capture(fixture.world, withDead), ecs_error::ENTITY_NOT_ALIVE);
        {
            const iteration_scope scope = fixture.world.lock_iteration();
            CONJURE_CHECK_ERROR(entity_archive::capture(fixture.world, set.captured), ecs_error::STRUCTURE_LOCKED);
        }
        const core::result<entity_archive> empty = entity_archive::capture(fixture.world, std::span<const entity>());
        CONJURE_REQUIRE_OK(empty);
        CHECK(empty.value().entity_count() == 0);
        CONJURE_CHECK_OK(empty.value().validate());
    }

    TEST_CASE("entity_archive - instantiate remaps internal references") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const linked_set set = make_linked_set(fixture);
        core::result<entity_archive> captured = entity_archive::capture(world, set.captured);
        CONJURE_REQUIRE_OK(captured);
        const std::shared_ptr<const entity_archive> archive =
          std::make_shared<const entity_archive>(std::move(captured).value());

        u32 added = 0;
        CHECK(world.observe(fixture.id<link>(), component_event::ADDED,
          [&](ecs::world &owner, entity target, component_id, command_buffer &) {
              // Observers see the final, remapped data.
              CHECK(owner.get<link>(target)->target.generation != ARCHIVE_LOCAL_GENERATION);
              ++added;
          }).is_valid());

        const core::result<entity> root = world.instantiate(archive);
        CONJURE_REQUIRE_OK(root);
        CHECK(world.entity_count() == 7);
        CHECK(added == 3);
        const entity newRoot = root.value();
        CHECK(newRoot != set.root);
        REQUIRE(world.is_alive(newRoot));
        CHECK(world.mask_of(newRoot) == fixture.mask<link, position, name_component>());
        const entity newChild = world.get<link>(newRoot)->target;
        REQUIRE(world.is_alive(newChild));
        CHECK(newChild != set.child);
        CHECK(world.mask_of(newChild) == fixture.mask<link, position>());
        const entity newGrandchild = world.get<link>(newChild)->target;
        REQUIRE(world.is_alive(newGrandchild));
        CHECK(world.has<player_tag>(newGrandchild));
        CHECK(world.get<link>(newGrandchild)->target == newRoot);
        CHECK(*world.get<position>(newRoot) == position{1.0f, 2.0f, 3.0f});
        CHECK(*world.get<position>(newChild) == position{4.0f, 5.0f, 6.0f});
        CHECK(world.get<name_component>(newRoot)->value == "root");
        // The originals keep their references.
        CHECK(world.get<link>(set.root)->target == set.child);
    }

    TEST_CASE("entity_archive - external reference becomes null") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const linked_set set = make_linked_set(fixture);
        core::result<entity_archive> captured = entity_archive::capture(world, set.captured);
        CONJURE_REQUIRE_OK(captured);
        const std::shared_ptr<const entity_archive> archive =
          std::make_shared<const entity_archive>(std::move(captured).value());
        const core::result<entity> root = world.instantiate(archive);
        CONJURE_REQUIRE_OK(root);
        const entity newChild = world.get<link>(root.value())->target;
        CHECK(world.get<link>(newChild)->other == NULL_ENTITY);
        CHECK(world.get<link>(root.value())->other == NULL_ENTITY);
    }

    TEST_CASE("entity_archive - keepExternalReferences preserves") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const linked_set set = make_linked_set(fixture);
        archive_options options = {};
        options.keepExternalReferences = true;
        core::result<entity_archive> captured = entity_archive::capture(world, set.captured, options);
        CONJURE_REQUIRE_OK(captured);
        CHECK(read_entity(captured.value().component_bytes(1, fixture.id<link>()), sizeof(entity)) == set.outsider);

        const std::shared_ptr<const entity_archive> archive =
          std::make_shared<const entity_archive>(std::move(captured).value());
        const core::result<entity> root = world.instantiate(archive);
        CONJURE_REQUIRE_OK(root);
        const entity newChild = world.get<link>(root.value())->target;
        CHECK(world.get<link>(newChild)->other == set.outsider);
        CHECK(world.get<link>(newChild)->target != set.grandchild);
    }

    TEST_CASE("entity_archive - instantiate_batch 1000 lands in expected chunk count") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> prototype = world.create_entity(fixture.mask<position, velocity>());
        CONJURE_REQUIRE_OK(prototype);
        CONJURE_REQUIRE_OK(world.set(prototype.value(), position{1.0f, 1.0f, 1.0f}));
        core::result<entity_archive> captured =
          entity_archive::capture(world, std::span<const entity>(&prototype.value(), 1));
        CONJURE_REQUIRE_OK(captured);
        const std::shared_ptr<const entity_archive> archive =
          std::make_shared<const entity_archive>(std::move(captured).value());

        std::vector<entity> roots(1000);
        std::vector<entity> everything = {};
        const u32 chunksBefore = world.stats().chunks;
        CONJURE_REQUIRE_OK(world.instantiate_batch(archive, roots, nullptr, &everything));
        CHECK(world.entity_count() == 1001);
        // One entry per copy: the reported list is the root list, copy-major.
        CHECK(everything == roots);
        const archetype *owner = nullptr;
        for (const archetype *candidate : world.archetypes()) {
            if (candidate->mask() == fixture.mask<position, velocity>()) {
                owner = candidate;
            }
        }
        REQUIRE(owner != nullptr);
        const u32 capacity = owner->layout().capacity;
        CHECK(owner->chunk_count() == (1001 + capacity - 1) / capacity);
        CHECK(world.stats().chunks - chunksBefore == owner->chunk_count() - 1);
        for (const entity root : roots) {
            CHECK(world.is_alive(root));
            CHECK(*world.get<position>(root) == position{1.0f, 1.0f, 1.0f});
        }
        CHECK(world.archetypes().size() == 2);

        std::vector<entity> none = {};
        CONJURE_REQUIRE_OK(world.instantiate_batch(archive, none));
        CHECK(world.entity_count() == 1001);
    }

    TEST_CASE("regression - review 2 instantiate_batch partial failure leaves ids and world intact") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const zero_capacity_archetype zero = make_zero_capacity_archetype(fixture);
        CONJURE_CHECK_ERROR(world.create_entity(zero.mask), ecs_error::ARCHETYPE_TOO_LARGE);

        // Entry 0 fits (position), entry 1 names the archetype that no longer fits a chunk.
        entity_archive archive(fixture.registry);
        archive_entry first = {};
        first.mask = fixture.mask<position>();
        archive_component positionBytes = {};
        positionBytes.id = fixture.id<position>();
        positionBytes.bytes.resize(sizeof(position));
        const position value{1.0f, 2.0f, 3.0f};
        std::memcpy(positionBytes.bytes.data(), &value, sizeof(value));
        first.components.push_back(std::move(positionBytes));
        archive.push_entry(std::move(first));
        archive_entry second = {};
        second.mask = zero.mask;
        for (const component_id id : zero.ids) {
            archive_component component = {};
            component.id = id;
            component.bytes.resize(fixture.registry.info(id).size);
            second.components.push_back(std::move(component));
        }
        archive.push_entry(std::move(second));
        CONJURE_REQUIRE_OK(archive.validate());
        const std::shared_ptr<const entity_archive> shared = std::make_shared<const entity_archive>(std::move(archive));

        // Direct instantiation: the copies of entry 0 were placed and must be undone.
        std::vector<entity> roots(3);
        CONJURE_CHECK_ERROR(world.instantiate_batch(shared, roots), ecs_error::ARCHETYPE_TOO_LARGE);
        CHECK(world.entity_count() == 0);
        CONJURE_CHECK_OK(world.validate());
        // A reused id list holds nothing from an earlier call once the instantiation failed.
        std::vector<entity> stale = {entity{1, 1}, entity{2, 1}};
        CONJURE_CHECK_ERROR(world.instantiate_batch(shared, roots, nullptr, &stale), ecs_error::ARCHETYPE_TOO_LARGE);
        CHECK(stale.empty());

        // Through a command buffer: the flush skips the instantiation and the buffer returns the ids.
        command_buffer commands(world);
        const core::result<entity> deferredRoot = commands.instantiate(shared);
        CONJURE_REQUIRE_OK(deferredRoot);
        const flush_report report = world.flush(commands);
        CHECK(report.applied == 0);
        CHECK(report.skipped == 1);
        CHECK(world.entity_count() == 0);
        CHECK_FALSE(world.is_alive(deferredRoot.value()));
        CONJURE_CHECK_OK(world.validate());

        // Every returned id is handed out exactly once again: no index is alive twice.
        std::vector<entity> fresh(12);
        CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position>(), fresh));
        std::unordered_set<u32> indices = {};
        for (const entity id : fresh) {
            CHECK(indices.insert(id.index).second);
            CHECK(world.is_alive(id));
        }
        CHECK(world.entity_count() == 12);
        CONJURE_CHECK_OK(world.validate());
    }

    TEST_CASE("regression - review 3 instantiate_batch empties outEntities on every failure path") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> prototype = world.create_entity(fixture.mask<position>());
        CONJURE_REQUIRE_OK(prototype);
        core::result<entity_archive> captured =
          entity_archive::capture(world, std::span<const entity>(&prototype.value(), 1));
        CONJURE_REQUIRE_OK(captured);
        const std::shared_ptr<const entity_archive> archive =
          std::make_shared<const entity_archive>(std::move(captured).value());
        std::array<entity, 1> roots = {};

        // Before the archive is looked at: a null archive.
        std::vector<entity> everything = {entity{1, 1}, entity{2, 1}};
        CONJURE_CHECK_ERROR(
          world.instantiate_batch(nullptr, roots, nullptr, &everything),
          core::error_code::INVALID_ARGUMENT
        );
        CHECK(everything.empty());

        // An archive that validates but belongs to another registry.
        component_registry otherRegistry = {};
        CONJURE_REQUIRE_OK(otherRegistry.register_component<position>());
        ecs::world otherWorld(otherRegistry);
        everything = {entity{1, 1}};
        CONJURE_CHECK_ERROR(
          otherWorld.instantiate_batch(archive, roots, nullptr, &everything),
          ecs_error::ARCHIVE_CORRUPT
        );
        CHECK(everything.empty());

        // An archive that fails validation (a local index beyond the entry count).
        entity_archive corrupt(fixture.registry);
        archive_entry entry = {};
        entry.mask = fixture.mask<link>();
        archive_component linkBytes = {};
        linkBytes.id = fixture.id<link>();
        const link outOfRange{entity::archive_local(5), NULL_ENTITY};
        linkBytes.bytes.resize(sizeof(link));
        std::memcpy(linkBytes.bytes.data(), &outOfRange, sizeof(link));
        entry.components.push_back(std::move(linkBytes));
        corrupt.push_entry(std::move(entry));
        const std::shared_ptr<const entity_archive> corruptShared =
          std::make_shared<const entity_archive>(std::move(corrupt));
        everything = {entity{1, 1}};
        CONJURE_CHECK_ERROR(
          world.instantiate_batch(corruptShared, roots, nullptr, &everything),
          ecs_error::ARCHIVE_CORRUPT
        );
        CHECK(everything.empty());
        CHECK(world.entity_count() == 1);

        // The success path still reports the ids.
        CONJURE_REQUIRE_OK(world.instantiate_batch(archive, roots, nullptr, &everything));
        REQUIRE(everything.size() == 1);
        CHECK(everything[0] == roots[0]);
    }

    TEST_CASE("entity_archive - instantiate_batch reports every entity copy-major in entry order") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        // Entry 0 carries components, entry 1 is an empty entity (the empty archetype), entry 2 a tag.
        const core::result<entity> first = world.create_entity(fixture.mask<position>());
        const core::result<entity> second = world.create_entity();
        const core::result<entity> third = world.create_entity(fixture.mask<player_tag>());
        CONJURE_REQUIRE_OK(first);
        CONJURE_REQUIRE_OK(second);
        CONJURE_REQUIRE_OK(third);
        const std::array<entity, 3> sources = {first.value(), second.value(), third.value()};
        core::result<entity_archive> captured = entity_archive::capture(world, sources);
        CONJURE_REQUIRE_OK(captured);
        const std::shared_ptr<const entity_archive> archive =
          std::make_shared<const entity_archive>(std::move(captured).value());

        std::array<entity, 2> roots = {};
        std::vector<entity> everything = {};
        CONJURE_REQUIRE_OK(world.instantiate_batch(archive, roots, nullptr, &everything));
        REQUIRE(everything.size() == 6);
        CHECK(everything[0] == roots[0]);
        CHECK(everything[3] == roots[1]);
        for (u32 copy = 0; copy < 2; ++copy) {
            CAPTURE(copy);
            CHECK(world.has<position>(everything[copy * 3 + 0]));
            CHECK(world.mask_of(everything[copy * 3 + 1]).none());
            CHECK(world.has<player_tag>(everything[copy * 3 + 2]));
        }
        CHECK(world.entity_count() == 9);

        // Through a command buffer the reserved ids are reported before the flush places them.
        command_buffer commands(world);
        std::array<entity, 1> deferredRoot = {};
        std::vector<entity> deferred = {};
        CONJURE_REQUIRE_OK(world.instantiate_batch(archive, deferredRoot, &commands, &deferred));
        REQUIRE(deferred.size() == 3);
        CHECK(deferred[0] == deferredRoot[0]);
        CHECK_FALSE(world.is_alive(deferred[1]));
        const flush_report report = world.flush(commands);
        CHECK(report.skipped == 0);
        CHECK(world.is_alive(deferred[1]));
        CHECK(world.mask_of(deferred[1]).none());
        CHECK(world.has<player_tag>(deferred[2]));

        // An empty archive or no copies reports nothing.
        std::vector<entity> none = {entity{1, 1}};
        CONJURE_REQUIRE_OK(world.instantiate_batch(archive, std::span<entity>(), nullptr, &none));
        CHECK(none.empty());
    }

    TEST_CASE("entity_archive - instantiate via command buffer resolves at flush") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const linked_set set = make_linked_set(fixture);
        core::result<entity_archive> captured = entity_archive::capture(world, set.captured);
        CONJURE_REQUIRE_OK(captured);
        const std::shared_ptr<const entity_archive> archive =
          std::make_shared<const entity_archive>(std::move(captured).value());

        command_buffer commands(world);
        const core::result<entity> viaWorld = world.instantiate(archive, &commands);
        const core::result<entity> viaBuffer = commands.instantiate(archive);
        CONJURE_REQUIRE_OK(viaWorld);
        CONJURE_REQUIRE_OK(viaBuffer);
        CHECK_FALSE(viaWorld.value().is_null());
        CHECK_FALSE(viaBuffer.value() == viaWorld.value());
        CHECK_FALSE(world.is_alive(viaWorld.value()));
        CHECK(world.entity_count() == 4);
        // The root id is usable in later commands of the same buffer.
        commands.add<health>(viaWorld.value(), health{7, 7});

        const flush_report report = world.flush(commands);
        CHECK(report.applied == 3);
        CHECK(report.skipped == 0);
        CHECK(world.entity_count() == 10);
        REQUIRE(world.is_alive(viaWorld.value()));
        REQUIRE(world.is_alive(viaBuffer.value()));
        CHECK(world.get<health>(viaWorld.value())->value == 7);
        const entity child = world.get<link>(viaWorld.value())->target;
        CHECK(world.is_alive(child));
        CHECK(child != set.child);
        CHECK(world.get<link>(world.get<link>(child)->target)->target == viaWorld.value());

        // A cleared buffer hands the reserved ids back.
        const core::result<entity> dropped = commands.instantiate(archive);
        CONJURE_REQUIRE_OK(dropped);
        commands.clear();
        CHECK(world.flush(commands).applied == 0);
        CHECK(world.entity_count() == 10);
        CHECK_FALSE(world.is_alive(dropped.value()));
    }

    TEST_CASE("entity_archive - corrupt local index is ARCHIVE_CORRUPT") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        entity_archive archive(fixture.registry);
        archive_entry entry = {};
        entry.mask = fixture.mask<link>();
        archive_component component = {};
        component.id = fixture.id<link>();
        component.bytes.resize(sizeof(link));
        const link bad{entity::archive_local(5), NULL_ENTITY};
        std::memcpy(component.bytes.data(), &bad, sizeof(bad));
        entry.components.push_back(std::move(component));
        archive.push_entry(std::move(entry));
        CONJURE_CHECK_ERROR(archive.validate(), ecs_error::ARCHIVE_CORRUPT);

        const std::shared_ptr<const entity_archive> shared = std::make_shared<const entity_archive>(std::move(archive));
        CONJURE_CHECK_ERROR(world.instantiate(shared), ecs_error::ARCHIVE_CORRUPT);
        command_buffer commands(world);
        CONJURE_CHECK_ERROR(commands.instantiate(shared), ecs_error::ARCHIVE_CORRUPT);
        CONJURE_CHECK_ERROR(world.instantiate(shared, &commands), ecs_error::ARCHIVE_CORRUPT);
        CHECK(world.entity_count() == 0);
        CHECK(commands.empty());

        // Other corruptions: mask without component, payload of the wrong size, unregistered id.
        entity_archive maskOnly(fixture.registry);
        archive_entry missing = {};
        missing.mask = fixture.mask<position>();
        maskOnly.push_entry(std::move(missing));
        CONJURE_CHECK_ERROR(maskOnly.validate(), ecs_error::ARCHIVE_CORRUPT);

        entity_archive wrongSize(fixture.registry);
        archive_entry sized = {};
        sized.mask = fixture.mask<position>();
        archive_component shortBytes = {};
        shortBytes.id = fixture.id<position>();
        shortBytes.bytes.resize(3);
        sized.components.push_back(std::move(shortBytes));
        wrongSize.push_entry(std::move(sized));
        CONJURE_CHECK_ERROR(wrongSize.validate(), ecs_error::ARCHIVE_CORRUPT);

        entity_archive unknown(fixture.registry);
        archive_entry unknownEntry = {};
        unknownEntry.mask.set(component_id{900});
        archive_component unknownComponent = {};
        unknownComponent.id = component_id{900};
        unknownEntry.components.push_back(std::move(unknownComponent));
        unknown.push_entry(std::move(unknownEntry));
        CONJURE_CHECK_ERROR(unknown.validate(), ecs_error::ARCHIVE_CORRUPT);

        // An archive from another registry is refused too.
        component_registry other = {};
        register_test_components(other);
        const std::shared_ptr<const entity_archive> foreign = std::make_shared<const entity_archive>(other);
        CONJURE_CHECK_ERROR(world.instantiate(foreign), ecs_error::ARCHIVE_CORRUPT);
        CONJURE_CHECK_ERROR(commands.instantiate(foreign), ecs_error::ARCHIVE_CORRUPT);
        CHECK_FALSE(world.instantiate(nullptr).has_value());
    }

    TEST_CASE("entity_archive - non trivial components copied not aliased") {
        tracked::reset_counters();
        {
            // Reflection and interned names register process-wide on first use.
            const ecs_fixture warmUp = {};
        }
        testing::memory_guard guard = {};
        {
            ecs_fixture fixture = {};
            world &world = fixture.world;
            const core::result<entity> source = world.create_entity(fixture.mask<name_component, tracked>());
            CONJURE_REQUIRE_OK(source);
            const std::string longText = "original text that is long enough to live on the heap";
            CONJURE_REQUIRE_OK(world.set(source.value(), name_component{longText}));
            CONJURE_REQUIRE_OK(world.set(source.value(), tracked(11)));

            core::result<entity_archive> captured =
              entity_archive::capture(world, std::span<const entity>(&source.value(), 1));
            CONJURE_REQUIRE_OK(captured);
            const i64 liveAfterCapture = tracked::live();
            CHECK(liveAfterCapture == 2);
            const std::shared_ptr<const entity_archive> archive =
              std::make_shared<const entity_archive>(std::move(captured).value());
            CHECK(tracked::live() == 2);

            // Changing the source after the capture leaves the archive and its copies alone.
            CONJURE_REQUIRE_OK(world.set(source.value(), name_component{"changed"}));
            std::vector<entity> copies(3);
            CONJURE_REQUIRE_OK(world.instantiate_batch(archive, copies));
            CHECK(tracked::live() == 5);
            for (const entity copy : copies) {
                CHECK(world.get<name_component>(copy)->value == longText);
                CHECK(world.get<tracked>(copy)->value == 11);
                const char *copyText = world.get<name_component>(copy)->value.data();
                CHECK(copyText != world.get<name_component>(source.value())->value.data());
            }
            CONJURE_REQUIRE_OK(world.set(copies[0], name_component{"copy edited"}));
            CHECK(world.get<name_component>(copies[1])->value != "copy edited");
            CONJURE_REQUIRE_OK(world.destroy_entity(copies[0]));
            CHECK(tracked::live() == 4);
        }
        CHECK(tracked::live() == 0);
        guard.check();
    }

    TEST_CASE("entity_archive - transient components are not captured") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> created = world.create_entity(fixture.mask<position, transient_component>());
        CONJURE_REQUIRE_OK(created);
        const core::result<entity_archive> captured =
          entity_archive::capture(world, std::span<const entity>(&created.value(), 1));
        CONJURE_REQUIRE_OK(captured);
        CHECK(captured.value().entries()[0].mask == fixture.mask<position>());
        CHECK(captured.value().entries()[0].components.size() == 1);
    }

    TEST_CASE("entity_archive - move semantics transfer ownership") {
        tracked::reset_counters();
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> created = world.create_entity(fixture.mask<tracked>());
        CONJURE_REQUIRE_OK(created);
        core::result<entity_archive> captured =
          entity_archive::capture(world, std::span<const entity>(&created.value(), 1));
        CONJURE_REQUIRE_OK(captured);
        CHECK(tracked::live() == 2);
        {
            entity_archive moved(std::move(captured).value());
            CHECK(moved.entity_count() == 1);
            entity_archive assigned(fixture.registry);
            assigned = std::move(moved);
            CHECK(assigned.entity_count() == 1);
            CHECK(moved.entity_count() == 0);
            CHECK(tracked::live() == 2);
        }
        CHECK(tracked::live() == 1);
    }

    TEST_CASE("regression - review 3 capture remaps references held by a reflection-less hook") {
        // What scripting's script_component does (R29): the entity lives where reflection cannot see it,
        // so capture must call the hook (real -> local) exactly as instantiate does (local -> real).
        struct hidden_link {
            u64 packed = 0;    // an entity, stored opaquely
        };
        static constexpr auto writeHook = [](const void *component, core::json::writer &out) -> core::result<void> {
            out.begin_object();
            out.key("packed");
            out.value(static_cast<const hidden_link *>(component)->packed);
            out.end_object();
            return core::result<void>::success();
        };
        static constexpr auto readHook = [](void *component, const core::json::value &in) -> core::result<void> {
            static_cast<hidden_link *>(component)->packed = in["packed"].as_u64();
            return core::result<void>::success();
        };
        static constexpr auto remapHook = [](void *component, const entity_remap &map) {
            auto *value = static_cast<hidden_link *>(component);
            value->packed = map.remap(entity::from_u64(value->packed)).to_u64();
        };
        component_registry registry = {};
        component_type_desc desc = {};
        desc.name = "test.hidden_link";
        desc.size = sizeof(hidden_link);
        desc.alignment = alignof(hidden_link);
        desc.flags = component_flags::TRIVIAL;
        desc.serializer.writeJson = writeHook;
        desc.serializer.readJson = readHook;
        desc.serializer.remapEntities = remapHook;
        const core::result<component_id> id = registry.register_described(desc);
        CONJURE_REQUIRE_OK(id);

        world world(registry);
        const core::result<entity> first = world.create_entity();
        const core::result<entity> second = world.create_entity();
        const core::result<entity> outsider = world.create_entity();
        CONJURE_REQUIRE_OK(first);
        CONJURE_REQUIRE_OK(second);
        CONJURE_REQUIRE_OK(outsider);
        const core::result<void *> firstCell = world.add_raw(first.value(), id.value());
        const core::result<void *> secondCell = world.add_raw(second.value(), id.value());
        CONJURE_REQUIRE_OK(firstCell);
        CONJURE_REQUIRE_OK(secondCell);
        static_cast<hidden_link *>(firstCell.value())->packed = second.value().to_u64();
        static_cast<hidden_link *>(secondCell.value())->packed = outsider.value().to_u64();

        const std::array<entity, 2> captured = {first.value(), second.value()};
        core::result<entity_archive> archive = entity_archive::capture(world, captured);
        CONJURE_REQUIRE_OK(archive);
        // Inside the archive the internal reference is local; the external one is the hook's business
        // and stays as it was.
        core::json::writer out = {};
        CONJURE_REQUIRE_OK(write_archive_json(archive.value(), out));
        const std::string text = out.finish();
        CHECK(text.find(std::format("\"packed\":{}", entity::archive_local(1).to_u64())) != std::string::npos);
        CHECK(text.find(std::format("\"packed\":{}", outsider.value().to_u64())) != std::string::npos);
        CHECK(text.find(std::format("\"packed\":{}", second.value().to_u64())) == std::string::npos);

        const std::shared_ptr<const entity_archive> shared =
          std::make_shared<const entity_archive>(std::move(archive).value());
        std::vector<entity> roots(1);
        std::vector<entity> created = {};
        CONJURE_REQUIRE_OK(world.instantiate_batch(shared, roots, nullptr, &created));
        REQUIRE(created.size() == 2);
        CHECK(created[0] != first.value());
        const auto *copiedFirst = static_cast<const hidden_link *>(world.get_raw(created[0], id.value()));
        const auto *copiedSecond = static_cast<const hidden_link *>(world.get_raw(created[1], id.value()));
        REQUIRE(copiedFirst != nullptr);
        REQUIRE(copiedSecond != nullptr);
        // The copy references its own sibling, not the authoring entity.
        CHECK(entity::from_u64(copiedFirst->packed) == created[1]);
        CHECK(entity::from_u64(copiedSecond->packed) == outsider.value());
        // The originals are untouched.
        CHECK(entity::from_u64(static_cast<const hidden_link *>(world.get_raw(first.value(), id.value()))->packed)
              == second.value());
    }
}
