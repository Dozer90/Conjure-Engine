#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"

#include "renderer/draw_list_builder.h"

#include "conjure/core/types.h"

#include <span>
#include <vector>

// Draw lists (docs/design/02_renderer.md 2.8, 10 "test_draw_list_builder"): key layout, stable
// sort order, instanced batching of equal opaque runs, far-to-near blended order without batching.

using namespace conjure;
using namespace conjure::renderer;

namespace {

    [[nodiscard]] draw_item item(
      draw_variant variant,
      u32 material,
      u32 mesh,
      u32 submesh,
      u32 instance,
      bool doubleSided = false,
      u8 layer = 0
    ) {
        draw_item value = {};
        value.variant = variant;
        value.materialIndex = material;
        value.meshIndex = mesh;
        value.submesh = submesh;
        value.instanceIndex = instance;
        value.doubleSided = doubleSided;
        value.layer = layer;
        return value;
    }
}

TEST_SUITE("renderer/draw_list_builder") {

    TEST_CASE("opaque_key - orders by variant, material, mesh, submesh then depth bucket") {
        const u64 base = draw_list_builder::opaque_key(draw_variant::OPAQUE_SURFACE, 5, 5, 0, 0.0f);
        CHECK(
          draw_list_builder::opaque_key(
            draw_variant::MASKED,
            0,
            0,
            0,
            0.0f
          ) > draw_list_builder::opaque_key(draw_variant::OPAQUE_SURFACE, 0xFFFFF, 0xFFFFF, 15, 255.0f)
        );
        CHECK(
          draw_list_builder::opaque_key(
            draw_variant::OPAQUE_SURFACE,
            6,
            0,
            0,
            0.0f
          ) > draw_list_builder::opaque_key(draw_variant::OPAQUE_SURFACE, 5, 0xFFFFF, 15, 255.0f)
        );
        CHECK(
          draw_list_builder::opaque_key(
            draw_variant::OPAQUE_SURFACE,
            5,
            6,
            0,
            0.0f
          ) > draw_list_builder::opaque_key(draw_variant::OPAQUE_SURFACE, 5, 5, 15, 255.0f)
        );
        CHECK(
          draw_list_builder::opaque_key(
            draw_variant::OPAQUE_SURFACE,
            5,
            5,
            1,
            0.0f
          ) > draw_list_builder::opaque_key(draw_variant::OPAQUE_SURFACE, 5, 5, 0, 255.0f)
        );
        CHECK(draw_list_builder::opaque_key(draw_variant::OPAQUE_SURFACE, 5, 5, 0, 10.0f) > base);
        CHECK(
          draw_list_builder::opaque_key(
            draw_variant::OPAQUE_SURFACE,
            5,
            5,
            0,
            300.0f
          ) == draw_list_builder::opaque_key(draw_variant::OPAQUE_SURFACE, 5, 5, 0, 255.0f)
        );
        CHECK(draw_list_builder::opaque_key(draw_variant::OPAQUE_SURFACE, 5, 5, 0, -3.0f) == base);
        // The 22-bit fields wrap: index 2^22 collides with 0 (the renderer never issues that many).
        CHECK(
          draw_list_builder::opaque_key(
            draw_variant::OPAQUE_SURFACE,
            1u << 22,
            0,
            0,
            0.0f
          ) == draw_list_builder::opaque_key(draw_variant::OPAQUE_SURFACE, 0, 0, 0, 0.0f)
        );
    }

    TEST_CASE("blend_key - orders by layer, then far to near, then material, above every opaque key") {
        const u64 opaqueMax = draw_list_builder::opaque_key(draw_variant::MASKED, 0xFFFFF, 0xFFFFF, 15, 255.0f);
        CHECK(draw_list_builder::blend_key(0, 0.0f, 100.0f, 0) > opaqueMax);
        CHECK(
          draw_list_builder::blend_key(1, 100.0f, 100.0f, 0) > draw_list_builder::blend_key(0, 0.0f, 100.0f, 0xFFFFFF)
        );
        // Farther sorts lower (drawn first).
        CHECK(draw_list_builder::blend_key(0, 90.0f, 100.0f, 0) < draw_list_builder::blend_key(0, 10.0f, 100.0f, 0));
        CHECK(draw_list_builder::blend_key(0, 10.0f, 100.0f, 1) > draw_list_builder::blend_key(0, 10.0f, 100.0f, 0));
        // Depth is clamped to the far plane and a zero far plane does not divide by zero.
        CHECK(draw_list_builder::blend_key(0, 500.0f, 100.0f, 0) == draw_list_builder::blend_key(0, 100.0f, 100.0f, 0));
        CHECK(draw_list_builder::blend_key(0, 5.0f, 0.0f, 0) == draw_list_builder::blend_key(0, 50.0f, 0.0f, 0));
    }

    TEST_CASE("draw_list_builder - equal opaque runs collapse into instanced batches in sorted order") {
        draw_list_builder builder = {};
        builder.add(item(draw_variant::OPAQUE_SURFACE, 2, 1, 0, 0), 10.0f, 100.0f);
        builder.add(item(draw_variant::OPAQUE_SURFACE, 1, 1, 0, 1), 20.0f, 100.0f);
        builder.add(item(draw_variant::OPAQUE_SURFACE, 2, 1, 0, 2), 5.0f, 100.0f);
        builder.add(item(draw_variant::OPAQUE_SURFACE, 1, 1, 1, 3), 20.0f, 100.0f);
        builder.add(item(draw_variant::MASKED, 1, 1, 0, 4), 1.0f, 100.0f);
        builder.add(item(draw_variant::OPAQUE_SURFACE, 1, 1, 0, 5), 2.0f, 100.0f);
        CHECK(builder.item_count() == 6);
        builder.build();
        const std::span<const draw_item> items = builder.items();
        REQUIRE(items.size() == 6);
        // Opaque material 1 mesh 1 submesh 0 (two items, near first), submesh 1, material 2 (two, near first), masked.
        CHECK(items[0].instanceIndex == 5);
        CHECK(items[1].instanceIndex == 1);
        CHECK(items[2].instanceIndex == 3);
        CHECK(items[3].instanceIndex == 2);
        CHECK(items[4].instanceIndex == 0);
        CHECK(items[5].instanceIndex == 4);
        const std::span<const draw_batch> batches = builder.batches();
        REQUIRE(batches.size() == 4);
        CHECK(batches[0].variant == draw_variant::OPAQUE_SURFACE);
        CHECK(batches[0].materialIndex == 1);
        CHECK(batches[0].meshIndex == 1);
        CHECK(batches[0].submesh == 0);
        CHECK(batches[0].firstItem == 0);
        CHECK(batches[0].instanceCount == 2);
        CHECK(batches[1].submesh == 1);
        CHECK(batches[1].firstItem == 2);
        CHECK(batches[1].instanceCount == 1);
        CHECK(batches[2].materialIndex == 2);
        CHECK(batches[2].firstItem == 3);
        CHECK(batches[2].instanceCount == 2);
        CHECK(batches[3].variant == draw_variant::MASKED);
        CHECK(batches[3].firstItem == 5);
        CHECK(batches[3].instanceCount == 1);
        // The sort keys of the sorted list are non-decreasing.
        for (usize index = 1; index < items.size(); ++index) {
            CHECK(items[index - 1].sortKey <= items[index].sortKey);
        }
    }

    TEST_CASE("draw_list_builder - double-sided items never merge with single-sided ones") {
        draw_list_builder builder = {};
        builder.add(item(draw_variant::OPAQUE_SURFACE, 1, 1, 0, 0, false), 1.0f, 100.0f);
        builder.add(item(draw_variant::OPAQUE_SURFACE, 1, 1, 0, 1, true), 1.0f, 100.0f);
        builder.add(item(draw_variant::OPAQUE_SURFACE, 1, 1, 0, 2, false), 1.0f, 100.0f);
        builder.build();
        const std::span<const draw_batch> batches = builder.batches();
        REQUIRE(batches.size() >= 2);
        u32 instances = 0;
        for (const draw_batch &batch : batches) {
            instances += batch.instanceCount;
            CHECK(batch.variant == draw_variant::OPAQUE_SURFACE);
        }
        CHECK(instances == 3);
        bool sawDoubleSided = false;
        for (const draw_batch &batch : batches) {
            if (batch.doubleSided) {
                sawDoubleSided = true;
                CHECK(batch.instanceCount == 1);
            }
        }
        CHECK(sawDoubleSided);
    }

    TEST_CASE("draw_list_builder - blended items sort far to near by layer and get one batch each") {
        draw_list_builder builder = {};
        builder.add(item(draw_variant::BLENDED, 1, 1, 0, 0, false, 0), 10.0f, 100.0f);
        builder.add(item(draw_variant::BLENDED, 1, 1, 0, 1, false, 0), 50.0f, 100.0f);
        builder.add(item(draw_variant::BLENDED, 1, 1, 0, 2, false, 0), 30.0f, 100.0f);
        builder.add(item(draw_variant::BLENDED, 1, 1, 0, 3, false, 1), 90.0f, 100.0f);   // layer 1 draws after layer 0
        builder.add(item(draw_variant::OPAQUE_SURFACE, 1, 1, 0, 4), 70.0f, 100.0f);
        builder.build();
        const std::span<const draw_item> items = builder.items();
        REQUIRE(items.size() == 5);
        CHECK(items[0].instanceIndex == 4);   // opaque first
        CHECK(items[1].instanceIndex == 1);   // far
        CHECK(items[2].instanceIndex == 2);
        CHECK(items[3].instanceIndex == 0);   // near
        CHECK(items[4].instanceIndex == 3);   // layer 1
        const std::span<const draw_batch> batches = builder.batches();
        REQUIRE(batches.size() == 5);
        for (usize index = 1; index < batches.size(); ++index) {
            CHECK(batches[index].variant == draw_variant::BLENDED);
            CHECK(batches[index].instanceCount == 1);
            CHECK(batches[index].firstItem == index);
        }
        // Two blended items at the same depth and material still do not merge.
        draw_list_builder same = {};
        same.add(item(draw_variant::BLENDED, 1, 1, 0, 0), 10.0f, 100.0f);
        same.add(item(draw_variant::BLENDED, 1, 1, 0, 1), 10.0f, 100.0f);
        same.build();
        CHECK(same.batches().size() == 2);
    }

    TEST_CASE("draw_list_builder - clear forgets the items and an empty build yields nothing") {
        draw_list_builder builder = {};
        builder.build();
        CHECK(builder.items().empty());
        CHECK(builder.batches().empty());
        builder.add(item(draw_variant::OPAQUE_SURFACE, 1, 1, 0, 0), 1.0f, 100.0f);
        builder.build();
        CHECK(builder.batches().size() == 1);
        builder.clear();
        CHECK(builder.item_count() == 0);
        CHECK(builder.items().empty());
        CHECK(builder.batches().empty());
        builder.add(item(draw_variant::MASKED, 3, 3, 2, 9), 1.0f, 100.0f);
        builder.build();
        REQUIRE(builder.batches().size() == 1);
        CHECK(builder.batches()[0].variant == draw_variant::MASKED);
        CHECK(builder.batches()[0].submesh == 2);
        CHECK(builder.items()[0].instanceIndex == 9);
    }

    TEST_CASE("draw_list_builder - indices beyond the 22-bit key fields and submeshes beyond 16 assert") {
        draw_list_builder builder = {};
        CONJURE_CHECK_ASSERTS(
          builder.add(item(draw_variant::OPAQUE_SURFACE, MAX_DRAW_KEY_INDEX + 1, 0, 0, 0), 1.0f, 100.0f)
        );
        CONJURE_CHECK_ASSERTS(
          builder.add(item(draw_variant::OPAQUE_SURFACE, 0, MAX_DRAW_KEY_INDEX + 1, 0, 0), 1.0f, 100.0f)
        );
        CONJURE_CHECK_ASSERTS(
          builder.add(item(draw_variant::MASKED, 0, 0, MAX_DRAW_KEY_SUBMESH, 0), 1.0f, 100.0f)
        );
        builder.clear();
        // The largest representable values are fine and keep their order.
        const u32 top = MAX_DRAW_KEY_INDEX;
        builder.add(item(draw_variant::OPAQUE_SURFACE, top, top, 15, 0), 1.0f, 100.0f);
        builder.add(item(draw_variant::OPAQUE_SURFACE, top - 1, top, 15, 1), 1.0f, 100.0f);
        builder.build();
        REQUIRE(builder.batches().size() == 2);
        CHECK(builder.batches()[0].materialIndex == MAX_DRAW_KEY_INDEX - 1);
        CHECK(builder.batches()[1].materialIndex == MAX_DRAW_KEY_INDEX);
    }

    TEST_CASE("draw_list_builder - a large list sorts stably by key with the depth bucket as the tie breaker") {
        draw_list_builder builder = {};
        const u32 count = 5000;
        for (u32 index = 0; index < count; ++index) {
            const u32 material = index % 7;
            const u32 mesh = index % 3;
            builder.add(
              item(draw_variant::OPAQUE_SURFACE, material, mesh, 0, index),
              static_cast<f32>(index % 100),
              100.0f
            );
        }
        builder.build();
        const std::span<const draw_item> items = builder.items();
        REQUIRE(items.size() == count);
        for (usize index = 1; index < items.size(); ++index) {
            CHECK(items[index - 1].sortKey <= items[index].sortKey);
            if (items[index - 1].sortKey == items[index].sortKey) {
                CHECK(items[index - 1].instanceIndex < items[index].instanceIndex);   // stable
            }
        }
        // 7 materials x 3 meshes = 21 batches at most, every item accounted for.
        CHECK(builder.batches().size() == 21);
        u32 total = 0;
        for (const draw_batch &batch : builder.batches()) {
            total += batch.instanceCount;
        }
        CHECK(total == count);
    }
}
