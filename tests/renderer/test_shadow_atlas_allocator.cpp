#include "conjure/testing/doctest.h"

#include "conjure/testing/approx.h"

#include "renderer/shadows/shadow_atlas_allocator.h"

#include "conjure/core/math.h"
#include "conjure/core/types.h"

#include <set>
#include <span>
#include <vector>

// The quadtree shadow atlas (docs/design/02_renderer.md R9, 3.4, 10 "test_shadow_atlas_allocator"):
// tiles of 1024 / 512 / 256 in a 4096 atlas, importance order, stable tiles for unchanged requests,
// halving before dropping, six tiles per point light.

using namespace conjure;
using namespace conjure::renderer;
using namespace conjure::testing;

namespace {

    [[nodiscard]] atlas_request request(u64 key, u16 size, u8 tiles = 1) {
        atlas_request value = {};
        value.lightKey = key;
        value.size = size;
        value.tileCount = tiles;
        return value;
    }

    /// <summary>
    /// True when no two tiles of the last allocation overlap and all lie inside the atlas.
    /// </summary>
    [[nodiscard]] bool tiles_disjoint(const shadow_atlas_allocator &allocator) {
        const std::span<const atlas_tile> tiles = allocator.tiles();
        for (usize a = 0; a < tiles.size(); ++a) {
            const bool insideX = tiles[a].x + tiles[a].size <= allocator.atlas_size();
            const bool insideY = tiles[a].y + tiles[a].size <= allocator.atlas_size();
            if (!insideX || !insideY) {
                return false;
            }
            for (usize b = a + 1; b < tiles.size(); ++b) {
                const bool separated = tiles[a].x + tiles[a].size <= tiles[b].x
                  || tiles[b].x + tiles[b].size <= tiles[a].x ||
                  tiles[a].y + tiles[a].size <= tiles[b].y || tiles[b].y + tiles[b].size <= tiles[a].y;
                if (!separated) {
                    return false;
                }
            }
        }
        return true;
    }
}

