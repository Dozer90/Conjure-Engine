#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "renderer_test_support.h"

#include "renderer/read_back_decode.h"

#include "conjure/renderer/material_asset.h"
#include "conjure/renderer/mesh_asset.h"
#include "conjure/renderer/render_scene.h"
#include "conjure/renderer/render_target.h"
#include "conjure/renderer/renderer.h"
#include "conjure/renderer/sprite_batch.h"

#include "conjure/core/image.h"
#include "conjure/core/log.h"
#include "conjure/core/math.h"
#include "conjure/core/types.h"
#include "conjure/rhi/null_device_stats.h"
#include "conjure/rhi/texture_format.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>

// render_target.h (docs/design/02_renderer.md 4.2, R12): off-screen targets created, resized and
// destroyed through the renderer, with the colour texture registered as a gpu_texture_id, and the depth
// kept by the first view of a frame into a target with depth (code sweep F384).

using namespace conjure;
using namespace conjure::renderer;
using namespace conjure::testing;
using namespace conjure::testing::renderer_support;

namespace {

    /// <summary>
    /// The sRGB encoding of a linear value, as the read-back decoders produce it.
    /// </summary>
    [[nodiscard]] u8 srgb8_of(f32 linear) noexcept {
        return encode_srgb8(linear);
    }

    /// <summary>
    /// What a frame of render_views cost: the transient textures the renderer's pool holds and the depth
    /// clears the null device recorded.
    /// </summary>
    struct frame_cost {
        u32 transientTexturesLive = 0;
        u32 depthClears = 0;
    };

    /// <summary>
    /// Renders one view per entry of 'targets' (an entry may repeat) for two frames, the first of which
    /// flushes the uploads, and returns the cost of the second. With 'withSphere' every view sees a unit
    /// sphere at the origin; shadows and SSAO stay off so the transients are the views' own buffers.
    /// </summary>
    [[nodiscard]] frame_cost render_views(
      headless_renderer &fixture,
      std::initializer_list<render_target_id> targets,
      bool withSphere
    ) {
        render_scene scene = {};
        for (const render_target_id target : targets) {
            scene.views.push_back(look_at_view(math::vec3(0.0f, 0.0f, 5.0f), math::vec3(0.0f), target));
        }
        scene.shadows.enabled = false;
        scene.ssao.enabled = false;
        if (withSphere) {
            const mesh_asset sphereMesh = mesh_asset::uv_sphere(1.0f);
            const gpu_mesh_id sphere = fixture->upload_mesh(test_asset_id(1), sphereMesh);
            const gpu_material_id material = fixture->upload_material(test_asset_id(2), material_asset());
            scene.meshes.push_back(instance_at(sphere, material, math::vec3(0.0f), sphereMesh.bounds()));
        }
        CONJURE_REQUIRE_OK(fixture->render(scene));
        fixture.device->reset_null_stats();
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().viewsRendered == targets.size());
        CHECK(fixture->stats().executed("depth_prepass") == withSphere);
        const std::optional<rhi::null_device_stats> nullStats = fixture.device->null_stats();
        REQUIRE(nullStats.has_value());
        frame_cost cost = {};
        cost.transientTexturesLive = fixture->stats().transientTexturesLive;
        cost.depthClears = nullStats->depthClears;
        return cost;
    }
}

