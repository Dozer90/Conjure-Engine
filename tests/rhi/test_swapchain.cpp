#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/test_environment.h"
#include "rhi_test_support.h"

#include "conjure/rhi/device.h"
#include "conjure/rhi/rhi_error.h"
#include "conjure/rhi/swapchain.h"
#include "conjure/rhi/texture_format.h"

#include "conjure/core/log.h"

#include <nvrhi/nvrhi.h>

#include <array>
#include <memory>
#include <string_view>

// swapchain.h: description validation on the null device and the real present path on the WARP
// D3D12 device ([gpu], docs/ARCHITECTURE.md 11 wave 2: "create and present a cleared swapchain").

using namespace conjure;
using namespace conjure::rhi;
using namespace conjure::testing;
using namespace conjure::testing::rhi_support;

TEST_SUITE("rhi/swapchain") {

    TEST_CASE("swapchain_desc - defaults are a 3-buffer vsync RGBA8_SRGB chain without a size") {
        const swapchain_desc desc = {};
        CHECK(desc.width == 0);
        CHECK(desc.height == 0);
        CHECK(desc.bufferCount == 3);
        CHECK(desc.format == texture_format::RGBA8_SRGB);
        CHECK(desc.vsync);
    }

    TEST_CASE("swapchain - a zero size is an invalid description") {
        log_capture logs = {};
        const std::unique_ptr<device> created = make_null_device();
        swapchain_desc desc = small_swapchain_desc();
        desc.width = 0;
        CONJURE_CHECK_ERROR(swapchain::create(*created, headless_window(), desc), rhi_error::INVALID_DESC);
        desc = small_swapchain_desc();
        desc.height = 0;
        const core::result<std::unique_ptr<swapchain>> failed = swapchain::create(*created, headless_window(), desc);
        CONJURE_CHECK_ERROR(failed, rhi_error::INVALID_DESC);
        CHECK(failed.get_error().message().find("width and height") != std::string::npos);
    }

    TEST_CASE("swapchain - the buffer count must be between 2 and 16") {
        log_capture logs = {};
        const std::unique_ptr<device> created = make_null_device();
        swapchain_desc desc = small_swapchain_desc();
        desc.bufferCount = 1;
        CONJURE_CHECK_ERROR(swapchain::create(*created, headless_window(), desc), rhi_error::INVALID_DESC);
        desc.bufferCount = 0;
        CONJURE_CHECK_ERROR(swapchain::create(*created, headless_window(), desc), rhi_error::INVALID_DESC);
        desc.bufferCount = 17;
        CONJURE_CHECK_ERROR(swapchain::create(*created, headless_window(), desc), rhi_error::INVALID_DESC);
        desc.bufferCount = 2;
        CONJURE_CHECK_OK(swapchain::create(*created, headless_window(), desc));
        desc.bufferCount = 16;
        const core::result<std::unique_ptr<swapchain>> largest = swapchain::create(*created, headless_window(), desc);
        CONJURE_REQUIRE_OK(largest);
        CHECK(largest.value()->buffer_count() == 16);
    }

    TEST_CASE("swapchain - only presentable formats are accepted") {
        log_capture logs = {};
        const std::unique_ptr<device> created = make_null_device();
        const std::array<texture_format, 4> accepted = {
            texture_format::RGBA8_UNORM,
            texture_format::RGBA8_SRGB,
            texture_format::BGRA8_SRGB,
            texture_format::RGBA16_FLOAT,
        };
        for (const texture_format format : accepted) {
            CAPTURE(to_string(format));
            swapchain_desc desc = small_swapchain_desc();
            desc.format = format;
            CONJURE_CHECK_OK(swapchain::create(*created, headless_window(), desc));
        }
        const std::array<texture_format, 6> rejected = {
            texture_format::UNKNOWN,
            texture_format::R8_UNORM,
            texture_format::D32_FLOAT,
            texture_format::RGBA32_FLOAT,
            texture_format::BC7_SRGB,
            texture_format::COUNT,
        };
        for (const texture_format format : rejected) {
            CAPTURE(to_string(format));
            swapchain_desc desc = small_swapchain_desc();
            desc.format = format;
            const core::result<std::unique_ptr<swapchain>> failed = swapchain::create(
              *created,
              headless_window(),
              desc
            );
            CONJURE_CHECK_ERROR(failed, rhi_error::INVALID_DESC);
            CHECK(failed.get_error().message().find("cannot be presented") != std::string::npos);
        }
    }

    TEST_CASE("swapchain - the description reports the created configuration") {
        log_capture logs = {};
        const std::unique_ptr<device> created = make_null_device();
        swapchain_desc desc = small_swapchain_desc(320, 200);
        desc.vsync = true;
        desc.bufferCount = 2;
        const core::result<std::unique_ptr<swapchain>> chain = swapchain::create(*created, headless_window(), desc);
        CONJURE_REQUIRE_OK(chain);
        CHECK(chain.value()->desc().width == 320);
        CHECK(chain.value()->desc().height == 200);
        CHECK(chain.value()->desc().bufferCount == 2);
        CHECK(chain.value()->desc().vsync);
        CHECK(&chain.value()->owner() == created.get());
        const std::string_view expectedLine =
          "created null swapchain 320x200 (2 buffers, RGBA8_SRGB, vsync on, window NONE)";
        CHECK(logs.contains(core::log_level::INFO, expectedLine));
    }

    TEST_CASE("swapchain - a present that observes device loss is logged, propagated and marks the device lost") {
        log_capture logs = {};
        logs.allow_errors();
        const std::unique_ptr<device> created = make_null_device();
        core::result<std::unique_ptr<swapchain>> chain = swapchain::create(
          *created,
          headless_window(),
          small_swapchain_desc()
        );
        CONJURE_REQUIRE_OK(chain);
        swapchain &target = *chain.value();
        CONJURE_CHECK_OK(target.acquire());
        CONJURE_CHECK_OK(target.present());
        CHECK(target.current_index() == 1);

        created->inject_null_fault(null_fault::PRESENT);
        CONJURE_CHECK_OK(target.acquire());
        const core::result<void> presented = target.present();

        CONJURE_CHECK_ERROR(presented, rhi_error::DEVICE_LOST);
        CHECK(created->is_lost());
        CHECK(logs.contains(core::log_level::ERR, "present failed"));
        CHECK(logs.contains(core::log_level::ERR, "injected null_fault"));
        // The failed present did not advance the image, and every later call sees the lost device.
        CHECK(target.current_index() == 1);
        CONJURE_CHECK_ERROR(target.acquire(), rhi_error::DEVICE_LOST);
        CONJURE_CHECK_ERROR(target.present(), rhi_error::DEVICE_LOST);
        CHECK(created->null_stats().value().presents == 2);
    }

    TEST_CASE("swapchain - [gpu] a headless D3D12 swapchain clears and presents every back buffer") {
        CONJURE_TEST_REQUIRE(precondition::SOFTWARE_GPU_ADAPTER);
        log_capture logs = {};
        const std::unique_ptr<device> created = make_software_device();
        core::result<std::unique_ptr<swapchain>> chain = swapchain::create(
          *created,
          headless_window(),
          small_swapchain_desc(256, 128)
        );
        CONJURE_REQUIRE_OK(chain);
        swapchain &target = *chain.value();
        CHECK(target.buffer_count() == 3);

        nvrhi::IDevice *native = created->native_device();
        nvrhi::CommandListHandle commandList = native->createCommandList();
        REQUIRE(commandList != nullptr);

        for (u32 frame = 0; frame < 6; ++frame) {
            CAPTURE(frame);
            const core::result<u32> acquired = target.acquire();
            CONJURE_REQUIRE_OK(acquired);
            CHECK(acquired.value() < target.buffer_count());
            nvrhi::ITexture *backBuffer = target.back_buffer(acquired.value());
            REQUIRE(backBuffer != nullptr);
            CHECK(backBuffer->getDesc().width == 256);
            CHECK(backBuffer->getDesc().height == 128);

            commandList->open();
            commandList->clearTextureFloat(backBuffer, nvrhi::AllSubresources, nvrhi::Color(0.1f, 0.2f, 0.3f, 1.0f));
            commandList->close();
            native->executeCommandList(commandList);

            CONJURE_CHECK_OK(target.present());
            created->run_garbage_collection();
            CHECK_FALSE(created->is_lost());
        }
        created->wait_idle();
        CHECK(logs.contains(core::log_level::INFO, "created d3d12 swapchain 256x128"));
    }

    TEST_CASE("swapchain - [gpu] a D3D12 swapchain resizes and keeps presenting") {
        CONJURE_TEST_REQUIRE(precondition::SOFTWARE_GPU_ADAPTER);
        log_capture logs = {};
        const std::unique_ptr<device> created = make_software_device();
        core::result<std::unique_ptr<swapchain>> chain = swapchain::create(
          *created,
          headless_window(),
          small_swapchain_desc(128, 128)
        );
        CONJURE_REQUIRE_OK(chain);
        swapchain &target = *chain.value();

        CONJURE_CHECK_OK(target.acquire());
        CONJURE_CHECK_OK(target.present());
        CONJURE_REQUIRE_OK(target.resize(64, 48));
        CHECK(target.desc().width == 64);
        CHECK(target.desc().height == 48);
        for (u32 index = 0; index < target.buffer_count(); ++index) {
            CAPTURE(index);
            REQUIRE(target.back_buffer(index) != nullptr);
            CHECK(target.back_buffer(index)->getDesc().width == 64);
            CHECK(target.back_buffer(index)->getDesc().height == 48);
        }
        CONJURE_CHECK_ERROR(target.resize(0, 48), rhi_error::INVALID_DESC);
        CONJURE_CHECK_OK(target.acquire());
        CONJURE_CHECK_OK(target.present());
        created->wait_idle();
        CHECK_FALSE(created->is_lost());
    }

    TEST_CASE("swapchain - [gpu] D3D12 rejects window handles it cannot present to") {
        CONJURE_TEST_REQUIRE(precondition::SOFTWARE_GPU_ADAPTER);
        log_capture logs = {};
        const std::unique_ptr<device> created = make_software_device();
        native_window nullHandle = {};
        nullHandle.kind = native_window_kind::WINDOWS;
        CONJURE_CHECK_ERROR(swapchain::create(*created, nullHandle, small_swapchain_desc()), rhi_error::INVALID_DESC);
        native_window wayland = {};
        wayland.kind = native_window_kind::WAYLAND;
        wayland.window = created.get();
        CONJURE_CHECK_ERROR(swapchain::create(*created, wayland, small_swapchain_desc()), rhi_error::INVALID_DESC);
    }
}
