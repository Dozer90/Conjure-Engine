# physics tests: one test_<header>.cpp per public header of conjure/physics plus the topic files of
# docs/design/14_physics.md 10 (test_layers, test_rigid_body, test_colliders, test_fixed_step,
# test_component_sync, test_physics_stack, test_determinism). Everything is headless; the world-level
# cases run under for_each_job_mode (0 workers inline, 4 workers parallel).

if(NOT TARGET conjure::physics)
    return()
endif()

conjure_add_test_module(physics
    SOURCES
        physics_test_fixture.h
        test_character_controller.cpp
        test_colliders.cpp
        test_collision_mesh_asset.cpp
        test_component_sync.cpp
        test_components.cpp
        test_contact_events.cpp
        test_determinism.cpp
        test_fixed_step.cpp
        test_layers.cpp
        test_log_category.cpp
        test_physics.cpp
        test_physics_debug_draw.cpp
        test_physics_error.cpp
        test_physics_layers.cpp
        test_physics_settings.cpp
        test_physics_stack.cpp
        test_physics_world.cpp
        test_queries.cpp
        test_register.cpp
        test_rigid_body.cpp
    TIMEOUT 300
)

conjure_header_check(conjure_physics)
