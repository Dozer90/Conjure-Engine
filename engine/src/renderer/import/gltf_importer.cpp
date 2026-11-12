#include "conjure/renderer/gltf_importer.h"
#include "conjure/renderer/log_category.h"
#include "renderer/import/file_bytes.h"

#include "conjure/assets/assets_error.h"
#include "conjure/core/assert.h"
#include "conjure/core/error.h"
#include "conjure/core/memory/tracked_allocation.h"
#include "conjure/core/vfs/path_utils.h"

#include <cgltf.h>

#include <algorithm>
#include <cstring>
#include <format>
#include <limits>
#include <memory>
#include <utility>

// glTF 2.0 through cgltf (docs/design/02_renderer.md 4.2). cgltf parses the JSON / GLB container;
// buffers are resolved here (GLB chunk, data URI, or the caller's reader) so the importer never
// touches the file system itself and runs unchanged on the asset pipeline's worker threads.

using namespace conjure;
using namespace conjure::renderer;

namespace {
    constexpr std::string_view KEY_GENERATE_TANGENTS = "generateTangents";
    constexpr std::string_view KEY_SCALE = "scale";
    constexpr std::string_view KEY_IMPORT_CAMERAS = "importCameras";
    constexpr std::string_view KEY_IMPORT_LIGHTS = "importLights";

    constexpr int GLTF_FILTER_NEAREST = 9728;
    constexpr int GLTF_FILTER_NEAREST_MIPMAP_NEAREST = 9984;
    constexpr int GLTF_FILTER_NEAREST_MIPMAP_LINEAR = 9986;
    constexpr int GLTF_WRAP_CLAMP = 33071;
    constexpr int GLTF_WRAP_MIRROR = 33648;

    // glTF caps byteStride at 252; cgltf does not, and cgltf_validate multiplies the stride by the
    // element count without an overflow check.
    constexpr cgltf_size MAX_BYTE_STRIDE = 252;

    constexpr std::array<std::string_view, 3> SUPPORTED_EXTENSIONS = {
      "KHR_materials_emissive_strength", "KHR_materials_unlit", "KHR_lights_punctual"
    };

    /// <summary>
    /// cgltf allocates through these hooks so its memory is counted by core::memory like everything
    /// else (CONVENTIONS.md 3: no malloc outside core::memory). tracked_free needs the size back, so
    /// every block carries it in a 16-byte header.
    /// </summary>
    constexpr usize ALLOCATION_HEADER = core::memory::DEFAULT_ALIGNMENT;

    void *gltf_alloc(void *, cgltf_size size) {
        const usize total = static_cast<usize>(size) + ALLOCATION_HEADER;
        void *block = core::memory::tracked_allocate(total, core::memory::DEFAULT_ALIGNMENT);
        if (block == nullptr) {
            return nullptr;
        }
        std::memcpy(block, &total, sizeof(total));
        return static_cast<std::byte *>(block) + ALLOCATION_HEADER;
    }

    void gltf_free(void *, void *pointer) {
        if (pointer == nullptr) {
            return;
        }
        std::byte *block = static_cast<std::byte *>(pointer) - ALLOCATION_HEADER;
        usize total = 0;
        std::memcpy(&total, block, sizeof(total));
        core::memory::tracked_free(block, total);
    }

    [[nodiscard]] cgltf_options make_options() noexcept {
        cgltf_options options = {};
        options.memory.alloc_func = &gltf_alloc;
        options.memory.free_func = &gltf_free;
        return options;
    }

    /// <summary>
    /// Owns the cgltf document and the buffer storage resolved for it.
    /// </summary>
    struct gltf_document {
        cgltf_data *data = nullptr;
        std::vector<std::vector<std::byte>> ownedBuffers = {};

        gltf_document() = default;
        ~gltf_document() {
            if (data != nullptr) {
                cgltf_free(data);
            }
        }

        gltf_document(const gltf_document &) = delete;
        gltf_document &operator=(const gltf_document &) = delete;
    };

    [[nodiscard]] core::error parse_failure(std::string_view detail) {
        return core::make_error(core::error_code::PARSE_FAILURE, "gltf: {}", detail);
    }

    [[nodiscard]] std::string_view cgltf_result_name(cgltf_result result) noexcept {
        switch (result) {
            case cgltf_result_success:
                return "success";
            case cgltf_result_data_too_short:
                return "data too short";
            case cgltf_result_unknown_format:
                return "unknown format";
            case cgltf_result_invalid_json:
                return "invalid JSON";
            case cgltf_result_invalid_gltf:
                return "invalid glTF";
            case cgltf_result_invalid_options:
                return "invalid options";
            case cgltf_result_file_not_found:
                return "file not found";
            case cgltf_result_io_error:
                return "I/O error";
            case cgltf_result_out_of_memory:
                return "out of memory";
            case cgltf_result_legacy_gltf:
                return "legacy glTF 1.0";
            default:
                return "unknown error";
        }
    }

    [[nodiscard]] u32 index_of(const cgltf_data &data, const cgltf_material *material) noexcept {
        return material == nullptr ? INVALID_INDEX : static_cast<u32>(material - data.materials);
    }

    [[nodiscard]] u32 index_of(const cgltf_data &data, const cgltf_texture *texture) noexcept {
        return texture == nullptr ? INVALID_INDEX : static_cast<u32>(texture - data.textures);
    }

    [[nodiscard]] u32 index_of(const cgltf_data &data, const cgltf_mesh *mesh) noexcept {
        return mesh == nullptr ? INVALID_INDEX : static_cast<u32>(mesh - data.meshes);
    }

