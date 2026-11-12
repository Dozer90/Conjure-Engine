#include "conjure/renderer/mesh_asset.h"
#include "conjure/renderer/renderer_error.h"
#include "renderer/assets/blob_io.h"
#include "renderer/import/tangent_generator.h"

#include "conjure/core/assert.h"
#include "conjure/core/error.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <utility>

using namespace conjure;
using namespace conjure::renderer;

namespace {
    constexpr std::array<char, 4> MESH_MAGIC = {'C', 'J', 'M', 'S'};
    constexpr usize MAX_MESH_VERTICES = usize{1} << 28;
    constexpr usize MAX_MESH_INDICES = usize{1} << 30;
    constexpr usize MAX_MESH_SUBMESHES = usize{1} << 16;

    [[nodiscard]] core::error invalid_geometry(std::string_view detail) {
        return core::make_error(core::error_code::INVALID_ARGUMENT, "mesh_asset::from_geometry: {}", detail);
    }

    [[nodiscard]] bool is_finite(const math::vec3 &value) noexcept {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    }

    [[nodiscard]] bool all_finite(std::span<const math::vec3> values) noexcept {
        return std::all_of(values.begin(), values.end(), [](const math::vec3 &value) { return is_finite(value); });
    }

    /// <summary>
    /// Finite corners with min <= max on every axis (what from_geometry computes for real positions).
    /// </summary>
    [[nodiscard]] bool is_valid_bounds(const math::aabb &bounds) noexcept {
        if (!is_finite(bounds.min) || !is_finite(bounds.max)) {
            return false;
        }
        return bounds.min.x <= bounds.max.x && bounds.min.y <= bounds.max.y && bounds.min.z <= bounds.max.z;
    }

    [[nodiscard]] u8 to_unorm8(f32 value) noexcept {
        return static_cast<u8>(std::lround(math::saturate(value) * 255.0f));
    }

