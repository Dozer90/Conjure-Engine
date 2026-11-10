#include "conjure/testing/doctest.h"

#include "conjure/renderer/limits.h"

#include "conjure/core/types.h"

#include <type_traits>

// limits.h (docs/design/02_renderer.md 2.1): the fixed budgets and the relations between them. The
// HLSL side of the same numbers is compared by test_gpu_types.cpp.

using namespace conjure;
using namespace conjure::renderer;

TEST_SUITE("renderer/limits") {

    TEST_CASE("limits - the budgets hold the values the design fixes") {
        CHECK(MAX_VIEWS == 8);
        CHECK(MAX_LIGHTS_PER_VIEW == 1024);
        CHECK(MAX_LIGHTS_PER_TILE == 64);
        CHECK(LIGHT_TILE_SIZE == 16);
        CHECK(MAX_SHADOW_CASCADES == 4);
        CHECK(MAX_SPRITES_PER_FRAME == 65536);
        CHECK(MAX_DEBUG_VERTICES == 1048576);
        CHECK(MAX_MATERIAL_SLOTS == 8);
        CHECK(INVALID_INDEX == 0xFFFFFFFFu);
        CHECK(MATERIAL_TEXTURE_SLOTS == 5);
        CHECK(ENVIRONMENT_MIP_COUNT == 6);
        CHECK(CUBEMAP_FACE_COUNT == 6);
        CHECK(EXPOSURE_HISTOGRAM_BINS == 256);
        CHECK(BLOOM_MIP_COUNT == 6);
        CHECK(TRANSIENT_EVICTION_FRAMES == 120);
        CHECK(UPLOAD_BUDGET_BYTES_PER_FRAME == u64{64} * 1024 * 1024);
    }

    TEST_CASE("limits - the relations the shaders and the packers rely on hold") {
        // A tile list holds a count plus MAX_LIGHTS_PER_TILE indices twice (lists A and B).
        CHECK(MAX_LIGHTS_PER_TILE < MAX_LIGHTS_PER_VIEW);
        CHECK(LIGHT_TILE_SIZE % 8 == 0);
        // The histogram is dispatched in groups of 256 threads, one per bin.
        CHECK(EXPOSURE_HISTOGRAM_BINS == 256);
        // Debug vertices are 16 bytes: the cap keeps the upload at 16 MB per list.
        CHECK(static_cast<u64>(MAX_DEBUG_VERTICES) * 16 == u64{16} * 1024 * 1024);
        // Sprites are 4 vertices and 6 indices each; the index buffer stays 32-bit.
        CHECK(static_cast<u64>(MAX_SPRITES_PER_FRAME) * 4 < u64{1} << 32);
        // Material slots fit the u8 slot index of the draw items.
        CHECK(MAX_MATERIAL_SLOTS <= 16);
        static_assert(std::is_same_v<decltype(MAX_VIEWS), const u32>, "limits are u32");
        static_assert(std::is_same_v<decltype(UPLOAD_BUDGET_BYTES_PER_FRAME), const u64>, "the byte budget is u64");
    }
}
