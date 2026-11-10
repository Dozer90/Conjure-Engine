#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/test_environment.h"
#include "renderer_test_support.h"

#include "renderer/binding_layouts.h"
#include "renderer/gpu_resource_cache.h"

#include "conjure/renderer/environment_asset.h"
#include "conjure/renderer/material_asset.h"
#include "conjure/renderer/mesh_asset.h"
#include "conjure/renderer/texture_asset.h"

#include "conjure/assets/asset_ref.h"
#include "conjure/core/image.h"
#include "conjure/core/jobs/job_system.h"
#include "conjure/core/log.h"
#include "conjure/core/math.h"
#include "conjure/core/types.h"
#include "conjure/rhi/device.h"
#include "conjure/rhi/null_device_stats.h"

#include <nvrhi/nvrhi.h>

#include <atomic>
#include <memory>
#include <optional>
#include <vector>

// GPU mirrors of the asset types (docs/design/02_renderer.md 2.2, 2.6, R10, 5, 10
// "test_gpu_resource_cache"): enqueue from any thread with the payload copied, ids valid at once
// and resolving to the fallback until flushed, the per-frame byte budget, material rebinding when
// a texture arrives, delayed release through the graveyard and device loss.

using namespace conjure;
using namespace conjure::renderer;
using namespace conjure::testing;
using namespace conjure::testing::renderer_support;

namespace {

    struct cache_fixture {
        std::unique_ptr<rhi::device> device = nullptr;
        binding_layouts layouts = {};
        nvrhi::CommandListHandle commandList = nullptr;
        std::unique_ptr<gpu_resource_cache> cache = nullptr;
        u64 frame = 0;

        explicit cache_fixture(u32 releaseDelay = 3) {
            device = make_device();
            nvrhi::IDevice &native = *device->native_device();
            core::result<binding_layouts> created = binding_layouts::create(native);
            CONJURE_REQUIRE_OK(created);
            layouts = std::move(created).value();
            commandList = native.createCommandList();
            REQUIRE(commandList != nullptr);
            commandList->open();
            core::result<std::unique_ptr<gpu_resource_cache>> result = gpu_resource_cache::create(
              native,
              layouts,
              *commandList,
              releaseDelay
            );
            commandList->close();
            CONJURE_REQUIRE_OK(result);
            cache = std::move(result).value();
            submit();
        }

        ~cache_fixture() {
            cache.reset();
            layouts = binding_layouts();
        }

        cache_fixture(const cache_fixture &) = delete;
        cache_fixture &operator=(const cache_fixture &) = delete;

        void submit() {
            nvrhi::ICommandList *list = commandList.Get();
            device->native_device()->executeCommandLists(&list, 1);
        }

        [[nodiscard]] upload_report flush(u64 budget = u64{64} * 1024 * 1024) {
            ++frame;
            commandList->open();
            const upload_report report = cache->flush_uploads(*commandList, frame, budget);
            commandList->close();
            submit();
            cache->collect(frame);
            return report;
        }
    };

    [[nodiscard]] texture_asset big_texture(u32 edge) {
        return texture_asset::solid_colour(core::rgba8{1, 2, 3, 255}, texture_colour_space::LINEAR, edge, edge);
    }
}