    /// <summary>
    /// Resolves every buffer: the GLB binary chunk, an embedded data URI or a file read through
    /// 'reader'. A null reader makes external files fail with NOT_FOUND.
    /// </summary>
    [[nodiscard]] core::result<void> resolve_buffers(gltf_document &document, const gltf_file_reader &reader) {
        cgltf_data &data = *document.data;
        document.ownedBuffers.resize(data.buffers_count);
        for (cgltf_size index = 0; index < data.buffers_count; ++index) {
            cgltf_buffer &buffer = data.buffers[index];
            if (buffer.data != nullptr) {
                continue;
            }
            if (buffer.uri == nullptr) {
                if (data.bin == nullptr || data.bin_size < buffer.size) {
                    return core::make_error(core::error_code::NOT_FOUND, "gltf: buffer {} has no data", index);
                }
                buffer.data = const_cast<void *>(data.bin);
                buffer.data_free_method = cgltf_data_free_method_none;
                continue;
            }
            const std::string_view uri(buffer.uri);
            if (uri.starts_with("data:")) {
                const usize comma = uri.find(',');
                if (comma == std::string_view::npos || !uri.substr(0, comma).ends_with(";base64")) {
                    return parse_failure(std::format("buffer {} has a data URI that is not base64", index));
                }
                const cgltf_options options = make_options();
                void *decoded = nullptr;
                const cgltf_result result = cgltf_load_buffer_base64(
                  &options,
                  buffer.size,
                  buffer.uri + comma + 1,
                  &decoded
                );
                if (result != cgltf_result_success || decoded == nullptr) {
                    return parse_failure(std::format("buffer {} base64 data does not decode", index));
                }
                buffer.data = decoded;
                buffer.data_free_method = cgltf_data_free_method_memory_free;
                continue;
            }
            if (!reader) {
                return core::make_error(core::error_code::NOT_FOUND, "gltf: external buffer '{}' cannot be read", uri);
            }
            std::string path(uri);
            cgltf_decode_uri(path.data());
            path.resize(std::strlen(path.c_str()));
            core::result<std::vector<std::byte>> bytes = reader(path);
            if (!bytes.has_value()) {
                return core::make_error(
                  core::error_code::NOT_FOUND,
                  "gltf: external buffer '{}': {}",
                  path,
                  bytes.get_error().message()
                );
            }
            if (bytes.value().size() < buffer.size) {
                return parse_failure(std::format("external buffer '{}' is shorter than declared", path));
            }
            document.ownedBuffers[index] = std::move(bytes).value();
            buffer.data = document.ownedBuffers[index].data();
            buffer.data_free_method = cgltf_data_free_method_none;
        }
        return core::result<void>();
    }

    /// <summary>
    /// Bounds what cgltf_validate multiplies without overflow checks, so its range checks cannot wrap:
    /// every buffer view lies inside its resolved buffer with a byteStride of at most 252, and no
    /// accessor or sparse block has more elements than its views have bytes. cgltf parses offsets as
    /// at most 2^63 - 1, so offset + stride * (count - 1) + element size stays below 2^64. Reads no
    /// buffer data, unlike cgltf_validate, which scans index data through the views.
    /// </summary>
    [[nodiscard]] core::result<void> check_view_ranges(const cgltf_data &data) {
        for (cgltf_size index = 0; index < data.buffer_views_count; ++index) {
            const cgltf_buffer_view &view = data.buffer_views[index];
            const bool outside = view.buffer == nullptr || view.offset > view.buffer->size
              || view.size > view.buffer->size - view.offset;
            if (outside) {
                return parse_failure(std::format("invalid document: buffer view {} lies outside its buffer", index));
            }
            if (view.stride > MAX_BYTE_STRIDE) {
                return parse_failure(
                  std::format("invalid document: buffer view {} has a byteStride above {}", index, MAX_BYTE_STRIDE)
                );
            }
        }
        for (cgltf_size index = 0; index < data.accessors_count; ++index) {
            const cgltf_accessor &accessor = data.accessors[index];
            const cgltf_accessor_sparse &sparse = accessor.sparse;
            const bool denseTooLong = accessor.buffer_view != nullptr && accessor.count > accessor.buffer_view->size;
            // cgltf_parse requires both sparse views, so they are set whenever is_sparse is.
            const bool sparseTooLong = accessor.is_sparse
              && (sparse.count > accessor.count || sparse.count > sparse.indices_buffer_view->size
                  || sparse.count > sparse.values_buffer_view->size);
            if (denseTooLong || sparseTooLong) {
                return parse_failure(
                  std::format("invalid document: accessor {} has more elements than its buffer views hold", index)
                );
            }
        }
        return core::result<void>();
    }

    [[nodiscard]] core::result<std::vector<f32>> unpack_floats(const cgltf_accessor &accessor, cgltf_size components) {
        const cgltf_size available = cgltf_num_components(accessor.type);
        if (available < components) {
            return core::make_error(
              core::error_code::UNSUPPORTED,
              "gltf: accessor has {} components, {} needed",
              available,
              components
            );
        }
        if (accessor.count == 0 || (accessor.buffer_view == nullptr && !accessor.is_sparse)) {
            return core::make_error(core::error_code::UNSUPPORTED, "gltf: accessor without data");
        }
        std::vector<f32> values(accessor.count * available);
        const cgltf_size unpacked = cgltf_accessor_unpack_floats(&accessor, values.data(), values.size());
        if (unpacked != values.size()) {
            return core::make_error(core::error_code::UNSUPPORTED, "gltf: accessor layout cannot be unpacked");
        }
        if (available == components) {
            return values;
        }
        std::vector<f32> narrowed(accessor.count * components);
        for (cgltf_size element = 0; element < accessor.count; ++element) {
            for (cgltf_size component = 0; component < components; ++component) {
                narrowed[element * components + component] = values[element * available + component];
            }
        }
        return narrowed;
    }

