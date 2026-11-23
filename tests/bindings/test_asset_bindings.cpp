#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "bindings_test_support.h"

#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_slot.h"
#include "conjure/assets/asset_types.h"
#include "conjure/assets/binary_asset.h"
#include "conjure/bindings/bindings_error.h"
#include "conjure/bindings/bindings_subsystem.h"
#include "conjure/bindings/marshal.h"
#include "conjure/scene/scene_serializer.h"

#include <array>
#include <string>

// asset_* (docs/design/15_bindings.md 10, 3.7, B9): identity and state without loading, residency held by the
// subsystem's script_residency, type checks, path lookups and the type id of registered names.

using namespace conjure;
using namespace conjure::bindings;
using namespace conjure::testing;
using namespace bindings_test;

namespace {

    constexpr std::string_view BLOB_PATH = "assets://data/blob.bytes";

    struct asset_fixture {
        bare_fixture base = {};
        assets::asset_id blob = test_uuid(5);
        assets::asset_type_id binaryType = assets::asset_type_traits<assets::binary_asset>::id();

        asset_fixture() {
            CONJURE_REQUIRE_OK(assets::register_binary_asset_type(base.assets.types()));
            const std::string content = "payload";
            base.add_blob(
              blob, binaryType, std::as_bytes(std::span<const char>(content.data(), content.size())), BLOB_PATH
            );
        }
    };
}

