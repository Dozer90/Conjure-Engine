#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"
#include "scripting_test_support.h"

#include "conjure/scripting/native_api.h"
#include "conjure/scripting/null_script_host.h"
#include "conjure/scripting/script_runtime.h"
#include "conjure/scripting/scripting_error.h"

#include <array>
#include <string>
#include <vector>

using namespace conjure;
using namespace conjure::scripting;
using namespace conjure::testing;
using namespace scripting_test;

namespace {

    /// <summary>
    /// A null host initialised on a bare table (no runtime), enough to drive the managed_api directly.
    /// </summary>
    struct bare_host {
        conjure_native_api api = make_native_api(nullptr);
        null_script_host host = {};

        bare_host() {
            CONJURE_REQUIRE_OK(host.initialize({}, api));
        }

        ~bare_host() {
            host.shutdown();
        }

        bare_host(const bare_host &) = delete;
        bare_host &operator=(const bare_host &) = delete;

        [[nodiscard]] const conjure_managed_api &managed() const noexcept {
            return host.managed_api();
        }

        [[nodiscard]] i32 create(u64 entity, u64 classHash, u8 flags = 0) {
            conjure_create_request request = {};
            request.entity = entity;
            request.class_hash = classHash;
            request.flags = flags;
            request.out_slot = -1;
            CHECK(managed().create_instances(&request, 1) == 1);
            return request.out_slot;
        }
    };
}

