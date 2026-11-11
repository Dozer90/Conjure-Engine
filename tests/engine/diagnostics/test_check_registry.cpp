#include "conjure/testing/doctest.h"

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/types.h"
#include "conjure/engine/diagnostics/check_registry.h"

#include <string>
#include <vector>

// diagnostics/check_registry.h (docs/design/08_testing.md 2.5): self-verifying samples recorded from any
// thread and asserted by the harness.

using namespace conjure;
using namespace conjure::engine::diagnostics;

TEST_SUITE("engine/diagnostics/check_registry") {

    TEST_CASE("check_registry - starts empty and passing") {
        const check_registry registry = {};
        CHECK(registry.all_passed());
        CHECK(registry.failed_count() == 0);
        CHECK(registry.record_count() == 0);
        CHECK(registry.records().empty());
    }

    TEST_CASE("check_registry - report keeps every record in order and counts failures") {
        check_registry registry = {};
        registry.report("gravity", true, "", 1);
        registry.report("collision", false, "expected 1 contact, got 0", 2);
        registry.report("audio", true, "voice playing", 3);
        CHECK(registry.record_count() == 3);
        CHECK(registry.failed_count() == 1);
        CHECK_FALSE(registry.all_passed());
        const std::vector<check_record> records = registry.records();
        REQUIRE(records.size() == 3);
        CHECK(records[0].name == "gravity");
        CHECK(records[0].passed);
        CHECK(records[0].frame == 1);
        CHECK(records[1].name == "collision");
        CHECK_FALSE(records[1].passed);
        CHECK(records[1].detail == "expected 1 contact, got 0");
        CHECK(records[2].detail == "voice playing");
    }

    TEST_CASE("check_registry - clear forgets everything") {
        check_registry registry = {};
        registry.report("a", false, "", 0);
        registry.clear();
        CHECK(registry.all_passed());
        CHECK(registry.record_count() == 0);
    }

    TEST_CASE("check_registry - reports from job threads are all kept") {
        core::jobs::job_system jobs = {};
        core::jobs::job_system_desc desc = {};
        desc.workerCount = 4;
        CONJURE_REQUIRE_OK(jobs.start(desc));
        check_registry registry = {};
        constexpr u32 COUNT = 256;
        std::vector<core::jobs::job_handle> handles = {};
        for (u32 index = 0; index < COUNT; ++index) {
            handles.push_back(jobs.schedule([&registry, index]() {
                registry.report("parallel", index % 2 == 0, std::to_string(index), index);
            }));
        }
        for (const core::jobs::job_handle &handle : handles) {
            jobs.wait(handle);
        }
        jobs.stop();
        CHECK(registry.record_count() == COUNT);
        CHECK(registry.failed_count() == COUNT / 2);
    }
}