TEST_SUITE("renderer/shadow_atlas_allocator") {

    TEST_CASE("atlas_light_key - follows the light's identity, never its index, and entity ids win") {
        light a = {};
        a.type = light_type::SPOT;
        a.position = math::vec3(1.0f, 2.0f, 3.0f);
        a.castsShadows = true;
        const u64 keyA = atlas_light_key(a);
        CHECK(keyA != 0);
        CHECK(atlas_light_key(a) == keyA);                 // deterministic
        light jitter = a;
        jitter.position.x += 0.001f;                        // below the 1 cm quantum
        jitter.intensity *= 2.0f;                           // not part of the identity
        jitter.colour = math::vec3(0.5f);
        CHECK(atlas_light_key(jitter) == keyA);
        light moved = a;
        moved.position.x += 0.05f;
        CHECK(atlas_light_key(moved) != keyA);
        light turned = a;
        turned.direction = glm::normalize(math::vec3(1.0f, -1.0f, 0.0f));
        CHECK(atlas_light_key(turned) != keyA);
        light point = a;
        point.type = light_type::POINT;
        CHECK(atlas_light_key(point) != keyA);
        light sharper = a;
        sharper.shadowResolution = 1024;
        CHECK(atlas_light_key(sharper) != keyA);
        light rounded = a;
        rounded.shadowResolution = 500;                     // rounds to the same 512 tile
        CHECK(atlas_light_key(rounded) == keyA);
        // An entity id is the identity even when every attribute changes.
        light tagged = a;
        tagged.entityId = 42;
        light taggedElsewhere = moved;
        taggedElsewhere.entityId = 42;
        CHECK(atlas_light_key(tagged) == atlas_light_key(taggedElsewhere));
        CHECK(atlas_light_key(tagged) != keyA);
        light other = a;
        other.entityId = 43;
        CHECK(atlas_light_key(other) != atlas_light_key(tagged));
        // Identical lights without ids are told apart by their occurrence; the first keeps the key.
        CHECK(atlas_light_key_occurrence(keyA, 0) == keyA);
        CHECK(atlas_light_key_occurrence(keyA, 1) != keyA);
        CHECK(atlas_light_key_occurrence(keyA, 1) != atlas_light_key_occurrence(keyA, 2));
        CHECK(atlas_light_key_occurrence(keyA, 1) != 0);
    }

    TEST_CASE("atlas_tile - uv_rect maps the tile into [0, 1] atlas coordinates") {
        atlas_tile tile = {};
        tile.x = 1024;
        tile.y = 2048;
        tile.size = 512;
        CONJURE_CHECK_APPROX(tile.uv_rect(4096), math::vec4(0.25f, 0.5f, 0.375f, 0.625f));
        CONJURE_CHECK_APPROX(atlas_tile().uv_rect(0), math::vec4(0.0f));
        CHECK(tile == tile);
        CHECK_FALSE(tile == atlas_tile());
    }

    TEST_CASE("shadow_atlas_allocator - the atlas holds sixteen 1024 tiles, then halves, then drops") {
        shadow_atlas_allocator allocator(4096);
        CHECK(allocator.atlas_size() == 4096);
        CHECK(allocator.min_tile_size() == 256);
        CHECK(allocator.max_tile_size() == 1024);
        std::vector<atlas_request> requests = {};
        for (u64 key = 1; key <= 16; ++key) {
            requests.push_back(request(key, 1024));
        }
        allocator.allocate(requests);
        REQUIRE(allocator.allocations().size() == 16);
        CHECK(allocator.tiles().size() == 16);
        CHECK(allocator.dropped_count() == 0);
        for (const atlas_allocation &allocation : allocator.allocations()) {
            CHECK(allocation.tileCount == 1);
            CHECK(allocation.grantedSize == 1024);
            CHECK(allocator.tiles()[allocation.firstTile].size == 1024);
        }
        CHECK(tiles_disjoint(allocator));

        // A seventeenth light is halved down to 256 until nothing fits, then dropped.
        requests.push_back(request(17, 1024));
        allocator.allocate(requests);
        CHECK(allocator.allocations()[16].tileCount == 0);
        CHECK(allocator.allocations()[16].grantedSize == 0);
        CHECK(allocator.dropped_count() == 1);
        CHECK(allocator.tiles().size() == 16);
        // With the first light asking for less, the seventeenth gets the space freed (halved).
        requests[0] = request(1, 512);
        allocator.allocate(requests);
        CHECK(allocator.allocations()[0].grantedSize == 512);
        CHECK(allocator.allocations()[16].tileCount == 1);
        CHECK(allocator.allocations()[16].grantedSize == 512);
        CHECK(allocator.dropped_count() == 0);
        CHECK(tiles_disjoint(allocator));
    }

    TEST_CASE("shadow_atlas_allocator - an unchanged request keeps its tile across frames") {
        shadow_atlas_allocator allocator(4096);
        std::vector<atlas_request> requests = {request(10, 1024), request(20, 512), request(30, 256)};
        allocator.allocate(requests);
        const atlas_tile first = allocator.tiles()[allocator.allocations()[0].firstTile];
        const atlas_tile second = allocator.tiles()[allocator.allocations()[1].firstTile];
        const atlas_tile third = allocator.tiles()[allocator.allocations()[2].firstTile];
        // Reordered by importance: the tiles stay where they were.
        std::vector<atlas_request> reordered = {request(30, 256), request(10, 1024), request(20, 512)};
        allocator.allocate(reordered);
        CHECK(allocator.tiles()[allocator.allocations()[1].firstTile] == first);
        CHECK(allocator.tiles()[allocator.allocations()[2].firstTile] == second);
        CHECK(allocator.tiles()[allocator.allocations()[0].firstTile] == third);
        CHECK(allocator.allocations()[0].lightKey == 30);
        CHECK(allocator.allocations()[1].lightKey == 10);
        // A changed size is a new request and may land elsewhere; the others still keep theirs.
        std::vector<atlas_request> changed = {request(30, 512), request(10, 1024), request(20, 512)};
        allocator.allocate(changed);
        CHECK(allocator.allocations()[0].grantedSize == 512);
        CHECK(allocator.tiles()[allocator.allocations()[1].firstTile] == first);
        CHECK(allocator.tiles()[allocator.allocations()[2].firstTile] == second);
        CHECK(tiles_disjoint(allocator));
        // A light that vanished frees its tile for the next frame.
        std::vector<atlas_request> fewer = {request(10, 1024)};
        allocator.allocate(fewer);
        CHECK(allocator.tiles().size() == 1);
        CHECK(allocator.tiles()[0] == first);
    }

    TEST_CASE("shadow_atlas_allocator - point lights take six tiles of one size, all or nothing") {
        shadow_atlas_allocator allocator(4096);
        std::vector<atlas_request> requests = {request(1, 1024, 6), request(2, 1024, 6), request(3, 1024, 6)};
        allocator.allocate(requests);
        CHECK(allocator.allocations()[0].tileCount == 6);
        CHECK(allocator.allocations()[0].grantedSize == 1024);
        CHECK(allocator.allocations()[1].tileCount == 6);
        // Twelve of sixteen slots are used: the third point light halves to 512 (six fit in the remaining four 1024
        // slots).
        CHECK(allocator.allocations()[2].tileCount == 6);
        CHECK(allocator.allocations()[2].grantedSize == 512);
        CHECK(allocator.tiles().size() == 18);
        CHECK(allocator.dropped_count() == 0);
        for (u32 face = 0; face < 6; ++face) {
            CHECK(allocator.tiles()[allocator.allocations()[2].firstTile + face].size == 512);
        }
        CHECK(tiles_disjoint(allocator));
        // A tile count of zero allocates nothing and is not a drop.
        std::vector<atlas_request> none = {request(5, 1024, 0)};
        allocator.allocate(none);
        CHECK(allocator.allocations()[0].tileCount == 0);
        CHECK(allocator.dropped_count() == 0);
        CHECK(allocator.tiles().empty());
    }

    TEST_CASE("shadow_atlas_allocator - requested sizes are rounded to the allowed set and clamped to the atlas") {
        shadow_atlas_allocator allocator(4096);
        std::vector<atlas_request> requests = {request(1, 700), request(2, 100), request(3, 5000), request(4, 1000)};
        allocator.allocate(requests);
        CHECK(allocator.allocations()[0].grantedSize == 512);
        CHECK(allocator.allocations()[1].grantedSize == 256);
        CHECK(allocator.allocations()[2].grantedSize == 1024);
        CHECK(allocator.allocations()[3].grantedSize == 1024);
        // A small atlas caps the maximum tile at its own size.
        shadow_atlas_allocator small(512);
        CHECK(small.max_tile_size() == 512);
        std::vector<atlas_request> big = {
          request(1, 1024), request(2, 1024), request(3, 1024), request(4, 1024), request(5, 1024), request(6, 1024)
        };
        small.allocate(big);
        // The most important light takes the whole atlas; granted tiles are never shrunk for later lights.
        CHECK(small.allocations()[0].grantedSize == 512);
        CHECK(small.allocations()[1].tileCount == 0);
        CHECK(small.allocations()[5].tileCount == 0);
        CHECK(small.dropped_count() == 5);
        CHECK(tiles_disjoint(small));
        std::vector<atlas_request> quarters = {
          request(1, 256), request(2, 1024), request(3, 1024), request(4, 1024), request(5, 1024), request(6, 1024)
        };
        small.reset();
        small.allocate(quarters);
        CHECK(small.allocations()[0].grantedSize == 256);
        CHECK(small.allocations()[1].grantedSize == 256);
        CHECK(small.allocations()[3].grantedSize == 256);
        CHECK(small.allocations()[4].tileCount == 0);
        CHECK(small.dropped_count() == 2);
        CHECK(tiles_disjoint(small));
        // An atlas below the minimum tile is raised to it.
        shadow_atlas_allocator tiny(10);
        CHECK(tiny.atlas_size() == 256);
        CHECK(tiny.max_tile_size() == 256);
    }

    TEST_CASE(
      "shadow_atlas_allocator - many mixed requests fill the atlas without overlap and in order of importance"
    ) {
        shadow_atlas_allocator allocator(4096);
        std::vector<atlas_request> requests = {};
        for (u64 key = 1; key <= 120; ++key) {
            const u16 size = key % 3 == 0 ? 1024 : (key % 3 == 1 ? 512 : 256);
            requests.push_back(request(key, size, key % 10 == 0 ? 6 : 1));
        }
        allocator.allocate(requests);
        CHECK(tiles_disjoint(allocator));
        u64 area = 0;
        for (const atlas_tile &tile : allocator.tiles()) {
            area += static_cast<u64>(tile.size) * tile.size;
        }
        CHECK(area <= u64{4096} * 4096);
        CHECK(area > u64{4096} * 4096 / 2);   // the atlas is well used
        // Once a request was dropped, every less important request of the same or larger size is dropped too.
        bool dropped = false;
        for (usize index = 0; index < requests.size(); ++index) {
            const atlas_allocation &allocation = allocator.allocations()[index];
            if (allocation.tileCount == 0) {
                dropped = true;
            } else if (dropped) {
                CHECK(allocation.grantedSize <= 256);
            }
        }
        CHECK(allocator.dropped_count() > 0);
        // The same frame again is identical (stable tiles).
        const std::vector<atlas_tile> before(allocator.tiles().begin(), allocator.tiles().end());
        allocator.allocate(requests);
        const std::vector<atlas_tile> after(allocator.tiles().begin(), allocator.tiles().end());
        CHECK(before == after);
    }

    TEST_CASE("shadow_atlas_allocator - reset forgets the history so the next frame allocates from scratch") {
        shadow_atlas_allocator allocator(4096);
        std::vector<atlas_request> requests = {request(1, 256), request(2, 1024)};
        allocator.allocate(requests);
        const atlas_tile small = allocator.tiles()[allocator.allocations()[0].firstTile];
        allocator.reset();
        CHECK(allocator.tiles().empty());
        CHECK(allocator.allocations().empty());
        CHECK(allocator.dropped_count() == 0);
        std::vector<atlas_request> swapped = {request(2, 1024), request(1, 256)};
        allocator.allocate(swapped);
        // Without history the 1024 tile is placed first, so the 256 tile is no longer at its old place.
        CHECK(allocator.tiles()[allocator.allocations()[0].firstTile].x == 0);
        CHECK(allocator.tiles()[allocator.allocations()[0].firstTile].y == 0);
        CHECK_FALSE(allocator.tiles()[allocator.allocations()[1].firstTile] == small);
        CHECK(tiles_disjoint(allocator));
        allocator.allocate({});
        CHECK(allocator.tiles().empty());
        CHECK(allocator.allocations().empty());
    }
}
