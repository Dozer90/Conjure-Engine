#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "rhi_test_support.h"

#include "conjure/rhi/device.h"
#include "conjure/rhi/rhi_error.h"
#include "conjure/rhi/swapchain.h"

#include "conjure/core/log.h"

#include <nvrhi/nvrhi.h>

#include <memory>

// Device loss (docs/design/02_renderer.md 6, 10 "test_device_lost"). NVRHI reports removal as a
// Fatal message; the message callback is reached through nvrhi::IDevice::getMessageCallback(), the
// one public route to it, which is why this file includes the NVRHI header.

using namespace conjure;
using namespace conjure::rhi;
using namespace conjure::testing;
using namespace conjure::testing::rhi_support;

TEST_SUITE("rhi/device_lost") {

    TEST_CASE("device_lost - an injected Fatal message sets is_lost") {
        log_capture logs = {};
        logs.allow_errors();
        const std::unique_ptr<device> created = make_null_device();
        nvrhi::IMessageCallback *callback = created->native_device()->getMessageCallback();
        REQUIRE(callback != nullptr);

        callback->message(nvrhi::MessageSeverity::Fatal, "GPU removed by test");

        CHECK(created->is_lost());
        CHECK(logs.contains(core::log_level::ERR, "GPU removed by test"));
        CHECK(logs.contains(core::log_level::ERR, "device lost"));
    }

    TEST_CASE("device_lost - Info, Warning and Error messages are logged at their level and keep the device") {
        log_capture logs = {};
        logs.allow_errors();
        const std::unique_ptr<device> created = make_null_device();
        nvrhi::IMessageCallback *callback = created->native_device()->getMessageCallback();
        REQUIRE(callback != nullptr);

        callback->message(nvrhi::MessageSeverity::Info, "info text");
        callback->message(nvrhi::MessageSeverity::Warning, "warning text");
        callback->message(nvrhi::MessageSeverity::Error, "error text");
        callback->message(nvrhi::MessageSeverity::Warning, nullptr);

        CHECK_FALSE(created->is_lost());
        CHECK(logs.contains(core::log_level::INFO, "nvrhi: info text"));
        CHECK(logs.contains(core::log_level::WARN, "nvrhi: warning text"));
        CHECK(logs.contains(core::log_level::ERR, "nvrhi: error text"));
        CHECK(logs.count(core::log_level::WARN) == 2);
    }

    TEST_CASE("device_lost - a lost device refuses to create a swapchain") {
        log_capture logs = {};
        logs.allow_errors();
        const std::unique_ptr<device> created = make_null_device();
        created->simulate_device_loss();
        CONJURE_CHECK_ERROR(
          swapchain::create(*created, headless_window(), small_swapchain_desc()),
          rhi_error::DEVICE_LOST
        );
    }

    TEST_CASE("device_lost - acquire, present and resize fail with DEVICE_LOST once the device is lost") {
        log_capture logs = {};
        logs.allow_errors();
        const std::unique_ptr<device> created = make_null_device();
        core::result<std::unique_ptr<swapchain>> chain = swapchain::create(
          *created,
          headless_window(),
          small_swapchain_desc()
        );
        CONJURE_REQUIRE_OK(chain);
        CONJURE_CHECK_OK(chain.value()->acquire());
        CONJURE_CHECK_OK(chain.value()->present());

        created->simulate_device_loss();

        CONJURE_CHECK_ERROR(chain.value()->acquire(), rhi_error::DEVICE_LOST);
        CONJURE_CHECK_ERROR(chain.value()->present(), rhi_error::DEVICE_LOST);
        CONJURE_CHECK_ERROR(chain.value()->resize(16, 16), rhi_error::DEVICE_LOST);
        // The back buffers stay readable so the renderer can tear down without dereferencing null.
        CHECK(chain.value()->back_buffer(0) != nullptr);
        const null_device_stats stats = created->null_stats().value();
        CHECK(stats.acquires == 1);
        CHECK(stats.presents == 1);
    }

    TEST_CASE("device_lost - the loss is sticky across garbage collection and wait_idle") {
        log_capture logs = {};
        logs.allow_errors();
        const std::unique_ptr<device> created = make_null_device();
        created->simulate_device_loss();
        created->run_garbage_collection();
        created->wait_idle();
        CHECK(created->is_lost());
    }

    TEST_CASE("device_lost - destroying a lost device and its swapchain is safe") {
        log_capture logs = {};
        logs.allow_errors();
        std::unique_ptr<device> created = make_null_device();
        core::result<std::unique_ptr<swapchain>> chain = swapchain::create(
          *created,
          headless_window(),
          small_swapchain_desc()
        );
        CONJURE_REQUIRE_OK(chain);
        created->simulate_device_loss();
        chain.value().reset();
        created.reset();
        CHECK(logs.contains(core::log_level::INFO, "destroying null device"));
    }
}
