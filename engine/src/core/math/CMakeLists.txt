# core/math - glm aliases, transform, aabb, plane, frustum, ray, rect, projection helpers.

conjure_add_core_area(math
    PUBLIC_HEADERS
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/math.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/math/aabb.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/math/frustum.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/math/glm_types.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/math/math.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/math/math_utils.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/math/plane.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/math/ray.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/math/rect.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/math/transform.h
    SOURCES
        aabb.cpp
        frustum.cpp
        look_rotation.cpp
        look_rotation.h
        projection.cpp
        ray.cpp
        transform.cpp
    PUBLIC_DEPS
        conjure::core_foundation
        conjure::thirdparty::glm
)
