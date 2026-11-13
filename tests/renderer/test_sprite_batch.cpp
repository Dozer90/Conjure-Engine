#include "conjure/testing/doctest.h"

#include "conjure/testing/approx.h"
#include "conjure/testing/log_capture.h"

#include "conjure/renderer/gpu_ids.h"
#include "conjure/renderer/limits.h"
#include "conjure/renderer/sprite_batch.h"

#include "conjure/core/log.h"
#include "conjure/core/math.h"
#include "conjure/core/types.h"

#include <span>
#include <string_view>
#include <vector>

// sprite_batch.h (docs/design/02_renderer.md 3.9): sorting by (space, layer, order, texture), four
// vertices and six indices per sprite, runs per texture and the per-frame cap.

using namespace conjure;
using namespace conjure::renderer;
using namespace conjure::testing;

namespace {

    [[nodiscard]] gpu_texture_id texture(u32 index) {
        gpu_texture_id id = {};
        id.index = index;
        id.generation = 1;
        return id;
    }

    [[nodiscard]] sprite make_sprite(sprite_space space, i16 layer, i16 order, u32 textureIndex) {
        sprite value = {};
        value.space = space;
        value.layer = layer;
        value.order = order;
        value.texture = texture(textureIndex);
        return value;
    }
}

TEST_SUITE("renderer/sprite_batch") {

    TEST_CASE("sprite - the default is a unit world sprite with the whole texture and white colour") {
        const sprite value = {};
        CHECK_FALSE(value.texture.is_valid());
        CHECK(value.space == sprite_space::WORLD);
        CONJURE_CHECK_APPROX(value.uvRect, math::vec4(0.0f, 0.0f, 1.0f, 1.0f));
        CONJURE_CHECK_APPROX(value.colour, math::vec4(1.0f));
        CONJURE_CHECK_APPROX(value.size, math::vec2(1.0f));
        CHECK(value.layer == 0);
        CHECK(value.order == 0);
        CHECK(value == sprite());
        static_assert(sizeof(sprite_vertex) == 32, "the GPU vertex is 32 bytes");
        CHECK(to_string(sprite_space::WORLD) == "WORLD");
        CHECK(to_string(sprite_space::SCREEN) == "SCREEN");
    }

    TEST_CASE("sort_key - orders by space, then layer, order and texture, each ascending") {
        const u64 world = sprite_batcher::sort_key(make_sprite(sprite_space::WORLD, 0, 0, 0));
        const u64 screen = sprite_batcher::sort_key(make_sprite(sprite_space::SCREEN, -100, -100, 0));
        CHECK(world < screen);
        const u64 layerLow = sprite_batcher::sort_key(make_sprite(sprite_space::WORLD, -5, 100, 9));
        const u64 layerHigh = sprite_batcher::sort_key(make_sprite(sprite_space::WORLD, 3, -100, 0));
        CHECK(layerLow < layerHigh);
        const u64 orderLow = sprite_batcher::sort_key(make_sprite(sprite_space::WORLD, 3, -1, 9));
        const u64 orderHigh = sprite_batcher::sort_key(make_sprite(sprite_space::WORLD, 3, 1, 0));
        CHECK(orderLow < orderHigh);
        const u64 textureLow = sprite_batcher::sort_key(make_sprite(sprite_space::WORLD, 3, 1, 1));
        const u64 textureHigh = sprite_batcher::sort_key(make_sprite(sprite_space::WORLD, 3, 1, 2));
        CHECK(textureLow < textureHigh);
        CHECK(sprite_batcher::sort_key(make_sprite(sprite_space::WORLD, 3, 1, 2)) == textureHigh);
    }

    TEST_CASE("sprite_batcher - emits four vertices and six indices per sprite in sorted order") {
        std::vector<sprite> sprites = {};
        sprites.push_back(make_sprite(sprite_space::SCREEN, 0, 0, 1));
        sprites.push_back(make_sprite(sprite_space::WORLD, 1, 0, 2));
        sprites.push_back(make_sprite(sprite_space::WORLD, 0, 5, 3));
        sprites.push_back(make_sprite(sprite_space::WORLD, 0, 2, 3));
        sprites[1].worldFromLocal = glm::translate(math::mat4(1.0f), math::vec3(10.0f, 0.0f, 0.0f));
        sprites[1].size = math::vec2(2.0f, 4.0f);
        sprites[1].colour = math::vec4(1.0f, 0.0f, 0.0f, 1.0f);
        sprites[1].uvRect = math::vec4(0.25f, 0.5f, 0.75f, 1.0f);

        sprite_batcher batcher = {};
        batcher.build(sprites);
        CHECK(batcher.sprite_count() == 4);
        CHECK(batcher.vertices().size() == 16);
        CHECK(batcher.indices().size() == 24);
        CHECK(batcher.dropped_count() == 0);

        // Runs: world texture 3 (two sprites), world texture 2, screen texture 1.
        const std::span<const sprite_run> runs = batcher.runs();
        REQUIRE(runs.size() == 3);
        CHECK(runs[0].space == sprite_space::WORLD);
        CHECK(runs[0].texture == texture(3));
        CHECK(runs[0].firstIndex == 0);
        CHECK(runs[0].indexCount == 12);
        CHECK(runs[1].space == sprite_space::WORLD);
        CHECK(runs[1].texture == texture(2));
        CHECK(runs[1].firstIndex == 12);
        CHECK(runs[1].indexCount == 6);
        CHECK(runs[2].space == sprite_space::SCREEN);
        CHECK(runs[2].texture == texture(1));
        CHECK(runs[2].firstIndex == 18);
        CHECK(runs[2].indexCount == 6);

        // Every index references a vertex of its own quad.
        const std::span<const u32> indices = batcher.indices();
        for (usize quad = 0; quad < 4; ++quad) {
            for (usize corner = 0; corner < 6; ++corner) {
                const u32 index = indices[quad * 6 + corner];
                CHECK(index >= quad * 4);
                CHECK(index < quad * 4 + 4);
            }
        }

        // The translated world sprite (third in sort order) spans its size around its origin.
        const std::span<const sprite_vertex> vertices = batcher.vertices();
        math::vec3 low(1.0e9f);
        math::vec3 high(-1.0e9f);
        for (usize corner = 8; corner < 12; ++corner) {
            low = glm::min(low, vertices[corner].position);
            high = glm::max(high, vertices[corner].position);
            CHECK(vertices[corner].colour == 0xFF0000FFu);
            CHECK(vertices[corner].uv.x >= 0.25f);
            CHECK(vertices[corner].uv.x <= 0.75f);
            CHECK(vertices[corner].uv.y >= 0.5f);
            CHECK(vertices[corner].uv.y <= 1.0f);
        }
        CONJURE_CHECK_APPROX(low, math::vec3(9.0f, -2.0f, 0.0f));
        CONJURE_CHECK_APPROX(high, math::vec3(11.0f, 2.0f, 0.0f));
    }

    TEST_CASE("sprite_batcher - an empty build emits nothing and clear keeps capacity") {
        sprite_batcher batcher = {};
        batcher.build({});
        CHECK(batcher.sprite_count() == 0);
        CHECK(batcher.runs().empty());
        std::vector<sprite> sprites(3, make_sprite(sprite_space::WORLD, 0, 0, 1));
        batcher.build(sprites);
        CHECK(batcher.sprite_count() == 3);
        CHECK(batcher.runs().size() == 1);
        batcher.clear();
        CHECK(batcher.sprite_count() == 0);
        CHECK(batcher.indices().empty());
        CHECK(batcher.runs().empty());
        batcher.build(sprites);
        CHECK(batcher.sprite_count() == 3);
    }

    TEST_CASE("sprite_batcher - sprites beyond MAX_SPRITES_PER_FRAME drop world sprites before screen sprites") {
        log_capture logs = {};
        std::vector<sprite> sprites(MAX_SPRITES_PER_FRAME + 2, make_sprite(sprite_space::WORLD, 0, 0, 1));
        // The screen sprites (the HUD) have the highest sort keys but keep their slots: the two world
        // sprites last in sort order (layer 5, the front-most) are the ones dropped.
        sprites[0] = make_sprite(sprite_space::SCREEN, 0, 0, 1);
        sprites[1] = make_sprite(sprite_space::SCREEN, 0, 0, 2);
        sprites[2] = make_sprite(sprite_space::WORLD, 5, 0, 3);
        sprites[3] = make_sprite(sprite_space::WORLD, 5, 0, 3);
        sprite_batcher batcher = {};
        batcher.build(sprites);
        CHECK(batcher.sprite_count() == MAX_SPRITES_PER_FRAME);
        CHECK(batcher.dropped_count() == 2);
        const std::span<const sprite_run> runs = batcher.runs();
        REQUIRE(runs.size() == 3);
        CHECK(runs[0].space == sprite_space::WORLD);
        CHECK(runs[0].texture == texture(1));
        CHECK(runs[0].indexCount == (MAX_SPRITES_PER_FRAME - 2) * 6);
        CHECK(runs[1].space == sprite_space::SCREEN);
        CHECK(runs[1].texture == texture(1));
        CHECK(runs[1].firstIndex == (MAX_SPRITES_PER_FRAME - 2) * 6);
        CHECK(runs[2].space == sprite_space::SCREEN);
        CHECK(runs[2].texture == texture(2));
        CHECK(runs[2].indexCount == 6);
        CHECK(logs.count(core::log_level::WARN) == 1);
        CHECK(logs.contains(core::log_level::WARN, "exceed MAX_SPRITES_PER_FRAME"));
        CHECK(logs.contains(core::log_level::WARN, "2 WORLD and 0 SCREEN sprites"));
        // The same overflow again does not warn again; a different one does.
        batcher.build(sprites);
        CHECK(logs.count(core::log_level::WARN) == 1);
        sprites.push_back(make_sprite(sprite_space::SCREEN, 0, 0, 3));
        batcher.build(sprites);
        CHECK(batcher.dropped_count() == 3);
        CHECK(batcher.runs().size() == 4);
        CHECK(batcher.runs().back().texture == texture(3));
        CHECK(logs.count(core::log_level::WARN) == 2);
        CHECK(logs.contains(core::log_level::WARN, "3 WORLD and 0 SCREEN sprites"));
    }

    TEST_CASE("sprite_batcher - more screen sprites than MAX_SPRITES_PER_FRAME drop every world sprite") {
        // Screen sprites alone fill the cap: they keep sort order among themselves, the last ones drop.
        log_capture logs = {};
        std::vector<sprite> sprites(MAX_SPRITES_PER_FRAME + 1, make_sprite(sprite_space::SCREEN, 0, 0, 1));
        sprites[0] = make_sprite(sprite_space::SCREEN, 1, 0, 2);
        sprites.push_back(make_sprite(sprite_space::WORLD, 0, 0, 3));
        sprites.push_back(make_sprite(sprite_space::WORLD, 0, 0, 3));
        sprite_batcher batcher = {};
        batcher.build(sprites);
        CHECK(batcher.sprite_count() == MAX_SPRITES_PER_FRAME);
        CHECK(batcher.dropped_count() == 3);
        REQUIRE(batcher.runs().size() == 1);
        CHECK(batcher.runs()[0].space == sprite_space::SCREEN);
        CHECK(batcher.runs()[0].texture == texture(1));
        CHECK(logs.contains(core::log_level::WARN, "2 WORLD and 1 SCREEN sprites"));
    }
}
