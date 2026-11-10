#include "integration_test_support.h"

#include "conjure/assets/asset_source.h"
#include "conjure/core/image.h"
#include "conjure/core/math.h"

#include <array>
#include <cstring>
#include <format>
#include <optional>
#include <utility>
#include <vector>

#ifndef CONJURE_TEST_SHADER_DIR
    #define CONJURE_TEST_SHADER_DIR ""
#endif

using namespace conjure;
using namespace integration_test;

namespace {

    constexpr std::string_view BASE64_ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    void append_bytes(std::vector<std::byte> &buffer, const void *data, usize size) {
        const usize offset = buffer.size();
        buffer.resize(offset + size);
        std::memcpy(buffer.data() + offset, data, size);
    }

    /// <summary>
    /// A 2 x 2 RGBA8 PNG of one colour through core::image, so the importer's PNG path runs.
    /// </summary>
    [[nodiscard]] std::vector<std::byte> solid_png() {
        core::image image(2, 2);
        for (u32 y = 0; y < 2; ++y) {
            for (u32 x = 0; x < 2; ++x) {
                image.set_pixel(x, y, core::rgba8{200, 100, 50, 255});
            }
        }
        core::result<std::vector<std::byte>> encoded = image.encode_png();
        REQUIRE(encoded.has_value());
        return std::move(encoded).value();
    }
}

std::filesystem::path integration_test::shader_directory() {
    return std::filesystem::path(CONJURE_TEST_SHADER_DIR);
}

std::string integration_test::base64_encode(std::span<const std::byte> bytes) {
    std::string out = {};
    out.reserve((bytes.size() + 2) / 3 * 4);
    usize index = 0;
    while (index + 3 <= bytes.size()) {
        const u32 triple = (static_cast<u32>(bytes[index]) << 16) | (static_cast<u32>(bytes[index + 1]) << 8)
          | static_cast<u32>(bytes[index + 2]);
        out.push_back(BASE64_ALPHABET[(triple >> 18) & 63]);
        out.push_back(BASE64_ALPHABET[(triple >> 12) & 63]);
        out.push_back(BASE64_ALPHABET[(triple >> 6) & 63]);
        out.push_back(BASE64_ALPHABET[triple & 63]);
        index += 3;
    }
    const usize remaining = bytes.size() - index;
    if (remaining == 1) {
        const u32 triple = static_cast<u32>(bytes[index]) << 16;
        out.push_back(BASE64_ALPHABET[(triple >> 18) & 63]);
        out.push_back(BASE64_ALPHABET[(triple >> 12) & 63]);
        out.append("==");
    } else if (remaining == 2) {
        const u32 triple = (static_cast<u32>(bytes[index]) << 16) | (static_cast<u32>(bytes[index + 1]) << 8);
        out.push_back(BASE64_ALPHABET[(triple >> 18) & 63]);
        out.push_back(BASE64_ALPHABET[(triple >> 12) & 63]);
        out.push_back(BASE64_ALPHABET[(triple >> 6) & 63]);
        out.push_back('=');
    }
    return out;
}

std::span<const std::byte> integration_test::bytes_of(std::string_view text) noexcept {
    return std::span<const std::byte>(reinterpret_cast<const std::byte *>(text.data()), text.size());
}

