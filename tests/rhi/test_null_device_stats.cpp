#include "conjure/testing/doctest.h"

#include "conjure/rhi/null_device_stats.h"

using namespace conjure;
using namespace conjure::rhi;

TEST_SUITE("rhi/null_device_stats") {

    TEST_CASE("null_device_stats - a default instance has every counter at zero and no markers") {
        const null_device_stats stats = {};
        CHECK(stats.texturesCreated == 0);
        CHECK(stats.buffersCreated == 0);
        CHECK(stats.commandListsSubmitted == 0);
        CHECK(stats.draws == 0);
        CHECK(stats.instancesDrawn == 0);
        CHECK(stats.dispatches == 0);
        CHECK(stats.bufferWriteBytes == 0);
        CHECK(stats.presents == 0);
        CHECK(stats.markerNames.empty());
    }

    TEST_CASE("null_device_stats - reset clears the counters and the marker names") {
        null_device_stats stats = {};
        stats.texturesCreated = 3;
        stats.draws = 10;
        stats.instancesDrawn = 100;
        stats.bufferWriteBytes = 4096;
        stats.presents = 2;
        stats.markersBegun = 2;
        stats.markerNames = {"depth_prepass", "forward_opaque"};

        stats.reset();

        CHECK(stats.texturesCreated == 0);
        CHECK(stats.draws == 0);
        CHECK(stats.instancesDrawn == 0);
        CHECK(stats.bufferWriteBytes == 0);
        CHECK(stats.presents == 0);
        CHECK(stats.markersBegun == 0);
        CHECK(stats.markerNames.empty());
    }

    TEST_CASE("null_device_stats - reset keeps the marker vector usable afterwards") {
        null_device_stats stats = {};
        stats.markerNames = {"a", "b", "c"};
        stats.reset();
        stats.markerNames.emplace_back("d");
        REQUIRE(stats.markerNames.size() == 1);
        CHECK(stats.markerNames[0] == "d");
    }
}
