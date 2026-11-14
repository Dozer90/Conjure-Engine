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

    TEST_CASE("regression - code sweep F288 [gpu] after a failed D3D12 resize acquire and present report OUT_OF_DATE") {
        CONJURE_TEST_REQUIRE(precondition::SOFTWARE_GPU_ADAPTER);
        log_capture logs = {};
        // When the executable runs as one process, an earlier case has switched the D3D12 debug layer on for
        // good, and the layer reports the refusal provoked below as an ERROR of its own.
        logs.allow_errors();
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

        // 32768 is twice the D3D12 texture limit, so ResizeBuffers refuses it; the REQUIRE stops the case
        // loudly should WARP ever accept the size.
        const core::result<void> refused = target.resize(32768, 32768);
        CONJURE_CHECK_ERROR(refused, rhi_error::SWAPCHAIN_CREATION_FAILED);
        REQUIRE_FALSE(refused.has_value());
        CHECK(target.buffer_count() == 0);
        CHECK(target.back_buffer(0) == nullptr);
        CONJURE_CHECK_ERROR(target.acquire(), rhi_error::OUT_OF_DATE);
        CONJURE_CHECK_ERROR(target.present(), rhi_error::OUT_OF_DATE);
        CHECK_FALSE(created->is_lost());

        // The next successful resize makes the chain usable again.
        CONJURE_REQUIRE_OK(target.resize(64, 48));
        CHECK(target.buffer_count() == 3);
        REQUIRE(target.back_buffer(2) != nullptr);
        CHECK(target.back_buffer(2)->getDesc().width == 64);
        CONJURE_CHECK_OK(target.acquire());
        CONJURE_CHECK_OK(target.present());
        created->wait_idle();
        CHECK_FALSE(created->is_lost());
    }

    TEST_CASE(
      "regression - code sweep F011 [gpu] a D3D12 swapchain under the debug layer presents, resizes and dies cleanly"
    ) {
        CONJURE_TEST_REQUIRE(precondition::SOFTWARE_GPU_ADAPTER);
        // No allow_errors(): the debug layer's messages are routed to the log, so an error it raises (and the
        // 0x87D break it follows OBJECT_DELETED_WHILE_STILL_IN_USE with) fails the case.
        log_capture logs = {};
        std::unique_ptr<device> created = make_software_device(true);
        core::result<std::unique_ptr<swapchain>> chain = swapchain::create(
          *created,
          headless_window(),
          small_swapchain_desc(256, 128)
        );
        CONJURE_REQUIRE_OK(chain);
        swapchain &target = *chain.value();
        nvrhi::IDevice *native = created->native_device();
        nvrhi::CommandListHandle commandList = native->createCommandList();
        REQUIRE(commandList != nullptr);
        const nvrhi::Color clearColour(0.3f, 0.2f, 0.1f, 1.0f);
        const auto presentFrames = [&](u32 frameCount) {
            for (u32 frame = 0; frame < frameCount; ++frame) {
                CAPTURE(frame);
                const core::result<u32> acquired = target.acquire();
                CONJURE_REQUIRE_OK(acquired);
                nvrhi::ITexture *backBuffer = target.back_buffer(acquired.value());
                REQUIRE(backBuffer != nullptr);
                commandList->open();
                commandList->clearTextureFloat(backBuffer, nvrhi::AllSubresources, clearColour);
                commandList->close();
                native->executeCommandList(commandList);
                CONJURE_CHECK_OK(target.present());
            }
        };

        presentFrames(6);
        // Straight after a present, with no wait_idle from the caller: the resize waits for the present itself.
        CONJURE_REQUIRE_OK(target.resize(64, 48));
        CHECK(target.back_buffer(0)->getDesc().width == 64);
        presentFrames(2);
        // The swapchain dies with its last present still queued, as it does when an application quits.
        commandList = nullptr;
        chain.value().reset();
        CHECK_FALSE(created->is_lost());
        created.reset();
        // Warnings are not counted: a machine without the optional "Graphics Tools" feature warns that the
        // layer is unavailable and runs the same sequence without it.
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("regression - code sweep F011 [gpu] a device after a debug-layer device reports the inherited layer") {
        CONJURE_TEST_REQUIRE(precondition::SOFTWARE_GPU_ADAPTER);
        log_capture logs = {};
        {
            const std::unique_ptr<device> first = make_software_device(true);
            CHECK(logs.contains(core::log_level::INFO, "debug layer on)"));
        }
        // The layer is an optional Windows feature ("Graphics Tools"); where it is missing nothing is inherited.
        const bool layerInstalled = !logs.contains(core::log_level::WARN, "debug layer requested but unavailable");
        const std::unique_ptr<device> second = make_software_device(false);
        const std::string_view inherited = "debug layer on (enabled by an earlier device of this process)";
        CHECK(logs.contains(core::log_level::INFO, inherited) == layerInstalled);
        CHECK(logs.contains(core::log_level::INFO, "debug layer off)") == !layerInstalled);
    }

    TEST_CASE("regression - code sweep F044 a null swapchain refuses to resize while a back buffer is referenced") {
        log_capture logs = {};
        const std::unique_ptr<device> created = make_null_device();
        core::result<std::unique_ptr<swapchain>> chain = swapchain::create(
          *created,
          headless_window(),
          small_swapchain_desc(64, 32)
        );
        CONJURE_REQUIRE_OK(chain);
        swapchain &target = *chain.value();

        // What a renderer's framebuffer cache does: a framebuffer keeps its attachments alive.
        nvrhi::FramebufferDesc framebufferDesc = {};
        framebufferDesc.addColorAttachment(target.back_buffer(1));
        nvrhi::FramebufferHandle framebuffer = created->native_device()->createFramebuffer(framebufferDesc);
        REQUIRE(framebuffer != nullptr);

        const core::result<void> refused = target.resize(128, 64);
        CONJURE_CHECK_ERROR(refused, rhi_error::SWAPCHAIN_CREATION_FAILED);
        REQUIRE_FALSE(refused.has_value());
        CHECK(refused.get_error().message().find("back buffer 1 is still referenced 1 time(s)") != std::string::npos);
        // Refused before anything changed: the chain is the one it was.
        CHECK(target.desc().width == 64);
        CHECK(target.buffer_count() == 3);
        CHECK(created->null_stats().value().resizes == 0);
        CONJURE_CHECK_OK(target.acquire());
        CONJURE_CHECK_OK(target.present());

        framebuffer = nullptr;
        CONJURE_REQUIRE_OK(target.resize(128, 64));
        CHECK(target.desc().width == 128);
        CHECK(target.back_buffer(1)->getDesc().height == 64);
        CHECK(created->null_stats().value().resizes == 1);
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
