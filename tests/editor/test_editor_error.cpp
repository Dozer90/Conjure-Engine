#include "conjure/testing/doctest.h"

#include "conjure/assets/assets_error.h"
#include "conjure/core/error.h"
#include "conjure/ecs/ecs_error.h"
#include "conjure/editor/editor_error.h"
#include "conjure/scene/scene_error.h"

// editor_error.h: the protocol error codes, their wire names and the mapping of engine errors onto them.

using namespace conjure;
using namespace conjure::editor;

TEST_SUITE("editor/editor_error") {

    TEST_CASE("protocol_error_code - every code has a unique wire name that parses back") {
        const std::span<const protocol_error_code> codes = all_error_codes();
        CHECK(codes.size() == static_cast<usize>(protocol_error_code::COUNT) - 1);
        for (const protocol_error_code code : codes) {
            const std::string_view name = to_string(code);
            CHECK_FALSE(name.empty());
            CHECK(parse_error_code(name) == code);
            for (const protocol_error_code other : codes) {
                if (other != code) {
                    CHECK(to_string(other) != name);
                }
            }
        }
        CHECK(to_string(protocol_error_code::INVALID_ARGS) == "INVALID_ARGS");
        CHECK(to_string(protocol_error_code::NONE) == "NONE");
        CHECK_FALSE(parse_error_code("not-a-code").has_value());
    }

    TEST_CASE("make_editor_error - carries the editor category, the code and the message") {
        const core::error error = make_editor_error(protocol_error_code::ENTITY_NOT_FOUND, "no such entity");
        CHECK(error.is(protocol_error_code::ENTITY_NOT_FOUND));
        CHECK(error.is_category<protocol_error_code>());
        CHECK(error.category_name() == "editor");
        CHECK(error.code_name() == "ENTITY_NOT_FOUND");
        CHECK(error.message() == "no such entity");
    }

    TEST_CASE("classify_error - keeps editor codes and maps engine categories") {
        CHECK(classify_error(make_editor_error(protocol_error_code::BUSY)) == protocol_error_code::BUSY);
        CHECK(classify_error(core::error(core::error_code::NOT_FOUND)) == protocol_error_code::PATH_NOT_FOUND);
        CHECK(classify_error(core::error(core::error_code::INVALID_ARGUMENT)) == protocol_error_code::INVALID_ARGS);
        CHECK(classify_error(core::error(core::error_code::UNSUPPORTED)) == protocol_error_code::NOT_AVAILABLE);
        CHECK(classify_error(core::error(core::error_code::TIMEOUT)) == protocol_error_code::TIMEOUT);
        CHECK(classify_error(core::error(core::error_code::CANCELLED)) == protocol_error_code::CANCELLED);
        CHECK(
          classify_error(core::error::from(scene::scene_error::GUID_NOT_FOUND)) == protocol_error_code::ENTITY_NOT_FOUND
        );
        CHECK(
          classify_error(
            core::error::from(ecs::ecs_error::COMPONENT_NOT_PRESENT)
          ) == protocol_error_code::COMPONENT_NOT_FOUND
        );
        CHECK(
          classify_error(core::error::from(ecs::ecs_error::UNKNOWN_COMPONENT)) == protocol_error_code::TYPE_NOT_FOUND
        );
        CHECK(
          classify_error(core::error::from(assets::asset_error_code::NOT_FOUND)) == protocol_error_code::ASSET_NOT_FOUND
        );
    }

    TEST_CASE("classify_error - anything unknown is INTERNAL") {
        CHECK(classify_error(core::error(core::error_code::INVALID_STATE)) == protocol_error_code::INTERNAL);
    }
}
