#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"

#include "reflect_test_types.h"

#include "conjure/core/event.h"
#include "conjure/core/reflect/type_info.h"
#include "conjure/core/reflect/type_registry.h"

#include <atomic>
#include <format>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::reflect;

namespace {
    // Macro-described types used only by the cases that register, break and repair the process-wide
    // registry, so no other case depends on their state.
    struct reclaimed {
        u32 value = 0;
    };

    enum class reclaimed_mode : u8 {
        ON,
        OFF
    };
}

CONJURE_REFLECT_STRUCT(reclaimed, "test.reclaimed", CONJURE_FIELD(value));
CONJURE_REFLECT_ENUM(reclaimed_mode, "test.reclaimed_mode", ON, OFF);

namespace {
    /// <summary>
    /// A small valid description: one u32 field named "value".
    /// </summary>
    reflect::type_info make_simple(std::string name, u32 size = 4) {
        field_info value = {};
        value.name = "value";
        value.kind = field_kind::U32;
        value.type = type_id_of<u32>();
        value.offset = 0;
        value.size = 4;

        reflect::type_info info = {};
        info.name = std::move(name);
        info.size = size;
        info.alignment = 4;
        info.fields = {value};
        return info;
    }

    reflect::type_info make_holder(std::string name, const reflect::type_info &nested) {
        field_info field = {};
        field.name = "nested";
        field.kind = field_kind::STRUCT;
        field.type = nested.type;
        field.offset = 0;
        field.size = nested.size;
        field.structType = &nested;

        reflect::type_info info = {};
        info.name = std::move(name);
        info.size = nested.size;
        info.alignment = nested.alignment;
        info.fields = {field};
        return info;
    }

    reflect::type_info make_array_holder(std::string name, const reflect::type_info &nested) {
        field_info field = {};
        field.name = "parts";
        field.kind = field_kind::ARRAY;
        field.elementKind = field_kind::STRUCT;
        field.arrayCount = 2;
        field.type = nested.type;
        field.offset = 0;
        field.size = nested.size * 2;
        field.structType = &nested;

        reflect::type_info info = {};
        info.name = std::move(name);
        info.size = nested.size * 2;
        info.alignment = nested.alignment;
        info.fields = {field};
        return info;
    }

    enum_info make_enum(std::string name) {
        enum_info info = {};
        info.name = std::move(name);
        info.underlyingSize = 1;
        info.values = {{"A", 0}, {"B", 1}};
        return info;
    }

    reflect::type_info make_enum_holder(std::string name, const enum_info &nested) {
        field_info field = {};
        field.name = "mode";
        field.kind = field_kind::ENUM;
        field.type = nested.type;
        field.offset = 0;
        field.size = nested.underlyingSize;
        field.enumType = &nested;

        reflect::type_info info = {};
        info.name = std::move(name);
        info.size = 4;
        info.alignment = 4;
        info.fields = {field};
        return info;
    }
}

