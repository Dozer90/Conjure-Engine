#include "physics/collision_mesh_importer.h"

#include "conjure/physics/log_category.h"
#include "conjure/physics/physics_error.h"

#include "conjure/assets/assets_error.h"
#include "conjure/core/containers/flat_hash_map.h"
#include "conjure/core/hash.h"
#include "conjure/core/log.h"
#include "conjure/core/memory/tracked_allocation.h"
#include "conjure/core/string_utils.h"
#include "conjure/core/vfs/path_utils.h"

#include <cgltf.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstring>
#include <format>
#include <optional>
#include <string>
#include <utility>

namespace conjure::physics::detail {

namespace {

    // The extensions claimed for a file without a meta. .gltf and .glb are read too (import() takes
    // everything that is not .obj as glTF) but are not claimed: by extension a glTF is a render model
    // (conjure.renderer.gltf), and collision geometry from one is chosen by naming this importer in the
    // file's .meta (14_physics.md 2.7; code sweep F137: both importers claimed them and the one
    // registered first, this one, took every model).
    constexpr std::array<std::string_view, 1> EXTENSIONS = {".obj"};
    constexpr usize CGLTF_HEADER_BYTES = 16;

    [[nodiscard]] std::string_view text_of(std::span<const std::byte> bytes) noexcept {
        return std::string_view(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    }

    [[nodiscard]] std::string_view trim(std::string_view text) noexcept {
        while (!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '\r')) {
            text.remove_prefix(1);
        }
        while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r')) {
            text.remove_suffix(1);
        }
        return text;
    }

    /// <summary>
    /// Splits a line into whitespace-separated tokens; nullopt when the line holds more than
    /// 'out.size()' of them (the caller reports the line instead of silently truncating it).
    /// </summary>
    [[nodiscard]] std::optional<usize> tokenize(std::string_view line, std::span<std::string_view> out) noexcept {
        usize count = 0;
        usize index = 0;
        while (index < line.size()) {
            while (index < line.size() && (line[index] == ' ' || line[index] == '\t')) {
                ++index;
            }
            const usize start = index;
            while (index < line.size() && line[index] != ' ' && line[index] != '\t') {
                ++index;
            }
            if (index == start) {
                continue;
            }
            if (count == out.size()) {
                return std::nullopt;
            }
            out[count] = line.substr(start, index - start);
            ++count;
        }
        return count;
    }

    /// <summary>
    /// core::str::parse_float rather than std::from_chars: floating-point from_chars is missing from
    /// libc++ before LLVM 20 (Apple Clang), and core already carries the guarded fallback.
    /// </summary>
    [[nodiscard]] bool parse_float(std::string_view token, f32 &out) noexcept {
        const std::optional<f32> value = core::str::parse_float(token);
        if (!value.has_value() || !std::isfinite(*value)) {
            return false;
        }
        out = *value;
        return true;
    }

    [[nodiscard]] bool parse_int(std::string_view token, i64 &out) noexcept {
        const std::from_chars_result parsed = std::from_chars(token.data(), token.data() + token.size(), out);
        return parsed.ec == std::errc() && parsed.ptr == token.data() + token.size();
    }

    [[nodiscard]] core::error parse_failure(usize line, std::string_view detail) {
        return core::make_error(physics_error::COLLISION_MESH_PARSE_FAILED, "line {}: {}", line, detail);
    }

    /*-----------------------------------------------------------------------*/
    // cgltf hooks: allocations through core::memory and buffers through the import context.

    struct cgltf_user_data {
        assets::import_context *context = nullptr;
        core::error failure = {};
        bool failed = false;
    };

    [[nodiscard]] void *cgltf_alloc(void *user, cgltf_size size) {
        CONJURE_UNUSED(user);
        // A 16-byte header keeps the size so the free hook can hand it back to tracked_free.
        std::byte *base = static_cast<std::byte *>(core::memory::tracked_allocate(size + 16, 16));
        if (base == nullptr) {
            return nullptr;
        }
        std::memcpy(base, &size, sizeof(size));
        return base + 16;
    }

    void cgltf_release(void *user, void *pointer) {
        CONJURE_UNUSED(user);
        if (pointer == nullptr) {
            return;
        }
        std::byte *base = static_cast<std::byte *>(pointer) - 16;
        cgltf_size size = 0;
        std::memcpy(&size, base, sizeof(size));
        core::memory::tracked_free(base, size + 16);
    }