    [[nodiscard]] core::result<std::vector<u32>> triangulate(const cgltf_primitive &primitive, u32 vertexCount) {
        std::vector<u32> sequence = {};
        if (primitive.indices != nullptr) {
            const cgltf_accessor &accessor = *primitive.indices;
            if (accessor.buffer_view == nullptr && !accessor.is_sparse) {
                return core::make_error(core::error_code::UNSUPPORTED, "gltf: index accessor without data");
            }
            sequence.resize(accessor.count);
            for (cgltf_size index = 0; index < accessor.count; ++index) {
                const cgltf_size value = cgltf_accessor_read_index(&accessor, index);
                if (value >= vertexCount) {
                    return parse_failure("index out of range");
                }
                sequence[index] = static_cast<u32>(value);
            }
        } else {
            sequence.resize(vertexCount);
            for (u32 index = 0; index < vertexCount; ++index) {
                sequence[index] = index;
            }
        }

        std::vector<u32> triangles = {};
        switch (primitive.type) {
            case cgltf_primitive_type_triangles: {
                if (sequence.size() < 3) {
                    return parse_failure("triangle list with fewer than three indices");
                }
                triangles.assign(
                  sequence.begin(),
                  sequence.begin() + static_cast<isize>(sequence.size() - sequence.size() % 3)
                );
                break;
            }
            case cgltf_primitive_type_triangle_strip: {
                if (sequence.size() < 3) {
                    return parse_failure("triangle strip with fewer than three indices");
                }
                for (usize index = 0; index + 2 < sequence.size(); ++index) {
                    if (index % 2 == 0) {
                        triangles.insert(triangles.end(), {sequence[index], sequence[index + 1], sequence[index + 2]});
                    } else {
                        triangles.insert(triangles.end(), {sequence[index + 1], sequence[index], sequence[index + 2]});
                    }
                }
                break;
            }
            case cgltf_primitive_type_triangle_fan: {
                if (sequence.size() < 3) {
                    return parse_failure("triangle fan with fewer than three indices");
                }
                for (usize index = 1; index + 1 < sequence.size(); ++index) {
                    triangles.insert(triangles.end(), {sequence[0], sequence[index], sequence[index + 1]});
                }
                break;
            }
            default: {
                return core::make_error(core::error_code::UNSUPPORTED, "gltf: points and lines are not supported");
            }
        }
        return triangles;
    }

    [[nodiscard]] core::result<mesh_asset> import_mesh(const cgltf_mesh &mesh, std::string name) {
        // Every primitive becomes one submesh; the geometry arrays are concatenated with offsets.
        mesh_geometry geometry = {};
        bool anyNormals = false;
        bool anyTangents = false;
        bool anyUv0 = false;
        bool anyUv1 = false;
        bool anyColours = false;
        for (cgltf_size primitiveIndex = 0; primitiveIndex < mesh.primitives_count; ++primitiveIndex) {
            const cgltf_primitive &primitive = mesh.primitives[primitiveIndex];
            for (cgltf_size attributeIndex = 0; attributeIndex < primitive.attributes_count; ++attributeIndex) {
                const cgltf_attribute &attribute = primitive.attributes[attributeIndex];
                anyNormals = anyNormals || attribute.type == cgltf_attribute_type_normal;
                anyTangents = anyTangents || attribute.type == cgltf_attribute_type_tangent;
                anyUv0 = anyUv0 || (attribute.type == cgltf_attribute_type_texcoord && attribute.index == 0);
                anyUv1 = anyUv1 || (attribute.type == cgltf_attribute_type_texcoord && attribute.index == 1);
                anyColours = anyColours || (attribute.type == cgltf_attribute_type_color && attribute.index == 0);
            }
        }
        // A mesh whose primitives disagree on normals falls back to generated attributes for all of
        // them; the common case is uniform attributes across primitives.
        bool allNormals = anyNormals;
        bool allTangents = anyTangents;

        for (cgltf_size primitiveIndex = 0; primitiveIndex < mesh.primitives_count; ++primitiveIndex) {
            const cgltf_primitive &primitive = mesh.primitives[primitiveIndex];
            const cgltf_accessor *positions = nullptr;
            const cgltf_accessor *normals = nullptr;
            const cgltf_accessor *tangents = nullptr;
            const cgltf_accessor *uv0 = nullptr;
            const cgltf_accessor *uv1 = nullptr;
            const cgltf_accessor *colours = nullptr;
            for (cgltf_size attributeIndex = 0; attributeIndex < primitive.attributes_count; ++attributeIndex) {
                const cgltf_attribute &attribute = primitive.attributes[attributeIndex];
                switch (attribute.type) {
                    case cgltf_attribute_type_position: {
                        positions = attribute.data;
                        break;
                    }
                    case cgltf_attribute_type_normal: {
                        normals = attribute.data;
                        break;
                    }
                    case cgltf_attribute_type_tangent: {
                        tangents = attribute.data;
                        break;
                    }
                    case cgltf_attribute_type_texcoord: {
                        if (attribute.index == 0) {
                            uv0 = attribute.data;
                        } else if (attribute.index == 1) {
                            uv1 = attribute.data;
                        }
                        break;
                    }
                    case cgltf_attribute_type_color: {
                        if (attribute.index == 0) {
                            colours = attribute.data;
                        }
                        break;
                    }
                    default: {
                        break;
                    }
                }
            }
            if (positions == nullptr) {
                return parse_failure(std::format("mesh '{}' primitive {} has no POSITION", name, primitiveIndex));
            }
            if (positions->count > std::numeric_limits<u32>::max() - geometry.positions.size()) {
                return parse_failure("mesh exceeds the vertex limit");
            }
            const u32 baseVertex = static_cast<u32>(geometry.positions.size());
            const u32 vertexCount = static_cast<u32>(positions->count);

            std::vector<f32> positionValues = {};
            CONJURE_TRY_ASSIGN(positionValues, unpack_floats(*positions, 3));
            for (u32 vertex = 0; vertex < vertexCount; ++vertex) {
                geometry.positions.emplace_back(
                  positionValues[vertex * 3],
                  positionValues[vertex * 3 + 1],
                  positionValues[vertex * 3 + 2]
                );
            }
            if (normals != nullptr && normals->count == vertexCount) {
                std::vector<f32> values = {};
                CONJURE_TRY_ASSIGN(values, unpack_floats(*normals, 3));
                for (u32 vertex = 0; vertex < vertexCount; ++vertex) {
                    geometry.normals.emplace_back(values[vertex * 3], values[vertex * 3 + 1], values[vertex * 3 + 2]);
                }
            } else {
                allNormals = false;
            }
            if (tangents != nullptr && tangents->count == vertexCount) {
                std::vector<f32> values = {};
                CONJURE_TRY_ASSIGN(values, unpack_floats(*tangents, 4));
                for (u32 vertex = 0; vertex < vertexCount; ++vertex) {
                    geometry.tangents.emplace_back(
                      values[vertex * 4],
                      values[vertex * 4 + 1],
                      values[vertex * 4 + 2],
                      values[vertex * 4 + 3] < 0.0f ? -1.0f : 1.0f
                    );
                }
            } else {
                allTangents = false;
            }
            if (anyUv0) {
                if (uv0 != nullptr && uv0->count == vertexCount) {
                    std::vector<f32> values = {};
                    CONJURE_TRY_ASSIGN(values, unpack_floats(*uv0, 2));
                    for (u32 vertex = 0; vertex < vertexCount; ++vertex) {
                        geometry.uv0.emplace_back(values[vertex * 2], values[vertex * 2 + 1]);
                    }
                } else {
                    geometry.uv0.resize(geometry.uv0.size() + vertexCount, math::vec2(0.0f));
                }
            }
            if (anyUv1) {
                if (uv1 != nullptr && uv1->count == vertexCount) {
                    std::vector<f32> values = {};
                    CONJURE_TRY_ASSIGN(values, unpack_floats(*uv1, 2));
                    for (u32 vertex = 0; vertex < vertexCount; ++vertex) {
                        geometry.uv1.emplace_back(values[vertex * 2], values[vertex * 2 + 1]);
                    }
                } else {
                    geometry.uv1.resize(geometry.uv1.size() + vertexCount, math::vec2(0.0f));
                }
            }
            if (anyColours) {
                if (colours != nullptr && colours->count == vertexCount) {
                    const cgltf_size components = cgltf_num_components(colours->type);
                    if (components != 3 && components != 4) {
                        return parse_failure(
                          std::format(
                            "mesh '{}' primitive {}: COLOR_0 must be VEC3 or VEC4, not {} components",
                            name,
                            primitiveIndex,
                            components
                          )
                        );
                    }
                    std::vector<f32> values = {};
                    CONJURE_TRY_ASSIGN(values, unpack_floats(*colours, components));
                    for (u32 vertex = 0; vertex < vertexCount; ++vertex) {
                        const f32 alpha = components >= 4 ? values[vertex * components + 3] : 1.0f;
                        geometry.colours.emplace_back(
                          values[vertex * components],
                          values[vertex * components + 1],
                          values[vertex * components + 2],
                          alpha
                        );
                    }
                } else {
                    geometry.colours.resize(geometry.colours.size() + vertexCount, math::vec4(1.0f));
                }
            }

            std::vector<u32> triangles = {};
            CONJURE_TRY_ASSIGN(triangles, triangulate(primitive, vertexCount));
            for (u32 &index : triangles) {
                index += baseVertex;
            }
            geometry.indices.insert(geometry.indices.end(), triangles.begin(), triangles.end());
            geometry.submeshIndexCounts.push_back(static_cast<u32>(triangles.size()));
            geometry.submeshMaterialSlots.push_back(
              static_cast<u32>(std::min<cgltf_size>(primitiveIndex, MAX_MATERIAL_SLOTS - 1))
            );
        }
        if (!allNormals) {
            geometry.normals.clear();
        }
        if (!allTangents) {
            geometry.tangents.clear();
        }
        core::result<mesh_asset> built = mesh_asset::from_geometry(geometry, std::move(name));
        if (!built.has_value()) {
            return parse_failure(built.get_error().message());
        }
        return built;
    }

