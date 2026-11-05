# core/time - clocks, random numbers, events, delegates and the engine-wide event bus.

conjure_add_core_area(time
    PUBLIC_HEADERS
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/clock.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/delegate.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/event.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/event_bus.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/random.h
    SOURCES
        clock.cpp
        event.cpp
        event_bus.cpp
        random.cpp
    PUBLIC_DEPS
        conjure::core_foundation
)