TEST_SUITE("renderer/gpu_resource_cache") {

    TEST_CASE("gpu_resource_cache - create builds the fallbacks and the six samplers") {
        cache_fixture fixture(3);
        CHECK(fixture.cache->fallback_mesh().indexCount == 36);
        CHECK(fixture.cache->fallback_mesh().vertexCount == 24);
        CHECK(fixture.cache->fallback_mesh().stream0 != nullptr);
        CHECK(fixture.cache->fallback_mesh().stream1 != nullptr);
        CHECK(fixture.cache->fallback_mesh().indices != nullptr);
        CHECK(fixture.cache->fallback_mesh().submeshes.size() == 1);
        CHECK(fixture.cache->fallback_texture(material_texture::BASE_COLOUR).texture != nullptr);
        CHECK(fixture.cache->fallback_texture(material_texture::NORMAL).texture != nullptr);
        CHECK(fixture.cache->fallback_texture(material_texture::EMISSIVE).texture != nullptr);
        CHECK(
          fixture.cache->fallback_texture(
            material_texture::BASE_COLOUR
          ).texture != fixture.cache->fallback_texture(material_texture::NORMAL).texture
        );
        CHECK(fixture.cache->fallback_material().bindingSet != nullptr);
        CHECK(fixture.cache->fallback_material().constants != nullptr);
        CHECK(fixture.cache->fallback_material().unlit);
        CHECK(fixture.cache->fallback_environment().sky != nullptr);
        CHECK(fixture.cache->fallback_environment().prefiltered != nullptr);
        CHECK(fixture.cache->fallback_environment().irradiance[0].x > 0.0f);
        for (const sampler_filter filter : {sampler_filter::LINEAR, sampler_filter::POINT}) {
            for (const texture_wrap wrap : {texture_wrap::REPEAT, texture_wrap::CLAMP, texture_wrap::MIRROR}) {
                CHECK(fixture.cache->sampler(filter, wrap) != nullptr);
            }
        }
        CHECK(
          fixture.cache->sampler(
            sampler_filter::LINEAR,
            texture_wrap::REPEAT
          ) != fixture.cache->sampler(sampler_filter::POINT, texture_wrap::REPEAT)
        );
        CHECK(
          fixture.cache->sampler(
            sampler_filter::LINEAR,
            texture_wrap::REPEAT
          ) != fixture.cache->sampler(sampler_filter::LINEAR, texture_wrap::CLAMP)
        );
        CHECK(fixture.cache->mesh_count() == 0);
        CHECK(fixture.cache->texture_count() == 0);
        CHECK(fixture.cache->material_count() == 0);
        CHECK(fixture.cache->environment_count() == 0);
        CHECK(fixture.cache->pending_count() == 0);
        CHECK(fixture.cache->graveyard_count() == 0);
        CHECK(fixture.device->null_stats()->samplersCreated == 6);
    }

    TEST_CASE("gpu_resource_cache - enqueue reserves the id at once and the fallback serves it until the flush") {
        log_capture logs = {};
        cache_fixture fixture(3);
        const gpu_mesh_id mesh = fixture.cache->enqueue_mesh(test_asset_id(1), mesh_asset::uv_sphere(1.0f, 4, 8));
        CHECK(mesh.is_valid());
        CHECK(fixture.cache->mesh_count() == 1);
        CHECK(fixture.cache->pending_count() == 1);
        CHECK_FALSE(fixture.cache->is_mesh_ready(mesh));
        CHECK(fixture.cache->find_mesh(test_asset_id(1)) == mesh);
        CHECK(&fixture.cache->resolve_mesh(mesh) == &fixture.cache->fallback_mesh());
        CHECK(fixture.cache->fallbacks_used() == 1);
        CHECK(&fixture.cache->resolve_mesh(mesh) == &fixture.cache->fallback_mesh());
        CHECK(fixture.cache->fallbacks_used() == 2);
        CHECK(logs.count(core::log_level::WARN) == 0);   // pending is not a warning
        CHECK(logs.count(core::log_level::DEBUG) <= 1);  // reported once

        const upload_report report = fixture.flush();
        CHECK(report.completed == 1);
        CHECK(report.pending == 0);
        CHECK(report.bytes == mesh_asset::uv_sphere(1.0f, 4, 8).gpu_byte_size());
        CHECK(fixture.cache->is_mesh_ready(mesh));
        CHECK(fixture.cache->pending_count() == 0);
        {
            const gpu_mesh &mirror = fixture.cache->resolve_mesh(mesh);
            CHECK(&mirror != &fixture.cache->fallback_mesh());
            CHECK(mirror.indexCount == mesh_asset::uv_sphere(1.0f, 4, 8).index_count());
            CHECK(mirror.vertexCount == mesh_asset::uv_sphere(1.0f, 4, 8).vertex_count());
            CHECK(mirror.stream0 != nullptr);
            CHECK(mirror.bounds == mesh_asset::uv_sphere(1.0f, 4, 8).bounds());
        }
        fixture.cache->reset_counters();
        CHECK(fixture.cache->fallbacks_used() == 0);
        // A second flush with nothing queued does nothing.
        const upload_report empty = fixture.flush();
        CHECK(empty.completed == 0);
        CHECK(empty.bytes == 0);
    }

    TEST_CASE("gpu_resource_cache - every mirror type uploads and resolves with its own data") {
        cache_fixture fixture(3);
        texture_asset textureValue = texture_asset::checker(
          core::rgba8{},
          core::rgba8{255, 255, 255, 255},
          8,
          4,
          texture_colour_space::SRGB
        );
        textureValue.set_filter(sampler_filter::POINT);
        textureValue.set_wrap(texture_wrap::MIRROR);
        const gpu_texture_id texture = fixture.cache->enqueue_texture(test_asset_id(2), textureValue);
        material_asset materialValue = {};
        materialValue.desc().alphaMode = alpha_mode::MASK;
        materialValue.desc().doubleSided = true;
        materialValue.desc().baseColourFactor = math::vec4(0.5f, 0.25f, 1.0f, 1.0f);
        materialValue.desc().textures[0] = assets::asset_ref<texture_asset>(test_asset_id(2));
        const gpu_material_id material = fixture.cache->enqueue_material(test_asset_id(3), materialValue);
        const gpu_environment_id environment = fixture.cache->enqueue_environment(
          test_asset_id(4),
          environment_asset::uniform(math::vec3(0.75f), 8)
        );
        CHECK(fixture.cache->pending_count() == 3);
        const upload_report report = fixture.flush();
        CHECK(report.completed == 3);
        CHECK(fixture.cache->is_texture_ready(texture));
        CHECK(fixture.cache->is_material_ready(material));
        CHECK(fixture.cache->is_environment_ready(environment));
        const gpu_texture &textureMirror = fixture.cache->resolve_texture(texture);
        CHECK(textureMirror.texture != nullptr);
        CHECK(textureMirror.width == 8);
        CHECK(textureMirror.height == 8);
        CHECK(textureMirror.mipCount == 4);
        CHECK(textureMirror.format == rhi::texture_format::RGBA8_SRGB);
        CHECK(textureMirror.filter == sampler_filter::POINT);
        CHECK(textureMirror.wrap == texture_wrap::MIRROR);
        const gpu_material &materialMirror = fixture.cache->resolve_material(material);
        CHECK(materialMirror.bindingSet != nullptr);
        CHECK(materialMirror.constants != nullptr);
        CHECK(materialMirror.alphaMode == alpha_mode::MASK);
        CHECK(materialMirror.doubleSided);
        CHECK_FALSE(materialMirror.unlit);
        CHECK(materialMirror.data.baseColourFactor.y == doctest::Approx(0.25f));
        CHECK((materialMirror.data.flags.x & 1u) != 0);   // base colour texture present
        CHECK(materialMirror.textureAssets[0] == test_asset_id(2));
        CHECK(materialMirror.textures[0] == texture);
        const gpu_environment &environmentMirror = fixture.cache->resolve_environment(environment);
        CHECK(environmentMirror.sky != nullptr);
        CHECK(environmentMirror.prefiltered != nullptr);
        CHECK(environmentMirror.prefilteredMipCount >= 1);
        CHECK(
          environmentMirror.irradiance[0].x == doctest::Approx(0.75f * 4.0f * math::PI * 0.282095f).epsilon(1.0e-3)
        );
        CHECK(fixture.cache->fallbacks_used() == 0);
    }

    TEST_CASE("gpu_resource_cache - a material enqueued before its texture is rebound when the texture arrives") {
        cache_fixture fixture(3);
        material_asset materialValue = {};
        materialValue.desc().textures[2] = assets::asset_ref<texture_asset>(test_asset_id(9));
        const gpu_material_id material = fixture.cache->enqueue_material(test_asset_id(3), materialValue);
        static_cast<void>(fixture.flush());
        nvrhi::IBindingSet *before = nullptr;
        {
            const gpu_material &mirror = fixture.cache->resolve_material(material);
            before = mirror.bindingSet.Get();
            CHECK_FALSE(mirror.textures[2].is_valid());
            CHECK(mirror.textureAssets[2] == test_asset_id(9));
        }
        const gpu_texture_id texture = fixture.cache->enqueue_texture(test_asset_id(9), big_texture(2));
        const upload_report arrival = fixture.flush();
        CHECK(arrival.materialsRebound == 1);
        CHECK(arrival.constantWrites == 1);   // the texture flags changed
        {
            const gpu_material &mirror = fixture.cache->resolve_material(material);
            CHECK(mirror.textures[2] == texture);
            CHECK(mirror.bindingSet != nullptr);
            CHECK(mirror.bindingSet.Get() != before);
        }
        // Replacing the texture under the same asset id keeps the texture id and rebinds again.
        const gpu_texture_id same = fixture.cache->enqueue_texture(test_asset_id(9), big_texture(4));
        CHECK(same == texture);
        static_cast<void>(fixture.flush());
        CHECK(fixture.cache->resolve_texture(texture).width == 4);
        CHECK(fixture.cache->resolve_material(material).textures[2] == texture);
    }

    TEST_CASE("gpu_resource_cache - the byte budget defers uploads to later frames but always runs at least one") {
        cache_fixture fixture(3);
        const texture_asset large = big_texture(256);   // 256 KB plus mips
        const gpu_texture_id a = fixture.cache->enqueue_texture(test_asset_id(1), large);
        const gpu_texture_id b = fixture.cache->enqueue_texture(test_asset_id(2), large);
        const gpu_texture_id c = fixture.cache->enqueue_texture(test_asset_id(3), large);
        CHECK(fixture.cache->pending_count() == 3);
        const upload_report first = fixture.flush(1);
        CHECK(first.completed == 1);
        CHECK(first.pending == 2);
        CHECK(first.bytes == large.gpu_byte_size());
        CHECK(fixture.cache->is_texture_ready(a));
        CHECK_FALSE(fixture.cache->is_texture_ready(b));
        CHECK(fixture.cache->pending_count() == 2);
        const upload_report second = fixture.flush(large.gpu_byte_size() * 2);
        CHECK(second.completed == 2);
        CHECK(second.pending == 0);
        CHECK(fixture.cache->is_texture_ready(b));
        CHECK(fixture.cache->is_texture_ready(c));
        // Exactly the budget fits exactly that many.
        const gpu_texture_id d = fixture.cache->enqueue_texture(test_asset_id(4), large);
        const gpu_texture_id e = fixture.cache->enqueue_texture(test_asset_id(5), large);
        const upload_report third = fixture.flush(large.gpu_byte_size());
        CHECK(third.completed == 1);
        CHECK(fixture.cache->is_texture_ready(d));
        CHECK_FALSE(fixture.cache->is_texture_ready(e));
        static_cast<void>(fixture.flush());
        CHECK(fixture.cache->is_texture_ready(e));
    }

    TEST_CASE("gpu_resource_cache - release makes the id stale at once and the GPU objects die after the delay") {
        log_capture logs = {};
        cache_fixture fixture(2);
        const gpu_mesh_id mesh = fixture.cache->enqueue_mesh(test_asset_id(1), mesh_asset::unit_cube());
        const gpu_texture_id texture = fixture.cache->enqueue_texture(test_asset_id(2), big_texture(2));
        const gpu_material_id material = fixture.cache->enqueue_material(test_asset_id(3), material_asset());
        const gpu_environment_id environment = fixture.cache->enqueue_environment(
          test_asset_id(4),
          environment_asset::uniform(math::vec3(0.5f), 4)
        );
        static_cast<void>(fixture.flush());
        const u64 releaseFrame = fixture.frame;
        fixture.cache->release_mesh(mesh, releaseFrame);
        fixture.cache->release_texture(texture, releaseFrame);
        fixture.cache->release_material(material, releaseFrame);
        fixture.cache->release_environment(environment, releaseFrame);
        CHECK_FALSE(fixture.cache->is_mesh_ready(mesh));
        CHECK_FALSE(fixture.cache->find_mesh(test_asset_id(1)).is_valid());
        CHECK_FALSE(fixture.cache->find_texture(test_asset_id(2)).is_valid());
        CHECK(fixture.cache->mesh_count() == 0);
        CHECK(fixture.cache->texture_count() == 0);
        CHECK(fixture.cache->material_count() == 0);
        CHECK(fixture.cache->environment_count() == 0);
        CHECK(fixture.cache->graveyard_count() > 0);
        const usize buried = fixture.cache->graveyard_count();
        CHECK(fixture.cache->retired_count() == 4);
        CHECK(&fixture.cache->resolve_mesh(mesh) == &fixture.cache->fallback_mesh());
        CHECK(
          &fixture.cache->resolve_texture(
            texture
          ) == &fixture.cache->fallback_texture(material_texture::BASE_COLOUR)
        );
        CHECK(&fixture.cache->resolve_material(material) == &fixture.cache->fallback_material());
        CHECK(&fixture.cache->resolve_environment(environment) == &fixture.cache->fallback_environment());
        CHECK(fixture.cache->fallbacks_used() == 4);
        static_cast<void>(fixture.cache->resolve_mesh(mesh));
        CHECK(logs.count(core::log_level::WARN) == 4);   // once per stale id
        CHECK(logs.contains(core::log_level::WARN, "stale or unknown"));
        // Releasing again or releasing a never-issued id is ignored.
        fixture.cache->release_mesh(mesh, releaseFrame);
        fixture.cache->release_mesh(gpu_mesh_id(), releaseFrame);
        CHECK(fixture.cache->graveyard_count() == buried);
        // The graveyard empties once the delay has passed; the entries die at the first collect.
        fixture.cache->collect(releaseFrame + 1);
        CHECK(fixture.cache->retired_count() == 0);
        CHECK(fixture.cache->graveyard_count() == buried);
        fixture.cache->collect(releaseFrame + 2);
        CHECK(fixture.cache->graveyard_count() == buried);
        fixture.cache->collect(releaseFrame + 3);
        CHECK(fixture.cache->graveyard_count() == 0);
        // A slot can be reused: the new id differs in generation.
        const gpu_mesh_id fresh = fixture.cache->enqueue_mesh(test_asset_id(1), mesh_asset::unit_cube());
        CHECK(fresh != mesh);
        CHECK(fixture.cache->find_mesh(test_asset_id(1)) == fresh);
    }

    TEST_CASE("gpu_resource_cache - a release before the flush drops the pending upload") {
        cache_fixture fixture(3);
        const gpu_mesh_id mesh = fixture.cache->enqueue_mesh(test_asset_id(1), mesh_asset::unit_cube());
        fixture.cache->release_mesh(mesh, 0);
        const upload_report report = fixture.flush();
        CHECK(report.completed == 0);
        CHECK(fixture.cache->mesh_count() == 0);
        CHECK_FALSE(fixture.cache->is_mesh_ready(mesh));
    }

    TEST_CASE("gpu_resource_cache - drop_all forgets every mirror and queued upload") {
        cache_fixture fixture(3);
        const gpu_mesh_id ready = fixture.cache->enqueue_mesh(test_asset_id(1), mesh_asset::unit_cube());
        static_cast<void>(fixture.flush());
        const gpu_texture_id pending = fixture.cache->enqueue_texture(test_asset_id(2), big_texture(2));
        fixture.cache->drop_all();
        CHECK(fixture.cache->mesh_count() == 0);
        CHECK(fixture.cache->texture_count() == 0);
        CHECK(fixture.cache->pending_count() == 0);
        CHECK(fixture.cache->graveyard_count() == 0);
        CHECK_FALSE(fixture.cache->is_mesh_ready(ready));
        CHECK_FALSE(fixture.cache->is_texture_ready(pending));
        CHECK_FALSE(fixture.cache->find_mesh(test_asset_id(1)).is_valid());
        const upload_report report = fixture.flush();
        CHECK(report.completed == 0);
        // The fallbacks survive so stale ids still resolve.
        CHECK(&fixture.cache->resolve_mesh(ready) == &fixture.cache->fallback_mesh());
        CHECK(fixture.cache->fallback_mesh().stream0 != nullptr);
    }

    TEST_CASE("gpu_resource_cache - external textures are registered ready and never destroyed by the cache") {
        cache_fixture fixture(3);
        nvrhi::TextureDesc desc = {};
        desc.width = 16;
        desc.height = 8;
        desc.format = nvrhi::Format::SRGBA8_UNORM;
        desc.isRenderTarget = true;
        desc.debugName = "external";
        nvrhi::TextureHandle external = fixture.device->native_device()->createTexture(desc);
        REQUIRE(external != nullptr);
        const gpu_texture_id id = fixture.cache->register_external_texture(
          external,
          rhi::texture_format::RGBA8_SRGB,
          16,
          8
        );
        CHECK(id.is_valid());
        CHECK(fixture.cache->is_texture_ready(id));
        CHECK(fixture.cache->texture_count() == 1);
        {
            const gpu_texture &mirror = fixture.cache->resolve_texture(id);
            CHECK(mirror.texture == external.Get());
            CHECK(mirror.width == 16);
            CHECK(mirror.height == 8);
            CHECK(mirror.wrap == texture_wrap::CLAMP);
        }
        fixture.cache->unregister_external_texture(id);
        CHECK(fixture.cache->texture_count() == 0);
        CHECK_FALSE(fixture.cache->is_texture_ready(id));
        CHECK(fixture.cache->graveyard_count() == 0);
        fixture.cache->collect(1);
        CHECK(fixture.cache->retired_count() == 0);
        CHECK(external->getDesc().width == 16);   // still alive, owned by the caller
        fixture.cache->unregister_external_texture(id);
    }

    TEST_CASE("gpu_resource_cache - enqueue is safe from job threads and every upload lands") {
        cache_fixture fixture(3);
        core::jobs::job_system jobs = {};
        core::jobs::job_system_desc desc = {};
        desc.workerCount = environment().jobThreadCount;
        CONJURE_REQUIRE_OK(jobs.start(desc));
        constexpr usize COUNT = 64;
        std::vector<gpu_mesh_id> meshes(COUNT);
        std::vector<gpu_texture_id> textures(COUNT);
        std::atomic<u32> done = 0;
        jobs.parallel_for(COUNT, [&](usize index) {
            meshes[index] = fixture.cache->enqueue_mesh(
              test_asset_id(100 + index),
              mesh_asset::quad(static_cast<f32>(index + 1))
            );
            textures[index] = fixture.cache->enqueue_texture(test_asset_id(200 + index), big_texture(2));
            done.fetch_add(1, std::memory_order_relaxed);
        });
        jobs.stop();
        CHECK(done.load() == COUNT);
        CHECK(fixture.cache->pending_count() == 2 * COUNT);
        CHECK(fixture.cache->mesh_count() == COUNT);
        CHECK(fixture.cache->texture_count() == COUNT);
        for (usize index = 0; index < COUNT; ++index) {
            CHECK(meshes[index].is_valid());
            CHECK(fixture.cache->find_mesh(test_asset_id(100 + index)) == meshes[index]);
            CHECK(fixture.cache->find_texture(test_asset_id(200 + index)) == textures[index]);
        }
        const upload_report report = fixture.flush();
        CHECK(report.completed == 2 * COUNT);
        for (usize index = 0; index < COUNT; ++index) {
            CHECK(fixture.cache->is_mesh_ready(meshes[index]));
            CHECK(fixture.cache->is_texture_ready(textures[index]));
        }
        for (usize index = 0; index < COUNT; ++index) {
            CHECK(
              fixture.cache->resolve_mesh(meshes[index]).bounds.max.x == doctest::Approx(static_cast<f32>(index + 1))
            );
        }
        CHECK(fixture.cache->fallbacks_used() == 0);
    }

    TEST_CASE("regression - two uploads of one id across a budget boundary keep the newest payload") {
        cache_fixture fixture(3);
        const texture_asset large = big_texture(256);
        // X (large) completes first and exhausts the budget; A-v1 is deferred; A-v2 (small) must not
        // overtake it, or the next flush would replay v1 over v2.
        static_cast<void>(fixture.cache->enqueue_texture(test_asset_id(1), large));
        const gpu_texture_id a = fixture.cache->enqueue_texture(test_asset_id(2), big_texture(64));
        const gpu_texture_id again = fixture.cache->enqueue_texture(test_asset_id(2), big_texture(2));
        CHECK(again == a);
        const upload_report first = fixture.flush(1);
        CHECK(first.completed == 1);
        CHECK(first.pending == 2);
        CHECK_FALSE(fixture.cache->is_texture_ready(a));
        const upload_report second = fixture.flush();
        CHECK(second.completed == 2);
        CHECK(second.pending == 0);
        CHECK(fixture.cache->resolve_texture(a).width == 2);
        // Nothing left: a further flush changes nothing.
        static_cast<void>(fixture.flush());
        CHECK(fixture.cache->resolve_texture(a).width == 2);
    }

    TEST_CASE("regression - a steady-state flush writes no material constant buffer") {
        cache_fixture fixture(3);
        std::vector<gpu_material_id> materials = {};
        for (u64 index = 0; index < 8; ++index) {
            material_asset value = {};
            value.desc().baseColourFactor = math::vec4(static_cast<f32>(index) / 8.0f);
            materials.push_back(fixture.cache->enqueue_material(test_asset_id(10 + index), value));
        }
        const upload_report built = fixture.flush();
        CHECK(built.completed == 8);
        CHECK(built.constantWrites == 0);   // build_material writes the constants itself
        fixture.device->reset_null_stats();
        const upload_report steady = fixture.flush();
        CHECK(steady.completed == 0);
        CHECK(steady.materialsRebound == 0);
        CHECK(steady.constantWrites == 0);
        CHECK(fixture.device->null_stats()->bufferWrites == 0);
        for (const gpu_material_id id : materials) {
            CHECK_FALSE(fixture.cache->resolve_material(id).constantsDirty);
        }
    }

    TEST_CASE("regression - releasing a texture rebinds its materials at the next flush with the graveyard delay") {
        cache_fixture fixture(3);
        material_asset value = {};
        value.desc().textures[0] = assets::asset_ref<texture_asset>(test_asset_id(9));
        const gpu_material_id material = fixture.cache->enqueue_material(test_asset_id(3), value);
        const gpu_texture_id texture = fixture.cache->enqueue_texture(test_asset_id(9), big_texture(2));
        static_cast<void>(fixture.flush());
        nvrhi::IBindingSet *bound = fixture.cache->resolve_material(material).bindingSet.Get();
        CHECK((fixture.cache->resolve_material(material).data.flags.x & 1u) != 0);
        // The arrival of the texture already buried the material's first binding set this frame.
        const usize buried = fixture.cache->graveyard_count();
        CHECK(buried == 1);
        const u64 releaseFrame = fixture.frame;
        fixture.device->reset_null_stats();
        fixture.cache->release_texture(texture, releaseFrame);
        // Nothing is created off the render thread: only the texture is buried and the material marked.
        CHECK(fixture.device->null_stats()->bindingSetsCreated == 0);
        CHECK(fixture.cache->graveyard_count() == buried + 1);
        CHECK(fixture.cache->resolve_material(material).rebindPending);
        CHECK(fixture.cache->resolve_material(material).bindingSet.Get() == bound);   // still valid this frame
        // The next flush rebinds (fallback texture, flag cleared) and buries the old set at its frame.
        const upload_report flushed = fixture.flush();
        CHECK(flushed.materialsRebound == 1);
        CHECK(flushed.constantWrites == 1);
        const gpu_material &rebound = fixture.cache->resolve_material(material);
        CHECK_FALSE(rebound.rebindPending);
        CHECK(rebound.bindingSet.Get() != bound);
        CHECK((rebound.data.flags.x & 1u) == 0);
        CHECK(fixture.cache->graveyard_count() == buried + 2);
        // Delay 3: what was buried at frame r dies at r + 4, the set buried by the flush (r + 1) at
        // r + 5; nothing dies at the very next collect as the old code did.
        fixture.cache->collect(releaseFrame + 3);
        CHECK(fixture.cache->graveyard_count() == buried + 2);
        fixture.cache->collect(releaseFrame + 4);
        CHECK(fixture.cache->graveyard_count() == 1);
        fixture.cache->collect(releaseFrame + 5);
        CHECK(fixture.cache->graveyard_count() == 0);
    }

    TEST_CASE("regression - an id reported pending still warns once when it later goes stale") {
        log_capture logs = {};
        cache_fixture fixture(3);
        const gpu_mesh_id mesh = fixture.cache->enqueue_mesh(test_asset_id(1), mesh_asset::unit_cube());
        static_cast<void>(fixture.cache->resolve_mesh(mesh));
        static_cast<void>(fixture.cache->resolve_mesh(mesh));
        CHECK(logs.count(core::log_level::DEBUG) == 1);
        CHECK(logs.count(core::log_level::WARN) == 0);
        static_cast<void>(fixture.flush());
        fixture.cache->release_mesh(mesh, fixture.frame);
        static_cast<void>(fixture.cache->resolve_mesh(mesh));
        static_cast<void>(fixture.cache->resolve_mesh(mesh));
        CHECK(logs.count(core::log_level::WARN) == 1);
        CHECK(logs.contains(core::log_level::WARN, "stale or unknown"));
        // A never-issued id warns once as well.
        gpu_mesh_id unknown = {};
        unknown.index = 77;
        unknown.generation = 5;
        static_cast<void>(fixture.cache->resolve_mesh(unknown));
        static_cast<void>(fixture.cache->resolve_mesh(unknown));
        CHECK(logs.count(core::log_level::WARN) == 2);
    }

    TEST_CASE("gpu_resource_cache - a mirror released mid-frame stays valid until collect and dies afterwards") {
        cache_fixture fixture(2);
        const gpu_mesh_id mesh = fixture.cache->enqueue_mesh(test_asset_id(1), mesh_asset::uv_sphere(1.0f, 4, 8));
        static_cast<void>(fixture.flush());
        const gpu_mesh &mirror = fixture.cache->resolve_mesh(mesh);
        nvrhi::IBuffer *stream0 = mirror.stream0.Get();
        const u32 indexCount = mirror.indexCount;
        // A job releases the id while the frame still points at the mirror (design 5).
        fixture.cache->release_mesh(mesh, fixture.frame);
        CHECK(fixture.cache->retired_count() == 1);
        CHECK(mirror.stream0.Get() == stream0);
        CHECK(mirror.indexCount == indexCount);
        // Growing the tables moves nothing the frame holds.
        for (u64 index = 0; index < 64; ++index) {
            static_cast<void>(fixture.cache->enqueue_mesh(test_asset_id(100 + index), mesh_asset::unit_cube()));
        }
        CHECK(mirror.stream0.Get() == stream0);
        CHECK(&fixture.cache->resolve_mesh(mesh) == &fixture.cache->fallback_mesh());
        fixture.cache->collect(fixture.frame);
        CHECK(fixture.cache->retired_count() == 0);
        CHECK(fixture.cache->graveyard_count() == 3);   // the three buffers wait for the delay
    }
}