std::string integration_test::textured_cube_gltf() {
    // Six faces, four vertices each, outward normals, one uv quad per face.
    constexpr std::array<math::vec3, 6> normals = {
        math::vec3(0.0f, 0.0f, 1.0f),  math::vec3(0.0f, 0.0f, -1.0f), math::vec3(1.0f, 0.0f, 0.0f),
        math::vec3(-1.0f, 0.0f, 0.0f), math::vec3(0.0f, 1.0f, 0.0f),  math::vec3(0.0f, -1.0f, 0.0f),
    };
    constexpr std::array<math::vec3, 6> tangents = {
        math::vec3(1.0f, 0.0f, 0.0f), math::vec3(-1.0f, 0.0f, 0.0f), math::vec3(0.0f, 0.0f, -1.0f),
        math::vec3(0.0f, 0.0f, 1.0f), math::vec3(1.0f, 0.0f, 0.0f),  math::vec3(1.0f, 0.0f, 0.0f),
    };
    constexpr std::array<math::vec2, 4> corners = {
        math::vec2(-1.0f, -1.0f), math::vec2(1.0f, -1.0f), math::vec2(1.0f, 1.0f), math::vec2(-1.0f, 1.0f),
    };
    std::vector<std::byte> buffer = {};
    for (u32 face = 0; face < 6; ++face) {
        const math::vec3 normal = normals[face];
        const math::vec3 tangent = tangents[face];
        const math::vec3 bitangent = glm::cross(normal, tangent);
        for (const math::vec2 corner : corners) {
            const math::vec3 position = normal * 0.5f + tangent * (corner.x * 0.5f) + bitangent * (corner.y * 0.5f);
            append_bytes(buffer, &position, sizeof(position));
        }
    }
    const usize normalsOffset = buffer.size();
    for (u32 face = 0; face < 6; ++face) {
        for (u32 corner = 0; corner < 4; ++corner) {
            append_bytes(buffer, &normals[face], sizeof(math::vec3));
        }
    }
    const usize uvOffset = buffer.size();
    for (u32 face = 0; face < 6; ++face) {
        for (const math::vec2 corner : corners) {
            const math::vec2 uv(corner.x * 0.5f + 0.5f, corner.y * 0.5f + 0.5f);
            append_bytes(buffer, &uv, sizeof(uv));
        }
    }
    const usize indexOffset = buffer.size();
    for (u32 face = 0; face < 6; ++face) {
        const std::array<u32, 6> indices = {
            face * 4 + 0, face * 4 + 1, face * 4 + 2, face * 4 + 0, face * 4 + 2, face * 4 + 3,
        };
        append_bytes(buffer, indices.data(), indices.size() * sizeof(u32));
    }
    const usize totalSize = buffer.size();
    const std::vector<std::byte> png = solid_png();

    return std::format(
      "{{\"asset\":{{\"version\":\"2.0\"}},\"scene\":0,\"scenes\":[{{\"nodes\":[0]}}],"
      "\"nodes\":[{{\"name\":\"root\",\"translation\":[1,2,3],\"children\":[1]}},"
      "{{\"name\":\"cube\",\"mesh\":0}}],"
      "\"meshes\":[{{\"name\":\"cube\",\"primitives\":[{{\"attributes\":{{\"POSITION\":0,\"NORMAL\":1,"
      "\"TEXCOORD_0\":2}},\"indices\":3,\"material\":0,\"mode\":4}}]}}],"
      "\"materials\":[{{\"name\":\"painted\",\"pbrMetallicRoughness\":{{\"baseColorFactor\":[0.9,0.8,0.7,1.0],"
      "\"baseColorTexture\":{{\"index\":0}},\"metallicFactor\":0.0,\"roughnessFactor\":0.5}}}}],"
      "\"textures\":[{{\"source\":0,\"sampler\":0}}],\"samplers\":[{{}}],"
      "\"images\":[{{\"mimeType\":\"image/png\",\"uri\":\"data:image/png;base64,{}\"}}],"
      "\"accessors\":[{{\"bufferView\":0,\"componentType\":5126,\"count\":24,\"type\":\"VEC3\","
      "\"min\":[-0.5,-0.5,-0.5],\"max\":[0.5,0.5,0.5]}},"
      "{{\"bufferView\":1,\"componentType\":5126,\"count\":24,\"type\":\"VEC3\"}},"
      "{{\"bufferView\":2,\"componentType\":5126,\"count\":24,\"type\":\"VEC2\"}},"
      "{{\"bufferView\":3,\"componentType\":5125,\"count\":36,\"type\":\"SCALAR\"}}],"
      "\"bufferViews\":[{{\"buffer\":0,\"byteOffset\":0,\"byteLength\":{}}},"
      "{{\"buffer\":0,\"byteOffset\":{},\"byteLength\":{}}},"
      "{{\"buffer\":0,\"byteOffset\":{},\"byteLength\":{}}},"
      "{{\"buffer\":0,\"byteOffset\":{},\"byteLength\":{}}}],"
      "\"buffers\":[{{\"byteLength\":{},\"uri\":\"data:application/octet-stream;base64,{}\"}}]}}",
      base64_encode(png),
      normalsOffset,
      normalsOffset,
      uvOffset - normalsOffset,
      uvOffset,
      indexOffset - uvOffset,
      indexOffset,
      totalSize - indexOffset,
      totalSize,
      base64_encode(buffer)
    );
}

/*===========================================================================*/
/*===========================================================================*/
// asset_pipeline

integration_test::asset_pipeline::asset_pipeline() {
    core::jobs::job_system_desc jobsDesc = {};
    jobsDesc.workerCount = 0;
    CONJURE_REQUIRE_OK(jobs.start(jobsDesc));

    assets::asset_manager_desc desc = {};
    desc.jobs = &jobs;
    manager = std::make_unique<assets::asset_manager>(std::move(desc));
    database = std::make_unique<assets::asset_database>(*fileSystem, importers, manager->types(), &jobs);
    CONJURE_REQUIRE_OK(database->add_mount("assets", "assets"));
    CONJURE_REQUIRE_OK(fileSystem->create_directories("assets"));
}

integration_test::asset_pipeline::~asset_pipeline() {
    if (manager != nullptr) {
        manager->shutdown();
    }
    manager.reset();
    database.reset();
    jobs.stop();
}

void integration_test::asset_pipeline::open() {
    CONJURE_REQUIRE_OK(database->open(".conjure/cache", "test"));
    manager->swap_source(std::make_unique<assets::cache_asset_source>(*database->cache(), *database));
}

std::string integration_test::asset_pipeline::write_source(std::string_view relativePath, std::string_view text) {
    const std::string filePath = std::string("assets/") + std::string(relativePath);
    CONJURE_REQUIRE_OK(fileSystem->write_file(filePath, bytes_of(text)));
    return std::string("assets://") + std::string(relativePath);
}

void integration_test::asset_pipeline::scan_and_import() {
    CONJURE_REQUIRE_OK(database->scan());
    CONJURE_REQUIRE_OK(database->import_stale(jobs));
}

assets::asset_id integration_test::asset_pipeline::id_of(std::string_view relativePath) const {
    const std::optional<assets::asset_id> found =
      database->find_by_path(std::string("assets://") + std::string(relativePath));
    REQUIRE_MESSAGE(found.has_value(), "unknown source ", std::string(relativePath));
    return *found;
}