    [[nodiscard]] cgltf_result cgltf_read_side_file(
      const cgltf_memory_options *memoryOptions,
      const cgltf_file_options *fileOptions,
      const char *path,
      cgltf_size *size,
      void **data
    ) {
        cgltf_user_data *user = static_cast<cgltf_user_data *>(fileOptions->user_data);
        core::result<std::vector<std::byte>> bytes = user->context->read_side_file(path);
        if (!bytes.has_value()) {
            user->failed = true;
            user->failure = bytes.get_error();
            return cgltf_result_file_not_found;
        }
        // On entry *size is the byteLength the document declares (cgltf's own reader enforces the same
        // contract). cgltf_validate checks accessors against that declared length only, so a shorter file
        // must fail here or the accessors would read past the end of the copy.
        const cgltf_size declared = *size;
        if (declared != 0 && bytes.value().size() < declared) {
            user->failed = true;
            user->failure = core::make_error(
              physics_error::COLLISION_MESH_PARSE_FAILED,
              "buffer '{}' holds {} bytes but the glTF declares {}",
              path,
              bytes.value().size(),
              declared
            );
            return cgltf_result_data_too_short;
        }
        void *copy = cgltf_alloc(memoryOptions->user_data, bytes.value().size());
        if (copy == nullptr) {
            return cgltf_result_out_of_memory;
        }
        std::memcpy(copy, bytes.value().data(), bytes.value().size());
        *size = bytes.value().size();
        *data = copy;
        return cgltf_result_success;
    }

