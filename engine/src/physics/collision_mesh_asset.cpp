#include "conjure/physics/collision_mesh_asset.h"

#include "physics/collision_mesh_internal.h"
#include "physics/jolt_conversions.h"

#include "conjure/physics/physics_error.h"

#include "conjure/core/math/math_utils.h"

#include <Jolt/Core/Array.h>
#include <Jolt/Geometry/IndexedTriangle.h>
#include <Jolt/Math/Float3.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>

#include <bit>
#include <cmath>
#include <cstring>
#include <utility>

namespace conjure::physics {

namespace {

    constexpr usize HEADER_BYTES = 16;

    [[nodiscard]] usize align_section(usize offset) noexcept {
        return math::align_up(offset, COLLISION_MESH_SECTION_ALIGNMENT);
    }

    void write_u32(std::vector<std::byte> &out, usize offset, u32 value) noexcept {
        for (usize index = 0; index < 4; ++index) {
            out[offset + index] = static_cast<std::byte>((value >> (8 * index)) & 0xFFu);
        }
    }

    void write_f32(std::vector<std::byte> &out, usize offset, f32 value) noexcept {
        write_u32(out, offset, std::bit_cast<u32>(value));
    }

    [[nodiscard]] u32 read_u32(std::span<const std::byte> bytes, usize offset) noexcept {
        u32 value = 0;
        for (usize index = 0; index < 4; ++index) {
            value |= static_cast<u32>(bytes[offset + index]) << (8 * index);
        }
        return value;
    }

    [[nodiscard]] f32 read_f32(std::span<const std::byte> bytes, usize offset) noexcept {
        return std::bit_cast<f32>(read_u32(bytes, offset));
    }

    [[nodiscard]] bool is_finite(const math::vec3 &value) noexcept {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    }

    /// <summary>
    /// True for a triangle with three distinct vertices and a non-zero area.
    /// </summary>
    [[nodiscard]] bool is_proper_triangle(std::span<const math::vec3> positions, u32 a, u32 b, u32 c) noexcept {
        if (a == b || b == c || a == c) {
            return false;
        }
        const math::vec3 edgeA = positions[b] - positions[a];
        const math::vec3 edgeB = positions[c] - positions[a];
        const math::vec3 cross = glm::cross(edgeA, edgeB);
        return glm::dot(cross, cross) > 0.0f;
    }