    /// <summary>
    /// Duplicates every attribute per triangle corner so flat normals can be assigned.
    /// </summary>
    [[nodiscard]] mesh_geometry unweld(const mesh_geometry &source) {
        mesh_geometry result = {};
        const usize count = source.indices.size();
        result.positions.reserve(count);
        result.indices.reserve(count);
        const bool hasTangents = !source.tangents.empty();
        const bool hasUv0 = !source.uv0.empty();
        const bool hasUv1 = !source.uv1.empty();
        const bool hasColours = !source.colours.empty();
        for (usize index = 0; index < count; ++index) {
            const u32 vertex = source.indices[index];
            result.positions.push_back(source.positions[vertex]);
            if (hasTangents) {
                result.tangents.push_back(source.tangents[vertex]);
            }
            if (hasUv0) {
                result.uv0.push_back(source.uv0[vertex]);
            }
            if (hasUv1) {
                result.uv1.push_back(source.uv1[vertex]);
            }
            if (hasColours) {
                result.colours.push_back(source.colours[vertex]);
            }
            result.indices.push_back(static_cast<u32>(index));
        }
        result.submeshIndexCounts = source.submeshIndexCounts;
        result.submeshMaterialSlots = source.submeshMaterialSlots;
        return result;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Packing

i16 conjure::renderer::pack_snorm16(f32 value) noexcept {
    const f32 clamped = math::clamp(value, -1.0f, 1.0f);
    return static_cast<i16>(std::lround(clamped * 32767.0f));
}

f32 conjure::renderer::unpack_snorm16(i16 value) noexcept {
    const f32 unpacked = static_cast<f32>(value) / 32767.0f;
    return math::clamp(unpacked, -1.0f, 1.0f);
}

u16 conjure::renderer::pack_half(f32 value) noexcept {
    const u32 bits = std::bit_cast<u32>(value);
    const u32 sign = (bits >> 16) & 0x8000u;
    const u32 exponent = (bits >> 23) & 0xFFu;
    const u32 mantissa = bits & 0x7FFFFFu;

    if (exponent == 0xFFu) {
        // Infinity keeps a zero mantissa, NaN keeps a non-zero one.
        const u32 nanBits = mantissa != 0 ? 0x200u : 0u;
        return static_cast<u16>(sign | 0x7C00u | nanBits);
    }

    const i32 unbiased = static_cast<i32>(exponent) - 127 + 15;
    if (unbiased >= 0x1F) {
        return static_cast<u16>(sign | 0x7C00u);
    }
    if (unbiased <= 0) {
        if (unbiased < -10) {
            return static_cast<u16>(sign);
        }
        // Subnormal half: shift the implicit one in and round to nearest even.
        const u32 fullMantissa = mantissa | 0x800000u;
        const u32 shift = static_cast<u32>(14 - unbiased);
        u32 half = fullMantissa >> shift;
        const u32 remainder = fullMantissa & ((1u << shift) - 1u);
        const u32 halfway = 1u << (shift - 1);
        if (remainder > halfway || (remainder == halfway && (half & 1u) != 0)) {
            ++half;
        }
        return static_cast<u16>(sign | half);
    }

    u32 half = sign | (static_cast<u32>(unbiased) << 10) | (mantissa >> 13);
    const u32 remainder = mantissa & 0x1FFFu;
    if (remainder > 0x1000u || (remainder == 0x1000u && (half & 1u) != 0)) {
        ++half;   // may carry into the exponent, which is the correct rounding
    }
    return static_cast<u16>(half);
}

f32 conjure::renderer::unpack_half(u16 value) noexcept {
    const u32 sign = (static_cast<u32>(value) & 0x8000u) << 16;
    const u32 exponent = (value >> 10) & 0x1Fu;
    const u32 mantissa = value & 0x3FFu;

    if (exponent == 0) {
        if (mantissa == 0) {
            return std::bit_cast<f32>(sign);
        }
        // Subnormal: normalise.
        u32 shiftedMantissa = mantissa;
        i32 adjust = -1;
        while ((shiftedMantissa & 0x400u) == 0) {
            shiftedMantissa <<= 1;
            ++adjust;
        }
        shiftedMantissa &= 0x3FFu;
        const u32 bits = sign | (static_cast<u32>(127 - 15 - adjust) << 23) | (shiftedMantissa << 13);
        return std::bit_cast<f32>(bits);
    }
    if (exponent == 0x1Fu) {
        const u32 bits = sign | 0x7F800000u | (mantissa << 13);
        return std::bit_cast<f32>(bits);
    }
    const u32 bits = sign | ((exponent + 127 - 15) << 23) | (mantissa << 13);
    return std::bit_cast<f32>(bits);
}

u32 conjure::renderer::pack_rgba8(const math::vec4 &colour) noexcept {
    return static_cast<u32>(to_unorm8(colour.x)) | (static_cast<u32>(to_unorm8(colour.y)) << 8) |
      (static_cast<u32>(to_unorm8(colour.z)) << 16) | (static_cast<u32>(to_unorm8(colour.w)) << 24);
}

math::vec4 conjure::renderer::unpack_rgba8(u32 packed) noexcept {
    return math::vec4(
      static_cast<f32>(packed & 0xFFu) / 255.0f,
      static_cast<f32>((packed >> 8) & 0xFFu) / 255.0f,
      static_cast<f32>((packed >> 16) & 0xFFu) / 255.0f,
      static_cast<f32>((packed >> 24) & 0xFFu) / 255.0f
    );
}

packed_vertex_attributes conjure::renderer::pack_vertex_attributes(
  const math::vec3 &normal,
  const math::vec3 &tangent,
  f32 tangentSign,
  const math::vec2 &uv0,
  const math::vec2 &uv1,
  const math::vec4 &colour
) noexcept {
    const math::vec3 unitNormal = math::safe_normalize(normal, math::vec3(0.0f, 0.0f, 1.0f));
    const math::vec3 unitTangent = math::safe_normalize(tangent, math::vec3(1.0f, 0.0f, 0.0f));
    packed_vertex_attributes packed = {};
    packed.normal = {pack_snorm16(unitNormal.x), pack_snorm16(unitNormal.y), pack_snorm16(unitNormal.z),
      pack_snorm16(tangentSign < 0.0f ? -1.0f : 1.0f)};
    packed.tangent = {pack_snorm16(unitTangent.x), pack_snorm16(unitTangent.y), pack_snorm16(unitTangent.z), 0};
    packed.uv0 = uv0;
    packed.uv1 = {pack_half(uv1.x), pack_half(uv1.y)};
    packed.colour = pack_rgba8(colour);
    return packed;
}

/*===========================================================================*/
/*===========================================================================*/
// mesh_asset

mesh_asset::mesh_asset() {}

mesh_asset::~mesh_asset() {}

core::result<mesh_asset> mesh_asset::from_geometry(const mesh_geometry &geometry, std::string name) {
    if (geometry.positions.empty()) {
        return invalid_geometry("no positions");
    }
    if (geometry.positions.size() > MAX_MESH_VERTICES) {
        return invalid_geometry("too many vertices");
    }
    if (geometry.indices.empty() || geometry.indices.size() % 3 != 0) {
        return invalid_geometry("index count must be a positive multiple of three");
    }
    const usize vertexCount = geometry.positions.size();
    const auto checkAttribute = [vertexCount](usize size, std::string_view attribute) -> core::result<void> {
        if (size != 0 && size != vertexCount) {
            return invalid_geometry(std::string(attribute) + " count differs from the position count");
        }
        return core::result<void>();
    };
    CONJURE_TRY(checkAttribute(geometry.normals.size(), "normal"));
    CONJURE_TRY(checkAttribute(geometry.tangents.size(), "tangent"));
    CONJURE_TRY(checkAttribute(geometry.uv0.size(), "uv0"));
    CONJURE_TRY(checkAttribute(geometry.uv1.size(), "uv1"));
    CONJURE_TRY(checkAttribute(geometry.colours.size(), "colour"));
    if (!all_finite(geometry.positions)) {
        return invalid_geometry("non-finite position");
    }
    for (const u32 index : geometry.indices) {
        if (index >= vertexCount) {
            return invalid_geometry("index out of range");
        }
    }

    // Submesh partition.
    std::vector<u32> indexCounts = geometry.submeshIndexCounts;
    if (indexCounts.empty()) {
        indexCounts.push_back(static_cast<u32>(geometry.indices.size()));
    }
    if (indexCounts.size() > MAX_MESH_SUBMESHES) {
        return invalid_geometry("too many submeshes");
    }
    u64 partitionTotal = 0;
    for (const u32 count : indexCounts) {
        if (count == 0 || count % 3 != 0) {
            return invalid_geometry("submesh index counts must be positive multiples of three");
        }
        partitionTotal += count;
    }
    if (partitionTotal != geometry.indices.size()) {
        return invalid_geometry("submesh index counts do not partition the index list");
    }
    if (!geometry.submeshMaterialSlots.empty() && geometry.submeshMaterialSlots.size() != indexCounts.size()) {
        return invalid_geometry("one material slot per submesh is required");
    }
    for (const u32 slot : geometry.submeshMaterialSlots) {
        if (slot >= MAX_MATERIAL_SLOTS) {
            return invalid_geometry("material slot out of range");
        }
    }

    // Missing normals are flat per face, which needs one vertex per corner.
    const mesh_geometry *source = &geometry;
    mesh_geometry unwelded = {};
    std::vector<math::vec3> generatedNormals = {};
    if (geometry.normals.empty()) {
        unwelded = unweld(geometry);
        generatedNormals = detail::flat_normals(unwelded.positions, unwelded.indices);
        source = &unwelded;
    }
    const std::span<const math::vec3> normals = source->normals.empty() ? std::span<const math::vec3>(generatedNormals)
                                                                        : std::span<const math::vec3>(source->normals);

    std::vector<math::vec4> generatedTangents = {};
    if (source->tangents.empty()) {
        generatedTangents = detail::generate_tangents(source->positions, normals, source->uv0, source->indices);
    }
    const std::span<const math::vec4> tangents = source->tangents.empty()
      ? std::span<const math::vec4>(generatedTangents)
                                                                          : std::span<const math::vec4>(
                                                                            source->tangents
                                                                          );

    mesh_asset result = {};
    result.mName = std::move(name);
    const usize packedCount = source->positions.size();
    result.mStreams.positions = source->positions;
    result.mStreams.indices = source->indices;
    result.mStreams.attributes.resize(packedCount);
    for (usize index = 0; index < packedCount; ++index) {
        const math::vec2 uv0 = source->uv0.empty() ? math::vec2(0.0f) : source->uv0[index];
        const math::vec2 uv1 = source->uv1.empty() ? math::vec2(0.0f) : source->uv1[index];
        const math::vec4 colour = source->colours.empty() ? math::vec4(1.0f) : source->colours[index];
        const math::vec4 tangent = tangents[index];
        result.mStreams.attributes[index] =
          pack_vertex_attributes(normals[index], math::vec3(tangent), tangent.w, uv0, uv1, colour);
        result.mBounds.expand(source->positions[index]);
    }

    u32 firstIndex = 0;
    for (usize submeshIndex = 0; submeshIndex < indexCounts.size(); ++submeshIndex) {
        submesh part = {};
        part.firstIndex = firstIndex;
        part.indexCount = indexCounts[submeshIndex];
        part.materialSlot = geometry.submeshMaterialSlots.empty()
          ? static_cast<u32>(std::min<usize>(submeshIndex, MAX_MATERIAL_SLOTS - 1))
          : geometry.submeshMaterialSlots[submeshIndex];
        for (u32 offset = 0; offset < part.indexCount; ++offset) {
            part.bounds.expand(source->positions[source->indices[firstIndex + offset]]);
        }
        firstIndex += part.indexCount;
        result.mSubmeshes.push_back(part);
    }
    return result;
}

mesh_asset mesh_asset::unit_cube() {
    mesh_geometry geometry = {};
    const std::array<math::vec3, 6> normals = {
      math::vec3(1.0f, 0.0f, 0.0f),  math::vec3(-1.0f, 0.0f, 0.0f), math::vec3(0.0f, 1.0f, 0.0f),
      math::vec3(0.0f, -1.0f, 0.0f), math::vec3(0.0f, 0.0f, 1.0f),  math::vec3(0.0f, 0.0f, -1.0f),
    };
    const std::array<math::vec3, 6> tangents = {
      math::vec3(0.0f, 0.0f, -1.0f), math::vec3(0.0f, 0.0f, 1.0f), math::vec3(1.0f, 0.0f, 0.0f),
      math::vec3(1.0f, 0.0f, 0.0f),  math::vec3(1.0f, 0.0f, 0.0f), math::vec3(-1.0f, 0.0f, 0.0f),
    };
    for (usize face = 0; face < 6; ++face) {
        const math::vec3 normal = normals[face];
        const math::vec3 tangent = tangents[face];
        const math::vec3 bitangent = glm::cross(normal, tangent);
        const u32 base = static_cast<u32>(geometry.positions.size());
        const std::array<math::vec2, 4> corners = {
          math::vec2(-1.0f, -1.0f), math::vec2(1.0f, -1.0f), math::vec2(1.0f, 1.0f), math::vec2(-1.0f, 1.0f)
        };
        for (const math::vec2 &corner : corners) {
            geometry.positions.push_back((normal + tangent * corner.x + bitangent * corner.y) * 0.5f);
            geometry.normals.push_back(normal);
            geometry.tangents.push_back(math::vec4(tangent, 1.0f));
            geometry.uv0.push_back(math::vec2(corner.x * 0.5f + 0.5f, 0.5f - corner.y * 0.5f));
        }
        geometry.indices.insert(geometry.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }
    core::result<mesh_asset> built = from_geometry(geometry, "unit_cube");
    CONJURE_ASSERT(built.has_value(), "unit_cube geometry is always valid");
    return built.has_value() ? std::move(built).value() : mesh_asset();
}

mesh_asset mesh_asset::uv_sphere(f32 radius, u32 rings, u32 segments) {
    const u32 ringCount = std::max<u32>(rings, 2);
    const u32 segmentCount = std::max<u32>(segments, 3);
    mesh_geometry geometry = {};
    for (u32 ring = 0; ring <= ringCount; ++ring) {
        const f32 v = static_cast<f32>(ring) / static_cast<f32>(ringCount);
        const f32 polar = v * math::PI;
        const f32 y = std::cos(polar);
        const f32 ringRadius = std::sin(polar);
        for (u32 segment = 0; segment <= segmentCount; ++segment) {
            const f32 u = static_cast<f32>(segment) / static_cast<f32>(segmentCount);
            const f32 azimuth = u * math::TWO_PI;
            const math::vec3 normal(ringRadius * std::cos(azimuth), y, ringRadius * std::sin(azimuth));
            geometry.positions.push_back(normal * radius);
            geometry.normals.push_back(normal);
            geometry.tangents.push_back(math::vec4(-std::sin(azimuth), 0.0f, std::cos(azimuth), 1.0f));
            geometry.uv0.push_back(math::vec2(u, v));
        }
    }
    const u32 stride = segmentCount + 1;
    for (u32 ring = 0; ring < ringCount; ++ring) {
        for (u32 segment = 0; segment < segmentCount; ++segment) {
            const u32 a = ring * stride + segment;
            const u32 b = a + stride;
            // Counter-clockwise seen from outside: ring above first, then the next segment.
            geometry.indices.insert(geometry.indices.end(), {a, a + 1, b, a + 1, b + 1, b});
        }
    }
    core::result<mesh_asset> built = from_geometry(geometry, "uv_sphere");
    CONJURE_ASSERT(built.has_value(), "uv_sphere geometry is always valid");
    return built.has_value() ? std::move(built).value() : mesh_asset();
}

mesh_asset mesh_asset::quad(f32 halfSize) {
    mesh_geometry geometry = {};
    geometry.positions = {
      math::vec3(-halfSize, -halfSize, 0.0f), math::vec3(halfSize, -halfSize, 0.0f),
      math::vec3(halfSize, halfSize, 0.0f),   math::vec3(-halfSize, halfSize, 0.0f),
    };
    geometry.normals.assign(4, math::vec3(0.0f, 0.0f, 1.0f));
    geometry.uv0 = {math::vec2(0.0f, 1.0f), math::vec2(1.0f, 1.0f), math::vec2(1.0f, 0.0f), math::vec2(0.0f, 0.0f)};
    geometry.indices = {0, 1, 2, 0, 2, 3};
    core::result<mesh_asset> built = from_geometry(geometry, "quad");
    CONJURE_ASSERT(built.has_value(), "quad geometry is always valid");
    return built.has_value() ? std::move(built).value() : mesh_asset();
}

core::result<mesh_asset> mesh_asset::parse_blob(std::span<const std::byte> bytes) {
    detail::blob_reader reader(bytes, "cmesh");
    CONJURE_TRY(reader.expect_magic(MESH_MAGIC));
    u32 version = 0;
    CONJURE_TRY_ASSIGN(version, reader.read_u32());
    if (version != MESH_BLOB_VERSION) {
        return reader.invalid("unsupported version");
    }
    u32 vertexCount = 0;
    u32 indexCount = 0;
    u32 submeshCount = 0;
    CONJURE_TRY_ASSIGN(vertexCount, reader.read_u32());
    CONJURE_TRY_ASSIGN(indexCount, reader.read_u32());
    CONJURE_TRY_ASSIGN(submeshCount, reader.read_u32());
    if (vertexCount == 0 || vertexCount > MAX_MESH_VERTICES) {
        return reader.invalid("vertex count out of range");
    }
    if (indexCount == 0 || indexCount % 3 != 0 || indexCount > MAX_MESH_INDICES) {
        return reader.invalid("index count out of range");
    }
    if (submeshCount == 0 || submeshCount > MAX_MESH_SUBMESHES) {
        return reader.invalid("submesh count out of range");
    }

    mesh_asset result = {};
    CONJURE_TRY_ASSIGN(result.mName, reader.read_string(detail::MAX_BLOB_NAME_LENGTH));
    CONJURE_TRY_ASSIGN(result.mStreams.positions, reader.read_array<math::vec3>(vertexCount, "positions"));
    if (!all_finite(result.mStreams.positions)) {
        return reader.invalid("non-finite position");
    }
    CONJURE_TRY_ASSIGN(
      result.mStreams.attributes,
      reader.read_array<packed_vertex_attributes>(vertexCount, "attributes")
    );
    CONJURE_TRY_ASSIGN(result.mStreams.indices, reader.read_array<u32>(indexCount, "indices"));
    for (const u32 index : result.mStreams.indices) {
        if (index >= vertexCount) {
            return reader.invalid("index out of range");
        }
    }
    u32 expectedFirst = 0;
    for (u32 submeshIndex = 0; submeshIndex < submeshCount; ++submeshIndex) {
        submesh part = {};
        CONJURE_TRY_ASSIGN(part.firstIndex, reader.read_u32());
        CONJURE_TRY_ASSIGN(part.indexCount, reader.read_u32());
        CONJURE_TRY_ASSIGN(part.materialSlot, reader.read_u32());
        CONJURE_TRY_ASSIGN(part.bounds.min, reader.read_vec3());
        CONJURE_TRY_ASSIGN(part.bounds.max, reader.read_vec3());
        if (part.firstIndex != expectedFirst || part.indexCount == 0 || part.indexCount % 3 != 0 ||
          static_cast<u64>(part.firstIndex) + part.indexCount > indexCount || part.materialSlot >= MAX_MATERIAL_SLOTS) {
            return reader.invalid("submesh range is inconsistent");
        }
        if (!is_valid_bounds(part.bounds)) {
            return reader.invalid("submesh bounds are not finite or inverted");
        }
        expectedFirst += part.indexCount;
        result.mSubmeshes.push_back(part);
    }
    if (expectedFirst != indexCount) {
        return reader.invalid("submeshes do not cover the index list");
    }
    CONJURE_TRY_ASSIGN(result.mBounds.min, reader.read_vec3());
    CONJURE_TRY_ASSIGN(result.mBounds.max, reader.read_vec3());
    if (!is_valid_bounds(result.mBounds)) {
        return reader.invalid("mesh bounds are not finite or inverted");
    }
    if (reader.remaining() != 0) {
        return reader.invalid("trailing bytes");
    }
    return result;
}

std::vector<std::byte> mesh_asset::write_blob() const {
    detail::blob_writer writer = {};
    writer.write_magic(MESH_MAGIC);
    writer.write_u32(MESH_BLOB_VERSION);
    writer.write_u32(vertex_count());
    writer.write_u32(index_count());
    writer.write_u32(static_cast<u32>(mSubmeshes.size()));
    writer.write_string(mName);
    writer.write_array<math::vec3>(mStreams.positions);
    writer.write_array<packed_vertex_attributes>(mStreams.attributes);
    writer.write_array<u32>(mStreams.indices);
    for (const submesh &part : mSubmeshes) {
        writer.write_u32(part.firstIndex);
        writer.write_u32(part.indexCount);
        writer.write_u32(part.materialSlot);
        writer.write_vec3(part.bounds.min);
        writer.write_vec3(part.bounds.max);
    }
    writer.write_vec3(mBounds.min);
    writer.write_vec3(mBounds.max);
    return writer.finish();
}

cpu_geometry_view mesh_asset::cpu_geometry() const noexcept {
    cpu_geometry_view view = {};
    view.positions = mStreams.positions;
    view.indices = mStreams.indices;
    return view;
}

u64 mesh_asset::gpu_byte_size() const noexcept {
    return static_cast<u64>(mStreams.positions.size()) * sizeof(math::vec3) +
      static_cast<u64>(mStreams.attributes.size()) * sizeof(packed_vertex_attributes) +
      static_cast<u64>(mStreams.indices.size()) * sizeof(u32);
}

bool mesh_asset::operator==(const mesh_asset &other) const noexcept {
    return mName == other.mName && mBounds == other.mBounds && mSubmeshes == other.mSubmeshes &&
      mStreams.positions == other.mStreams.positions && mStreams.attributes == other.mStreams.attributes &&
      mStreams.indices == other.mStreams.indices;
}

/*===========================================================================*/
/*===========================================================================*/
// mesh_loader

mesh_loader::mesh_loader() {}

mesh_loader::~mesh_loader() {}

core::result<mesh_asset> mesh_loader::load_cpu_typed(
  const assets::load_context &,
  std::span<const std::byte> payloadBytes
) const {
    return mesh_asset::parse_blob(payloadBytes);
}

core::result<mesh_asset> mesh_loader::finalise_typed(
  const assets::finalise_context &,
  mesh_asset &&intermediate
) const {
    return std::move(intermediate);
}

std::optional<mesh_asset> mesh_loader::create_fallback_typed() const {
    return mesh_asset::unit_cube();
}