TEST_SUITE("bindings/asset_bindings") {

    TEST_CASE("asset - ref_get reports state, version and type without loading") {
        asset_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const conjure_uuid id = uuid_of(fixture.blob);
        conjure_asset_info info = {};
        CHECK(api.asset_ref_get(&id, 0, &info) == 0);
        CHECK(info.state == static_cast<uint8_t>(assets::asset_state::UNLOADED));
        CHECK(info.version == 0);
        CHECK(info.type_id == static_cast<uint64_t>(fixture.binaryType));
        CHECK(fixture.base.assets.state_of(fixture.blob) == assets::asset_state::UNLOADED);
        CHECK(fixture.base.assets.stats().pending == 0);
        CHECK(api.asset_ref_get(&id, static_cast<uint64_t>(fixture.binaryType), &info) == 0);
        CHECK(api.asset_ref_get(&id, 12345, &info) == to_status(bindings_error::ASSET_TYPE_MISMATCH));
        CHECK(api.asset_ref_get(nullptr, 0, &info) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.asset_ref_get(&id, 0, nullptr) == to_status(bindings_error::INVALID_ARGUMENT));
        const conjure_uuid null = {};
        CHECK(api.asset_ref_get(&null, 0, &info) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.asset_state_of(&null) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.asset_state_of(&id) == static_cast<int32_t>(assets::asset_state::UNLOADED));
        CHECK(api.asset_state_of(nullptr) == to_status(bindings_error::INVALID_ARGUMENT));
    }

    TEST_CASE("regression - a refused asset_ref_bind creates no slot") {
        // Slots are never freed (05): the type check runs before bind_slot, so a refused bind leaves nothing.
        asset_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const conjure_uuid id = uuid_of(fixture.blob);
        const uint64_t prefabType = static_cast<uint64_t>(assets::make_asset_type_id("conjure.scene.prefab"));
        const u32 slotsBefore = fixture.base.assets.stats().slots;
        const void *slot = nullptr;
        CHECK(api.asset_ref_bind(&id, prefabType, &slot) == to_status(bindings_error::ASSET_TYPE_MISMATCH));
        CHECK(slot == nullptr);
        CHECK(fixture.base.assets.find_slot(fixture.blob) == nullptr);
        CHECK(fixture.base.assets.stats().slots == slotsBefore);
        // The matching type still binds.
        CHECK(api.asset_ref_bind(&id, static_cast<uint64_t>(fixture.binaryType), &slot) == 0);
        CHECK(slot != nullptr);
        CHECK(fixture.base.assets.stats().slots == slotsBefore + 1);
    }

    TEST_CASE("regression - asset_load without a residency neither binds nor loads") {
        asset_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const conjure_uuid id = uuid_of(fixture.blob);
        const u32 slotsBefore = fixture.base.assets.stats().slots;
        scripting_bindings_subsystem::set_active_residency(nullptr);
        CHECK(api.asset_load(&id, 0, 1) == to_status(bindings_error::NOT_SUPPORTED));
        CHECK(api.asset_load_blocking(&id, 0) == to_status(bindings_error::NOT_SUPPORTED));
        scripting_bindings_subsystem::set_active_residency(&fixture.base.residency);
        CHECK(fixture.base.assets.find_slot(fixture.blob) == nullptr);
        CHECK(fixture.base.assets.stats().slots == slotsBefore);
        CHECK(fixture.base.assets.stats().pending == 0);
        CHECK(fixture.base.assets.state_of(fixture.blob) == assets::asset_state::UNLOADED);
    }

    TEST_CASE("regression - the read-only asset entries never create a slot") {
        // Slots are never freed (05): a uuid a script merely polls must not cost one. Only asset_ref_bind
        // and the loads bind; the reads answer from the slot table and the sources.
        asset_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const conjure_uuid known = uuid_of(fixture.blob);
        const conjure_uuid unknown = uuid_of(test_uuid(4242));
        const u32 slotsBefore = fixture.base.assets.stats().slots;
        conjure_asset_info info = {};
        CHECK(api.asset_ref_get(&known, 0, &info) == 0);
        CHECK(info.state == static_cast<uint8_t>(assets::asset_state::UNLOADED));
        CHECK(info.type_id == static_cast<uint64_t>(fixture.binaryType));        // the sources know the type
        CHECK(fixture.base.assets.find_slot(fixture.blob) == nullptr);
        CHECK(api.asset_ref_get(&unknown, 0, &info) == to_status(bindings_error::NOT_FOUND));
        uint64_t typeId = 0;
        CHECK(api.asset_type_of(&known, &typeId) == 0);
        CHECK(typeId == static_cast<uint64_t>(fixture.binaryType));
        CHECK(api.asset_type_of(&unknown, &typeId) == to_status(bindings_error::NOT_FOUND));
        CHECK(api.asset_state_of(&unknown) == static_cast<int32_t>(assets::asset_state::UNLOADED));
        CHECK(api.asset_load(&unknown, 0, 1) == to_status(bindings_error::NOT_FOUND));
        CHECK(api.asset_load_blocking(&unknown, 0) == to_status(bindings_error::NOT_FOUND));
        CHECK(api.asset_release(&unknown) == 0);
        for (u32 repeat = 0; repeat < 16; ++repeat) {
            const conjure_uuid garbage = uuid_of(test_uuid(5000 + repeat));
            CHECK(api.asset_ref_get(&garbage, 0, &info) == to_status(bindings_error::NOT_FOUND));
        }
        CHECK(fixture.base.assets.stats().slots == slotsBefore);
        // asset_ref_bind is the entry that binds: one slot, then the reads see it.
        const void *slot = nullptr;
        CHECK(api.asset_ref_bind(&known, 0, &slot) == 0);
        CHECK(slot != nullptr);
        CHECK(fixture.base.assets.stats().slots == slotsBefore + 1);
        CHECK(fixture.base.assets.find_slot(fixture.blob) == slot);
        CHECK(api.asset_ref_get(&known, 0, &info) == 0);
        CHECK(info.type_id == static_cast<uint64_t>(fixture.binaryType));
        CHECK(api.asset_ref_bind(&unknown, 0, &slot) == 0);                    // authored ids may bind early
        CHECK(fixture.base.assets.stats().slots == slotsBefore + 2);
    }

    TEST_CASE("asset - load holds a handle and release drops it") {
        asset_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const conjure_uuid id = uuid_of(fixture.blob);
        CHECK(api.asset_load(&id, 0, static_cast<uint8_t>(conjure_asset_load_priority::NORMAL)) == 0);
        CHECK(fixture.base.residency.holds(fixture.blob));
        CHECK(fixture.base.residency.size() == 1);
        fixture.base.assets.flush();
        CHECK(api.asset_state_of(&id) == static_cast<int32_t>(assets::asset_state::LOADED));
        conjure_asset_info info = {};
        REQUIRE(api.asset_ref_get(&id, 0, &info) == 0);
        CHECK(info.version == 1);
        CHECK(fixture.base.assets.collect() == 0);                   // held by the residency
        CHECK(api.asset_release(&id) == 1);
        CHECK(api.asset_release(&id) == 0);
        CHECK(fixture.base.residency.size() == 0);
        CHECK(fixture.base.assets.collect() == 1);                   // nothing holds it any more
        CHECK(api.asset_state_of(&id) == static_cast<int32_t>(assets::asset_state::UNLOADED));
        CHECK(api.asset_release(nullptr) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.asset_load(&id, 0, 7) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.asset_load(nullptr, 0, 1) == to_status(bindings_error::INVALID_ARGUMENT));
        const conjure_uuid unknown = uuid_of(test_uuid(99));
        CHECK(api.asset_load(&unknown, 0, 1) == to_status(bindings_error::NOT_FOUND));
    }

    TEST_CASE("asset - loading twice is a no-op") {
        asset_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const conjure_uuid id = uuid_of(fixture.blob);
        CHECK(api.asset_load(&id, 0, 1) == 0);
        CHECK(api.asset_load(&id, 0, 2) == 0);
        CHECK(api.asset_load(&id, 0, 0) == 0);
        CHECK(fixture.base.residency.size() == 1);
        fixture.base.assets.flush();
        CHECK(api.asset_release(&id) == 1);
        CHECK(fixture.base.residency.size() == 0);
    }

    TEST_CASE("asset - a type mismatch is rejected before loading") {
        asset_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const conjure_uuid id = uuid_of(fixture.blob);
        const uint64_t prefabType = static_cast<uint64_t>(assets::make_asset_type_id("conjure.scene.prefab"));
        CHECK(api.asset_load(&id, prefabType, 1) == to_status(bindings_error::ASSET_TYPE_MISMATCH));
        CHECK(api.asset_load_blocking(&id, prefabType) == to_status(bindings_error::ASSET_TYPE_MISMATCH));
        CHECK(fixture.base.residency.size() == 0);
        CHECK(fixture.base.assets.state_of(fixture.blob) == assets::asset_state::UNLOADED);
        const void *slot = nullptr;
        CHECK(api.asset_ref_bind(&id, prefabType, &slot) == to_status(bindings_error::ASSET_TYPE_MISMATCH));
        CHECK(slot == nullptr);
    }

    TEST_CASE("asset - find and path_of") {
        asset_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const utf8_argument path = utf8(BLOB_PATH);
        conjure_uuid found = {};
        CHECK(api.asset_find(path.data, path.length, &found) == 0);
        CHECK(marshal::to_uuid(found) == fixture.blob);
        const utf8_argument missing = utf8("assets://data/missing.bytes");
        CHECK(api.asset_find(missing.data, missing.length, &found) == to_status(bindings_error::NOT_FOUND));
        CHECK(api.asset_find(nullptr, 4, &found) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.asset_find(path.data, path.length, nullptr) == to_status(bindings_error::INVALID_ARGUMENT));
        // No asset database in a bare world (a cooked project): the path table is NOT_SUPPORTED.
        std::array<char, 64> buffer = {};
        CHECK(api.asset_path_of(&found, buffer.data(), 64) == to_status(bindings_error::NOT_SUPPORTED));
        CHECK(api.asset_path_of(nullptr, buffer.data(), 64) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.asset_path_of(&found, buffer.data(), -1) == to_status(bindings_error::INVALID_ARGUMENT));
    }

    TEST_CASE("asset - load_blocking loads synchronously and holds the handle") {
        log_capture logs = {};
        asset_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const conjure_uuid id = uuid_of(fixture.blob);
        CHECK(api.asset_load_blocking(&id, static_cast<uint64_t>(fixture.binaryType)) == 0);
        CHECK(api.asset_state_of(&id) == static_cast<int32_t>(assets::asset_state::LOADED));
        CHECK(fixture.base.residency.holds(fixture.blob));
        CHECK(api.asset_load_blocking(&id, 0) == 0);                    // already resident: still held once
        CHECK(fixture.base.residency.size() == 1);
        const conjure_uuid unknown = uuid_of(test_uuid(98));
        CHECK(api.asset_load_blocking(&unknown, 0) == to_status(bindings_error::NOT_FOUND));
        CHECK(api.asset_load_blocking(nullptr, 0) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("asset - ref_bind returns a stable slot") {
        asset_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const conjure_uuid id = uuid_of(fixture.blob);
        const void *first = nullptr;
        const void *second = nullptr;
        CHECK(api.asset_ref_bind(&id, 0, &first) == 0);
        CHECK(api.asset_ref_bind(&id, static_cast<uint64_t>(fixture.binaryType), &second) == 0);
        REQUIRE(first != nullptr);
        CHECK(first == second);
        CHECK(first == fixture.base.assets.bind_slot(fixture.blob));
        const assets::asset_slot *slot = static_cast<const assets::asset_slot *>(first);
        CHECK(slot->id == fixture.blob);
        CHECK(api.asset_ref_bind(&id, 0, nullptr) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.asset_ref_bind(nullptr, 0, &first) == to_status(bindings_error::INVALID_ARGUMENT));
    }

    TEST_CASE("asset - type_id matches make_asset_type_id for registered types") {
        asset_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const utf8_argument binary = utf8("conjure.assets.binary");
        const uint64_t typeId = api.asset_type_id(binary.data, binary.length);
        CHECK(typeId == static_cast<uint64_t>(assets::make_asset_type_id("conjure.assets.binary")));
        CHECK(typeId == static_cast<uint64_t>(fixture.binaryType));
        const utf8_argument unknown = utf8("conjure.nothing.here");
        CHECK(api.asset_type_id(unknown.data, unknown.length) == 0);
        CHECK(last_error_code() == bindings_error::NOT_FOUND);
        CHECK(api.asset_type_id(nullptr, 3) == 0);
        CHECK(last_error_code() == bindings_error::INVALID_ARGUMENT);
        uint64_t outType = 0;
        const conjure_uuid id = uuid_of(fixture.blob);
        CHECK(api.asset_type_of(&id, &outType) == 0);
        CHECK(outType == typeId);
        const conjure_uuid stranger = uuid_of(test_uuid(97));
        CHECK(api.asset_type_of(&stranger, &outType) == to_status(bindings_error::NOT_FOUND));
        CHECK(api.asset_type_of(&id, nullptr) == to_status(bindings_error::INVALID_ARGUMENT));
    }

    TEST_CASE("asset - residency releases everything on clear") {
        asset_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const conjure_uuid id = uuid_of(fixture.blob);
        REQUIRE(api.asset_load(&id, 0, 1) == 0);
        fixture.base.assets.flush();
        CHECK(fixture.base.residency.size() == 1);
        fixture.base.residency.clear();
        CHECK(fixture.base.residency.size() == 0);
        CHECK_FALSE(fixture.base.residency.holds(fixture.blob));
        CHECK(fixture.base.assets.collect() == 1);
        // Without an installed residency the loading entries are NOT_SUPPORTED.
        scripting_bindings_subsystem::set_active_residency(nullptr);
        CHECK(api.asset_load(&id, 0, 1) == to_status(bindings_error::NOT_SUPPORTED));
        CHECK(api.asset_release(&id) == to_status(bindings_error::NOT_SUPPORTED));
        scripting_bindings_subsystem::set_active_residency(&fixture.base.residency);
    }

    TEST_CASE("script_residency - hold, holds, release and size") {
        asset_fixture fixture = {};
        script_residency residency = {};
        CHECK(residency.size() == 0);
        CHECK_FALSE(residency.hold(fixture.blob, assets::asset_handle{}));         // unbound handle
        CHECK_FALSE(residency.hold(core::uuid{}, fixture.base.assets.make_handle(fixture.blob)));
        CHECK(residency.hold(fixture.blob, fixture.base.assets.make_handle(fixture.blob)));
        CHECK(residency.hold(fixture.blob, fixture.base.assets.make_handle(fixture.blob)));
        CHECK(residency.size() == 1);
        CHECK(residency.holds(fixture.blob));
        CHECK_FALSE(residency.holds(test_uuid(1)));
        CHECK(residency.release(fixture.blob));
        CHECK_FALSE(residency.release(fixture.blob));
        CHECK(residency.size() == 0);
    }
}
