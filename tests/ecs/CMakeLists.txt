# ecs tests: one test_<header>.cpp per public header of conjure/ecs plus the topic files
# docs/design/01_ecs.md section 10 names. Every public header also compiles on its own.

if(NOT TARGET conjure::ecs)
    return()
endif()

conjure_add_test_module(ecs
    SOURCES
        ecs_test_fixture.h
        test_archetype.cpp
        test_change_detection.cpp
        test_chunk_layout.cpp
        test_chunk_view.cpp
        test_command_buffer.cpp
        test_component.cpp
        test_component_mask.cpp
        test_component_registry.cpp
        test_component_type_desc.cpp
        test_dynamic_components.cpp
        test_ecs.cpp
        test_ecs_error.cpp
        test_entity.cpp
        test_entity_allocator.cpp
        test_entity_archive.cpp
        test_entity_handle.cpp
        test_log_category.cpp
        test_observer.cpp
        test_parallel_iteration.cpp
        test_query.cpp
        test_scheduler.cpp
        test_serialization.cpp
        test_system.cpp
        test_system_phase.cpp
        test_view.cpp
        test_world.cpp
        test_world_components.cpp
        test_world_entities.cpp
        test_world_hash.cpp
)

conjure_header_check(conjure_ecs)