    void apply_sampler(const cgltf_sampler *sampler, texture_asset &texture) noexcept {
        if (sampler == nullptr) {
            return;
        }
        const bool nearest = sampler->mag_filter == GLTF_FILTER_NEAREST ||
          sampler->min_filter == GLTF_FILTER_NEAREST_MIPMAP_NEAREST
          || sampler->min_filter == GLTF_FILTER_NEAREST_MIPMAP_LINEAR;
        texture.set_filter(nearest ? sampler_filter::POINT : sampler_filter::LINEAR);
        if (sampler->wrap_s == GLTF_WRAP_CLAMP) {
            texture.set_wrap(texture_wrap::CLAMP);
        } else if (sampler->wrap_s == GLTF_WRAP_MIRROR) {
            texture.set_wrap(texture_wrap::MIRROR);
        } else {
            texture.set_wrap(texture_wrap::REPEAT);
        }
    }

    [[nodiscard]] core::result<std::vector<std::byte>> image_bytes(
      const cgltf_image &image,
      const gltf_file_reader &reader,
      u32 imageIndex
    ) {
        if (image.buffer_view != nullptr) {
            const cgltf_buffer_view &view = *image.buffer_view;
            if (view.buffer == nullptr || view.buffer->data == nullptr) {
                return core::make_error(core::error_code::NOT_FOUND, "gltf: image {} buffer has no data", imageIndex);
            }
            // check_view_ranges already bounds every view; the copy below must not depend on that alone.
            if (view.offset > view.buffer->size || view.size > view.buffer->size - view.offset) {
                return parse_failure(std::format("image {} buffer view exceeds its buffer", imageIndex));
            }
            const std::byte *start = static_cast<const std::byte *>(view.buffer->data) + view.offset;
            return std::vector<std::byte>(start, start + view.size);
        }
        if (image.uri == nullptr) {
            return core::make_error(
              core::error_code::NOT_FOUND,
              "gltf: image {} has neither a URI nor a buffer view",
              imageIndex
            );
        }
        const std::string_view uri(image.uri);
        if (uri.starts_with("data:")) {
            const usize comma = uri.find(',');
            if (comma == std::string_view::npos || !uri.substr(0, comma).ends_with(";base64")) {
                return parse_failure(std::format("image {} has a data URI that is not base64", imageIndex));
            }
            // cgltf needs the exact decoded size: every four characters carry three bytes, a final
            // group of two or three characters carries one or two, with or without '=' padding
            // (many exporters omit it).
            std::string_view encoded = uri.substr(comma + 1);
            while (!encoded.empty() && encoded.back() == '=') {
                encoded.remove_suffix(1);
            }
            const usize decodedSize = (encoded.size() * 3) / 4;
            const cgltf_options options = make_options();
            void *decoded = nullptr;
            const cgltf_result result = cgltf_load_buffer_base64(&options, decodedSize, encoded.data(), &decoded);
            if (result != cgltf_result_success || decoded == nullptr) {
                return parse_failure(std::format("image {} base64 data does not decode", imageIndex));
            }
            const std::byte *start = static_cast<const std::byte *>(decoded);
            std::vector<std::byte> bytes(start, start + decodedSize);
            gltf_free(nullptr, decoded);
            return bytes;
        }
        if (!reader) {
            return core::make_error(core::error_code::NOT_FOUND, "gltf: external image '{}' cannot be read", uri);
        }
        std::string path(uri);
        cgltf_decode_uri(path.data());
        path.resize(std::strlen(path.c_str()));
        core::result<std::vector<std::byte>> bytes = reader(path);
        if (!bytes.has_value()) {
            return core::make_error(
              core::error_code::NOT_FOUND,
              "gltf: external image '{}': {}",
              path,
              bytes.get_error().message()
            );
        }
        return bytes;
    }

