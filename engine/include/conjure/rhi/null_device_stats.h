#pragma once

#include "conjure/core/api.h"
#include "conjure/core/types.h"

#include <string>
#include <vector>

// What the null backend recorded (docs/design/02_renderer.md 7, 08_testing.md 8.4). Resources
// are counted when they are created, command-list work when it is recorded, submissions when
// executeCommandLists runs, and debug markers keep their names in recording order so renderer
// tests can assert on the pass stream. Read a snapshot with device::null_stats().
//
// The null device is the instrument the renderer's allocation gates measure through
// (tests/benchmarks `renderer.null_frame_10k`, allocs 0), so its own bookkeeping is allocation-free
// in steady state: the counters are plain integers and the marker names, the one growing list, are
// stored only after a consumer asked for them by calling device::reset_null_stats() and at most
// MAX_RECORDED_MARKER_NAMES per reset (the rest are counted in markerNamesDropped). A headless run
// that never resets the stats therefore stores no names and never grows.

namespace conjure::rhi {

    /// <summary>
    /// Marker names kept per reset_null_stats(); beginMarker calls beyond this are counted in
    /// markersBegun and markerNamesDropped but their names are not stored.
    /// </summary>
    inline constexpr u32 MAX_RECORDED_MARKER_NAMES = 4096;

    struct CONJURE_API null_device_stats {
        // Resource creation, by type.
        u32 heapsCreated = 0;
        u32 texturesCreated = 0;
        u32 stagingTexturesCreated = 0;
        u32 buffersCreated = 0;
        u32 shadersCreated = 0;
        u32 samplersCreated = 0;
        u32 inputLayoutsCreated = 0;
        u32 framebuffersCreated = 0;
        u32 bindingLayoutsCreated = 0;
        u32 bindingSetsCreated = 0;
        u32 descriptorTablesCreated = 0;
        u32 graphicsPipelinesCreated = 0;
        u32 computePipelinesCreated = 0;
        u32 meshletPipelinesCreated = 0;
        u32 eventQueriesCreated = 0;
        u32 timerQueriesCreated = 0;
        u32 commandListsCreated = 0;

        // Command list lifecycle and device calls.
        u32 commandListsOpened = 0;
        u32 commandListsClosed = 0;
        u32 commandListsSubmitted = 0;
        u32 garbageCollections = 0;
        u32 waitsForIdle = 0;

        // Recorded work.
        u32 draws = 0;                  // draw + drawIndexed
        u32 drawsIndirect = 0;          // drawIndirect, drawIndexedIndirect, drawIndexedIndirectCount
        u32 instancesDrawn = 0;         // sum of instanceCount over draw + drawIndexed
        u32 dispatches = 0;             // dispatch
        u32 dispatchesIndirect = 0;     // dispatchIndirect
        u32 meshDispatches = 0;         // dispatchMesh and its indirect variants
        u32 graphicsStateChanges = 0;
        u32 computeStateChanges = 0;
        u32 pushConstantWrites = 0;
        u32 textureClears = 0;          // clearTextureFloat + clearTextureUInt
        u32 depthClears = 0;
        u32 bufferClears = 0;
        u32 bufferWrites = 0;
        u64 bufferWriteBytes = 0;
        u32 textureWrites = 0;
        u32 bufferCopies = 0;
        u32 textureCopies = 0;
        u32 timerQueriesBegun = 0;
        u32 markersBegun = 0;
        u32 markersEnded = 0;
        // beginMarker names in recording order since the last device::reset_null_stats(); empty
        // until the first reset (nothing is stored for a consumer that never asked) and capped at
        // MAX_RECORDED_MARKER_NAMES.
        std::vector<std::string> markerNames = {};
        u32 markerNamesDropped = 0;     // beginMarker calls whose name was not stored (cap or no consumer)

        // Null swapchains.
        u32 swapchainsCreated = 0;
        u32 acquires = 0;
        u32 presents = 0;
        u32 resizes = 0;

        /// <summary>
        /// Resets every counter and forgets the marker names (the vector keeps its capacity).
        /// </summary>
        void reset() noexcept;
    };
}