TEST_SUITE("scripting/null_script_host") {

    TEST_CASE("null_script_host - initialize fills the managed api and becomes active") {
        null_script_host host = {};
        CHECK(null_script_host::active() == nullptr);
        CHECK(host.status() == script_host_status::UNINITIALIZED);
        conjure_native_api api = make_native_api(nullptr);
        CONJURE_REQUIRE_OK(host.initialize({}, api));
        CHECK(host.status() == script_host_status::READY);
        CHECK(null_script_host::active() == &host);
        CONJURE_CHECK_OK(validate_managed_api(host.managed_api()));
        CHECK(host.managed_api().size == sizeof(conjure_managed_api));
        CHECK(host.managed_api().layout_hash == CONJURE_NATIVE_API_HASH);
        CHECK(host.recorder().count("initialize") == 1);
        host.shutdown();
        CHECK(null_script_host::active() == nullptr);
        CHECK(host.status() == script_host_status::UNINITIALIZED);
        CHECK(host.recorder().shutdown_count() == 1);
    }

    TEST_CASE("null_script_host - slots are reused LIFO") {
        bare_host fixture = {};
        const i32 first = fixture.create(1, 100);
        const i32 second = fixture.create(2, 100);
        const i32 third = fixture.create(3, 100);
        CHECK(first == 0);
        CHECK(second == 1);
        CHECK(third == 2);
        CHECK(fixture.host.live_slot_count() == 3);
        CHECK(fixture.host.slot_entity(second) == 2);

        const std::array<i32, 2> destroyed = {first, third};
        CHECK(fixture.managed().destroy_instances(destroyed.data(), 2, 1) == 2);
        CHECK_FALSE(fixture.host.is_slot_live(first));
        CHECK(fixture.host.free_list() == std::vector<i32>{first, third});
        CHECK(fixture.create(4, 100) == third);
        CHECK(fixture.create(5, 100) == first);
        CHECK(fixture.create(6, 100) == 3);
        CHECK(fixture.host.recorder().destroys().size() == 2);
        CHECK(fixture.host.recorder().destroys()[0].callOnDestroy);

        // Destroying a slot that was never created is recorded but counts nothing.
        const std::array<i32, 1> dead = {42};
        CHECK(fixture.managed().destroy_instances(dead.data(), 1, 0) == 0);
        CHECK(fixture.host.recorder().destroys().size() == 3);
    }

    TEST_CASE("null_script_host - collect_fields echoes apply_fields unless overridden") {
        bare_host fixture = {};
        const i32 slot = fixture.create(7, 100);
        const std::array<u8, 6> blob = {1, 2, 3, 4, 5, 6};
        CHECK(fixture.managed().apply_fields(slot, blob.data(), static_cast<i32>(blob.size())) == 0);
        CHECK(fixture.host.slot_fields(slot) == std::vector<u8>(blob.begin(), blob.end()));

        std::array<u8, 16> buffer = {};
        CHECK(fixture.managed().collect_fields(slot, buffer.data(), 4) == 6);     // too small: size only
        CHECK(buffer[0] == 0);
        CHECK(fixture.managed().collect_fields(slot, buffer.data(), 16) == 6);
        CHECK(buffer[5] == 6);

        fixture.host.set_collect_override(slot, {9, 9});
        CHECK(fixture.managed().collect_fields(slot, buffer.data(), 16) == 2);
        CHECK(buffer[1] == 9);
        CHECK(fixture.host.recorder().collect_fields_count() == 3);
        CHECK(fixture.managed().apply_fields(99, blob.data(), 1) == status_of(script_error::INSTANCE_NOT_FOUND));
        CHECK(fixture.managed().collect_fields(99, buffer.data(), 16) == status_of(script_error::INSTANCE_NOT_FOUND));
    }

    TEST_CASE("null_script_host - run_phase is recorded in order with per-slot counters") {
        bare_host fixture = {};
        const i32 slot = fixture.create(1, 100);
        CHECK_FALSE(fixture.host.is_slot_started(slot));
        CHECK(fixture.managed().run_phase(static_cast<u8>(script_phase::FIXED_UPDATE), 0.5f) == 1);
        CHECK(fixture.managed().run_phase(static_cast<u8>(script_phase::UPDATE), 0.25f) == 1);
        CHECK(fixture.managed().run_phase(static_cast<u8>(script_phase::LATE_UPDATE), 0.125f) == 1);
        CHECK(fixture.managed().run_phase(9, 0.0f) == status_of(script_error::NOT_SUPPORTED));
        CHECK(fixture.host.is_slot_started(slot));
        CHECK(fixture.host.slot_phase_count(slot, script_phase::UPDATE) == 1);
        CHECK(fixture.host.slot_phase_count(slot, script_phase::COUNT) == 0);
        const auto &phases = fixture.host.recorder().phases();
        REQUIRE(phases.size() == 4);
        CHECK(phases[0].phase == script_phase::FIXED_UPDATE);
        CHECK(phases[0].deltaTime == 0.5f);
        CHECK(phases[1].phase == script_phase::UPDATE);
        CHECK(phases[2].phase == script_phase::LATE_UPDATE);
        CHECK(phases[3].phase == script_phase::COUNT);
        const std::vector<std::string> &calls = fixture.host.recorder().calls();
        const std::vector<std::string> expected = {
          "initialize", "create_instances", "run_phase", "run_phase", "run_phase", "run_phase"
        };
        CHECK(calls == expected);
    }

    TEST_CASE("null_script_host - load registers fake and fixture classes through the native table") {
        scripting_fixture fixture = {};
        temp_directory directory = {};
        CONJURE_REQUIRE_OK(directory.write_text("Game.dll", "placeholder"));
        CONJURE_REQUIRE_OK(directory.write_text(
          "Game.classes.json",
          R"({"classes":[{"name":"Game.FromFixture","callbacks":["UPDATE","DESTROY"],)"
          R"("fields":[{"name":"speed","kind":"FLOAT","default":2.5,"meta":{"min":0}},)"
          R"({"name":"tags","kind":"LIST","elementKind":"STRING","default":["a"]}]}]})"
        ));
        fixture.null_host().add_fake_class(make_class("Game.Fake", {script_callback::CREATE}, {
          make_field("count", script_field_kind::INT32, "3"),
        }));

        CONJURE_REQUIRE_OK(fixture.runtime.load_game_assembly(directory.file("Game.dll")));
        CHECK(fixture.null_host().is_assembly_loaded());
        CHECK(fixture.null_host().loaded_assembly() == directory.file("Game.dll").string());
        CHECK(fixture.runtime.classes().size() == 2);

        const script_class_info *fake = fixture.runtime.classes().find("Game.Fake");
        REQUIRE(fake != nullptr);
        CHECK(fake->has_callback(script_callback::CREATE));
        REQUIRE(fake->find_field("count") != nullptr);
        CHECK(fake->find_field("count")->defaultValue.root().as_i64() == 3);

        const script_class_info *fromFixture = fixture.runtime.classes().find("Game.FromFixture");
        REQUIRE(fromFixture != nullptr);
        CHECK(fromFixture->has_callback(script_callback::UPDATE));
        CHECK(fromFixture->has_callback(script_callback::DESTROY));
        CHECK_FALSE(fromFixture->has_callback(script_callback::START));
        const script_field_info *speed = fromFixture->find_field("speed");
        REQUIRE(speed != nullptr);
        CHECK(speed->kind == script_field_kind::FLOAT);
        CHECK(speed->defaultValue.root().as_f64() == doctest::Approx(2.5));
        CHECK(speed->meta.root()["min"].as_i64() == 0);
        const script_field_info *tags = fromFixture->find_field("tags");
        REQUIRE(tags != nullptr);
        CHECK(tags->kind == script_field_kind::LIST);
        CHECK(tags->elementKind == script_field_kind::STRING);
        CHECK(tags->defaultValue.root().size() == 1);
    }

    TEST_CASE("null_script_host - a fixture that only exists as classes.json loads too") {
        scripting_fixture fixture = {};
        temp_directory directory = {};
        CONJURE_REQUIRE_OK(directory.write_text("Only.classes.json", R"({"classes":[{"name":"Game.Only"}]})"));
        CONJURE_REQUIRE_OK(fixture.runtime.load_game_assembly(directory.file("Only.dll")));
        CHECK(fixture.runtime.classes().find("Game.Only") != nullptr);
    }

    TEST_CASE("null_script_host - loading a missing assembly fails and leaves the active schema alone") {
        scripting_fixture fixture = {};
        temp_directory directory = {};
        fixture.load_fake_classes({make_class("Game.Keep", {})});
        CHECK(fixture.runtime.classes().size() == 1);
        CONJURE_CHECK_ERROR(
          fixture.runtime.load_game_assembly(directory.file("Missing.dll")),
          script_error::ASSEMBLY_LOAD_FAILED
        );
        CHECK(fixture.runtime.classes().size() == 1);
        CHECK(fixture.runtime.classes().find("Game.Keep") != nullptr);
    }

    TEST_CASE("null_script_host - a malformed classes fixture fails the load") {
        scripting_fixture fixture = {};
        log_capture logs = {};
        logs.allow_errors();
        temp_directory directory = {};
        CONJURE_REQUIRE_OK(directory.write_text("Bad.classes.json", R"({"classes":[{"callbacks":[]}]})"));
        CONJURE_CHECK_ERROR(
          fixture.runtime.load_game_assembly(directory.file("Bad.dll")),
          script_error::ASSEMBLY_LOAD_FAILED
        );
        CHECK(logs.count(core::log_level::ERR) == 1);
    }

    TEST_CASE("null_script_host - parse_classes_fixture validates its input") {
        CONJURE_CHECK_ERROR(null_script_host::parse_classes_fixture("nope"), core::error_code::PARSE_FAILURE);
        CONJURE_CHECK_ERROR(null_script_host::parse_classes_fixture("{}"), core::error_code::PARSE_FAILURE);
        CONJURE_CHECK_ERROR(
          null_script_host::parse_classes_fixture(R"({"classes":[{"name":"A","callbacks":["NOPE"]}]})"),
          core::error_code::PARSE_FAILURE
        );
        CONJURE_CHECK_ERROR(
          null_script_host::parse_classes_fixture(R"({"classes":[{"name":"A","fields":[{"kind":"INT32"}]}]})"),
          core::error_code::PARSE_FAILURE
        );
        CONJURE_CHECK_ERROR(
          null_script_host::parse_classes_fixture(
            R"({"classes":[{"name":"A","fields":[{"name":"f","kind":"NOPE"}]}]})"
          ),
          core::error_code::PARSE_FAILURE
        );
        const core::result<std::vector<script_class_info>> parsed =
          null_script_host::parse_classes_fixture(
            R"({"classes":[{"name":"A","fields":[{"name":"f","kind":"LIST"}]}]})"
          );
        CONJURE_REQUIRE_OK(parsed);
        REQUIRE(parsed.value().size() == 1);
        CHECK(parsed.value()[0].hash == hash_of("A"));
        CHECK(parsed.value()[0].fields[0].elementKind == script_field_kind::INT32);
    }

    TEST_CASE("null_script_host - load and unload results are configurable") {
        scripting_fixture fixture = {};
        fixture.null_host().set_load_result(status_of(script_error::ASSEMBLY_LOAD_FAILED));
        CONJURE_CHECK_ERROR(fixture.runtime.load_game_assembly({}), script_error::ASSEMBLY_LOAD_FAILED);
        CHECK(fixture.null_host().recorder().loaded_assemblies().size() == 1);
        fixture.null_host().set_load_result(0);
        CONJURE_REQUIRE_OK(fixture.runtime.load_game_assembly({}));

        fixture.null_host().set_unload_result(1);
        CHECK(fixture.null_host().managed_api().unload_game_assembly() == 1);
        CHECK(fixture.null_host().recorder().unload_count() == 1);
        CHECK_FALSE(fixture.null_host().is_assembly_loaded());
        fixture.null_host().set_unload_result(0);
        CHECK(fixture.null_host().managed_api().unload_game_assembly() == 0);
    }

    TEST_CASE("null_script_host - dispatch_collisions and collect_garbage are recorded") {
        bare_host fixture = {};
        const i32 slot = fixture.create(1, 100);
        std::array<conjure_collision_record, 2> records = {};
        records[0].slot = slot;
        records[0].other = 2;
        records[0].impulse = 3.0f;
        records[0].kind = static_cast<u8>(collision_kind::TRIGGER_EXIT);
        records[1].slot = slot;
        records[1].kind = 200;
        CHECK(fixture.managed().dispatch_collisions(records.data(), 2) == 2);
        REQUIRE(fixture.host.recorder().collisions().size() == 2);
        CHECK(fixture.host.recorder().collisions()[0].kind == collision_kind::TRIGGER_EXIT);
        CHECK(fixture.host.recorder().collisions()[0].other == 2);
        CHECK(fixture.host.recorder().collisions()[0].impulse == 3.0f);
        CHECK(fixture.host.recorder().collisions()[1].kind == collision_kind::COUNT);
        CHECK(fixture.managed().collect_garbage() == 0);
        CHECK(fixture.host.recorder().collect_garbage_count() == 1);
        fixture.host.reset_recorder();
        CHECK(fixture.host.recorder().count("run_phase") == 0);
        CHECK(fixture.host.recorder().calls().empty());
    }

    TEST_CASE("null_script_host - the managed table fails softly once the host is gone") {
        conjure_managed_api copy = {};
        {
            bare_host fixture = {};
            copy = fixture.managed();
        }
        CHECK(null_script_host::active() == nullptr);
        CHECK(copy.run_phase(0, 0.0f) == status_of(script_error::NOT_INITIALIZED));
        CHECK(copy.unload_game_assembly() == status_of(script_error::NOT_INITIALIZED));
        CHECK(copy.collect_garbage() == status_of(script_error::NOT_INITIALIZED));
        CHECK(copy.shutdown() == status_of(script_error::NOT_INITIALIZED));
        CHECK(copy.load_game_assembly(nullptr, 0) == status_of(script_error::NOT_INITIALIZED));
    }

    TEST_CASE("null_script_host - fake classes can be replaced, removed and cleared") {
        null_script_host host = {};
        host.add_fake_class(make_class("Game.A", {}));
        host.add_fake_class(make_class("Game.A", {script_callback::UPDATE}));
        CHECK(host.remove_fake_class(hash_of("Game.A")));
        CHECK_FALSE(host.remove_fake_class(hash_of("Game.A")));
        host.add_fake_class(make_class("Game.B", {}));
        host.clear_fake_classes();
        CHECK_FALSE(host.remove_fake_class(hash_of("Game.B")));
    }
}
