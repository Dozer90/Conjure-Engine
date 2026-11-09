#include "conjure/testing/doctest.h"
#include "assets_test_support.h"

#include "conjure/assets/asset_ref.h"
#include "conjure/assets/asset_slot.h"

#include "conjure/core/json/document.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/reflect/json_conversion.h"
#include "conjure/core/reflect/reflect.h"
#include "conjure/core/uuid.h"

#include <cstring>
#include <string>
#include <type_traits>

using namespace conjure;
using namespace conjure::assets;
using namespace conjure::assets::testing;

namespace {
    /// <summary>
    /// A component-like struct holding a reference, the way audio_source or a renderer component does.
    /// </summary>
    struct ref_holder {
        u32 before = 0;
        asset_ref<test_asset> asset = {};
        f32 after = 0.0f;

        [[nodiscard]] bool operator==(const ref_holder &) const noexcept = default;
    };
}

CONJURE_REFLECT_STRUCT(ref_holder, "test.ref_holder",
    CONJURE_FIELD(before), CONJURE_FIELD(asset), CONJURE_FIELD(after));

TEST_SUITE("assets/asset_ref") {

    TEST_CASE("asset_ref - the uuid leads the handle so ASSET_REF fields serialise it (R25)") {
        const asset_id id = test_id(11);
        asset_slot slot = {};
        const asset_ref<test_asset> ref(id, &slot);
        asset_id leading = {};
        std::memcpy(&leading, &ref, sizeof(asset_id));
        CHECK(leading == id);
        static_assert(sizeof(asset_ref<test_asset>) == sizeof(asset_id) + sizeof(const asset_slot *));
    }

    TEST_CASE("asset_ref - reflects as an ASSET_REF field under one type-erased name") {
        using core::reflect::field_info;
        using core::reflect::field_kind;
        using core::reflect::type_id;
        CHECK(core::reflect::kind_of<asset_ref<test_asset>>() == field_kind::ASSET_REF);
        CHECK(core::reflect::kind_of<asset_ref<other_asset>>() == field_kind::ASSET_REF);
        CHECK(core::reflect::reflect_name_of<asset_ref<test_asset>>() == ASSET_REF_TYPE_NAME);
        CHECK(core::reflect::reflect_name_of<asset_ref<other_asset>>() == "conjure.assets.asset_ref");

        const core::reflect::type_info info = core::reflect::describe_struct<ref_holder>();
        REQUIRE(info.fields.size() == 3);
        const field_info &field = info.fields[1];
        CHECK(field.name == "asset");
        CHECK(field.kind == field_kind::ASSET_REF);
        CHECK(field.offset == offsetof(ref_holder, asset));
        CHECK(field.size == sizeof(asset_ref<test_asset>));
        CHECK(field.type == type_id::from_name(ASSET_REF_TYPE_NAME));
        CHECK(info.isTriviallyCopyable);
    }

    TEST_CASE("asset_ref - JSON writes the uuid string and reads back an unbound reference") {
        asset_slot slot = {};
        ref_holder holder = {};
        holder.before = 7;
        holder.asset = asset_ref<test_asset>(test_id(5), &slot);
        holder.after = 1.5f;

        core::json::writer out = {};
        CONJURE_REQUIRE_OK(core::reflect::to_json(holder, out));
        const std::string text = out.finish();
        const core::result<core::json::document> document = core::json::document::parse(text);
        CONJURE_REQUIRE_OK(document);
        CHECK(document.value().root()["asset"].as_string() == test_id(5).to_string());

        ref_holder restored = {};
        restored.asset = asset_ref<test_asset>(test_id(9), &slot);
        CONJURE_REQUIRE_OK(core::reflect::from_json(restored, document.value().root()));
        CHECK(restored.before == 7);
        CHECK(restored.after == doctest::Approx(1.5));
        CHECK(restored.asset.id() == test_id(5));
        CHECK_FALSE(restored.asset.is_bound());
        CHECK(restored.asset.get() == nullptr);
        CHECK(restored.asset == holder.asset);     // equality is by uuid

        const core::result<core::json::document> bad = core::json::document::parse(R"({"asset": 12})");
        CONJURE_REQUIRE_OK(bad);
        const core::result<void> rejected = core::reflect::from_json(restored, bad.value().root());
        REQUIRE_FALSE(rejected.has_value());
        CHECK(rejected.get_error().message().find("field 'asset'") != std::string::npos);
    }

    TEST_CASE("asset_ref - is 24 bytes and trivially copyable") {
        static_assert(sizeof(asset_ref<test_asset>) == 24);
        static_assert(std::is_trivially_copyable_v<asset_ref<test_asset>>);
        static_assert(std::is_trivially_destructible_v<asset_ref<test_asset>>);
        CHECK(sizeof(asset_ref<other_asset>) == 24);
    }

    TEST_CASE("asset_ref - default is unbound and reads nullptr") {
        const asset_ref<test_asset> ref = {};
        CHECK_FALSE(ref.is_bound());
        CHECK(ref.id().is_null());
        CHECK(ref.get() == nullptr);
        CHECK(ref.state() == asset_state::UNLOADED);
        CHECK(ref.version() == 0);
        CHECK(ref.slot() == nullptr);
    }

    TEST_CASE("asset_ref - an unbound reference keeps the uuid") {
        const asset_id id = test_id(7);
        const asset_ref<test_asset> ref(id);
        CHECK(ref.id() == id);
        CHECK_FALSE(ref.is_bound());
        CHECK(ref.get() == nullptr);
    }

    TEST_CASE("asset_ref - get is nullptr before a publish and reads the payload after") {
        asset_slot slot = {};
        slot.id = test_id(1);
        slot.type.store(asset_type_traits<test_asset>::id());
        const asset_ref<test_asset> ref(slot.id, &slot);
        CHECK(ref.is_bound());
        CHECK(ref.get() == nullptr);

        test_asset payload = {};
        payload.content = "hello";
        slot.object.store(&payload, std::memory_order_release);
        slot.version.fetch_add(1);
        slot.state.store(asset_state::LOADED);

        REQUIRE(ref.get() != nullptr);
        CHECK(ref.get()->content == "hello");
        CHECK(ref.state() == asset_state::LOADED);
        CHECK(ref.version() == 1);
    }

    TEST_CASE("asset_ref - version changes on publish and on unload") {
        asset_slot slot = {};
        const asset_ref<test_asset> ref(test_id(2), &slot);
        test_asset payload = {};
        CHECK(ref.version() == 0);
        slot.object.store(&payload);
        slot.version.fetch_add(1);
        CHECK(ref.version() == 1);
        slot.object.store(nullptr);
        slot.version.fetch_add(1);
        CHECK(ref.version() == 2);
        CHECK(ref.get() == nullptr);
    }

    TEST_CASE("asset_ref - equality is by uuid regardless of binding") {
        asset_slot slot = {};
        const asset_ref<test_asset> bound(test_id(3), &slot);
        const asset_ref<test_asset> unbound(test_id(3));
        const asset_ref<test_asset> other(test_id(4));
        CHECK(bound == unbound);
        CHECK_FALSE(bound == other);
        CHECK(bound != other);
    }

    TEST_CASE("asset_ref - the uuid round-trips through its string form") {
        const asset_ref<test_asset> ref(test_id(5));
        const std::string text = ref.id().to_string();
        const std::optional<core::uuid> parsed = core::uuid::parse(text);
        REQUIRE(parsed.has_value());
        CHECK(asset_ref<test_asset>(*parsed) == ref);
    }

    TEST_CASE("asset_ref - copies are independent views of the same slot") {
        asset_slot slot = {};
        const asset_ref<test_asset> original(test_id(6), &slot);
        asset_ref<test_asset> copy = original;
        CHECK(copy.slot() == &slot);
        copy = asset_ref<test_asset>();
        CHECK(original.slot() == &slot);
        CHECK_FALSE(copy.is_bound());
    }

    TEST_CASE("asset_handle - typed access from a handle agrees with the typed ref") {
        asset_slot slot = {};
        slot.id = test_id(8);
        slot.type.store(asset_type_traits<test_asset>::id());
        test_asset payload = {};
        payload.content = "typed";
        slot.object.store(&payload);
        slot.state.store(asset_state::LOADED);
        {
            const asset_handle handle(&slot);
            const asset_ref<test_asset> ref = handle.ref<test_asset>();
            CHECK(ref.is_bound());
            CHECK(ref.get()->content == "typed");
            CHECK(handle.get<test_asset>() == ref.get());
        }
        CHECK(slot.strongRefs.load() == 0);
    }
}