    [[nodiscard]] material_desc import_material(
      const cgltf_data &data,
      const cgltf_material &material,
      material_texture_indices &textureIndices,
      std::vector<std::string> &warnings
    ) {
        material_desc desc = {};
        textureIndices.fill(INVALID_INDEX);
        const auto bind = [&](const cgltf_texture_view &view, material_texture slot) {
            if (view.texture == nullptr) {
                return;
            }
            const usize index = static_cast<usize>(slot);
            textureIndices[index] = index_of(data, view.texture);
            desc.textureUvSets[index] = static_cast<u8>(std::min(view.texcoord, 1));
            if (view.has_transform) {
                warnings.push_back(
                  std::format(
                    "material '{}': KHR_texture_transform is ignored",
                    material.name != nullptr ? material.name : ""
                  )
                );
            }
        };
        if (material.has_pbr_metallic_roughness) {
            const cgltf_pbr_metallic_roughness &pbr = material.pbr_metallic_roughness;
            desc.baseColourFactor = math::vec4(
              pbr.base_color_factor[0],
              pbr.base_color_factor[1],
              pbr.base_color_factor[2],
              pbr.base_color_factor[3]
            );
            desc.metallicFactor = pbr.metallic_factor;
            desc.roughnessFactor = pbr.roughness_factor;
            bind(pbr.base_color_texture, material_texture::BASE_COLOUR);
            bind(pbr.metallic_roughness_texture, material_texture::METALLIC_ROUGHNESS);
        }
        bind(material.normal_texture, material_texture::NORMAL);
        if (material.normal_texture.texture != nullptr) {
            desc.normalScale = material.normal_texture.scale;
        }
        bind(material.occlusion_texture, material_texture::OCCLUSION);
        if (material.occlusion_texture.texture != nullptr) {
            desc.occlusionStrength = material.occlusion_texture.scale;
        }
        bind(material.emissive_texture, material_texture::EMISSIVE);
        desc.emissiveFactor = math::vec3(
          material.emissive_factor[0],
          material.emissive_factor[1],
          material.emissive_factor[2]
        );
        if (material.has_emissive_strength) {
            desc.emissiveStrength = material.emissive_strength.emissive_strength;
        }
        switch (material.alpha_mode) {
            case cgltf_alpha_mode_mask: {
                desc.alphaMode = alpha_mode::MASK;
                break;
            }
            case cgltf_alpha_mode_blend: {
                desc.alphaMode = alpha_mode::BLEND;
                break;
            }
            default: {
                desc.alphaMode = alpha_mode::SOLID;
                break;
            }
        }
        desc.alphaCutoff = material.alpha_cutoff;
        desc.doubleSided = material.double_sided;
        desc.unlit = material.unlit;
        return desc;
    }

    [[nodiscard]] light import_light(const cgltf_light &source) noexcept {
        light value = {};
        switch (source.type) {
            case cgltf_light_type_directional: {
                value.type = light_type::DIRECTIONAL;
                break;
            }
            case cgltf_light_type_spot: {
                value.type = light_type::SPOT;
                break;
            }
            default: {
                value.type = light_type::POINT;
                break;
            }
        }
        value.colour = math::vec3(source.color[0], source.color[1], source.color[2]);
        value.intensity = source.intensity;
        value.range = source.range > 0.0f ? source.range : value.range;
        if (value.type == light_type::SPOT) {
            value.innerConeRadians = source.spot_inner_cone_angle;
            value.outerConeRadians = source.spot_outer_cone_angle > 0.0f ? source.spot_outer_cone_angle
              : value.outerConeRadians;
        }
        return value;
    }

    [[nodiscard]] camera import_camera(const cgltf_camera &source) noexcept {
        camera value = {};
        if (source.type == cgltf_camera_type_orthographic) {
            value.projection = projection_type::ORTHOGRAPHIC;
            value.orthographicHeight = source.data.orthographic.ymag * 2.0f;
            value.nearPlane = source.data.orthographic.znear;
            value.farPlane = source.data.orthographic.zfar;
        } else {
            value.projection = projection_type::PERSPECTIVE;
            value.verticalFovRadians = source.data.perspective.yfov;
            value.nearPlane = source.data.perspective.znear;
            value.farPlane = source.data.perspective.has_zfar ? source.data.perspective.zfar : 1000.0f;
        }
        if (value.nearPlane <= 0.0f) {
            value.nearPlane = 0.1f;
        }
        if (value.farPlane <= value.nearPlane) {
            value.farPlane = value.nearPlane + 1000.0f;
        }
        return value;
    }

    struct node_import_context {
        const cgltf_data &data;
        const gltf_import_options &options;
        gltf_import_result &result;
        std::vector<u32> meshIndexByGltfMesh;
    };

    /// <summary>
    /// A node waiting to be imported: the glTF node and the model index of its parent.
    /// </summary>
    struct pending_node {
        const cgltf_node *node = nullptr;
        u32 parent = INVALID_INDEX;
    };

