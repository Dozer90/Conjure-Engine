#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/memory_guard.h"
#include "conjure/testing/test_environment.h"
#include "rhi_test_support.h"

#include "conjure/rhi/backend.h"
#include "conjure/rhi/device.h"
#include "conjure/rhi/log_category.h"
#include "conjure/rhi/rhi_error.h"

#include "conjure/core/error.h"
#include "conjure/core/log.h"

#include <array>
#include <memory>
#include <optional>

using namespace conjure;
using namespace conjure::rhi;
using namespace conjure::testing;
using namespace conjure::testing::rhi_support;

TEST_SUITE("rhi/device") {

    TEST_CASE("device - create builds a null device with the documented capabilities") {
        log_capture logs = {};
        const std::unique_ptr<device> created = make_null_device();

        CHECK(created->backend() == graphics_backend::NULL_DEVICE);
        CHECK(created->desc().backend == graphics_backend::NULL_DEVICE);
        CHECK_FALSE(created->desc().enableDebugLayer);
        CHECK(created->native_device() != nullptr);
        CHECK_FALSE(created->is_lost());

        const device_capabilities &capabilities = created->capabilities();
        CHECK(capabilities.backend == graphics_backend::NULL_DEVICE);
        CHECK(capabilities.adapterName == "Conjure Null Device");
        CHECK(capabilities.maxTextureDimension2d == 16384);
        CHECK(capabilities.dedicatedVideoMemoryBytes == 0);
        CHECK(capabilities.computeQueue);
        CHECK(capabilities.timerQueries);
        CHECK(capabilities.softwareAdapter);
        CHECK(logs.contains(core::log_level::INFO, "created null device on 'Conjure Null Device'"));
    }

    TEST_CASE("device - create and destroy leak nothing") {
        log_capture logs = {};
        // Captured log lines are allocations the guard would see, so the category is muted inside it.
        const core::log_level previousLevel = LOG_RHI.level();
        LOG_RHI.set_level(core::log_level::OFF);
        memory_guard guard = {};
        for (int iteration = 0; iteration < 8; ++iteration) {
            const std::unique_ptr<device> created = make_null_device();
            CHECK(created->native_device() != nullptr);
        }
        guard.check();
        LOG_RHI.set_level(previousLevel);
        CHECK(logs.count() == 0);
    }

    TEST_CASE("device - the description is kept verbatim") {
        log_capture logs = {};
        device_desc desc = null_device_desc();
        desc.adapterIndex = 3;
        desc.preferSoftwareAdapter = true;
        desc.window.kind = native_window_kind::X11;
        desc.window.x11Window = 77;
        const core::result<std::unique_ptr<device>> created = device::create(desc);
        CONJURE_REQUIRE_OK(created);
        CHECK(created.value()->desc().adapterIndex == 3);
        CHECK(created.value()->desc().preferSoftwareAdapter);
        CHECK(created.value()->desc().window.kind == native_window_kind::X11);
        CHECK(created.value()->desc().window.x11Window == 77);
    }

    TEST_CASE("device - enableDebugLayer wraps the device in the NVRHI validation layer") {
        log_capture logs = {};
        const std::unique_ptr<device> created = make_null_device(true);
        CHECK(created->desc().enableDebugLayer);
        CHECK(created->native_device() != nullptr);
        CHECK(logs.contains(core::log_level::INFO, "debug layer on"));
        // The validation layer and the raw null device both answer; the stats still reach the null device.
        created->run_garbage_collection();
        const std::optional<null_device_stats> stats = created->null_stats();
        REQUIRE(stats.has_value());
        CHECK(stats->garbageCollections == 1);
    }

    TEST_CASE("device - enableGpuValidation without the debug layer is an invalid description") {
        device_desc desc = null_device_desc();
        desc.enableGpuValidation = true;
        const core::result<std::unique_ptr<device>> created = device::create(desc);
        CONJURE_CHECK_ERROR(created, rhi_error::INVALID_DESC);
        CHECK(created.get_error().message().find("enableGpuValidation") != std::string::npos);
    }

    TEST_CASE("device - create reports UNSUPPORTED exactly for the backends that are not compiled in") {
        // Every optional backend is exercised in every build: one that is compiled out must answer
        // UNSUPPORTED, one that is compiled in must never answer UNSUPPORTED. The compiled case asks
        // for an adapter index no machine has, so it fails fast with DEVICE_CREATION_FAILED (adapter
        // enumeration only, no GPU needed) or, on a machine that somehow has that many, succeeds.
        log_capture logs = {};
        const std::array<graphics_backend, 2> optional = {graphics_backend::D3D12, graphics_backend::VULKAN};
        for (const graphics_backend backend : optional) {
            CAPTURE(to_string(backend));
            device_desc desc = {};
            desc.backend = backend;
            desc.adapterIndex = 0xFFFFFFF0u;
            const core::result<std::unique_ptr<device>> created = device::create(desc);
            if (is_backend_compiled(backend)) {
                CHECK_FALSE(has_error(created, core::error_code::UNSUPPORTED));
                CHECK((created.has_value() || has_error(created, rhi_error::DEVICE_CREATION_FAILED)));
            } else {
                CONJURE_CHECK_ERROR(created, core::error_code::UNSUPPORTED);
                CHECK(created.get_error().message().find("not compiled") != std::string::npos);
            }
        }
    }

    TEST_CASE("device - an out-of-range backend is an invalid description") {
        device_desc desc = {};
        desc.backend = graphics_backend::COUNT;
        const core::result<std::unique_ptr<device>> created = device::create(desc);
        CONJURE_CHECK_ERROR(created, rhi_error::INVALID_DESC);
        CHECK(created.get_error().message().find("out of range") != std::string::npos);
        desc.backend = static_cast<graphics_backend>(200);
        CONJURE_CHECK_ERROR(device::create(desc), rhi_error::INVALID_DESC);
    }

    TEST_CASE("regression - review F6 an out-of-range backend is not reported as an uncompiled null device") {
        device_desc desc = {};
        desc.backend = graphics_backend::COUNT;
        const core::result<std::unique_ptr<device>> created = device::create(desc);
        REQUIRE_FALSE(created.has_value());
        CHECK_FALSE(created.get_error().is(core::error_code::UNSUPPORTED));
        CHECK(created.get_error().message().find("'null'") == std::string::npos);
    }

    TEST_CASE("device - run_garbage_collection and wait_idle reach the backend") {
        log_capture logs = {};
        const std::unique_ptr<device> created = make_null_device();
        created->run_garbage_collection();
        created->run_garbage_collection();
        created->wait_idle();
        const std::optional<null_device_stats> stats = created->null_stats();
        REQUIRE(stats.has_value());
        CHECK(stats->garbageCollections == 2);
        CHECK(stats->waitsForIdle == 1);
        CHECK_FALSE(created->is_lost());
    }

    TEST_CASE("device - null_stats snapshots and reset_null_stats clears") {
        log_capture logs = {};
        const std::unique_ptr<device> created = make_null_device();
        created->run_garbage_collection();
        const std::optional<null_device_stats> before = created->null_stats();
        REQUIRE(before.has_value());
        CHECK(before->garbageCollections == 1);

        created->reset_null_stats();
        const std::optional<null_device_stats> after = created->null_stats();
        REQUIRE(after.has_value());
        CHECK(after->garbageCollections == 0);
        // The earlier snapshot is a copy and does not change.
        CHECK(before->garbageCollections == 1);
    }

    TEST_CASE("device - simulate_device_loss marks the device lost and logs an error") {
        log_capture logs = {};
        logs.allow_errors();
        const std::unique_ptr<device> created = make_null_device();
        CHECK_FALSE(created->is_lost());
        created->simulate_device_loss();
        CHECK(created->is_lost());
        CHECK(logs.contains(core::log_level::ERR, "device lost"));
    }

    TEST_CASE("device - a failed waitForIdle marks the device lost and logs an error") {
        log_capture logs = {};
        logs.allow_errors();
        const std::unique_ptr<device> created = make_null_device();
        created->inject_null_fault(null_fault::WAIT_IDLE);
        CHECK_FALSE(created->is_lost());

        created->wait_idle();

        CHECK(created->is_lost());
        CHECK(logs.contains(core::log_level::ERR, "waitForIdle failed on the null device"));
        const std::optional<null_device_stats> stats = created->null_stats();
        REQUIRE(stats.has_value());
        CHECK(stats->waitsForIdle == 1);
    }

    TEST_CASE("device - an injected null fault fires once and NONE disarms it") {
        log_capture logs = {};
        const std::unique_ptr<device> created = make_null_device();

        // Armed then disarmed: the wait succeeds and the device stays healthy.
        created->inject_null_fault(null_fault::WAIT_IDLE);
        created->inject_null_fault(null_fault::NONE);
        created->wait_idle();
        CHECK_FALSE(created->is_lost());

        // A PRESENT fault does not affect wait_idle; only the matching call consumes it.
        created->inject_null_fault(null_fault::PRESENT);
        created->wait_idle();
        CHECK_FALSE(created->is_lost());
        created->inject_null_fault(null_fault::NONE);
        CHECK(created->null_stats().value().waitsForIdle == 2);
    }

    TEST_CASE("device - [gpu] the software D3D12 adapter creates a device") {
        CONJURE_TEST_REQUIRE(precondition::SOFTWARE_GPU_ADAPTER);
        log_capture logs = {};
        const std::unique_ptr<device> created = make_software_device();

        CHECK(created->backend() == graphics_backend::D3D12);
        CHECK(created->native_device() != nullptr);
        CHECK_FALSE(created->is_lost());
        const device_capabilities &capabilities = created->capabilities();
        CHECK(capabilities.backend == graphics_backend::D3D12);
        CHECK(capabilities.softwareAdapter);
        CHECK_FALSE(capabilities.adapterName.empty());
        CHECK(capabilities.maxTextureDimension2d == 16384);
        CHECK(capabilities.computeQueue);
        CHECK(capabilities.timerQueries);
        CHECK_FALSE(created->null_stats().has_value());

        created->run_garbage_collection();
        created->wait_idle();
        CHECK_FALSE(created->is_lost());
        CHECK(logs.contains(core::log_level::INFO, "created d3d12 device"));
    }

    TEST_CASE("device - [gpu] the D3D12 debug layer is accepted when requested") {
        CONJURE_TEST_REQUIRE(precondition::SOFTWARE_GPU_ADAPTER);
        log_capture logs = {};
        const std::unique_ptr<device> created = make_software_device(true);
        CHECK(created->desc().enableDebugLayer);
        CHECK_FALSE(created->is_lost());
        created->wait_idle();
    }

    TEST_CASE("device - [gpu] simulate_device_loss works on a real backend too") {
        CONJURE_TEST_REQUIRE(precondition::SOFTWARE_GPU_ADAPTER);
        log_capture logs = {};
        logs.allow_errors();
        const std::unique_ptr<device> created = make_software_device();
        created->simulate_device_loss();
        CHECK(created->is_lost());
    }

    TEST_CASE("device - [gpu] inject_null_fault has no effect on a real backend") {
        CONJURE_TEST_REQUIRE(precondition::SOFTWARE_GPU_ADAPTER);
        log_capture logs = {};
        const std::unique_ptr<device> created = make_software_device();
        created->inject_null_fault(null_fault::WAIT_IDLE);
        created->wait_idle();
        CHECK_FALSE(created->is_lost());
    }

    TEST_CASE("device - [gpu] a hardware adapter index past the last adapter fails") {
        CONJURE_TEST_REQUIRE(precondition::SOFTWARE_GPU_ADAPTER);
        log_capture logs = {};
        device_desc desc = {};
        desc.backend = graphics_backend::D3D12;
        desc.adapterIndex = 1000;
        const core::result<std::unique_ptr<device>> created = device::create(desc);
        CONJURE_CHECK_ERROR(created, rhi_error::DEVICE_CREATION_FAILED);
        CHECK(created.get_error().message().find("index 1000") != std::string::npos);
    }
}