TEST_SUITE("renderer/render_target") {

    TEST_CASE("read_back - the 8-bit texel decoders copy or swizzle and unknown formats decode to nothing") {
        const std::array<std::byte, 4> bytes = {std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
        const core::rgba8 rgba = decode_read_back_texel(rhi::texture_format::RGBA8_SRGB, bytes.data());
        CHECK(rgba == core::rgba8{1, 2, 3, 4});
        CHECK(decode_read_back_texel(rhi::texture_format::RGBA8_UNORM, bytes.data()) == rgba);
        const core::rgba8 bgra = decode_read_back_texel(rhi::texture_format::BGRA8_SRGB, bytes.data());
        CHECK(bgra == core::rgba8{3, 2, 1, 4});
        CHECK(decode_read_back_texel(rhi::texture_format::R8_UNORM, bytes.data()) == core::rgba8{});
    }

    TEST_CASE("read_back - RGBA16_FLOAT texels are sRGB-encoded with the alpha clamped") {
        const std::array<u16, 4> halves = {pack_half(1.0f), pack_half(0.5f), pack_half(0.0f), pack_half(0.25f)};
        std::array<std::byte, 8> bytes = {};
        std::memcpy(bytes.data(), halves.data(), bytes.size());
        const core::rgba8 pixel = decode_read_back_texel(rhi::texture_format::RGBA16_FLOAT, bytes.data());
        CHECK(pixel.r == 255);
        CHECK(pixel.g == srgb8_of(0.5f));
        CHECK(pixel.g == 188);   // the familiar mid-grey of linear 0.5
        CHECK(pixel.b == 0);
        CHECK(pixel.a == 64);
        // Values above one clamp, negative ones floor at zero, NaN decodes to zero.
        const std::array<u16, 4> wild = {pack_half(4.0f), pack_half(-1.0f), u16{0x7E00}, pack_half(2.0f)};
        std::memcpy(bytes.data(), wild.data(), bytes.size());
        const core::rgba8 clamped = decode_read_back_texel(rhi::texture_format::RGBA16_FLOAT, bytes.data());
        CHECK(clamped.r == 255);
        CHECK(clamped.g == 0);
        CHECK(clamped.b == 0);
        CHECK(clamped.a == 255);
    }

    TEST_CASE("read_back - R11G11B10_FLOAT channels decode their biased exponents and denormals") {
        CHECK(decode_r11g11b10_channel(0, 6) == 0.0f);
        CHECK(decode_r11g11b10_channel(15u << 6, 6) == doctest::Approx(1.0f));
        CHECK(decode_r11g11b10_channel((14u << 6) | 32u, 6) == doctest::Approx(0.75f));   // 2^-1 * 1.5
        CHECK(decode_r11g11b10_channel(16u << 5, 5) == doctest::Approx(2.0f));
        CHECK(decode_r11g11b10_channel(1, 6) == doctest::Approx(std::ldexp(1.0f, -20)));   // smallest denormal
        CHECK(decode_r11g11b10_channel(1, 5) == doctest::Approx(std::ldexp(1.0f, -19)));
        CHECK(std::isinf(decode_r11g11b10_channel(31u << 6, 6)));
        CHECK(std::isnan(decode_r11g11b10_channel((31u << 6) | 1u, 6)));
        // Packed: red 1.0, green 0.5, blue 2.0 (clamps to white), alpha always opaque.
        const u32 packed = (15u << 6) | ((14u << 6) << 11) | ((16u << 5) << 22);
        std::array<std::byte, 4> bytes = {};
        std::memcpy(bytes.data(), &packed, sizeof(packed));
        const core::rgba8 pixel = decode_read_back_texel(rhi::texture_format::R11G11B10_FLOAT, bytes.data());
        CHECK(pixel.r == 255);
        CHECK(pixel.g == srgb8_of(0.5f));
        CHECK(pixel.b == 255);
        CHECK(pixel.a == 255);
        CHECK(encode_srgb8(std::numeric_limits<f32>::quiet_NaN()) == 0);
        CHECK(encode_srgb8(std::numeric_limits<f32>::infinity()) == 255);
    }

    TEST_CASE("render_target_desc - defaults are an sRGB target with depth and compares by value") {
        const render_target_desc desc = {};
        CHECK(desc.size == math::uvec2(0, 0));
        CHECK(desc.colourFormat == rhi::texture_format::RGBA8_SRGB);
        CHECK(desc.withDepth);
        render_target_desc other = desc;
        other.size = math::uvec2(4, 4);
        CHECK_FALSE(other == desc);
        CHECK(other == other);
    }

    TEST_CASE("is_render_target_colour_format - accepts the five documented formats only") {
        CHECK(is_render_target_colour_format(rhi::texture_format::RGBA8_UNORM));
        CHECK(is_render_target_colour_format(rhi::texture_format::RGBA8_SRGB));
        CHECK(is_render_target_colour_format(rhi::texture_format::BGRA8_SRGB));
        CHECK(is_render_target_colour_format(rhi::texture_format::RGBA16_FLOAT));
        CHECK(is_render_target_colour_format(rhi::texture_format::R11G11B10_FLOAT));
        CHECK_FALSE(is_render_target_colour_format(rhi::texture_format::UNKNOWN));
        CHECK_FALSE(is_render_target_colour_format(rhi::texture_format::R8_UNORM));
        CHECK_FALSE(is_render_target_colour_format(rhi::texture_format::D32_FLOAT));
        CHECK_FALSE(is_render_target_colour_format(rhi::texture_format::BC7_SRGB));
        CHECK_FALSE(is_render_target_colour_format(rhi::texture_format::RGBA32_FLOAT));
        CHECK_FALSE(is_render_target_colour_format(rhi::texture_format::COUNT));
    }

    TEST_CASE("render_target - a fresh target reports its description, a registered colour texture and no frames") {
        headless_renderer fixture = make_headless_renderer();
        render_target_desc desc = {};
        desc.size = math::uvec2(64, 48);
        desc.colourFormat = rhi::texture_format::RGBA16_FLOAT;
        core::result<render_target_id> created = fixture->create_render_target(desc);
        CONJURE_REQUIRE_OK(created);
        CHECK(created.value().is_valid());
        render_target *target = fixture->get_render_target(created.value());
        REQUIRE(target != nullptr);
        CHECK(target->desc() == desc);
        CHECK(target->size() == math::uvec2(64, 48));
        CHECK(target->colour_texture().is_valid());
        CHECK(target->has_depth());
        CHECK(target->frames_rendered() == 0);

        render_target_desc noDepth = {};
        noDepth.size = math::uvec2(8, 8);
        noDepth.withDepth = false;
        core::result<render_target_id> second = fixture->create_render_target(noDepth);
        CONJURE_REQUIRE_OK(second);
        CHECK(second.value() != created.value());
        render_target *secondTarget = fixture->get_render_target(second.value());
        REQUIRE(secondTarget != nullptr);
        CHECK_FALSE(secondTarget->has_depth());
        CHECK(secondTarget->colour_texture() != target->colour_texture());
        CHECK(fixture->get_render_target(render_target_id()) == nullptr);
    }

    TEST_CASE("render_target - frames_rendered counts the frames with a view into the target") {
        headless_renderer fixture = make_headless_renderer();
        render_target_desc desc = {};
        desc.size = math::uvec2(32, 32);
        core::result<render_target_id> created = fixture->create_render_target(desc);
        CONJURE_REQUIRE_OK(created);
        render_scene scene = {};
        scene.views.push_back(look_at_view(math::vec3(0.0f, 0.0f, 5.0f), math::vec3(0.0f), created.value()));
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->get_render_target(created.value())->frames_rendered() == 2);
        // A frame without a view into the target does not count.
        CONJURE_REQUIRE_OK(fixture->render(render_scene()));
        CHECK(fixture->get_render_target(created.value())->frames_rendered() == 2);
    }

    TEST_CASE("render_target - resize keeps the ids, replaces the textures and resets the frame count") {
        headless_renderer fixture = make_headless_renderer();
        render_target_desc desc = {};
        desc.size = math::uvec2(16, 16);
        core::result<render_target_id> created = fixture->create_render_target(desc);
        CONJURE_REQUIRE_OK(created);
        render_target *target = fixture->get_render_target(created.value());
        REQUIRE(target != nullptr);
        const gpu_texture_id before = target->colour_texture();
        render_scene scene = {};
        scene.views.push_back(look_at_view(math::vec3(0.0f, 0.0f, 5.0f), math::vec3(0.0f), created.value()));
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(target->frames_rendered() == 1);

        CONJURE_REQUIRE_OK(fixture->resize_render_target(created.value(), math::uvec2(40, 20)));
        CHECK(fixture->get_render_target(created.value()) == target);
        CHECK(target->size() == math::uvec2(40, 20));
        CHECK(target->desc().colourFormat == desc.colourFormat);
        CHECK(target->has_depth());
        CHECK(target->colour_texture().is_valid());
        CHECK(target->colour_texture() == before);
        CHECK(target->frames_rendered() == 0);
        // Rendering into the resized target works and the read-back has the new size.
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(target->frames_rendered() == 1);
        core::result<core::image> image = fixture->read_back(created.value());
        CONJURE_REQUIRE_OK(image);
        CHECK(image.value().width() == 40);
        CHECK(image.value().height() == 20);

        CONJURE_CHECK_ERROR(
          fixture->resize_render_target(created.value(), math::uvec2(0, 20)),
          core::error_code::INVALID_ARGUMENT
        );
        CHECK(target->size() == math::uvec2(40, 20));
        CONJURE_CHECK_ERROR(
          fixture->resize_render_target(render_target_id(), math::uvec2(4, 4)),
          core::error_code::NOT_FOUND
        );
    }

    TEST_CASE("regression - sweep-2026-11-12 F385 a sprite showing a target's colour texture survives its resize") {
        // resize_render_target used to retire the colour id and adopt the replacement's, so a handle taken
        // before the resize (an ImGui image of the editor viewport, a sprite) resolved to a stale id and
        // drew the fallback. The id now stays and follows the new texture.
        log_capture logs = {};
        headless_renderer fixture = make_headless_renderer();
        render_target_desc desc = {};
        desc.size = math::uvec2(16, 16);
        core::result<render_target_id> shown = fixture->create_render_target(desc);
        core::result<render_target_id> screen = fixture->create_render_target(desc);
        CONJURE_REQUIRE_OK(shown);
        CONJURE_REQUIRE_OK(screen);
        const gpu_texture_id handle = fixture->get_render_target(shown.value())->colour_texture();
        render_scene scene = {};
        scene.views.push_back(look_at_view(math::vec3(0.0f, 0.0f, 5.0f), math::vec3(0.0f), screen.value()));
        sprite image = {};
        image.texture = handle;
        image.space = sprite_space::SCREEN;
        image.size = math::vec2(8.0f);
        scene.sprites.push_back(image);
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().spritesDrawn == 1);
        CHECK(fixture->stats().fallbacksUsed == 0);

        CONJURE_REQUIRE_OK(fixture->resize_render_target(shown.value(), math::uvec2(48, 32)));
        CHECK(fixture->get_render_target(shown.value())->colour_texture() == handle);
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().spritesDrawn == 1);
        CHECK(fixture->stats().fallbacksUsed == 0);
        CHECK_FALSE(logs.contains(core::log_level::WARN, "stale or unknown"));

        // Destroying the target retires the id as before.
        fixture->destroy_render_target(shown.value());
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().fallbacksUsed >= 1);
    }

    TEST_CASE("render_target - destroy forgets the target and views into it are skipped") {
        headless_renderer fixture = make_headless_renderer();
        render_target_desc desc = {};
        desc.size = math::uvec2(16, 16);
        core::result<render_target_id> created = fixture->create_render_target(desc);
        CONJURE_REQUIRE_OK(created);
        fixture->destroy_render_target(created.value());
        CHECK(fixture->get_render_target(created.value()) == nullptr);
        CONJURE_CHECK_ERROR(fixture->read_back(created.value()), core::error_code::NOT_FOUND);
        fixture->destroy_render_target(created.value());   // twice is harmless
        fixture->destroy_render_target(render_target_id());
        render_scene scene = {};
        scene.views.push_back(look_at_view(math::vec3(0.0f, 0.0f, 5.0f), math::vec3(0.0f), created.value()));
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().viewsSkipped == 1);
        CHECK(fixture->stats().viewsRendered == 0);
        // A new target after the destroy gets a fresh id with a different generation or index.
        core::result<render_target_id> again = fixture->create_render_target(desc);
        CONJURE_REQUIRE_OK(again);
        CHECK(again.value() != created.value());
        CHECK(fixture->get_render_target(again.value()) != nullptr);
    }

    TEST_CASE("regression - code sweep F384 a view into a target with depth draws into the target's depth") {
        // The target's depth texture used to be allocated and never bound: every view drew into a transient
        // depth buffer of the pool, and the scene depth died with the frame. The view now imports the
        // target's depth in place of the transient, so the pool holds one texture fewer.
        log_capture logs = {};
        headless_renderer owning = make_headless_renderer();
        headless_renderer plain = make_headless_renderer();
        const render_target_id withDepth = make_target(*owning, math::uvec2(64, 48), true);
        const render_target_id withoutDepth = make_target(*plain, math::uvec2(64, 48), false);
        const frame_cost owned = render_views(owning, {withDepth}, true);
        const frame_cost transient = render_views(plain, {withoutDepth}, true);
        CHECK(transient.transientTexturesLive > 1);
        CHECK(owned.transientTexturesLive + 1 == transient.transientTexturesLive);
        // The prepass clears the depth it binds first, imported or not, and nothing clears it twice.
        CHECK(owned.depthClears == 1);
        CHECK(transient.depthClears == 1);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("render_target - only the first view of a frame into a target owns its depth") {
        log_capture logs = {};
        headless_renderer owning = make_headless_renderer();
        headless_renderer plain = make_headless_renderer();
        const math::uvec2 size(32, 32);
        const render_target_id first = make_target(*owning, size, true);
        const render_target_id second = make_target(*owning, size, true);
        const render_target_id firstPlain = make_target(*plain, size, false);
        const render_target_id secondPlain = make_target(*plain, size, false);

        SUBCASE("two views into one target: the second renders with a transient depth buffer") {
            const frame_cost owned = render_views(owning, {first, first}, true);
            const frame_cost transient = render_views(plain, {firstPlain, firstPlain}, true);
            CHECK(owned.transientTexturesLive + 1 == transient.transientTexturesLive);
            CHECK(owned.depthClears == 2);
            CHECK(transient.depthClears == 2);
        }
        SUBCASE("two views into two targets: each view owns the depth of its own target") {
            const frame_cost owned = render_views(owning, {first, second}, true);
            const frame_cost transient = render_views(plain, {firstPlain, secondPlain}, true);
            CHECK(owned.transientTexturesLive + 2 == transient.transientTexturesLive);
        }
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("regression - code sweep F384 a frame that draws nothing still clears the target's depth") {
        // With nothing to draw no pass binds a depth buffer. The target's depth would keep an earlier
        // frame's scene, so the owning view clears it to the far plane itself; a transient needs no clear,
        // and a later view of the frame into the same target leaves the owner's depth alone.
        log_capture logs = {};
        headless_renderer owning = make_headless_renderer();
        headless_renderer plain = make_headless_renderer();
        const render_target_id withDepth = make_target(*owning, math::uvec2(16, 16), true);
        const render_target_id withoutDepth = make_target(*plain, math::uvec2(16, 16), false);

        SUBCASE("one view") {
            CHECK(render_views(owning, {withDepth}, false).depthClears == 1);
            CHECK(render_views(plain, {withoutDepth}, false).depthClears == 0);
        }
        SUBCASE("two views into the same target") {
            CHECK(render_views(owning, {withDepth, withDepth}, false).depthClears == 1);
            CHECK(render_views(plain, {withoutDepth, withoutDepth}, false).depthClears == 0);
        }
        CHECK(logs.count(core::log_level::ERR) == 0);
    }
}