    [[nodiscard]] model_node convert_node(const node_import_context &context, const cgltf_node &node, u32 parent) {
        model_node entry = {};
        entry.name = node.name != nullptr ? node.name : "";
        entry.parent = parent;
        std::array<f32, 16> matrix = {};
        cgltf_node_transform_local(&node, matrix.data());
        math::mat4 local(1.0f);
        std::memcpy(&local, matrix.data(), sizeof(local));
        entry.localTransform = math::transform::from_matrix(local);
        const bool isRoot = parent == INVALID_INDEX;
        if (isRoot && context.options.scale != 1.0f) {
            entry.localTransform.position *= context.options.scale;
            entry.localTransform.scale *= context.options.scale;
        }
        if (node.mesh != nullptr) {
            const u32 gltfMesh = index_of(context.data, node.mesh);
            entry.mesh = gltfMesh < context.meshIndexByGltfMesh.size() ? context.meshIndexByGltfMesh[gltfMesh]
              : INVALID_INDEX;
            const cgltf_size slotCount = std::min<cgltf_size>(node.mesh->primitives_count, MAX_MATERIAL_SLOTS);
            for (cgltf_size primitive = 0; primitive < slotCount; ++primitive) {
                entry.materials[primitive] = index_of(context.data, node.mesh->primitives[primitive].material);
            }
        }
        if (context.options.importLights && node.light != nullptr) {
            entry.hasLight = true;
            entry.lightData = import_light(*node.light);
        }
        if (context.options.importCameras && node.camera != nullptr) {
            entry.hasCamera = true;
            entry.cameraData = import_camera(*node.camera);
        }
        return entry;
    }

    /// <summary>
    /// Adds a root node and its subtree to the model, parents before children in document order.
    /// </summary>
    void import_tree(node_import_context &context, const cgltf_node &root) {
        // An explicit stack instead of recursion: a hostile file can nest nodes as deep as it likes,
        // which would overflow the call stack of an importer worker. Children are pushed in reverse so
        // they pop in document order.
        std::vector<pending_node> stack = {};
        stack.push_back(pending_node{&root, INVALID_INDEX});
        while (!stack.empty()) {
            const pending_node current = stack.back();
            stack.pop_back();
            const u32 index = context.result.model.add_node(convert_node(context, *current.node, current.parent));
            for (cgltf_size child = current.node->children_count; child > 0; --child) {
                const cgltf_node *next = current.node->children[child - 1];
                if (next != nullptr) {
                    stack.push_back(pending_node{next, index});
                }
            }
        }
    }

    /// <summary>
    /// Reads buffers and images of a model on disk. URIs are UTF-8 and may only name files in the
    /// model's directory or below: an absolute path or a '..' that leaves it fails with INVALID_ARGUMENT.
    /// </summary>
    [[nodiscard]] gltf_file_reader directory_reader(const std::filesystem::path &directory) {
        return [directory](std::string_view relativePath) -> core::result<std::vector<std::byte>> {
            const std::filesystem::path relative =
              std::filesystem::path(std::u8string(relativePath.begin(), relativePath.end())).lexically_normal();
            const bool escapes = relative.empty() || relative.has_root_name() || relative.has_root_directory()
              || *relative.begin() == "..";
            if (escapes) {
                return core::make_error(
                  core::error_code::INVALID_ARGUMENT,
                  "gltf: '{}' points outside the model's directory",
                  relativePath
                );
            }
            return detail::read_file_bytes(directory / relative);
        };
    }
}

/*===========================================================================*/
/*===========================================================================*/
// gltf_importer

gltf_importer::gltf_importer() {}

gltf_importer::~gltf_importer() {}

core::result<gltf_import_result> gltf_importer::import(
  const std::filesystem::path &sourcePath,
  const gltf_import_options &options
) const {
    std::vector<std::byte> bytes = {};
    CONJURE_TRY_ASSIGN(bytes, detail::read_file_bytes(sourcePath));
    core::result<gltf_import_result> imported = import_bytes(
      bytes,
      options,
      directory_reader(sourcePath.parent_path())
    );
    if (imported.has_value() && imported.value().model.name().empty()) {
        imported.value().model.set_name(detail::path_utf8(sourcePath.stem()));
    }
    return imported;
}