    void cgltf_release_side_file(
      const cgltf_memory_options *memoryOptions,
      const cgltf_file_options *fileOptions,
      void *data
    ) {
        CONJURE_UNUSED(fileOptions);
        cgltf_release(memoryOptions->user_data, data);
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
                return "invalid json";
            case cgltf_result_invalid_gltf:
                return "invalid gltf";
            case cgltf_result_invalid_options:
                return "invalid options";
            case cgltf_result_file_not_found:
                return "file not found";
            case cgltf_result_io_error:
                return "io error";
            case cgltf_result_out_of_memory:
                return "out of memory";
            case cgltf_result_legacy_gltf:
                return "legacy gltf";
            case cgltf_result_max_enum:
                break;
        }
        return "unknown";
    }

    /// <summary>
    /// RAII owner of a parsed cgltf document.
    /// </summary>
    class cgltf_document {
        private:
            cgltf_data *mData = nullptr;

        public:
            explicit cgltf_document(cgltf_data *data) noexcept :
                mData(data) {}

            ~cgltf_document() {
                if (mData != nullptr) {
                    cgltf_free(mData);
                }
            }

            cgltf_document(const cgltf_document &) = delete;
            cgltf_document &operator=(const cgltf_document &) = delete;

            [[nodiscard]] cgltf_data *get() const noexcept {
                return mData;
            }
    };

    /// <summary>
    /// Appends the triangle (base + first, base + second, base + third): the corners swap when the node
    /// mirrors its mesh, so the triangle keeps facing the way it was authored.
    /// </summary>
    void append_triangle(std::vector<u32> &out, u32 base, const std::array<u32, 3> &corners, bool mirrored) {
        out.push_back(base + corners[0]);
        out.push_back(base + (mirrored ? corners[2] : corners[1]));
        out.push_back(base + (mirrored ? corners[1] : corners[2]));
    }

    /// <summary>
    /// Appends every triangle primitive of a mesh instanced by a node, positions transformed by the
    /// node's world matrix.
    /// </summary>
    [[nodiscard]] core::result<void> append_node(
      const cgltf_node &node,
      raw_geometry &out,
      assets::import_context &context
    ) {
        if (node.mesh == nullptr) {
            return core::result<void>::success();
        }
        std::array<cgltf_float, 16> matrix = {};
        cgltf_node_transform_world(&node, matrix.data());
        // cgltf and glm are both column-major.
        math::mat4 world(1.0f);
        for (glm::length_t column = 0; column < 4; ++column) {
            for (glm::length_t row = 0; row < 4; ++row) {
                world[column][row] = matrix[static_cast<usize>(column * 4 + row)];
            }
        }
        // A negative determinant (an odd number of negative scale axes) turns the winding around, and
        // Jolt collides with the front face of a mesh triangle only.
        const bool mirrored = glm::determinant(math::mat3(world)) < 0.0f;
        const char *meshName = node.mesh->name == nullptr ? "" : node.mesh->name;

        for (cgltf_size primitiveIndex = 0; primitiveIndex < node.mesh->primitives_count; ++primitiveIndex) {
            const cgltf_primitive &primitive = node.mesh->primitives[primitiveIndex];
            if (primitive.type != cgltf_primitive_type_triangles) {
                context.add_warning(std::format(
                  "mesh '{}' primitive {} is not a triangle list and was skipped",
                  meshName,
                  primitiveIndex
                ));
                continue;
            }
            const cgltf_accessor *positions = nullptr;
            for (cgltf_size attribute = 0; attribute < primitive.attributes_count; ++attribute) {
                if (primitive.attributes[attribute].type == cgltf_attribute_type_position) {
                    positions = primitive.attributes[attribute].data;
                    break;
                }
            }
            if (positions == nullptr || cgltf_num_components(positions->type) != 3) {
                return core::make_error(physics_error::COLLISION_MESH_PARSE_FAILED, "primitive without POSITION");
            }
            // cgltf_validate does not check this outside meshopt, and the merged index list would only be
            // checked as a whole: a ragged primitive would shift every triangle of the ones after it.
            const cgltf_size cornerCount = primitive.indices == nullptr ? positions->count : primitive.indices->count;
            if (cornerCount % 3 != 0) {
                return core::make_error(
                  physics_error::COLLISION_MESH_PARSE_FAILED,
                  "mesh '{}' primitive {}: {} indices is not a multiple of three",
                  meshName,
                  primitiveIndex,
                  cornerCount
                );
            }

            const u32 base = static_cast<u32>(out.positions.size());
            std::vector<cgltf_float> floats(positions->count * 3);
            if (cgltf_accessor_unpack_floats(positions, floats.data(), floats.size()) != floats.size()) {
                return core::make_error(
                  physics_error::COLLISION_MESH_PARSE_FAILED,
                  "POSITION accessor could not be read"
                );
            }
            for (cgltf_size vertex = 0; vertex < positions->count; ++vertex) {
                const math::vec4 local(floats[vertex * 3], floats[vertex * 3 + 1], floats[vertex * 3 + 2], 1.0f);
                out.positions.push_back(math::vec3(world * local));
            }

            if (primitive.indices == nullptr) {
                for (cgltf_size vertex = 0; vertex < positions->count; vertex += 3) {
                    const u32 first = static_cast<u32>(vertex);
                    append_triangle(out.indices, base, {first, first + 1, first + 2}, mirrored);
                }
                continue;
            }
            std::vector<u32> indices(primitive.indices->count);
            if (cgltf_accessor_unpack_indices(
              primitive.indices,
              indices.data(),
              sizeof(u32),
              indices.size()
            ) != indices.size()) {
                return core::make_error(physics_error::COLLISION_MESH_PARSE_FAILED, "index accessor could not be read");
            }
            for (usize corner = 0; corner < indices.size(); corner += 3) {
                const std::array<u32, 3> triangle = {indices[corner], indices[corner + 1], indices[corner + 2]};
                append_triangle(out.indices, base, triangle, mirrored);
            }
        }
        return core::result<void>::success();
    }

    [[nodiscard]] core::result<raw_geometry> parse_gltf(assets::import_context &context) {
        cgltf_user_data user = {};
        user.context = &context;

        cgltf_options options = {};
        options.memory.alloc_func = &cgltf_alloc;
        options.memory.free_func = &cgltf_release;
        options.file.read = &cgltf_read_side_file;
        options.file.release = &cgltf_release_side_file;
        options.file.user_data = &user;

        const std::span<const std::byte> bytes = context.source_bytes();
        if (bytes.size() < CGLTF_HEADER_BYTES) {
            return core::make_error(
              physics_error::COLLISION_MESH_PARSE_FAILED,
              "{}: file too short",
              context.source_path()
            );
        }
        cgltf_data *raw = nullptr;
        const cgltf_result parsed = cgltf_parse(&options, bytes.data(), bytes.size(), &raw);
        if (parsed != cgltf_result_success) {
            return core::make_error(
              physics_error::COLLISION_MESH_PARSE_FAILED,
              "{}: cgltf {}",
              context.source_path(),
              cgltf_result_name(parsed)
            );
        }
        const cgltf_document document(raw);

        // An empty base path makes cgltf hand the buffer uri itself to the read hook, which resolves it
        // next to the source through the import context.
        const cgltf_result loaded = cgltf_load_buffers(&options, raw, "");
        if (loaded != cgltf_result_success) {
            if (user.failed) {
                return core::make_error(
                  physics_error::COLLISION_MESH_PARSE_FAILED,
                  "{}: buffer: {}",
                  context.source_path(),
                  user.failure.to_string()
                );
            }
            return core::make_error(
              physics_error::COLLISION_MESH_PARSE_FAILED,
              "{}: cgltf buffers: {}",
              context.source_path(),
              cgltf_result_name(loaded)
            );
        }
        // Accessors and buffer views against their buffers, indices against their vertex counts, node
        // cycles: what the readers below take on trust.
        const cgltf_result validated = cgltf_validate(raw);
        if (validated != cgltf_result_success) {
            return core::make_error(
              physics_error::COLLISION_MESH_PARSE_FAILED,
              "{}: cgltf validate: {}",
              context.source_path(),
              cgltf_result_name(validated)
            );
        }

        raw_geometry geometry = {};
        for (cgltf_size nodeIndex = 0; nodeIndex < raw->nodes_count; ++nodeIndex) {
            CONJURE_TRY(append_node(raw->nodes[nodeIndex], geometry, context));
        }
        return geometry;
    }

    /*-----------------------------------------------------------------------*/
    // Welding

    struct cell_key {
        i64 x = 0;
        i64 y = 0;
        i64 z = 0;

        [[nodiscard]] bool operator==(const cell_key &) const noexcept = default;
    };

    struct cell_hash {
        [[nodiscard]] std::size_t operator()(const cell_key &key) const noexcept {
            u64 hash = core::hash::combine(static_cast<u64>(key.x), static_cast<u64>(key.y));
            hash = core::hash::combine(hash, static_cast<u64>(key.z));
            return static_cast<std::size_t>(hash);
        }
    };

    [[nodiscard]] i64 cell_of(f32 value, f32 cellSize) noexcept {
        return static_cast<i64>(std::floor(value / cellSize));
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Free functions

core::result<raw_geometry> parse_obj(std::string_view text) {
    raw_geometry geometry = {};
    usize lineNumber = 0;
    usize offset = 0;
    std::array<std::string_view, 64> tokens = {};
    while (offset <= text.size()) {
        ++lineNumber;
        const usize end = text.find('\n', offset);
        const std::string_view rawLine = text.substr(
          offset,
          end == std::string_view::npos ? std::string_view::npos : end - offset
        );
        offset = end == std::string_view::npos ? text.size() + 1 : end + 1;

        std::string_view line = trim(rawLine);
        const usize comment = line.find('#');
        if (comment != std::string_view::npos) {
            line = trim(line.substr(0, comment));
        }
        if (line.empty()) {
            continue;
        }
        const std::optional<usize> tokenized = tokenize(line, tokens);
        if (!tokenized.has_value()) {
            return parse_failure(lineNumber, "too many values on the line (at most 63 face vertices)");
        }
        const usize tokenCount = *tokenized;
        if (tokenCount == 0) {
            continue;
        }
        if (tokens[0] == "v") {
            if (tokenCount < 4) {
                return parse_failure(lineNumber, "vertex needs three coordinates");
            }
            math::vec3 position = math::vec3(0.0f);
            if (!parse_float(
              tokens[1],
              position.x
            ) || !parse_float(tokens[2], position.y) || !parse_float(tokens[3], position.z)) {
                return parse_failure(lineNumber, "vertex coordinate is not a number");
            }
            geometry.positions.push_back(position);
            continue;
        }
        if (tokens[0] != "f") {
            continue;
        }
        if (tokenCount < 4) {
            return parse_failure(lineNumber, "face needs at least three vertices");
        }
        std::array<u32, 63> face = {};
        const usize cornerCount = tokenCount - 1;
        for (usize corner = 0; corner < cornerCount; ++corner) {
            std::string_view token = tokens[corner + 1];
            const usize slash = token.find('/');
            if (slash != std::string_view::npos) {
                token = token.substr(0, slash);
            }
            i64 index = 0;
            if (!parse_int(token, index) || index == 0) {
                return parse_failure(lineNumber, "face index is not an integer");
            }
            const i64 vertexCount = static_cast<i64>(geometry.positions.size());
            const i64 resolved = index < 0 ? vertexCount + index : index - 1;
            if (resolved < 0 || resolved >= vertexCount) {
                return parse_failure(lineNumber, "face index out of range");
            }
            face[corner] = static_cast<u32>(resolved);
        }
        for (usize corner = 1; corner + 1 < cornerCount; ++corner) {
            geometry.indices.push_back(face[0]);
            geometry.indices.push_back(face[corner]);
            geometry.indices.push_back(face[corner + 1]);
        }
    }
    return geometry;
}

raw_geometry weld_vertices(const raw_geometry &source, f32 weldDistance) {
    raw_geometry welded = {};
    welded.positions.reserve(source.positions.size());
    welded.indices.reserve(source.indices.size());
    if (!(weldDistance > 0.0f)) {
        welded = source;
        return welded;
    }

    // Every welded vertex is registered in its cell: the cell maps to its newest vertex and nextInCell chains
    // the older ones, so a vertex in a cell that already holds another, farther one is still found.
    constexpr u32 NONE = 0xFFFFFFFFu;
    const f32 cellSize = weldDistance;
    const f32 weldSquared = weldDistance * weldDistance;
    core::flat_hash_map<cell_key, u32, cell_hash> headOfCell = {};
    headOfCell.reserve(source.positions.size());
    std::vector<u32> nextInCell = {};
    nextInCell.reserve(source.positions.size());
    std::vector<u32> remap(source.positions.size());

    for (usize index = 0; index < source.positions.size(); ++index) {
        const math::vec3 &position = source.positions[index];
        const cell_key home = {cell_of(
          position.x,
          cellSize
        ), cell_of(position.y, cellSize), cell_of(position.z, cellSize)};
        // The lowest index within reach wins whatever the walk order: first occurrence, deterministic.
        u32 found = NONE;
        for (i64 dx = -1; dx <= 1; ++dx) {
            for (i64 dy = -1; dy <= 1; ++dy) {
                for (i64 dz = -1; dz <= 1; ++dz) {
                    const cell_key neighbour = {home.x + dx, home.y + dy, home.z + dz};
                    const auto head = headOfCell.find(neighbour);
                    if (head == headOfCell.end()) {
                        continue;
                    }
                    for (u32 candidate = head->second; candidate != NONE; candidate = nextInCell[candidate]) {
                        const math::vec3 delta = welded.positions[candidate] - position;
                        if (glm::dot(delta, delta) <= weldSquared) {
                            found = std::min(found, candidate);
                        }
                    }
                }
            }
        }
        if (found == NONE) {
            found = static_cast<u32>(welded.positions.size());
            welded.positions.push_back(position);
            nextInCell.push_back(NONE);
            const auto [slot, inserted] = headOfCell.try_emplace(home, found);
            if (!inserted) {
                nextInCell[found] = slot->second;
                slot->second = found;
            }
        }
        remap[index] = found;
    }
    for (const u32 index : source.indices) {
        welded.indices.push_back(index < remap.size() ? remap[index] : index);
    }
    return welded;
}


/*===========================================================================*/
/*===========================================================================*/
// collision_mesh_importer

collision_mesh_importer::collision_mesh_importer() {}

collision_mesh_importer::~collision_mesh_importer() {}

std::string_view collision_mesh_importer::id() const noexcept {
    return COLLISION_MESH_IMPORTER_ID;
}

u32 collision_mesh_importer::version() const noexcept {
    return VERSION;
}

std::span<const std::string_view> collision_mesh_importer::extensions() const noexcept {
    return EXTENSIONS;
}

core::json::document collision_mesh_importer::default_settings() const {
    return core::json::object({{"weld_distance", DEFAULT_WELD_DISTANCE}});
}

core::result<assets::import_output> collision_mesh_importer::import(assets::import_context &context) const {
    const std::string extension = core::vfs::path_utils::extension(context.source_path());
    core::result<raw_geometry> geometry = extension == ".obj"
      ? parse_obj(text_of(context.source_bytes()))
      : parse_gltf(context);
    if (!geometry.has_value()) {
        return core::make_error(assets::assets_error::IMPORT_FAILED, geometry.get_error().to_string());
    }

    f32 weldDistance = DEFAULT_WELD_DISTANCE;
    const core::json::value weldSetting = context.settings()["weld_distance"];
    if (weldSetting.is_number()) {
        weldDistance = weldSetting.as_f32(DEFAULT_WELD_DISTANCE);
    }
    const raw_geometry welded = weld_vertices(geometry.value(), weldDistance);

    const core::result<collision_mesh_asset> mesh = collision_mesh_asset::from_geometry(
      welded.positions,
      welded.indices
    );
    if (!mesh.has_value()) {
        return core::make_error(
          assets::assets_error::IMPORT_FAILED,
          std::format("{}: {}", context.source_path(), mesh.get_error().to_string())
        );
    }
    if (mesh.value().triangle_count() * 3 != welded.indices.size()) {
        context.add_warning(std::format(
          "{} degenerate triangles dropped",
          (welded.indices.size() - mesh.value().triangle_count() * 3) / 3
        ));
    }

    assets::imported_asset primary = {};
    primary.displayName = std::string(core::vfs::path_utils::stem(context.source_path()));
    primary.type = assets::asset_type_traits<collision_mesh_asset>::id();
    primary.payload = mesh.value().to_payload();

    assets::import_output output = {};
    output.assets.push_back(std::move(primary));
    return output;
}
}
