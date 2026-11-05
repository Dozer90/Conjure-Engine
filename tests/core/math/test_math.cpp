#include "conjure/testing/doctest.h"

#include "math_test.h"

#include "conjure/testing/assert_capture.h"

#include "conjure/core/math.h"

#include "conjure/core/math/math.h"

#include <array>
#include <optional>
#include <thread>
#include <vector>

using namespace conjure;
using namespace conjure::math;

namespace {
    constexpr f32 FOV_Y = radians(70.0f);
    constexpr f32 ASPECT = 16.0f / 9.0f;
    constexpr f32 NEAR_DISTANCE = 0.25f;
    constexpr f32 FAR_DISTANCE = 300.0f;

    f32 depth_of(const mat4 &projection, f32 distance) {
        const std::optional<vec3> ndc = project(vec3(0.0f, 0.0f, -distance), projection);
        REQUIRE(ndc.has_value());
        return ndc->z;
    }
}

TEST_SUITE("core/math") {

    TEST_CASE("math - the umbrella and convenience headers expose every type of the area") {
        [[maybe_unused]] const vec3 vector = vec3(1.0f);
        [[maybe_unused]] const quat rotation = quat(1.0f, 0.0f, 0.0f, 0.0f);
        [[maybe_unused]] const mat4 matrix = mat4(1.0f);
        [[maybe_unused]] const transform pose = {};
        [[maybe_unused]] const aabb box = {};
        [[maybe_unused]] const plane surface = {};
        [[maybe_unused]] const frustum volume = {};
        [[maybe_unused]] const ray line = {};
        [[maybe_unused]] const rect area = {};
        [[maybe_unused]] const urect viewport = {};
        CHECK(nearly_equal(radians(180.0f), PI));
        CHECK(align_up(u32{5}, 4) == 8);
        CHECK(std::format("{} {}", vector, rotation) == "(1, 1, 1) (1, 0, 0, 0)");
    }

    TEST_CASE("math - perspective equals glm::perspective and maps near to 0 and far to 1") {
        const mat4 projection = perspective(FOV_Y, ASPECT, NEAR_DISTANCE, FAR_DISTANCE);
        const mat4 fromGlm = glm::perspective(FOV_Y, ASPECT, NEAR_DISTANCE, FAR_DISTANCE);
        CHECK_NEARLY(projection, fromGlm, math_test::TIGHT_EPSILON);
        const mat4 explicitZeroToOne = glm::perspectiveRH_ZO(FOV_Y, ASPECT, NEAR_DISTANCE, FAR_DISTANCE);
        CHECK_NEARLY(projection, explicitZeroToOne, math_test::TIGHT_EPSILON);

        CHECK(nearly_equal(depth_of(projection, NEAR_DISTANCE), 0.0f, math_test::TIGHT_EPSILON));
        CHECK(nearly_equal(depth_of(projection, FAR_DISTANCE), 1.0f, math_test::TIGHT_EPSILON));
        const f32 middle = depth_of(projection, 10.0f);
        CHECK(middle > 0.0f);
        CHECK(middle < 1.0f);
        CHECK(depth_of(projection, 20.0f) > middle);   // depth grows with distance

        // The vertical field of view is honoured: a point on the top edge projects to y = 1.
        const f32 halfHeight = std::tan(FOV_Y * 0.5f) * 10.0f;
        const std::optional<vec3> topEdge = project(vec3(0.0f, halfHeight, -10.0f), projection);
        REQUIRE(topEdge.has_value());
        CHECK(nearly_equal(topEdge->y, 1.0f, math_test::LOOSE_EPSILON));
        const std::optional<vec3> rightEdge = project(vec3(halfHeight * ASPECT, 0.0f, -10.0f), projection);
        REQUIRE(rightEdge.has_value());
        CHECK(nearly_equal(rightEdge->x, 1.0f, math_test::LOOSE_EPSILON));
    }

    TEST_CASE("math - infinite_perspective matches glm and approaches depth 1 at infinity") {
        const mat4 projection = infinite_perspective(FOV_Y, ASPECT, NEAR_DISTANCE);
        CHECK_NEARLY(projection, glm::infinitePerspectiveRH_ZO(FOV_Y, ASPECT, NEAR_DISTANCE), math_test::TIGHT_EPSILON);
        CHECK_NEARLY(projection, glm::infinitePerspective(FOV_Y, ASPECT, NEAR_DISTANCE), math_test::TIGHT_EPSILON);

        CHECK(nearly_equal(depth_of(projection, NEAR_DISTANCE), 0.0f, math_test::TIGHT_EPSILON));
        const f32 distantDepth = depth_of(projection, 1.0e6f);
        CHECK(distantDepth < 1.0f);
        CHECK(distantDepth > 0.999f);
        CHECK(depth_of(projection, 100.0f) < distantDepth);

        // The x/y columns agree with the finite projection of the same field of view.
        const mat4 finite = perspective(FOV_Y, ASPECT, NEAR_DISTANCE, FAR_DISTANCE);
        CHECK_NEARLY(projection[0], finite[0], math_test::TIGHT_EPSILON);
        CHECK_NEARLY(projection[1], finite[1], math_test::TIGHT_EPSILON);
    }

    TEST_CASE("math - orthographic equals glm::ortho and maps the box to clip space") {
        const mat4 projection = orthographic(-8.0f, 8.0f, -4.5f, 4.5f, 1.0f, 50.0f);
        CHECK_NEARLY(projection, glm::ortho(-8.0f, 8.0f, -4.5f, 4.5f, 1.0f, 50.0f), math_test::TIGHT_EPSILON);
        CHECK_NEARLY(projection, glm::orthoRH_ZO(-8.0f, 8.0f, -4.5f, 4.5f, 1.0f, 50.0f), math_test::TIGHT_EPSILON);

        const std::optional<vec3> minCorner = project(vec3(-8.0f, -4.5f, -1.0f), projection);
        REQUIRE(minCorner.has_value());
        CHECK_NEARLY(*minCorner, vec3(-1.0f, -1.0f, 0.0f), math_test::TIGHT_EPSILON);
        const std::optional<vec3> maxCorner = project(vec3(8.0f, 4.5f, -50.0f), projection);
        REQUIRE(maxCorner.has_value());
        CHECK_NEARLY(*maxCorner, vec3(1.0f, 1.0f, 1.0f), math_test::TIGHT_EPSILON);
        const std::optional<vec3> center = project(vec3(0.0f, 0.0f, -25.5f), projection);
        REQUIRE(center.has_value());
        CHECK_NEARLY(*center, vec3(0.0f, 0.0f, 0.5f), math_test::TIGHT_EPSILON);
    }

    TEST_CASE("math - projection helpers assert on invalid parameters") {
        testing::expect_assert asserts = {};
        [[maybe_unused]] const mat4 badFov = perspective(0.0f, ASPECT, NEAR_DISTANCE, FAR_DISTANCE);
        CHECK(asserts.count() == 1);
        [[maybe_unused]] const mat4 badAspect = perspective(FOV_Y, -1.0f, NEAR_DISTANCE, FAR_DISTANCE);
        CHECK(asserts.count() == 2);
        [[maybe_unused]] const mat4 badNear = perspective(FOV_Y, ASPECT, 0.0f, FAR_DISTANCE);
        CHECK(asserts.count() == 3);
        [[maybe_unused]] const mat4 badFar = perspective(FOV_Y, ASPECT, 10.0f, 5.0f);
        CHECK(asserts.count() == 4);
        [[maybe_unused]] const mat4 badInfinite = infinite_perspective(PI, ASPECT, NEAR_DISTANCE);
        CHECK(asserts.count() == 5);
        [[maybe_unused]] const mat4 badInfiniteNear = infinite_perspective(FOV_Y, ASPECT, -1.0f);
        CHECK(asserts.count() == 6);
        [[maybe_unused]] const mat4 badOrtho = orthographic(1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f);
        CHECK(asserts.count() == 7);
        [[maybe_unused]] const mat4 badOrthoDepth = orthographic(-1.0f, 1.0f, -1.0f, 1.0f, 2.0f, 2.0f);
        CHECK(asserts.count() == 8);
    }

    TEST_CASE("math - look_at matches glm::lookAt and inverts the camera transform") {
        const vec3 eye = vec3(3.0f, 4.0f, 5.0f);
        const vec3 target = vec3(-1.0f, 0.5f, 2.0f);
        const vec3 up = vec3(0.0f, 1.0f, 0.0f);

        const mat4 view = look_at(eye, target, up);
        CHECK_NEARLY(view, glm::lookAt(eye, target, up), math_test::TIGHT_EPSILON);
        CHECK_NEARLY(view, glm::inverse(transform::look_at(eye, target, up).to_matrix()), math_test::LOOSE_EPSILON);

        // The eye maps to the origin and the target lies on the -Z axis of camera space.
        CHECK_NEARLY(vec3(view * vec4(eye, 1.0f)), vec3(0.0f), math_test::LOOSE_EPSILON);
        const vec3 cameraSpaceTarget = vec3(view * vec4(target, 1.0f));
        CHECK(nearly_zero(cameraSpaceTarget.x, math_test::LOOSE_EPSILON));
        CHECK(nearly_zero(cameraSpaceTarget.y, math_test::LOOSE_EPSILON));
        CHECK(nearly_equal(cameraSpaceTarget.z, -glm::distance(eye, target), math_test::LOOSE_EPSILON));
        // World up maps to a direction with positive y in camera space.
        CHECK(vec3(view * vec4(up, 0.0f)).y > 0.0f);
    }

    TEST_CASE("math - look_at handles degenerate inputs without NaN") {
        // Eye equal to target: a pure translation.
        const mat4 still = look_at(vec3(1.0f, 2.0f, 3.0f), vec3(1.0f, 2.0f, 3.0f), vec3(0.0f, 1.0f, 0.0f));
        CHECK_NEARLY(still, glm::translate(mat4(1.0f), vec3(-1.0f, -2.0f, -3.0f)), math_test::TIGHT_EPSILON);

        // Up parallel to the view direction: still orthonormal, still looks at the target.
        const vec3 eye = vec3(0.0f, 10.0f, 0.0f);
        const mat4 downward = look_at(eye, vec3(0.0f), vec3(0.0f, 1.0f, 0.0f));
        const mat3 basis = mat3(downward);
        CHECK_NEARLY(basis * glm::transpose(basis), mat3(1.0f), math_test::LOOSE_EPSILON);
        CHECK_NEARLY(vec3(downward * vec4(0.0f, 0.0f, 0.0f, 1.0f)), vec3(0.0f, 0.0f, -10.0f), math_test::LOOSE_EPSILON);
        const transform camera = transform::look_at(eye, vec3(0.0f), vec3(0.0f, 1.0f, 0.0f));
        CHECK_NEARLY(downward, glm::inverse(camera.to_matrix()), math_test::LOOSE_EPSILON);
    }

    TEST_CASE("math - project and unproject round trip") {
        const mat4 projection = perspective(FOV_Y, ASPECT, NEAR_DISTANCE, FAR_DISTANCE);
        const mat4 view = look_at(vec3(1.0f, 2.0f, 3.0f), vec3(0.0f), vec3(0.0f, 1.0f, 0.0f));
        const mat4 viewProjection = projection * view;
        const mat4 inverseViewProjection = glm::inverse(viewProjection);

        const std::array<vec3, 3> points = {vec3(0.0f), vec3(-2.0f, 1.0f, -4.0f), vec3(0.5f, 0.5f, -1.0f)};
        for (const vec3 &point : points) {
            const std::optional<vec3> ndc = project(point, viewProjection);
            REQUIRE(ndc.has_value());
            const std::optional<vec3> back = unproject(*ndc, inverseViewProjection);
            REQUIRE(back.has_value());
            CHECK_NEARLY(*back, point, 1.0e-3f);
        }

        // Normalised device corners come back to the frustum corners.
        const std::optional<vec3> nearCenter = unproject(vec3(0.0f, 0.0f, 0.0f), glm::inverse(projection));
        REQUIRE(nearCenter.has_value());
        CHECK_NEARLY(*nearCenter, vec3(0.0f, 0.0f, -NEAR_DISTANCE), math_test::LOOSE_EPSILON);
        const std::optional<vec3> farCenter = unproject(vec3(0.0f, 0.0f, 1.0f), glm::inverse(projection));
        REQUIRE(farCenter.has_value());
        // Depth 1 maps to the far plane; float precision there is about 1e-4 relative.
        CHECK_NEARLY(*farCenter, vec3(0.0f, 0.0f, -FAR_DISTANCE), 0.1f);
    }

    TEST_CASE("math - project and unproject report points at infinity") {
        const mat4 projection = perspective(FOV_Y, ASPECT, NEAR_DISTANCE, FAR_DISTANCE);
        CHECK_FALSE(project(vec3(1.0f, 2.0f, 0.0f), projection).has_value());   // on the camera plane (w = 0)
        CHECK(project(vec3(1.0f, 2.0f, 1.0f), projection).has_value());         // behind the camera still projects

        const mat4 infinite = infinite_perspective(FOV_Y, ASPECT, NEAR_DISTANCE);
        CHECK_FALSE(unproject(vec3(0.0f, 0.0f, 1.0f), glm::inverse(infinite)).has_value());
        CHECK(unproject(vec3(0.0f, 0.0f, 0.5f), glm::inverse(infinite)).has_value());
        CHECK_FALSE(unproject(vec3(0.0f), mat4(0.0f)).has_value());
    }

    TEST_CASE("math - every function is pure: concurrent callers get identical results") {
        constexpr usize THREAD_COUNT = 8;
        constexpr usize ITERATIONS = 2000;

        const transform pose = math_test::sample_transform();
        const mat4 viewProjection = perspective(FOV_Y, ASPECT, NEAR_DISTANCE, FAR_DISTANCE)
          * look_at(vec3(1.0f, 2.0f, 3.0f), vec3(0.0f), vec3(0.0f, 1.0f, 0.0f));
        const frustum expectedFrustum = frustum::from_view_projection(viewProjection);
        const transform expectedInverse = pose.inverse();
        const aabb expectedBox = aabb::from_center_extents(vec3(1.0f), vec3(2.0f)).transformed(pose.to_matrix());
        const mat4 inverseViewProjection = glm::inverse(viewProjection);
        const ray pick = ray::from_viewport(vec2(100.0f, 50.0f), uvec2(640u, 480u), inverseViewProjection);
        const bool expectedIntersects = expectedFrustum.intersects(expectedBox);

        std::vector<std::thread> threads = {};
        std::array<bool, THREAD_COUNT> identical = {};
        for (usize threadIndex = 0; threadIndex < THREAD_COUNT; ++threadIndex) {
            threads.emplace_back([&, threadIndex] {
                bool allMatch = true;
                for (usize iteration = 0; iteration < ITERATIONS && allMatch; ++iteration) {
                    allMatch = frustum::from_view_projection(viewProjection) == expectedFrustum
                      && pose.inverse() == expectedInverse
                      && aabb::from_center_extents(vec3(1.0f), vec3(2.0f)).transformed(pose.to_matrix()) == expectedBox
                      && ray::from_viewport(vec2(100.0f, 50.0f), uvec2(640u, 480u), inverseViewProjection) == pick
                      && frustum::from_view_projection(viewProjection).intersects(expectedBox) == expectedIntersects;
                }
                identical[threadIndex] = allMatch;
            });
        }
        for (std::thread &thread : threads) {
            thread.join();
        }
        for (const bool match : identical) {
            CHECK(match);
        }
    }
}
