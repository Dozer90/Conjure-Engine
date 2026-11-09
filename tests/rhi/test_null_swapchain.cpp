#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/memory_guard.h"
#include "rhi_test_support.h"

#include "conjure/rhi/device.h"
#include "conjure/rhi/log_category.h"
#include "conjure/rhi/rhi_error.h"
#include "conjure/rhi/swapchain.h"
#include "conjure/rhi/texture_format.h"

#include "conjure/core/log.h"

#include <nvrhi/nvrhi.h>

#include <memory>
#include <string>

// The null swapchain (docs/design/02_renderer.md 7, 10 "test_null_swapchain"). Back-buffer
// descriptors are read through nvrhi::ITexture::getDesc(), hence the NVRHI include.

using namespace conjure;
using namespace conjure::rhi;
using namespace conjure::testing;
using namespace conjure::testing::rhi_support;

namespace {
    std::unique_ptr<swapchain> make_chain(device &target, const swapchain_desc &desc) {
        core::result<std::unique_ptr<swapchain>> chain = swapchain::create(target, headless_window(), desc);
        CONJURE_REQUIRE_OK(chain);
        return std::move(chain).value();
    }
}

TEST_SUITE("rhi/null_swapchain") {

    TEST_CASE("null_swapchain - create gives bufferCount back buffers with the requested size and format") {
        log_capture logs = {};
        const std::unique_ptr<device> created = make_null_device();
        const std::unique_ptr<swapchain> chain = make_chain(*created, small_swapchain_desc(640, 360));

        CHECK(chain->buffer_count() == 3);
        CHECK(chain->current_index() == 0);
        CHECK(chain->desc().width == 640);
        CHECK(chain->desc().height == 360);
        CHECK(chain->desc().format == texture_format::RGBA8_SRGB);
        CHECK(&chain->owner() == created.get());
        for (u32 index = 0; index < chain->buffer_count(); ++index) {
            CAPTURE(index);
            nvrhi::ITexture *buffer = chain->back_buffer(index);
            REQUIRE(buffer != nullptr);
            const nvrhi::TextureDesc &desc = buffer->getDesc();
            CHECK(desc.width == 640);
            CHECK(desc.height == 360);
            CHECK(desc.format == nvrhi::Format::SRGBA8_UNORM);
            CHECK(desc.isRenderTarget);
            CHECK(desc.initialState == nvrhi::ResourceStates::Present);
            CHECK(desc.keepInitialState);
            CHECK(desc.debugName == "swapchain buffer " + std::to_string(index));
        }
        CHECK(chain->back_buffer(3) == nullptr);
        CHECK(chain->back_buffer(0xFFFFFFFFu) == nullptr);

        const null_device_stats stats = created->null_stats().value();
        CHECK(stats.swapchainsCreated == 1);
        CHECK(stats.texturesCreated == 3);
    }

    TEST_CASE("null_swapchain - every presentable format maps to its NVRHI format") {
        log_capture logs = {};
        const std::unique_ptr<device> created = make_null_device();
        swapchain_desc desc = small_swapchain_desc();

        desc.format = texture_format::RGBA8_UNORM;
        CHECK(make_chain(*created, desc)->back_buffer(0)->getDesc().format == nvrhi::Format::RGBA8_UNORM);
        desc.format = texture_format::BGRA8_SRGB;
        CHECK(make_chain(*created, desc)->back_buffer(0)->getDesc().format == nvrhi::Format::SBGRA8_UNORM);
        desc.format = texture_format::RGBA16_FLOAT;
        CHECK(make_chain(*created, desc)->back_buffer(0)->getDesc().format == nvrhi::Format::RGBA16_FLOAT);
    }

    TEST_CASE("null_swapchain - acquire returns the current index and present cycles it") {
        log_capture logs = {};
        const std::unique_ptr<device> created = make_null_device();
        const std::unique_ptr<swapchain> chain = make_chain(*created, small_swapchain_desc());

        for (u32 frame = 0; frame < 7; ++frame) {
            CAPTURE(frame);
            const core::result<u32> acquired = chain->acquire();
            CONJURE_REQUIRE_OK(acquired);
            CHECK(acquired.value() == frame % 3);
            CHECK(chain->current_index() == frame % 3);
            CONJURE_CHECK_OK(chain->present());
        }
        CHECK(chain->current_index() == 7 % 3);

        const null_device_stats stats = created->null_stats().value();
        CHECK(stats.acquires == 7);
        CHECK(stats.presents == 7);
    }

    TEST_CASE("null_swapchain - acquiring twice without presenting returns the same index") {
        log_capture logs = {};
        const std::unique_ptr<device> created = make_null_device();
        const std::unique_ptr<swapchain> chain = make_chain(*created, small_swapchain_desc());
        CHECK(chain->acquire().value() == 0);
        CHECK(chain->acquire().value() == 0);
        CONJURE_CHECK_OK(chain->present());
        CHECK(chain->acquire().value() == 1);
    }

    TEST_CASE("null_swapchain - two buffers alternate") {
        log_capture logs = {};
        const std::unique_ptr<device> created = make_null_device();
        swapchain_desc desc = small_swapchain_desc();
        desc.bufferCount = 2;
        const std::unique_ptr<swapchain> chain = make_chain(*created, desc);
        CHECK(chain->buffer_count() == 2);
        CHECK(chain->acquire().value() == 0);
        CONJURE_CHECK_OK(chain->present());
        CHECK(chain->acquire().value() == 1);
        CONJURE_CHECK_OK(chain->present());
        CHECK(chain->acquire().value() == 0);
    }

    TEST_CASE("null_swapchain - resize to zero is rejected and changes nothing") {
        log_capture logs = {};
        const std::unique_ptr<device> created = make_null_device();
        const std::unique_ptr<swapchain> chain = make_chain(*created, small_swapchain_desc(64, 32));
        nvrhi::ITexture *before = chain->back_buffer(0);

        CONJURE_CHECK_ERROR(chain->resize(0, 32), rhi_error::INVALID_DESC);
        CONJURE_CHECK_ERROR(chain->resize(64, 0), rhi_error::INVALID_DESC);
        CONJURE_CHECK_ERROR(chain->resize(0, 0), rhi_error::INVALID_DESC);

        CHECK(chain->desc().width == 64);
        CHECK(chain->desc().height == 32);
        CHECK(chain->back_buffer(0) == before);
        CHECK(created->null_stats().value().resizes == 0);
    }

    TEST_CASE("null_swapchain - resize recreates the back buffers at the new size and restarts the cycle") {
        log_capture logs = {};
        const std::unique_ptr<device> created = make_null_device();
        const std::unique_ptr<swapchain> chain = make_chain(*created, small_swapchain_desc(64, 32));
        CONJURE_CHECK_OK(chain->acquire());
        CONJURE_CHECK_OK(chain->present());
        CHECK(chain->current_index() == 1);

        CONJURE_REQUIRE_OK(chain->resize(1920, 1080));

        CHECK(chain->desc().width == 1920);
        CHECK(chain->desc().height == 1080);
        CHECK(chain->buffer_count() == 3);
        CHECK(chain->current_index() == 0);
        for (u32 index = 0; index < chain->buffer_count(); ++index) {
            CAPTURE(index);
            REQUIRE(chain->back_buffer(index) != nullptr);
            CHECK(chain->back_buffer(index)->getDesc().width == 1920);
            CHECK(chain->back_buffer(index)->getDesc().height == 1080);
        }
        const null_device_stats stats = created->null_stats().value();
        CHECK(stats.resizes == 1);
        CHECK(stats.texturesCreated == 6);
    }

    TEST_CASE("null_swapchain - buffer_count equals the number of non-null back buffers") {
        log_capture logs = {};
        const std::unique_ptr<device> created = make_null_device();
        swapchain_desc desc = small_swapchain_desc();
        desc.bufferCount = 5;
        const std::unique_ptr<swapchain> chain = make_chain(*created, desc);

        u32 nonNull = 0;
        for (u32 index = 0; index < 16; ++index) {
            if (chain->back_buffer(index) != nullptr) {
                ++nonNull;
            }
        }
        CHECK(chain->buffer_count() == 5);
        CHECK(chain->buffer_count() == nonNull);
        CHECK(chain->buffer_count() == chain->desc().bufferCount);
        CHECK(chain->back_buffer(chain->buffer_count()) == nullptr);

        CONJURE_REQUIRE_OK(chain->resize(128, 128));
        nonNull = 0;
        for (u32 index = 0; index < 16; ++index) {
            if (chain->back_buffer(index) != nullptr) {
                ++nonNull;
            }
        }
        CHECK(chain->buffer_count() == nonNull);
    }

    TEST_CASE("null_swapchain - the window handle is ignored") {
        log_capture logs = {};
        const std::unique_ptr<device> created = make_null_device();
        native_window window = {};
        window.kind = native_window_kind::WAYLAND;
        window.display = created.get();
        CONJURE_CHECK_OK(swapchain::create(*created, window, small_swapchain_desc()));
    }

    TEST_CASE("null_swapchain - create and destroy leak nothing and wait for the device") {
        log_capture logs = {};
        const std::unique_ptr<device> created = make_null_device();
        // Captured log lines are allocations the guard would see, so the category is muted inside it.
        const core::log_level previousLevel = LOG_RHI.level();
        LOG_RHI.set_level(core::log_level::OFF);
        memory_guard guard = {};
        {
            const std::unique_ptr<swapchain> chain = make_chain(*created, small_swapchain_desc());
            CONJURE_CHECK_OK(chain->present());
        }
        guard.check();
        LOG_RHI.set_level(previousLevel);
        const null_device_stats stats = created->null_stats().value();
        CHECK(stats.waitsForIdle == 1);
        CHECK(stats.garbageCollections == 1);
    }
}
