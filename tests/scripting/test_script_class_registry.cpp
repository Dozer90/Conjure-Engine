#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"
#include "scripting_test_support.h"

#include "conjure/core/hash.h"
#include "conjure/scripting/script_class_registry.h"
#include "conjure/scripting/scripting_error.h"

#include <limits>
#include <utility>

using namespace conjure;
using namespace conjure::scripting;
using namespace conjure::testing;
using namespace scripting_test;

TEST_SUITE("scripting/script_class_registry") {

    TEST_CASE("script_class_registry - register and lookup by hash and name") {
        script_class_registry registry = {};
        CHECK(registry.size() == 0);
        CHECK_FALSE(registry.is_staging_open());
        registry.begin_staging();
        CHECK(registry.is_staging_open());
        CONJURE_REQUIRE_OK(registry.stage_class(make_class("Game.Player", {script_callback::UPDATE})));
        CHECK(registry.find(hash_of("Game.Player")) == nullptr);        // staged, not active
        CHECK(registry.find_staged(hash_of("Game.Player")) != nullptr);
        registry.commit_staging();
        CHECK_FALSE(registry.is_staging_open());
        CHECK(registry.size() == 1);
        const script_class_info *player = registry.find(hash_of("Game.Player"));
        REQUIRE(player != nullptr);
        CHECK(registry.find("Game.Player") == player);
        CHECK(player->fullName == "Game.Player");
        CHECK(registry.index_of(player->hash) == 0);
        CHECK(registry.index_of(42) == std::numeric_limits<u32>::max());
        CHECK(registry.find(42) == nullptr);
        CHECK(registry.find("Game.Missing") == nullptr);
        CHECK(registry.find_staged(player->hash) == nullptr);
    }

    TEST_CASE("script_class_registry - duplicate class is rejected in staging") {
        script_class_registry registry = {};
        registry.begin_staging();
        CONJURE_REQUIRE_OK(registry.stage_class(make_class("Game.A", {})));
        CONJURE_CHECK_ERROR(registry.stage_class(make_class("Game.A", {})), core::error_code::ALREADY_EXISTS);
        CHECK(registry.staged_classes().size() == 1);
    }

    TEST_CASE("script_class_registry - staging requires begin_staging") {
        script_class_registry registry = {};
        CONJURE_CHECK_ERROR(registry.stage_class(make_class("Game.A", {})), core::error_code::INVALID_STATE);
        CONJURE_CHECK_ERROR(
          registry.stage_field(1, make_field("f", script_field_kind::INT32)),
          core::error_code::INVALID_STATE
        );
    }

    TEST_CASE("script_class_registry - commit_staging without an open staging keeps the active table") {
        script_class_registry registry = {};
        registry.begin_staging();
        CONJURE_REQUIRE_OK(registry.stage_class(make_class("Game.Kept", {})));
        registry.commit_staging();
        REQUIRE(registry.find("Game.Kept") != nullptr);

        expect_assert guard = {};
        registry.commit_staging();                  // a second commit
        CHECK(registry.find("Game.Kept") != nullptr);
        registry.begin_staging();
        CONJURE_REQUIRE_OK(registry.stage_class(make_class("Game.Dropped", {})));
        registry.discard_staging();
        registry.commit_staging();                  // a commit after a discard
        CHECK(registry.find("Game.Kept") != nullptr);
        CHECK(registry.find("Game.Dropped") == nullptr);
        CHECK(registry.size() == 1);
        CHECK_FALSE(registry.is_staging_open());
#if CONJURE_ASSERTS_ENABLED
        CHECK(guard.count() == 2);
#endif
    }

    TEST_CASE("script_class_registry - swap_staging replaces the active schema and clears staging") {
        script_class_registry registry = {};
        registry.begin_staging();
        CONJURE_REQUIRE_OK(registry.stage_class(make_class("Game.Old", {})));
        registry.commit_staging();
        REQUIRE(registry.find("Game.Old") != nullptr);

        registry.begin_staging();
        CONJURE_REQUIRE_OK(registry.stage_class(make_class("Game.New", {})));
        CHECK(registry.find("Game.Old") != nullptr);
        registry.commit_staging();
        CHECK(registry.find("Game.Old") == nullptr);
        CHECK(registry.find("Game.New") != nullptr);
        CHECK(registry.staged_classes().empty());
        CHECK(registry.classes().size() == 1);

        SUBCASE("discard keeps the active schema") {
            registry.begin_staging();
            CONJURE_REQUIRE_OK(registry.stage_class(make_class("Game.Discarded", {})));
            registry.discard_staging();
            CHECK_FALSE(registry.is_staging_open());
            CHECK(registry.find("Game.New") != nullptr);
            CHECK(registry.find("Game.Discarded") == nullptr);
            CHECK(registry.staged_classes().empty());
        }
        SUBCASE("clear empties everything") {
            registry.clear();
            CHECK(registry.size() == 0);
            CHECK(registry.find("Game.New") == nullptr);
        }
    }

    TEST_CASE("script_class_registry - fields are sorted by hash after commit") {
        script_class_registry registry = {};
        registry.begin_staging();
        script_class_info info = {};
        info.fullName = "Game.Fields";
        info.hash = hash_of(info.fullName);
        CONJURE_REQUIRE_OK(registry.stage_class(info));
        for (const std::string_view name : {"zeta", "alpha", "mid", "omega", "beta"}) {
            CONJURE_REQUIRE_OK(registry.stage_field(info.hash, make_field(name, script_field_kind::FLOAT)));
        }
        CONJURE_CHECK_ERROR(
          registry.stage_field(info.hash, make_field("alpha", script_field_kind::FLOAT)),
          core::error_code::ALREADY_EXISTS
        );
        CONJURE_CHECK_ERROR(
          registry.stage_field(99, make_field("x", script_field_kind::FLOAT)),
          script_error::CLASS_NOT_FOUND
        );
        registry.commit_staging();

        const script_class_info *committed = registry.find(info.hash);
        REQUIRE(committed != nullptr);
        REQUIRE(committed->fields.size() == 5);
        for (usize index = 1; index < committed->fields.size(); ++index) {
            CAPTURE(index);
            CHECK(committed->fields[index - 1].hash < committed->fields[index].hash);
        }
        for (const std::string_view name : {"zeta", "alpha", "mid", "omega", "beta"}) {
            const script_field_info *field = committed->find_field(name);
            REQUIRE(field != nullptr);
            CHECK(field->name == name);
            CHECK(committed->find_field(hash_of(name)) == field);
        }
        CHECK(committed->find_field("missing") == nullptr);
    }

    TEST_CASE("script_class_registry - callback mask is preserved") {
        script_class_registry registry = {};
        registry.begin_staging();
        CONJURE_REQUIRE_OK(registry.stage_class(make_class("Game.Masked", {
          script_callback::CREATE, script_callback::LATE_UPDATE, script_callback::TRIGGER_EXIT
        })));
        registry.commit_staging();
        const script_class_info *masked = registry.find("Game.Masked");
        REQUIRE(masked != nullptr);
        CHECK(masked->has_callback(script_callback::CREATE));
        CHECK(masked->has_callback(script_callback::LATE_UPDATE));
        CHECK(masked->has_callback(script_callback::TRIGGER_EXIT));
        CHECK_FALSE(masked->has_callback(script_callback::UPDATE));
        CHECK_FALSE(masked->has_callback(script_callback::DESTROY));
        const u16 expected = static_cast<u16>(
          callback_bit(script_callback::CREATE) | callback_bit(script_callback::LATE_UPDATE) |
          callback_bit(script_callback::TRIGGER_EXIT)
        );
        CHECK(masked->callbackMask == expected);
    }

    TEST_CASE("script_name_hash - is the FNV-1a 64 of the UTF-8 name") {
        CHECK(script_name_hash("") == core::hash::FNV1A64_OFFSET_BASIS);
        CHECK(script_name_hash("a") == 0xAF63DC4C8601EC8Cull);
        CHECK(script_name_hash("Game.Player") == core::hash::fnv1a64("Game.Player"));
    }

    TEST_CASE("script_class_registry - is movable") {
        script_class_registry first = {};
        first.begin_staging();
        CONJURE_REQUIRE_OK(first.stage_class(make_class("Game.Moved", {})));
        first.commit_staging();
        script_class_registry second = std::move(first);
        CHECK(second.find("Game.Moved") != nullptr);
        CHECK(second.size() == 1);
    }
}
