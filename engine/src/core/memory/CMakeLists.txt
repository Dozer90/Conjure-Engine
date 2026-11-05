# core/memory - counted heap allocation and the engine's allocators (arena, pool, frame).

conjure_add_core_area(memory
    PUBLIC_HEADERS
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/memory/allocation_counters.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/memory/arena.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/memory/frame_allocator.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/memory/memory.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/memory/pool_allocator.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/memory/tracked_allocation.h
    SOURCES
        allocation_counters.cpp
        arena.cpp
        frame_allocator.cpp
        pool_allocator.cpp
        tracked_allocation.cpp
    PUBLIC_DEPS
        conjure::core_foundation
)
