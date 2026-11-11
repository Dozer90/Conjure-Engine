# conjure_bindings - the bindings-owned entries of the native scripting table, the counting thunks, the
# marshalling rules and the subsystem that fills the table before the handshake (docs/design/15_bindings.md).
# Depends publicly on scripting, scene, assets and engine; physics, audio and renderer compile in under their
# CONJURE_WITH_* options (the stubs cover a compiled-out group); modules and platform stay private.

if(NOT CONJURE_WITH_SCRIPTING OR NOT TARGET conjure::scripting OR NOT TARGET conjure::engine)
    message(STATUS "bindings: scripting or engine is not available, module skipped")
    return()
endif()

set(CONJURE_BINDINGS_PUBLIC_HEADERS
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/bindings/bindings.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/bindings/bindings_error.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/bindings/bindings_subsystem.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/bindings/call_counters.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/bindings/log_category.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/bindings/marshal.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/bindings/native_api_entries.h
)

set(CONJURE_BINDINGS_SOURCES
    bindings_error.cpp
    bindings_subsystem.cpp
    call_counters.cpp
    entries_asset.cpp
    entries_diagnostics.cpp
    entries_entity.cpp
    entries_input.cpp
    entries_modules.cpp
    entries_prefab.cpp
    entries_scene.cpp
    entries_transform.cpp
    entry_support.h
    log_category.cpp
    marshal.cpp
    native_api_fill.cpp
)

set(CONJURE_BINDINGS_PRIVATE_DEPS
    conjure::modules
    conjure::platform
)

# A compiled-out group keeps its stubs (15_bindings.md B3); the real entries exist only with the module.
if(CONJURE_WITH_PHYSICS AND TARGET conjure::physics)
    list(APPEND CONJURE_BINDINGS_SOURCES entries_physics.cpp)
    list(APPEND CONJURE_BINDINGS_PRIVATE_DEPS conjure::physics)
endif()
if(CONJURE_WITH_AUDIO AND TARGET conjure::audio)
    list(APPEND CONJURE_BINDINGS_SOURCES entries_audio.cpp)
    list(APPEND CONJURE_BINDINGS_PRIVATE_DEPS conjure::audio)
endif()
if(CONJURE_WITH_RENDERER AND TARGET conjure::renderer)
    list(APPEND CONJURE_BINDINGS_PRIVATE_DEPS conjure::renderer)
endif()

conjure_add_module(bindings
    PUBLIC_HEADERS
        ${CONJURE_BINDINGS_PUBLIC_HEADERS}
    SOURCES
        ${CONJURE_BINDINGS_SOURCES}
    PUBLIC_DEPS
        conjure::core
        conjure::ecs
        conjure::assets
        conjure::scene
        conjure::scripting
        conjure::engine
    PRIVATE_DEPS
        ${CONJURE_BINDINGS_PRIVATE_DEPS}
)
