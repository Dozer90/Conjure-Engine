#include "conjure/testing/doctest.h"

#include "conjure/renderer/gpu_ids.h"

#include "conjure/core/types.h"

#include <functional>
#include <type_traits>
#include <unordered_set>

// gpu_ids.h (docs/design/02_renderer.md 2.2): five distinct handle types sharing the index +
// generation layout, packable into 64 bits for ImTextureID-style use and hashing.

using namespace conjure;
using namespace conjure::renderer;

TEST_SUITE("renderer/gpu_ids") {

    TEST_CASE("gpu_id - a default id is invalid and generation 0 is never valid") {
        const gpu_mesh_id mesh = {};
        CHECK_FALSE(mesh.is_valid());
        CHECK(mesh.index == 0);
        CHECK(mesh.generation == 0);
        gpu_texture_id texture = {};
        texture.index = 12;
        CHECK_FALSE(texture.is_valid());
        texture.generation = 1;
        CHECK(texture.is_valid());
    }

    TEST_CASE("gpu_id - to_u64 packs the generation above the index and from_u64 inverts it") {
        gpu_material_id material = {};
        material.index = 0xABCDEF01u;
        material.generation = 0x1234;
        const u64 packed = material.to_u64();
        CHECK(packed == ((u64{0x1234} << 32) | 0xABCDEF01u));
        const gpu_material_id restored = gpu_material_id::from_u64(packed);
        CHECK(restored == material);
        CHECK(restored.index == 0xABCDEF01u);
        CHECK(restored.generation == 0x1234);
        CHECK(gpu_material_id::from_u64(0) == gpu_material_id());
    }

    TEST_CASE("gpu_id - ids compare by index then generation and hash by their packed value") {
        gpu_environment_id a = {};
        a.index = 1;
        a.generation = 1;
        gpu_environment_id b = a;
        b.generation = 2;
        gpu_environment_id c = a;
        c.index = 2;
        CHECK(a != b);
        CHECK(a < b);
        CHECK(b < c);
        CHECK(std::hash<gpu_environment_id>()(a) == static_cast<std::size_t>(a.to_u64()));
        std::unordered_set<render_target_id> set = {};
        render_target_id target = {};
        target.index = 5;
        target.generation = 3;
        set.insert(target);
        set.insert(target);
        CHECK(set.size() == 1);
        CHECK(set.contains(target));
    }

    TEST_CASE("gpu_id - the five handle types are distinct and trivially copyable") {
        static_assert(!std::is_same_v<gpu_mesh_id, gpu_texture_id>, "mesh and texture ids are different types");
        static_assert(!std::is_same_v<gpu_material_id, gpu_environment_id>, "material and environment ids differ");
        static_assert(!std::is_same_v<render_target_id, gpu_texture_id>, "render target ids are their own type");
        static_assert(std::is_trivially_copyable_v<gpu_texture_id>, "ids are copied by value");
        static_assert(sizeof(gpu_texture_id) == 8, "ids pack into 64 bits");
        static_assert(std::is_same_v<decltype(gpu_mesh_id::index), u32>, "the index is 32 bits");
        static_assert(std::is_same_v<decltype(gpu_mesh_id::generation), u16>, "the generation is 16 bits");
        CHECK(true);
    }
}
