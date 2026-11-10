# conjure_scene - hierarchy and transforms, name/tag/layer/guid, scene and prefab files, snapshots and the
# scene manager (docs/design/13_scene.md). Depends publicly on conjure::ecs and conjure::assets (and through
# them on conjure::core); no third-party dependency and no renderer, physics or scripting include (R7, R29).

if(NOT TARGET conjure::ecs OR NOT TARGET conjure::assets)
    message(STATUS "scene: conjure::ecs or conjure::assets is not available, module skipped")
    return()
endif()

set(CONJURE_SCENE_PUBLIC_HEADERS
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/scene/components.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/scene/guid_index.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/scene/hierarchy.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/scene/log_category.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/scene/prefab.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/scene/register.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/scene/scene.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/scene/scene_error.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/scene/scene_manager.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/scene/scene_serializer.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/scene/scene_snapshot.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/scene/transform_system.h
)

conjure_add_module(scene
    PUBLIC_HEADERS
        ${CONJURE_SCENE_PUBLIC_HEADERS}
    SOURCES
        archive_tools.cpp
        archive_tools.h
        asset_ref_raw.h
        components.cpp
        guid_index.cpp
        hierarchy.cpp
        log_category.cpp
        prefab.cpp
        prefab_importer.cpp
        prefab_importer.h
        register.cpp
        scene.cpp
        scene_error.cpp
        scene_json.cpp
        scene_json.h
        scene_link_pass.cpp
        scene_loader.cpp
        scene_loader.h
        scene_manager.cpp
        scene_observers.cpp
        scene_serializer.cpp
        scene_snapshot.cpp
        transform_propagation.cpp
        transform_system.cpp
    PUBLIC_DEPS
        conjure::core
        conjure::ecs
        conjure::assets
)