TEST_SUITE("core/type_registry") {

    TEST_CASE("type_registry - a fresh registry is empty") {
        const type_registry local = {};
        CHECK(local.type_count() == 0);
        CHECK(local.enum_count() == 0);
        CHECK(local.types().empty());
        CHECK(local.enums().empty());
        CHECK(local.find(type_id::from_name("test.anything")) == nullptr);
        CHECK(local.find("test.anything") == nullptr);
        CHECK(local.find_enum(type_id::from_name("test.anything")) == nullptr);
        CHECK(local.find_enum("test.anything") == nullptr);
        CHECK(local.find(type_id::null()) == nullptr);
    }

    TEST_CASE("type_registry - register_type stores a copy and finds it by id and by name") {
        type_registry local = {};
        const reflect::type_info source = make_simple("test.simple");

        const result<const reflect::type_info *> registered = local.register_type(source);
        REQUIRE(registered.has_value());
        const reflect::type_info *stored = registered.value();
        REQUIRE(stored != nullptr);
        CHECK(stored->name == "test.simple");
        CHECK(stored->type == type_id::from_name("test.simple"));
        CHECK(stored->fields.size() == 1);
        CHECK(stored->has_same_layout(source));

        CHECK(local.find(type_id::from_name("test.simple")) == stored);
        CHECK(local.find("test.simple") == stored);
        CHECK(local.find("test.Simple") == nullptr);
        CHECK(local.type_count() == 1);
        REQUIRE(local.types().size() == 1);
        CHECK(local.types()[0] == stored);
    }

    TEST_CASE("type_registry - a null id is derived from the name and a mismatching id is rejected") {
        type_registry local = {};

        reflect::type_info derived = make_simple("test.derived");
        CHECK(derived.type.is_null());
        const result<const reflect::type_info *> registered = local.register_type(derived);
        REQUIRE(registered.has_value());
        CHECK(registered.value()->type == type_id::from_name("test.derived"));

        reflect::type_info explicitId = make_simple("test.explicit");
        explicitId.type = type_id::from_name("test.explicit");
        CHECK(local.register_type(explicitId).has_value());

        reflect::type_info mismatched = make_simple("test.mismatched");
        mismatched.type = type_id::from_name("test.something_else");
        const result<const reflect::type_info *> rejected = local.register_type(mismatched);
        REQUIRE_FALSE(rejected.has_value());
        CHECK(rejected.get_error().generic_code() == error_code::INVALID_ARGUMENT);
        CHECK(rejected.get_error().message().find("does not match") != std::string::npos);
        CHECK(local.find("test.mismatched") == nullptr);
    }

    TEST_CASE("type_registry - register_type validates the layout") {
        type_registry local = {};
        reflect::type_info broken = make_simple("test.broken", 2);
        const result<const reflect::type_info *> rejected = local.register_type(broken);
        REQUIRE_FALSE(rejected.has_value());
        CHECK(rejected.get_error().generic_code() == error_code::INVALID_ARGUMENT);
        CHECK(local.type_count() == 0);
    }

    TEST_CASE("type_registry - registering the same layout again returns the existing description") {
        type_registry local = {};
        const result<const reflect::type_info *> first = local.register_type(make_simple("test.same"));
        REQUIRE(first.has_value());

        reflect::type_info again = make_simple("test.same");
        again.fields[0].attributes.tooltip = "attributes do not take part in the layout";
        const result<const reflect::type_info *> second = local.register_type(again);
        REQUIRE(second.has_value());
        CHECK(second.value() == first.value());
        CHECK(local.type_count() == 1);
        CHECK(first.value()->fields[0].attributes.tooltip.empty());
    }

    TEST_CASE("type_registry - registering the same id with a different layout fails with ALREADY_EXISTS") {
        type_registry local = {};
        REQUIRE(local.register_type(make_simple("test.conflict")).has_value());

        reflect::type_info different = make_simple("test.conflict", 8);
        const result<const reflect::type_info *> rejected = local.register_type(different);
        REQUIRE_FALSE(rejected.has_value());
        CHECK(rejected.get_error().generic_code() == error_code::ALREADY_EXISTS);
        CHECK(rejected.get_error().message().find("different layout") != std::string::npos);

        reflect::type_info renamedField = make_simple("test.conflict");
        renamedField.fields[0].name = "other";
        CHECK_FALSE(local.register_type(renamedField).has_value());
        CHECK(local.type_count() == 1);
    }

    TEST_CASE("type_registry - types are listed in registration order") {
        type_registry local = {};
        const reflect::type_info *first = local.register_type(make_simple("test.order.a")).value();
        const reflect::type_info *second = local.register_type(make_simple("test.order.b")).value();
        const reflect::type_info *third = local.register_type(make_simple("test.order.c")).value();

        const std::span<const reflect::type_info *const> listed = local.types();
        REQUIRE(listed.size() == 3);
        CHECK(listed[0] == first);
        CHECK(listed[1] == second);
        CHECK(listed[2] == third);
    }

    TEST_CASE("type_registry - registered descriptions keep their address while more types are added") {
        type_registry local = {};
        std::vector<const reflect::type_info *> pointers = {};
        for (int index = 0; index < 1000; ++index) {
            pointers.push_back(local.register_type(make_simple(std::format("test.stable.{}", index))).value());
        }
        for (int index = 0; index < 1000; ++index) {
            CHECK(pointers[static_cast<usize>(index)]->name == std::format("test.stable.{}", index));
            CHECK(local.find(std::format("test.stable.{}", index)) == pointers[static_cast<usize>(index)]);
        }
        CHECK(local.type_count() == 1000);
        CHECK(local.types().size() == 1000);
    }

    TEST_CASE("type_registry - on_registered fires once per new type with the stored description") {
        type_registry local = {};
        std::vector<const reflect::type_info *> seen = {};
        const event_subscription token = local.on_registered().subscribe([&seen](const reflect::type_info &info) {
            seen.push_back(&info);
        });

        const reflect::type_info *first = local.register_type(make_simple("test.event.a")).value();
        REQUIRE(seen.size() == 1);
        CHECK(seen[0] == first);

        CHECK(local.register_type(make_simple("test.event.a")).value() == first);
        CHECK(seen.size() == 1);

        CHECK_FALSE(local.register_type(make_simple("test.event.a", 8)).has_value());
        CHECK(seen.size() == 1);

        const reflect::type_info *second = local.register_type(make_simple("test.event.b")).value();
        REQUIRE(seen.size() == 2);
        CHECK(seen[1] == second);
    }

    TEST_CASE("type_registry - a registration handler may register further types") {
        type_registry local = {};
        std::vector<std::string> order = {};
        const auto onRegistered = [&local, &order](const reflect::type_info &info) {
            order.push_back(info.name);
            if (info.name == "test.chain.outer") {
                CHECK(local.register_type(make_simple("test.chain.inner")).has_value());
            }
        };
        const event_subscription token = local.on_registered().subscribe(onRegistered);

        REQUIRE(local.register_type(make_simple("test.chain.outer")).has_value());
        REQUIRE(order.size() == 2);
        CHECK(order[0] == "test.chain.outer");
        CHECK(order[1] == "test.chain.inner");
        CHECK(local.type_count() == 2);
    }

    TEST_CASE("type_registry - unregister_type removes a type and emits on_unregistered") {
        type_registry local = {};
        std::vector<std::string> removed = {};
        const event_subscription token = local.on_unregistered().subscribe([&removed](const reflect::type_info &info) {
            removed.push_back(info.name);
        });

        const reflect::type_info *stored = local.register_type(make_simple("test.remove")).value();
        REQUIRE(local.register_type(make_simple("test.keep")).has_value());
        const type_id id = stored->type;

        REQUIRE(local.unregister_type(id).has_value());
        CHECK(local.find(id) == nullptr);
        CHECK(local.find("test.remove") == nullptr);
        CHECK(local.type_count() == 1);
        REQUIRE(local.types().size() == 1);
        CHECK(local.types()[0]->name == "test.keep");
        REQUIRE(removed.size() == 1);
        CHECK(removed[0] == "test.remove");

        const result<void> again = local.unregister_type(id);
        REQUIRE_FALSE(again.has_value());
        CHECK(again.get_error().generic_code() == error_code::NOT_FOUND);
        CHECK(removed.size() == 1);
    }

    TEST_CASE("type_registry - unregistering a type referenced by a struct field is INVALID_STATE") {
        type_registry local = {};
        const reflect::type_info *inner = local.register_type(make_simple("test.ref.inner")).value();
        const reflect::type_info *outer = local.register_type(make_holder("test.ref.outer", *inner)).value();

        const result<void> blocked = local.unregister_type(inner->type);
        REQUIRE_FALSE(blocked.has_value());
        CHECK(blocked.get_error().generic_code() == error_code::INVALID_STATE);
        CHECK(blocked.get_error().message().find("test.ref.outer.nested") != std::string::npos);
        CHECK(local.find(inner->type) == inner);

        REQUIRE(local.unregister_type(outer->type).has_value());
        CHECK(local.unregister_type(inner->type).has_value());
        CHECK(local.type_count() == 0);
    }

    TEST_CASE("type_registry - unregistering a type referenced by an array of structs is INVALID_STATE") {
        type_registry local = {};
        const reflect::type_info *inner = local.register_type(make_simple("test.array_ref.inner")).value();
        REQUIRE(local.register_type(make_array_holder("test.array_ref.outer", *inner)).has_value());

        const result<void> blocked = local.unregister_type(inner->type);
        REQUIRE_FALSE(blocked.has_value());
        CHECK(blocked.get_error().generic_code() == error_code::INVALID_STATE);
    }

    TEST_CASE("type_registry - a type can be registered again after it was unregistered") {
        type_registry local = {};
        const reflect::type_info *first = local.register_type(make_simple("test.again")).value();
        REQUIRE(local.unregister_type(first->type).has_value());

        const reflect::type_info *second = local.register_type(make_simple("test.again", 8)).value();
        REQUIRE(second != nullptr);
        CHECK(second->size == 8);
        CHECK(local.find("test.again") == second);
        CHECK(local.type_count() == 1);

        // The retired description stays readable (its memory is kept for lock-free readers).
        CHECK(first->name == "test.again");
        CHECK(first->size == 4);
    }

    TEST_CASE("type_registry - register_enum validates and stores enumerations") {
        type_registry local = {};
        const result<const enum_info *> registered = local.register_enum(make_enum("test.enum"));
        REQUIRE(registered.has_value());
        const enum_info *stored = registered.value();
        CHECK(stored->name == "test.enum");
        CHECK(stored->type == type_id::from_name("test.enum"));
        CHECK(local.find_enum(stored->type) == stored);
        CHECK(local.find_enum("test.enum") == stored);
        CHECK(local.find_enum("test.Enum") == nullptr);
        CHECK(local.enum_count() == 1);
        REQUIRE(local.enums().size() == 1);
        CHECK(local.enums()[0] == stored);

        CHECK(local.register_enum(make_enum("test.enum")).value() == stored);
        CHECK(local.enum_count() == 1);

        enum_info different = make_enum("test.enum");
        different.values.push_back({"C", 2});
        const result<const enum_info *> conflict = local.register_enum(different);
        REQUIRE_FALSE(conflict.has_value());
        CHECK(conflict.get_error().generic_code() == error_code::ALREADY_EXISTS);

        enum_info mismatched = make_enum("test.other_enum");
        mismatched.type = type_id::from_name("test.enum");
        const result<const enum_info *> badId = local.register_enum(mismatched);
        REQUIRE_FALSE(badId.has_value());
        CHECK(badId.get_error().generic_code() == error_code::INVALID_ARGUMENT);
    }

    TEST_CASE("type_registry - register_enum rejects malformed descriptions") {
        type_registry local = {};

        enum_info unnamed = make_enum("");
        CHECK(local.register_enum(unnamed).get_error().generic_code() == error_code::INVALID_ARGUMENT);

        enum_info badSize = make_enum("test.bad_size");
        badSize.underlyingSize = 3;
        const result<const enum_info *> size = local.register_enum(badSize);
        REQUIRE_FALSE(size.has_value());
        CHECK(size.get_error().message().find("1, 2, 4 or 8") != std::string::npos);

        enum_info duplicate = make_enum("test.duplicate");
        duplicate.values.push_back({"A", 5});
        const result<const enum_info *> twice = local.register_enum(duplicate);
        REQUIRE_FALSE(twice.has_value());
        CHECK(twice.get_error().message().find("declared twice") != std::string::npos);

        enum_info unnamedValue = make_enum("test.unnamed_value");
        unnamedValue.values.push_back({"", 7});
        CHECK_FALSE(local.register_enum(unnamedValue).has_value());

        enum_info tooLarge = make_enum("test.too_large");
        tooLarge.values.push_back({"HUGE", 300});
        const result<const enum_info *> range = local.register_enum(tooLarge);
        REQUIRE_FALSE(range.has_value());
        CHECK(range.get_error().message().find("does not fit") != std::string::npos);

        CHECK(local.enum_count() == 0);
    }

    TEST_CASE("type_registry - unregister_enum removes enumerations unless a field uses them") {
        type_registry local = {};
        const enum_info *stored = local.register_enum(make_enum("test.unreg_enum")).value();
        const reflect::type_info *holder = local.register_type(make_enum_holder("test.enum_holder", *stored)).value();

        const result<void> blocked = local.unregister_enum(stored->type);
        REQUIRE_FALSE(blocked.has_value());
        CHECK(blocked.get_error().generic_code() == error_code::INVALID_STATE);
        CHECK(blocked.get_error().message().find("test.enum_holder.mode") != std::string::npos);

        REQUIRE(local.unregister_type(holder->type).has_value());
        REQUIRE(local.unregister_enum(stored->type).has_value());
        CHECK(local.find_enum(stored->type) == nullptr);
        CHECK(local.enum_count() == 0);

        const result<void> missing = local.unregister_enum(stored->type);
        REQUIRE_FALSE(missing.has_value());
        CHECK(missing.get_error().generic_code() == error_code::NOT_FOUND);
    }

    TEST_CASE("type_registry - the process-wide registry is filled lazily by type_info_of and enum_info_of") {
        const reflect::type_info *inner = type_info_of<reflect_test::inner>();
        REQUIRE(inner != nullptr);
        CHECK(inner->name == "test.inner");
        CHECK(type_info_of<reflect_test::inner>() == inner);
        CHECK(type_info_of<const reflect_test::inner &>() == inner);
        CHECK(registry().find(type_id_of<reflect_test::inner>()) == inner);
        CHECK(registry().find("test.inner") == inner);
        CHECK(&registry() == &registry());

        const enum_info *weapon = enum_info_of<reflect_test::weapon_kind>();
        REQUIRE(weapon != nullptr);
        CHECK(weapon->name == "test.weapon_kind");
        CHECK(registry().find_enum("test.weapon_kind") == weapon);
        CHECK(enum_info_of<reflect_test::weapon_kind>() == weapon);

        const enum_info *flags = enum_info_of<reflect_test::body_flags>();
        REQUIRE(flags != nullptr);
        CHECK(flags->isFlags);
        const enum_info *layers = enum_info_of<reflect_test::layer_mask>();
        REQUIRE(layers != nullptr);
        CHECK(layers->isFlags);
        REQUIRE(layers->values.size() == 2);
        CHECK(layers->values[1].first == "SECOND");
    }

    TEST_CASE("type_registry - type_info_of registers nested structs and enums along the way") {
        const reflect::type_info *everything = type_info_of<reflect_test::everything>();
        REQUIRE(everything != nullptr);
        CHECK(everything->find_field("nested")->structType == type_info_of<reflect_test::inner>());
        CHECK(registry().find("test.inner") != nullptr);
        CHECK(registry().find_enum("test.weapon_kind") != nullptr);
        CHECK(registry().find_enum("test.body_flags") != nullptr);
        CHECK(registry().find_enum("test.signed_mode") != nullptr);
    }

    TEST_CASE("type_registry - unreflected types have no description") {
        CHECK(type_info_of<reflect_test::not_reflected>() == nullptr);
        CHECK(type_info_of<int>() == nullptr);
        CHECK(type_info_of<f32>() == nullptr);
        CHECK(type_info_of<reflect_test::weapon_kind>() == nullptr);
        CHECK(enum_info_of<reflect_test::inner>() == nullptr);
        CHECK(enum_info_of<int>() == nullptr);
    }

    TEST_CASE("type_registry - the process-wide registry refuses a conflicting layout under a reflected name") {
        testing::log_capture logs = {};
        // The macro type must own the name before the impostor is tried, whatever ran before.
        const reflect::type_info *owner = type_info_of<reflect_test::inner>();
        REQUIRE(owner != nullptr);

        reflect::type_info impostor = make_simple("test.inner");
        const result<const reflect::type_info *> rejected = registry().register_type(impostor);
        REQUIRE_FALSE(rejected.has_value());
        CHECK(rejected.get_error().generic_code() == error_code::ALREADY_EXISTS);
        CHECK(registry().find("test.inner") == owner);
        CHECK(type_info_of<reflect_test::inner>() == owner);
    }

    TEST_CASE("type_registry - type_info_of recovers once a conflicting dynamic type is unregistered") {
        testing::log_capture logs = {};
        logs.allow_errors();
        const type_id id = type_id_of<reclaimed>();
        if (registry().find(id) != nullptr) {
            REQUIRE(registry().unregister_type(id).has_value());
        }

        // A dynamic type with a different layout takes the macro type's name first.
        const reflect::type_info impostor = make_simple("test.reclaimed", 8);
        const result<const reflect::type_info *> taken = registry().register_type(impostor);
        REQUIRE(taken.has_value());
        CHECK(type_info_of<reclaimed>() == nullptr);
        CHECK(logs.contains("cannot register type 'test.reclaimed'"));
        CHECK(type_info_of<reclaimed>() == nullptr);
        CHECK(registry().find(id) == taken.value());

        // The failure is not cached: once the impostor is gone the macro type registers.
        REQUIRE(registry().unregister_type(id).has_value());
        const reflect::type_info *fresh = type_info_of<reclaimed>();
        REQUIRE(fresh != nullptr);
        CHECK(fresh != taken.value());
        CHECK(fresh->size == sizeof(reclaimed));
        CHECK(registry().find(id) == fresh);
        CHECK(type_info_of<reclaimed>() == fresh);
    }

    TEST_CASE("type_registry - type_info_of returns the fresh registration after unregister_type") {
        const reflect::type_info *first = type_info_of<reclaimed>();
        REQUIRE(first != nullptr);
        REQUIRE(registry().unregister_type(first->type).has_value());
        CHECK(registry().find(first->type) == nullptr);

        const reflect::type_info *second = type_info_of<reclaimed>();
        REQUIRE(second != nullptr);
        CHECK(second != first);
        CHECK(second->has_same_layout(*first));
        CHECK(registry().find(first->type) == second);
        CHECK(type_info_of<reclaimed>() == second);

        // The retired description is still readable, as documented for lock-free readers.
        CHECK(first->name == "test.reclaimed");
    }

    TEST_CASE("type_registry - enum_info_of recovers once a conflicting enumeration is unregistered") {
        testing::log_capture logs = {};
        logs.allow_errors();
        const type_id id = type_id_of<reclaimed_mode>();
        if (registry().find_enum(id) != nullptr) {
            REQUIRE(registry().unregister_enum(id).has_value());
        }

        const enum_info *taken = registry().register_enum(make_enum("test.reclaimed_mode")).value();
        CHECK(enum_info_of<reclaimed_mode>() == nullptr);
        CHECK(logs.contains("cannot register enum 'test.reclaimed_mode'"));
        CHECK(registry().find_enum(id) == taken);

        REQUIRE(registry().unregister_enum(id).has_value());
        const enum_info *fresh = enum_info_of<reclaimed_mode>();
        REQUIRE(fresh != nullptr);
        CHECK(fresh != taken);
        REQUIRE(fresh->values.size() == 2);
        CHECK(fresh->values[0].first == "ON");
        CHECK(registry().find_enum(id) == fresh);
        CHECK(enum_info_of<reclaimed_mode>() == fresh);

        REQUIRE(registry().unregister_enum(id).has_value());
        const enum_info *again = enum_info_of<reclaimed_mode>();
        REQUIRE(again != nullptr);
        CHECK(again != fresh);
        CHECK(registry().find_enum(id) == again);
    }

    TEST_CASE("type_registry - register_type rejects struct and enum pointers that are not live in this registry") {
        type_registry local = {};

        // A never-registered (stack) description would dangle.
        const reflect::type_info stackInner = make_simple("test.stack_inner");
        const result<const reflect::type_info *> stackHolder =
          local.register_type(make_holder("test.stack_holder", stackInner));
        REQUIRE_FALSE(stackHolder.has_value());
        CHECK(stackHolder.get_error().generic_code() == error_code::INVALID_ARGUMENT);
        CHECK(stackHolder.get_error().message().find("'nested'") != std::string::npos);
        CHECK(stackHolder.get_error().message().find("not registered here") != std::string::npos);

        // A description owned by another registry is foreign too.
        type_registry other = {};
        const reflect::type_info *foreign = other.register_type(make_simple("test.foreign_inner")).value();
        CHECK_FALSE(local.register_type(make_holder("test.foreign_holder", *foreign)).has_value());
        CHECK(other.register_type(make_holder("test.foreign_holder", *foreign)).has_value());

        const result<const reflect::type_info *> arrayHolder =
          local.register_type(make_array_holder("test.stack_array_holder", stackInner));
        REQUIRE_FALSE(arrayHolder.has_value());
        CHECK(arrayHolder.get_error().message().find("'parts'") != std::string::npos);

        const enum_info stackEnum = make_enum("test.stack_enum");
        const result<const reflect::type_info *> enumHolder =
          local.register_type(make_enum_holder("test.stack_enum_holder", stackEnum));
        REQUIRE_FALSE(enumHolder.has_value());
        CHECK(enumHolder.get_error().generic_code() == error_code::INVALID_ARGUMENT);
        CHECK(enumHolder.get_error().message().find("'mode'") != std::string::npos);
        CHECK(enumHolder.get_error().message().find("enum description") != std::string::npos);

        // A retired description is not live either.
        const reflect::type_info *retired = local.register_type(make_simple("test.retired_inner")).value();
        REQUIRE(local.unregister_type(retired->type).has_value());
        CHECK_FALSE(local.register_type(make_holder("test.retired_holder", *retired)).has_value());

        CHECK(local.type_count() == 0);
    }

    TEST_CASE("type_registry - types and enums keep registration order across an unregistration") {
        type_registry local = {};
        const reflect::type_info *first = local.register_type(make_simple("test.gap.a")).value();
        const reflect::type_info *second = local.register_type(make_simple("test.gap.b")).value();
        const reflect::type_info *third = local.register_type(make_simple("test.gap.c")).value();
        REQUIRE(local.unregister_type(second->type).has_value());

        std::span<const reflect::type_info *const> listed = local.types();
        REQUIRE(listed.size() == 2);
        CHECK(listed[0] == first);
        CHECK(listed[1] == third);

        const reflect::type_info *fourth = local.register_type(make_simple("test.gap.d")).value();
        listed = local.types();
        REQUIRE(listed.size() == 3);
        CHECK(listed[2] == fourth);

        const enum_info *alpha = local.register_enum(make_enum("test.gap_enum.a")).value();
        const enum_info *beta = local.register_enum(make_enum("test.gap_enum.b")).value();
        const enum_info *gamma = local.register_enum(make_enum("test.gap_enum.c")).value();
        REQUIRE(local.unregister_enum(beta->type).has_value());
        const std::span<const enum_info *const> enums = local.enums();
        REQUIRE(enums.size() == 2);
        CHECK(enums[0] == alpha);
        CHECK(enums[1] == gamma);
    }

    TEST_CASE("type_registry - a registration handler may unregister the type it was told about") {
        type_registry local = {};
        std::vector<std::string> events = {};
        const auto onRegistered = [&local, &events](const reflect::type_info &info) {
            events.push_back("+" + info.name);
            if (info.name == "test.transient") {
                CHECK(local.unregister_type(info.type).has_value());
            }
        };
        const auto onUnregistered = [&events](const reflect::type_info &info) {
            events.push_back("-" + info.name);
        };
        const event_subscription added = local.on_registered().subscribe(onRegistered);
        const event_subscription removed = local.on_unregistered().subscribe(onUnregistered);

        REQUIRE(local.register_type(make_simple("test.transient")).has_value());
        REQUIRE(events.size() == 2);
        CHECK(events[0] == "+test.transient");
        CHECK(events[1] == "-test.transient");
        CHECK(local.type_count() == 0);
        CHECK(local.find("test.transient") == nullptr);
    }

    TEST_CASE("type_registry - events report a registration before the unregistration racing it") {
        type_registry local = {};
        constexpr int NAME_COUNT = 300;

        // Handlers are serialised by the registry, so a plain vector is safe here.
        std::vector<std::pair<std::string, bool>> events = {};
        const auto onRegistered = [&events](const reflect::type_info &info) {
            events.emplace_back(info.name, true);
        };
        const auto onUnregistered = [&events](const reflect::type_info &info) {
            events.emplace_back(info.name, false);
        };
        const event_subscription added = local.on_registered().subscribe(onRegistered);
        const event_subscription removed = local.on_unregistered().subscribe(onUnregistered);

        std::atomic<int> registrationFailures = 0;
        std::thread registrar([&local, &registrationFailures] {
            for (int index = 0; index < NAME_COUNT; ++index) {
                const result<const reflect::type_info *> registered =
                  local.register_type(make_simple(std::format("test.race.{}", index)));
                if (!registered.has_value()) {
                    registrationFailures.fetch_add(1);
                }
            }
        });
        std::thread remover([&local] {
            for (int index = 0; index < NAME_COUNT; ++index) {
                const type_id id = type_id::from_name(std::format("test.race.{}", index));
                while (!local.unregister_type(id).has_value()) {
                    std::this_thread::yield();
                }
            }
        });
        registrar.join();
        remover.join();

        CHECK(registrationFailures.load() == 0);
        REQUIRE(events.size() == static_cast<usize>(2 * NAME_COUNT));
        std::vector<int> balance(NAME_COUNT, 0);
        int violations = 0;
        constexpr std::string_view PREFIX = "test.race.";
        for (const std::pair<std::string, bool> &entry : events) {
            const usize index = static_cast<usize>(std::stoi(entry.first.substr(PREFIX.size())));
            int &seen = balance[index];
            if (entry.second) {
                violations += seen == 0 ? 0 : 1;
                seen = 1;
            } else {
                violations += seen == 1 ? 0 : 1;
                seen = 2;
            }
        }
        CHECK(violations == 0);
        CHECK(local.type_count() == 0);
    }

    TEST_CASE("type_registry - lookups stay lock-free and correct while other threads register") {
        type_registry local = {};
        constexpr int STABLE_COUNT = 64;
        constexpr int WRITER_COUNT = 8;
        constexpr int PER_WRITER = 400;
        constexpr int READER_COUNT = 4;

        std::vector<const reflect::type_info *> stable = {};
        for (int index = 0; index < STABLE_COUNT; ++index) {
            stable.push_back(local.register_type(make_simple(std::format("test.stable.{}", index))).value());
        }

        std::atomic<bool> stop = false;
        std::atomic<int> readerFailures = 0;
        std::vector<std::thread> readers = {};
        for (int readerIndex = 0; readerIndex < READER_COUNT; ++readerIndex) {
            readers.emplace_back([&local, &stable, &stop, &readerFailures] {
                while (!stop.load()) {
                    for (int index = 0; index < STABLE_COUNT; ++index) {
                        const reflect::type_info *expected = stable[static_cast<usize>(index)];
                        if (local.find(expected->type) != expected) {
                            readerFailures.fetch_add(1);
                        }
                    }
                }
            });
        }

        std::atomic<int> writerFailures = 0;
        std::vector<std::thread> writers = {};
        for (int writerIndex = 0; writerIndex < WRITER_COUNT; ++writerIndex) {
            writers.emplace_back([&local, &writerFailures, writerIndex] {
                for (int index = 0; index < PER_WRITER; ++index) {
                    const std::string name = std::format("test.thread.{}.{}", writerIndex, index);
                    const result<const reflect::type_info *> registered = local.register_type(make_simple(name));
                    if (!registered.has_value() || local.find(name) != registered.value()) {
                        writerFailures.fetch_add(1);
                    }
                }
            });
        }
        for (std::thread &thread : writers) {
            thread.join();
        }
        stop.store(true);
        for (std::thread &thread : readers) {
            thread.join();
        }

        CHECK(readerFailures.load() == 0);
        CHECK(writerFailures.load() == 0);
        CHECK(local.type_count() == static_cast<usize>(STABLE_COUNT + WRITER_COUNT * PER_WRITER));
        for (int writerIndex = 0; writerIndex < WRITER_COUNT; ++writerIndex) {
            for (int index = 0; index < PER_WRITER; ++index) {
                const std::string name = std::format("test.thread.{}.{}", writerIndex, index);
                const reflect::type_info *found = local.find(name);
                REQUIRE(found != nullptr);
                CHECK(found->name == name);
            }
        }
    }

    TEST_CASE("type_registry - concurrent registration of the same type yields one description") {
        type_registry local = {};
        constexpr int THREAD_COUNT = 8;
        std::atomic<int> ready = 0;
        std::atomic<bool> go = false;
        std::vector<const reflect::type_info *> results(THREAD_COUNT, nullptr);

        std::vector<std::thread> threads = {};
        for (int threadIndex = 0; threadIndex < THREAD_COUNT; ++threadIndex) {
            threads.emplace_back([&local, &ready, &go, &results, threadIndex] {
                ready.fetch_add(1);
                while (!go.load()) {
                    std::this_thread::yield();
                }
                const result<const reflect::type_info *> registered = local.register_type(make_simple("test.race"));
                results[static_cast<usize>(threadIndex)] = registered.has_value() ? registered.value() : nullptr;
            });
        }
        while (ready.load() < THREAD_COUNT) {
            std::this_thread::yield();
        }
        go.store(true);
        for (std::thread &thread : threads) {
            thread.join();
        }

        CHECK(local.type_count() == 1);
        for (const reflect::type_info *pointer : results) {
            CHECK(pointer == local.find("test.race"));
        }
    }

    TEST_CASE("type_registry - lookups of stable types stay correct while other types are removed and re-added") {
        type_registry local = {};
        constexpr int STABLE_COUNT = 32;
        constexpr int CHURN_COUNT = 32;
        constexpr int ROUNDS = 200;

        std::vector<const reflect::type_info *> stable = {};
        for (int index = 0; index < STABLE_COUNT; ++index) {
            stable.push_back(local.register_type(make_simple(std::format("test.churn.stable.{}", index))).value());
        }

        std::atomic<bool> stop = false;
        std::atomic<int> failures = 0;
        std::thread reader([&local, &stable, &stop, &failures] {
            while (!stop.load()) {
                for (const reflect::type_info *expected : stable) {
                    if (local.find(expected->type) != expected) {
                        failures.fetch_add(1);
                    }
                }
            }
        });

        for (int round = 0; round < ROUNDS; ++round) {
            for (int index = 0; index < CHURN_COUNT; ++index) {
                const std::string name = std::format("test.churn.{}", index);
                REQUIRE(local.register_type(make_simple(name)).has_value());
            }
            for (int index = 0; index < CHURN_COUNT; ++index) {
                REQUIRE(local.unregister_type(type_id::from_name(std::format("test.churn.{}", index))).has_value());
            }
        }
        stop.store(true);
        reader.join();

        CHECK(failures.load() == 0);
        CHECK(local.type_count() == static_cast<usize>(STABLE_COUNT));
        CHECK(local.find("test.churn.0") == nullptr);
    }
}
