#include "conjure/testing/doctest.h"

#include "conjure/testing/approx.h"

#include "conjure/bindings/bindings_error.h"
#include "conjure/bindings/marshal.h"
#include "conjure/core/math/math.h"
#include "conjure/core/math/transform.h"
#include "conjure/ecs/entity.h"

#include <array>
#include <cstring>
#include <string>
#include <vector>

// marshal.h (docs/design/15_bindings.md 2.1, B5): the blittable conversions, the x, y, z, w quaternion
// storage (R58), the string and array protocols and the structural guard.

using namespace conjure;
using namespace conjure::bindings;
using namespace conjure::bindings::marshal;

TEST_SUITE("bindings/marshal") {

    TEST_CASE("marshal - entity roundtrips through the packed u64") {
        ecs::entity value = {};
        value.index = 123456;
        value.generation = 7;
        const uint64_t packed = from_entity(value);
        CHECK(packed == ((static_cast<u64>(7) << 32) | 123456u));
        CHECK(to_entity(packed) == value);
    }

    TEST_CASE("marshal - null entity is zero") {
        CHECK(from_entity(ecs::NULL_ENTITY) == 0);
        CHECK(to_entity(0).is_null());
    }

    TEST_CASE("marshal - uuid high and low halves roundtrip") {
        const core::uuid id(0x0123456789ABCDEFull, 0xFEDCBA9876543210ull);
        const conjure_uuid raw = from_uuid(id);
        CHECK(raw.high == 0x0123456789ABCDEFull);
        CHECK(raw.low == 0xFEDCBA9876543210ull);
        CHECK(to_uuid(raw) == id);
        CHECK(to_uuid(conjure_uuid{0, 0}).is_null());
    }

    TEST_CASE("marshal - quat storage is xyzw") {
        CHECK(quat_storage_is_xyzw());
        const math::quat constructed(0.1f, 0.2f, 0.3f, 0.4f);   // (w, x, y, z)
        const conjure_quat_xyzw raw = from_quat(constructed);
        CHECK(raw.x == 0.2f);
        CHECK(raw.y == 0.3f);
        CHECK(raw.z == 0.4f);
        CHECK(raw.w == 0.1f);
        std::array<f32, 4> bytes = {};
        std::memcpy(bytes.data(), &constructed, sizeof(bytes));
        CHECK(bytes[0] == raw.x);
        CHECK(bytes[3] == raw.w);
        const math::quat back = to_quat(raw);
        CHECK(back.w == 0.1f);
        CHECK(back.x == 0.2f);
    }

    TEST_CASE("marshal - transform memcpy equals the by-name conversion") {
        math::transform value = {};
        value.position = math::vec3(1.0f, 2.0f, 3.0f);
        value.rotation = glm::angleAxis(0.5f, glm::normalize(math::vec3(0.0f, 1.0f, 1.0f)));
        value.scale = math::vec3(2.0f, 3.0f, 4.0f);
        const conjure_transform raw = from_transform(value);
        CHECK(raw.position.x == 1.0f);
        CHECK(raw.position.z == 3.0f);
        CHECK(raw.rotation.x == value.rotation.x);
        CHECK(raw.rotation.w == value.rotation.w);
        CHECK(raw.scale.y == 3.0f);
        conjure_transform byName = {};
        byName.position = from_vec3(value.position);
        byName.rotation = from_quat(value.rotation);
        byName.scale = from_vec3(value.scale);
        CHECK(std::memcmp(&raw, &byName, sizeof(raw)) == 0);
        CHECK(to_transform(raw) == value);
    }

    TEST_CASE("marshal - mat4 keeps column-major bytes") {
        math::mat4 matrix = math::mat4(1.0f);
        matrix[3] = math::vec4(5.0f, 6.0f, 7.0f, 1.0f);   // translation in the fourth column
        matrix[1][2] = 9.0f;
        const conjure_mat4 raw = from_mat4(matrix);
        CHECK(raw.m[12] == 5.0f);
        CHECK(raw.m[13] == 6.0f);
        CHECK(raw.m[14] == 7.0f);
        CHECK(raw.m[6] == 9.0f);
        CHECK(to_mat4(raw) == matrix);
    }

    TEST_CASE("marshal - vectors convert component by component") {
        CHECK(to_vec2(conjure_vec2{1.0f, 2.0f}) == math::vec2(1.0f, 2.0f));
        CHECK(from_vec2(math::vec2(3.0f, 4.0f)).y == 4.0f);
        CHECK(to_vec3(conjure_vec3{1.0f, 2.0f, 3.0f}) == math::vec3(1.0f, 2.0f, 3.0f));
        CHECK(from_vec3(math::vec3(1.0f, 2.0f, 3.0f)).z == 3.0f);
        CHECK(to_vec4(conjure_vec4{1.0f, 2.0f, 3.0f, 4.0f}) == math::vec4(1.0f, 2.0f, 3.0f, 4.0f));
        CHECK(from_vec4(math::vec4(1.0f, 2.0f, 3.0f, 4.0f)).w == 4.0f);
        const math::vec4 colour = rgba_to_color(0xFF800040u);
        CONJURE_CHECK_APPROX(colour.x, 1.0f);
        CONJURE_CHECK_APPROX(colour.y, 128.0f / 255.0f);
        CONJURE_CHECK_APPROX(colour.z, 0.0f);
        CONJURE_CHECK_APPROX(colour.w, 64.0f / 255.0f);
    }

    TEST_CASE("marshal - read_utf8 rejects a null pointer with a length") {
        const std::string text = "hello";
        const core::result<std::string_view> read = read_utf8(text.data(), 5);
        CONJURE_REQUIRE_OK(read);
        CHECK(read.value() == "hello");
        CHECK(read_utf8(nullptr, 0).value().empty());
        CHECK(read_utf8(text.data(), 0).value().empty());
        CONJURE_CHECK_ERROR(read_utf8(nullptr, 3), bindings_error::INVALID_ARGUMENT);
        CONJURE_CHECK_ERROR(read_utf8(text.data(), -1), bindings_error::INVALID_ARGUMENT);
    }

    TEST_CASE("marshal - write_utf8 returns the needed length and truncates") {
        std::array<char, 8> buffer = {};
        buffer.fill('#');
        CHECK(write_utf8("hello world", buffer.data(), 8) == 11);
        CHECK(std::string_view(buffer.data(), 8) == "hello wo");          // no terminator written
        CHECK(write_utf8("hello world", nullptr, 0) == 11);                 // length query
        CHECK(write_utf8("hello world", buffer.data(), 0) == 11);
        CHECK(write_utf8("hello world", buffer.data(), -4) == 11);          // negative capacity writes nothing
        CHECK(std::string_view(buffer.data(), 8) == "hello wo");
        buffer.fill('#');
        CHECK(write_utf8("hi", buffer.data(), 8) == 2);
        CHECK(std::string_view(buffer.data(), 3) == "hi#");
        CHECK(write_utf8("", buffer.data(), 8) == 0);
    }

    TEST_CASE("marshal - write_entities follows the count protocol") {
        std::vector<ecs::entity> entities = {};
        for (u32 index = 1; index <= 5; ++index) {
            ecs::entity value = {};
            value.index = index;
            value.generation = 1;
            entities.push_back(value);
        }
        std::array<uint64_t, 3> out = {};
        CHECK(write_entities(entities, out.data(), 3) == 5);                // full count, three written
        CHECK(to_entity(out[0]) == entities[0]);
        CHECK(to_entity(out[2]) == entities[2]);
        CHECK(write_entities(entities, nullptr, 0) == 5);                   // count query
        CHECK(write_entities(entities, out.data(), 0) == 5);
        std::array<uint64_t, 8> wide = {};
        CHECK(write_entities(entities, wide.data(), 8) == 5);
        CHECK(wide[5] == 0);
        CHECK(write_entities({}, wide.data(), 8) == 0);
    }

    TEST_CASE("marshal - structural_guard follows the world lock") {
        structural_guard guard = {};
        CHECK_FALSE(guard.deferred());
        CHECK_FALSE(guard.can_defer());
        ecs::component_registry registry = {};
        ecs::world world(registry);
        guard.world = &world;
        CHECK_FALSE(guard.deferred());
        {
            const ecs::iteration_scope lock = world.lock_iteration();
            CHECK(guard.deferred());
            CHECK_FALSE(guard.can_defer());     // no runtime: a locked request has nowhere to go
        }
        CHECK_FALSE(guard.deferred());
    }
}