core::result<gltf_import_result> gltf_importer::import_bytes(
  std::span<const std::byte> bytes,
  const gltf_import_options &options,
  const gltf_file_reader &reader
) const {
    if (bytes.empty()) {
        return parse_failure("no data");
    }
    gltf_document document = {};
    const cgltf_options parseOptions = make_options();
    const cgltf_result parsed = cgltf_parse(&parseOptions, bytes.data(), bytes.size(), &document.data);
    if (parsed != cgltf_result_success || document.data == nullptr) {
        return parse_failure(cgltf_result_name(parsed));
    }
    CONJURE_TRY(resolve_buffers(document, reader));
    // check_view_ranges keeps cgltf_validate's unchecked size arithmetic from wrapping, so it runs
    // first. cgltf_validate then bounds every accessor by its buffer view and rejects node cycles
    // before anything reads through them; it scans index data, so both need the resolved buffers.
    CONJURE_TRY(check_view_ranges(*document.data));
    const cgltf_result valid = cgltf_validate(document.data);
    if (valid != cgltf_result_success) {
        return parse_failure(std::format("invalid document: {}", cgltf_result_name(valid)));
    }
    const cgltf_data &data = *document.data;

    gltf_import_result result = {};
    for (cgltf_size index = 0; index < data.extensions_used_count; ++index) {
        const std::string_view extension(data.extensions_used[index]);
        const bool supported = std::find(
          SUPPORTED_EXTENSIONS.begin(),
          SUPPORTED_EXTENSIONS.end(),
          extension
        ) != SUPPORTED_EXTENSIONS.end();
        if (!supported) {
            result.warnings.push_back(std::format("extension '{}' is not supported and was ignored", extension));
        }
    }
    if (data.skins_count > 0) {
        result.warnings.push_back("skins are not supported; the mesh is imported unskinned");
    }
    if (data.animations_count > 0) {
        result.warnings.push_back("animations are not supported and were ignored");
    }

    // Textures: one per glTF texture, decoded in the colour space of its first use.
    std::vector<texture_colour_space> colourSpaces(data.textures_count, texture_colour_space::LINEAR);
    std::vector<bool> colourSpaceKnown(data.textures_count, false);
    const auto noteUse = [&](
      const cgltf_texture_view &view,
      texture_colour_space space,
      std::string_view materialName
    ) {
        const u32 index = index_of(data, view.texture);
        if (index == INVALID_INDEX || index >= data.textures_count) {
            return;
        }
        if (!colourSpaceKnown[index]) {
            colourSpaces[index] = space;
            colourSpaceKnown[index] = true;
        } else if (colourSpaces[index] != space) {
            result.warnings.push_back(
              std::format("material '{}': texture {} is used both as colour and as data", materialName, index)
            );
        }
    };
    for (cgltf_size index = 0; index < data.materials_count; ++index) {
        const cgltf_material &material = data.materials[index];
        const std::string_view materialName = material.name != nullptr ? material.name : "";
        if (material.has_pbr_metallic_roughness) {
            noteUse(material.pbr_metallic_roughness.base_color_texture, texture_colour_space::SRGB, materialName);
            noteUse(
              material.pbr_metallic_roughness.metallic_roughness_texture,
              texture_colour_space::LINEAR,
              materialName
            );
        }
        noteUse(material.normal_texture, texture_colour_space::LINEAR, materialName);
        noteUse(material.occlusion_texture, texture_colour_space::LINEAR, materialName);
        noteUse(material.emissive_texture, texture_colour_space::SRGB, materialName);
    }
    for (cgltf_size index = 0; index < data.textures_count; ++index) {
        const cgltf_texture &texture = data.textures[index];
        if (texture.image == nullptr) {
            return parse_failure(std::format("texture {} has no image", index));
        }
        std::vector<std::byte> encoded = {};
        CONJURE_TRY_ASSIGN(encoded, image_bytes(*texture.image, reader, static_cast<u32>(texture.image - data.images)));
        core::result<texture_asset> decoded = texture_asset::decode(encoded, colourSpaces[index], true);
        if (!decoded.has_value()) {
            return core::make_error(
              core::error_code::PARSE_FAILURE,
              "gltf: texture {}: {}",
              index,
              decoded.get_error().message()
            );
        }
        texture_asset &asset = decoded.value();
        apply_sampler(texture.sampler, asset);
        asset.set_name(
          texture.name != nullptr ? texture.name : (
            texture.image->name != nullptr ? texture.image->name : std::format("texture_{}", index)
          )
        );
        result.textures.push_back(std::move(asset));
    }

    // Materials.
    for (cgltf_size index = 0; index < data.materials_count; ++index) {
        const cgltf_material &material = data.materials[index];
        material_texture_indices textureIndices = {};
        const material_desc desc = import_material(data, material, textureIndices, result.warnings);
        result.materials.emplace_back(
          desc,
          material.name != nullptr ? material.name : std::format("material_{}", index)
        );
        result.materialTextures.push_back(textureIndices);
    }

    // Meshes.
    std::vector<u32> meshIndexByGltfMesh(data.meshes_count, INVALID_INDEX);
    for (cgltf_size index = 0; index < data.meshes_count; ++index) {
        const cgltf_mesh &mesh = data.meshes[index];
        if (mesh.primitives_count == 0) {
            result.warnings.push_back(std::format("mesh {} has no primitives", index));
            continue;
        }
        mesh_asset asset = {};
        CONJURE_TRY_ASSIGN(asset, import_mesh(mesh, mesh.name != nullptr ? mesh.name : std::format("mesh_{}", index)));
        meshIndexByGltfMesh[index] = static_cast<u32>(result.meshes.size());
        result.meshes.push_back(std::move(asset));
    }
    if (!options.generateTangents) {
        // Tangents are always present in the packed stream; the option only documents that glTF
        // tangents are preferred when present, which from_geometry already does.
        result.warnings.push_back(
          "generateTangents = false: tangents are still generated for primitives without TANGENT"
        );
    }

    // Node tree: the default scene, the first scene, or every parentless node.
    node_import_context context = {data, options, result, std::move(meshIndexByGltfMesh)};
    const cgltf_scene *scene = data.scene != nullptr ? data.scene : (data.scenes_count > 0 ? &data.scenes[0] : nullptr);
    if (scene != nullptr) {
        for (cgltf_size index = 0; index < scene->nodes_count; ++index) {
            if (scene->nodes[index] != nullptr) {
                import_tree(context, *scene->nodes[index]);
            }
        }
    } else {
        for (cgltf_size index = 0; index < data.nodes_count; ++index) {
            if (data.nodes[index].parent == nullptr) {
                import_tree(context, data.nodes[index]);
            }
        }
    }
    // The model's mesh / material tables are filled by index so node references stay valid.
    for (usize index = 0; index < result.meshes.size(); ++index) {
        static_cast<void>(result.model.add_mesh(assets::asset_id()));
    }
    for (usize index = 0; index < result.materials.size(); ++index) {
        static_cast<void>(result.model.add_material(assets::asset_id()));
    }
    return result;
}

/*===========================================================================*/
/*===========================================================================*/
// Options JSON

core::result<gltf_import_options> conjure::renderer::gltf_import_options_from_json(const core::json::value &source) {
    gltf_import_options options = {};
    if (!source.is_valid() || source.is_null()) {
        return options;
    }
    if (!source.is_object()) {
        return core::make_error(core::error_code::INVALID_ARGUMENT, "gltf import settings must be a JSON object");
    }
    const auto readBool = [&source](std::string_view key, bool &target) -> core::result<void> {
        const core::json::value member = source[key];
        if (!member.is_valid()) {
            return core::result<void>();
        }
        if (!member.is_bool()) {
            return core::make_error(
              core::error_code::INVALID_ARGUMENT,
              "gltf import settings: {} must be a boolean",
              key
            );
        }
        target = member.as_bool();
        return core::result<void>();
    };
    CONJURE_TRY(readBool(KEY_GENERATE_TANGENTS, options.generateTangents));
    CONJURE_TRY(readBool(KEY_IMPORT_CAMERAS, options.importCameras));
    CONJURE_TRY(readBool(KEY_IMPORT_LIGHTS, options.importLights));
    if (const core::json::value scale = source[KEY_SCALE]; scale.is_valid()) {
        if (!scale.is_number() || !(scale.as_f32(0.0f) > 0.0f)) {
            return core::make_error(
              core::error_code::INVALID_ARGUMENT,
              "gltf import settings: scale must be a positive number"
            );
        }
        options.scale = scale.as_f32(1.0f);
    }
    return options;
}

