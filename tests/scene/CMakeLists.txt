# scene tests: one test_<header>.cpp per public header of conjure/scene plus the topic files of
# docs/design/13_scene.md 10 (test_name_tag_layer, test_transform_propagation, test_scene_serialisation).
# Everything is headless; the [parallel] cases run on a worker pool of at least four threads.

if(NOT TARGET conjure::scene)
    return()
endif()

conjure_add_test_module(scene
    SOURCES
        scene_test_fixture.h
        test_components.cpp
        test_guid_index.cpp
        test_hierarchy.cpp
        test_log_category.cpp
        test_name_tag_layer.cpp
        test_prefab.cpp
        test_register.cpp
        test_scene.cpp
        test_scene_error.cpp
        test_scene_manager.cpp
        test_scene_serialisation.cpp
        test_scene_serializer.cpp
        test_scene_snapshot.cpp
        test_transform_propagation.cpp
        test_transform_system.cpp
)

conjure_header_check(conjure_scene)