    [[nodiscard]] core::error jolt_failure(const char *what, const JPH::String &message) {
        return core::make_error(physics_error::SHAPE_CREATE_FAILED, "{}: {}", what, message.c_str());
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

collision_mesh_asset::collision_mesh_asset() {}

collision_mesh_asset::~collision_mesh_asset() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

core::result<collision_mesh_asset> collision_mesh_asset::from_geometry(
  std::span<const math::vec3> positions,
  std::span<const u32> indices
) {
    if (indices.size() % 3 != 0) {
        return core::make_error(
          physics_error::GEOMETRY_INVALID,
          "index count {} is not a multiple of three",
          indices.size()
        );
    }
    if (positions.size() > 0xFFFFFFFFu) {
        return core::make_error(physics_error::GEOMETRY_INVALID, "too many vertices");
    }
    for (usize index = 0; index < positions.size(); ++index) {
        if (!is_finite(positions[index])) {
            return core::make_error(physics_error::GEOMETRY_INVALID, "position {} is not finite", index);
        }
    }
    const u32 vertexCount = static_cast<u32>(positions.size());
    for (usize index = 0; index < indices.size(); ++index) {
        if (indices[index] >= vertexCount) {
            return core::make_error(
              physics_error::GEOMETRY_INVALID,
              "index {} ({}) is out of range for {} vertices",
              index,
              indices[index],
              vertexCount
            );
        }
    }

    collision_mesh_asset mesh = {};
    mesh.mPositions.assign(positions.begin(), positions.end());
    mesh.mIndices.reserve(indices.size());
    for (usize triangle = 0; triangle + 2 < indices.size(); triangle += 3) {
        const u32 a = indices[triangle];
        const u32 b = indices[triangle + 1];
        const u32 c = indices[triangle + 2];
        if (!is_proper_triangle(positions, a, b, c)) {
            continue;
        }
        mesh.mIndices.push_back(a);
        mesh.mIndices.push_back(b);
        mesh.mIndices.push_back(c);
    }
    if (mesh.mIndices.empty()) {
        return core::make_error(physics_error::GEOMETRY_INVALID, "no non-degenerate triangle");
    }
    for (const math::vec3 &position : mesh.mPositions) {
        mesh.mBounds.expand(position);
    }
    return mesh;
}

core::result<collision_mesh_asset> collision_mesh_asset::from_payload(std::span<const std::byte> payload) {
    if (payload.size() < HEADER_BYTES) {
        return core::make_error(physics_error::GEOMETRY_INVALID, "payload shorter than its header");
    }
    const u32 vertexCount = read_u32(payload, 0);
    const u32 indexCount = read_u32(payload, 4);
    const usize positionsOffset = HEADER_BYTES;
    const usize positionsBytes = static_cast<usize>(vertexCount) * 12;
    const usize indicesOffset = align_section(positionsOffset + positionsBytes);
    const usize indicesBytes = static_cast<usize>(indexCount) * 4;
    if (indicesOffset > payload.size() || indicesBytes > payload.size() - indicesOffset) {
        return core::make_error(physics_error::GEOMETRY_INVALID, "payload declares more data than it holds");
    }

    std::vector<math::vec3> positions(vertexCount);
    for (usize index = 0; index < vertexCount; ++index) {
        const usize at = positionsOffset + index * 12;
        positions[index] = math::vec3(read_f32(payload, at), read_f32(payload, at + 4), read_f32(payload, at + 8));
    }
    std::vector<u32> indices(indexCount);
    for (usize index = 0; index < indexCount; ++index) {
        indices[index] = read_u32(payload, indicesOffset + index * 4);
    }
    return from_geometry(positions, indices);
}

std::vector<std::byte> collision_mesh_asset::to_payload() const {
    const usize positionsOffset = HEADER_BYTES;
    const usize positionsBytes = mPositions.size() * 12;
    const usize indicesOffset = align_section(positionsOffset + positionsBytes);
    const usize indicesBytes = mIndices.size() * 4;
    std::vector<std::byte> payload(align_section(indicesOffset + indicesBytes));

    write_u32(payload, 0, static_cast<u32>(mPositions.size()));
    write_u32(payload, 4, static_cast<u32>(mIndices.size()));
    for (usize index = 0; index < mPositions.size(); ++index) {
        const usize at = positionsOffset + index * 12;
        write_f32(payload, at, mPositions[index].x);
        write_f32(payload, at + 4, mPositions[index].y);
        write_f32(payload, at + 8, mPositions[index].z);
    }
    for (usize index = 0; index < mIndices.size(); ++index) {
        write_u32(payload, indicesOffset + index * 4, mIndices[index]);
    }
    return payload;
}

void collision_mesh_asset::set_shapes(std::shared_ptr<const detail::collision_mesh_shapes> shapes) noexcept {
    mShapes = std::move(shapes);
}


/*===========================================================================*/
/*===========================================================================*/
// Jolt shape construction (collision_mesh_internal.h)

core::result<JPH::RefConst<JPH::Shape>> detail::build_mesh_shape(const collision_mesh_asset &mesh) {
    JPH::VertexList vertices = {};
    vertices.reserve(mesh.vertex_count());
    for (const math::vec3 &position : mesh.positions()) {
        vertices.push_back(JPH::Float3(position.x, position.y, position.z));
    }
    JPH::IndexedTriangleList triangles = {};
    triangles.reserve(mesh.triangle_count());
    const std::span<const u32> indices = mesh.indices();
    for (usize triangle = 0; triangle + 2 < indices.size(); triangle += 3) {
        triangles.push_back(JPH::IndexedTriangle(indices[triangle], indices[triangle + 1], indices[triangle + 2], 0));
    }

    JPH::MeshShapeSettings settings(std::move(vertices), std::move(triangles));
    const JPH::Shape::ShapeResult created = settings.Create();
    if (created.HasError()) {
        return jolt_failure("mesh shape", created.GetError());
    }
    return JPH::RefConst<JPH::Shape>(created.Get());
}

core::result<JPH::RefConst<JPH::Shape>> detail::build_convex_shape(const collision_mesh_asset &mesh, f32 convexRadius) {
    JPH::Array<JPH::Vec3> points = {};
    points.reserve(mesh.vertex_count());
    for (const math::vec3 &position : mesh.positions()) {
        points.push_back(jolt::to_jolt(position));
    }

    JPH::ConvexHullShapeSettings settings(points, convexRadius);
    const JPH::Shape::ShapeResult created = settings.Create();
    if (created.HasError()) {
        return jolt_failure("convex hull", created.GetError());
    }
    return JPH::RefConst<JPH::Shape>(created.Get());
}
}