core::json::document conjure::renderer::gltf_import_options_to_document(const gltf_import_options &options) {
    core::json::writer out = {};
    out.begin_object();
    out.key(KEY_GENERATE_TANGENTS);
    out.value(options.generateTangents);
    out.key(KEY_IMPORT_CAMERAS);
    out.value(options.importCameras);
    out.key(KEY_IMPORT_LIGHTS);
    out.value(options.importLights);
    out.key(KEY_SCALE);
    out.value(options.scale);
    out.end_object();
    core::result<core::json::document> parsed = core::json::document::parse(out.finish());
    CONJURE_ASSERT(parsed.has_value(), "gltf_import_options writes valid JSON");
    return parsed.has_value() ? std::move(parsed).value() : core::json::document::object();
}

/*===========================================================================*/
/*===========================================================================*/
// gltf_asset_importer

gltf_asset_importer::gltf_asset_importer() {}

gltf_asset_importer::~gltf_asset_importer() {}

std::string_view gltf_asset_importer::id() const noexcept {
    return GLTF_IMPORTER_ID;
}

u32 gltf_asset_importer::version() const noexcept {
    return GLTF_IMPORTER_VERSION;
}

std::span<const std::string_view> gltf_asset_importer::extensions() const noexcept {
    return GLTF_EXTENSIONS;
}

core::json::document gltf_asset_importer::default_settings() const {
    return gltf_import_options_to_document(gltf_import_options());
}

core::result<assets::import_output> gltf_asset_importer::import(assets::import_context &context) const {
    core::result<gltf_import_options> options = gltf_import_options_from_json(context.settings());
    if (!options.has_value()) {
        return assets::make_asset_error(assets::asset_error_code::IMPORT_FAILED, options.get_error().message());
    }
    const gltf_file_reader reader = [&context](std::string_view relativePath) {
        return context.read_side_file(relativePath);
    };
    const gltf_importer importer = {};
    core::result<gltf_import_result> imported = importer.import_bytes(context.source_bytes(), options.value(), reader);
    if (!imported.has_value()) {
        return assets::make_asset_error(assets::asset_error_code::IMPORT_FAILED, imported.get_error().message());
    }
    gltf_import_result &result = imported.value();
    for (const std::string &warning : result.warnings) {
        context.add_warning(warning);
    }

    assets::import_output output = {};
    std::vector<assets::asset_id> textureIds = {};
    for (usize index = 0; index < result.textures.size(); ++index) {
        const assets::asset_id id = context.sub_asset_id(std::format("texture/{}", index));
        textureIds.push_back(id);
        assets::imported_asset asset = {};
        asset.subPath = std::format("texture/{}", index);
        asset.displayName = std::string(result.textures[index].name());
        asset.type = assets::asset_type_traits<texture_asset>::id();
        asset.payload = result.textures[index].write_blob();
        output.assets.push_back(std::move(asset));
    }

    model_asset model = {};
    model.set_name(std::string(core::vfs::path_utils::stem(context.source_path())));
    for (usize index = 0; index < result.materials.size(); ++index) {
        const assets::asset_id id = context.sub_asset_id(std::format("material/{}", index));
        material_asset &material = result.materials[index];
        assets::imported_asset asset = {};
        asset.subPath = std::format("material/{}", index);
        asset.displayName = std::string(material.name());
        asset.type = assets::asset_type_traits<material_asset>::id();
        for (usize slot = 0; slot < MATERIAL_TEXTURE_SLOTS; ++slot) {
            const u32 textureIndex = result.materialTextures[index][slot];
            if (textureIndex != INVALID_INDEX && textureIndex < textureIds.size()) {
                material.desc().textures[slot] = assets::asset_ref<texture_asset>(textureIds[textureIndex]);
                asset.dependencies.push_back(textureIds[textureIndex]);
            }
        }
        std::sort(asset.dependencies.begin(), asset.dependencies.end());
        asset.dependencies.erase(
          std::unique(asset.dependencies.begin(), asset.dependencies.end()),
          asset.dependencies.end()
        );
        asset.payload = material.write_blob();
        output.assets.push_back(std::move(asset));
        static_cast<void>(model.add_material(id));
    }
    for (usize index = 0; index < result.meshes.size(); ++index) {
        const assets::asset_id id = context.sub_asset_id(std::format("mesh/{}", index));
        assets::imported_asset asset = {};
        asset.subPath = std::format("mesh/{}", index);
        asset.displayName = std::string(result.meshes[index].name());
        asset.type = assets::asset_type_traits<mesh_asset>::id();
        asset.payload = result.meshes[index].write_blob();
        output.assets.push_back(std::move(asset));
        static_cast<void>(model.add_mesh(id));
    }
    for (const model_node &node : result.model.nodes()) {
        static_cast<void>(model.add_node(node));
    }

    assets::imported_asset primary = {};
    primary.displayName = std::string(model.name());
    primary.type = assets::asset_type_traits<model_asset>::id();
    primary.dependencies.assign(model.meshes().begin(), model.meshes().end());
    primary.dependencies.insert(primary.dependencies.end(), model.materials().begin(), model.materials().end());
    std::sort(primary.dependencies.begin(), primary.dependencies.end());
    primary.dependencies.erase(
      std::unique(primary.dependencies.begin(), primary.dependencies.end()),
      primary.dependencies.end()
    );
    primary.payload = model.write_blob();
    output.assets.insert(output.assets.begin(), std::move(primary));
    return output;
}
