#pragma once

// Umbrella header for the core memory area: counted heap allocation and the engine's allocators.
//
//   tracked_allocate / tracked_free / tracked_reallocate   counted, aligned heap memory
//   counters() / snapshot()                                process-wide allocation counters
//   arena                                                  bump allocator, reset as a whole
//   pool_allocator                                         fixed-size blocks in slabs
//   frame_allocator                                        per-frame, per-thread scratch memory

#include "conjure/core/memory/allocation_counters.h"
#include "conjure/core/memory/arena.h"
#include "conjure/core/memory/frame_allocator.h"
#include "conjure/core/memory/pool_allocator.h"
#include "conjure/core/memory/tracked_allocation.h"
