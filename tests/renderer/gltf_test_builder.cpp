#include "conjure/testing/doctest.h"
#include "gltf_test_builder.h"

#include "conjure/core/image.h"
#include "conjure/core/math.h"

#include <cstring>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::testing;
using namespace conjure::testing::gltf_builder;

namespace {

    constexpr u32 COMPONENT_FLOAT = 5126;
    constexpr u32 COMPONENT_UNSIGNED_INT = 5125;
    constexpr u32 TARGET_ARRAY_BUFFER = 34962;
    constexpr u32 TARGET_ELEMENT_ARRAY_BUFFER = 34963;

    constexpr std::string_view BASE64_ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    [[nodiscard]] std::string number(f32 value) {
        return std::format("{}", value);
    }

    [[nodiscard]] std::string vec3_json(const math::vec3 &value) {
        return std::format("[{}, {}, {}]", value.x, value.y, value.z);
    }

    [[nodiscard]] std::string vec4_json(const math::vec4 &value) {
        return std::format("[{}, {}, {}, {}]", value.x, value.y, value.z, value.w);
    }

    void append_bytes(std::vector<std::byte> &out, const void *data, usize size) {
        const std::byte *bytes = static_cast<const std::byte *>(data);
        out.insert(out.end(), bytes, bytes + size);
    }

    void pad_to_four(std::vector<std::byte> &out) {
        while (out.size() % 4 != 0) {
            out.push_back(std::byte{0});
        }
    }
}

std::string conjure::testing::gltf_builder::base64_encode(std::span<const std::byte> bytes) {
    std::string out = {};
    out.reserve((bytes.size() + 2) / 3 * 4);
    usize index = 0;
    while (index + 3 <= bytes.size()) {
        const u32 triple = (static_cast<u32>(bytes[index]) << 16) | (static_cast<u32>(bytes[index + 1]) << 8) |
                           static_cast<u32>(bytes[index + 2]);
        out.push_back(BASE64_ALPHABET[(triple >> 18) & 0x3F]);
        out.push_back(BASE64_ALPHABET[(triple >> 12) & 0x3F]);
        out.push_back(BASE64_ALPHABET[(triple >> 6) & 0x3F]);
        out.push_back(BASE64_ALPHABET[triple & 0x3F]);
        index += 3;
    }
    const usize remaining = bytes.size() - index;
    if (remaining == 1) {
        const u32 triple = static_cast<u32>(bytes[index]) << 16;
        out.push_back(BASE64_ALPHABET[(triple >> 18) & 0x3F]);
        out.push_back(BASE64_ALPHABET[(triple >> 12) & 0x3F]);
        out.append("==");
    } else if (remaining == 2) {
        const u32 triple = (static_cast<u32>(bytes[index]) << 16) | (static_cast<u32>(bytes[index + 1]) << 8);
        out.push_back(BASE64_ALPHABET[(triple >> 18) & 0x3F]);
        out.push_back(BASE64_ALPHABET[(triple >> 12) & 0x3F]);
        out.push_back(BASE64_ALPHABET[(triple >> 6) & 0x3F]);
        out.push_back('=');
    }
    return out;
}

/*===========================================================================*/
/*===========================================================================*/
// Layout

struct gltf_test_builder::layout {
    struct view {
        usize offset = 0;
        usize length = 0;
        std::optional<u32> target = std::nullopt;
    };

    struct accessor {
        u32 view = 0;
        u32 componentType = COMPONENT_FLOAT;
        u32 count = 0;
        std::string type = "VEC3";
        std::string minMax = {};
    };

    struct primitive {
        std::optional<u32> position = std::nullopt;
        std::optional<u32> normal = std::nullopt;
        std::optional<u32> tangent = std::nullopt;
        std::optional<u32> texcoord = std::nullopt;
        std::optional<u32> colour = std::nullopt;
        std::optional<u32> indices = std::nullopt;
    };

    std::vector<std::byte> buffer = {};
    std::vector<view> views = {};
    std::vector<accessor> accessors = {};
    std::vector<std::vector<primitive>> meshes = {};   // the primitives of each mesh, add_mesh's first
    std::vector<u32> imageViews = {};
};

gltf_test_builder::gltf_test_builder() = default;

gltf_test_builder::~gltf_test_builder() = default;

u32 gltf_test_builder::add_mesh(mesh_input mesh) {
    mMeshes.push_back(std::move(mesh));
    mExtraPrimitives.emplace_back();
    return static_cast<u32>(mMeshes.size() - 1);
}

void gltf_test_builder::add_primitive(u32 mesh, mesh_input primitive) {
    REQUIRE(mesh < mExtraPrimitives.size());
    mExtraPrimitives[mesh].push_back(std::move(primitive));
}

u32 gltf_test_builder::add_material(material_input material) {
    mMaterials.push_back(std::move(material));
    return static_cast<u32>(mMaterials.size() - 1);
}

u32 gltf_test_builder::add_solid_texture(u32 width, u32 height, u8 r, u8 g, u8 b, u8 a) {
    core::image image(width, height);
    image.fill(core::rgba8{r, g, b, a});
    core::result<std::vector<std::byte>> encoded = image.encode_png();
    CONJURE_REQUIRE_OK(encoded);
    mImages.push_back(std::move(encoded).value());
    return static_cast<u32>(mImages.size() - 1);
}

u32 gltf_test_builder::add_node(node_input node) {
    mNodes.push_back(std::move(node));
    return static_cast<u32>(mNodes.size() - 1);
}

u32 gltf_test_builder::add_light(light_input light) {
    mLights.push_back(std::move(light));
    return static_cast<u32>(mLights.size() - 1);
}

void gltf_test_builder::use_extension(std::string_view name) {
    mExtensionsUsed.emplace_back(name);
}

void gltf_test_builder::add_skin_and_animation() {
    mSkin = true;
    mAnimation = true;
}

mesh_input gltf_test_builder::quad(f32 halfSize, std::optional<u32> material) {
    mesh_input mesh = {};
    mesh.name = "quad";
    mesh.positions = {
      math::vec3(-halfSize, -halfSize, 0.0f),
      math::vec3(halfSize, -halfSize, 0.0f),
      math::vec3(halfSize, halfSize, 0.0f),
      math::vec3(-halfSize, halfSize, 0.0f),
    };
    mesh.normals.assign(4, math::vec3(0.0f, 0.0f, 1.0f));
    mesh.uv0 = {math::vec2(0.0f, 1.0f), math::vec2(1.0f, 1.0f), math::vec2(1.0f, 0.0f), math::vec2(0.0f, 0.0f)};
    mesh.indices = {0, 1, 2, 0, 2, 3};
    mesh.material = material;
    return mesh;
}

mesh_input gltf_test_builder::cube(f32 halfSize, std::optional<u32> material) {
    mesh_input mesh = {};
    mesh.name = "cube";
    const math::vec3 normals[6] = {
      math::vec3(1.0f, 0.0f, 0.0f),  math::vec3(-1.0f, 0.0f, 0.0f), math::vec3(0.0f, 1.0f, 0.0f),
      math::vec3(0.0f, -1.0f, 0.0f), math::vec3(0.0f, 0.0f, 1.0f),  math::vec3(0.0f, 0.0f, -1.0f),
    };
    const math::vec3 tangents[6] = {
      math::vec3(0.0f, 0.0f, -1.0f), math::vec3(0.0f, 0.0f, 1.0f), math::vec3(1.0f, 0.0f, 0.0f),
      math::vec3(1.0f, 0.0f, 0.0f),  math::vec3(1.0f, 0.0f, 0.0f), math::vec3(-1.0f, 0.0f, 0.0f),
    };
    for (u32 face = 0; face < 6; ++face) {
        const math::vec3 normal = normals[face];
        const math::vec3 tangent = tangents[face];
        const math::vec3 bitangent = glm::cross(normal, tangent);
        const u32 base = static_cast<u32>(mesh.positions.size());
        const math::vec2 uvs[4] = {
          math::vec2(0.0f, 1.0f), math::vec2(1.0f, 1.0f), math::vec2(1.0f, 0.0f), math::vec2(0.0f, 0.0f)
        };
        const f32 signsU[4] = {-1.0f, 1.0f, 1.0f, -1.0f};
        const f32 signsV[4] = {-1.0f, -1.0f, 1.0f, 1.0f};
        for (u32 corner = 0; corner < 4; ++corner) {
            mesh.positions.push_back((normal + tangent * signsU[corner] + bitangent * signsV[corner]) * halfSize);
            mesh.normals.push_back(normal);
            mesh.uv0.push_back(uvs[corner]);
        }
        mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }
    mesh.material = material;
    return mesh;
}

void gltf_test_builder::pack(layout &out) const {
    const auto addView = [&out](const void *data, usize size, std::optional<u32> target) -> u32 {
        pad_to_four(out.buffer);
        layout::view view = {};
        view.offset = out.buffer.size();
        view.length = size;
        view.target = target;
        append_bytes(out.buffer, data, size);
        out.views.push_back(view);
        return static_cast<u32>(out.views.size() - 1);
    };
    const auto addAccessor = [&out](
      u32 view,
      u32 componentType,
      u32 count,
      std::string type,
      std::string minMax
    ) -> u32 {
        layout::accessor accessor = {};
        accessor.view = view;
        accessor.componentType = componentType;
        accessor.count = count;
        accessor.type = std::move(type);
        accessor.minMax = std::move(minMax);
        out.accessors.push_back(std::move(accessor));
        return static_cast<u32>(out.accessors.size() - 1);
    };

    const auto packPrimitive = [&addView, &addAccessor](const mesh_input &mesh) -> layout::primitive {
        layout::primitive primitive = {};
        if (!mesh.positions.empty()) {
            math::vec3 low(std::numeric_limits<f32>::max());
            math::vec3 high(std::numeric_limits<f32>::lowest());
            for (const math::vec3 &position : mesh.positions) {
                low = glm::min(low, position);
                high = glm::max(high, position);
            }
            const u32 view = addView(
              mesh.positions.data(),
              mesh.positions.size() * sizeof(math::vec3),
              TARGET_ARRAY_BUFFER
            );
            const std::string minMax = std::format(", \"min\": {}, \"max\": {}", vec3_json(low), vec3_json(high));
            primitive.position = addAccessor(
              view,
              COMPONENT_FLOAT,
              static_cast<u32>(mesh.positions.size()),
              "VEC3",
              minMax
            );
        }
        if (!mesh.normals.empty()) {
            const u32 view = addView(
              mesh.normals.data(),
              mesh.normals.size() * sizeof(math::vec3),
              TARGET_ARRAY_BUFFER
            );
            primitive.normal = addAccessor(view, COMPONENT_FLOAT, static_cast<u32>(mesh.normals.size()), "VEC3", {});
        }
        if (!mesh.tangents.empty()) {
            const u32 view = addView(
              mesh.tangents.data(),
              mesh.tangents.size() * sizeof(math::vec4),
              TARGET_ARRAY_BUFFER
            );
            primitive.tangent = addAccessor(view, COMPONENT_FLOAT, static_cast<u32>(mesh.tangents.size()), "VEC4", {});
        }
        if (!mesh.uv0.empty()) {
            const u32 view = addView(mesh.uv0.data(), mesh.uv0.size() * sizeof(math::vec2), TARGET_ARRAY_BUFFER);
            primitive.texcoord = addAccessor(view, COMPONENT_FLOAT, static_cast<u32>(mesh.uv0.size()), "VEC2", {});
        }
        if (!mesh.colours.empty()) {
            const u32 view = addView(
              mesh.colours.data(),
              mesh.colours.size() * sizeof(math::vec4),
              TARGET_ARRAY_BUFFER
            );
            primitive.colour = addAccessor(view, COMPONENT_FLOAT, static_cast<u32>(mesh.colours.size()), "VEC4", {});
        }
        if (!mesh.indices.empty()) {
            const u32 view = addView(
              mesh.indices.data(),
              mesh.indices.size() * sizeof(u32),
              TARGET_ELEMENT_ARRAY_BUFFER
            );
            primitive.indices = addAccessor(
              view,
              COMPONENT_UNSIGNED_INT,
              static_cast<u32>(mesh.indices.size()),
              "SCALAR",
              {}
            );
        }
        return primitive;
    };
    for (usize index = 0; index < mMeshes.size(); ++index) {
        std::vector<layout::primitive> primitives = {};
        primitives.push_back(packPrimitive(mMeshes[index]));
        for (const mesh_input &extra : mExtraPrimitives[index]) {
            primitives.push_back(packPrimitive(extra));
        }
        out.meshes.push_back(std::move(primitives));
    }
    for (const std::vector<std::byte> &image : mImages) {
        out.imageViews.push_back(addView(image.data(), image.size(), std::nullopt));
    }
    pad_to_four(out.buffer);
}

void gltf_test_builder::use_image_data_uris(bool padded) {
    mImageDataUris = true;
    mImageDataUriPadding = padded;
}

std::string gltf_test_builder::json(const layout &packed, std::string_view bufferUri) const {
    std::string text = "{\n  \"asset\": {\"version\": \"2.0\", \"generator\": \"conjure tests\"},\n";

    // Extensions.
    std::vector<std::string> extensions = mExtensionsUsed;
    if (!mLights.empty()) {
        extensions.emplace_back("KHR_lights_punctual");
    }
    for (const material_input &material : mMaterials) {
        if (material.unlit) {
            extensions.emplace_back("KHR_materials_unlit");
        }
        if (material.textureTransform) {
            extensions.emplace_back("KHR_texture_transform");
        }
    }
    if (!extensions.empty()) {
        text.append("  \"extensionsUsed\": [");
        for (usize index = 0; index < extensions.size(); ++index) {
            text.append(index == 0 ? "\"" : ", \"");
            text.append(extensions[index]);
            text.append("\"");
        }
        text.append("],\n");
    }
    if (!mLights.empty()) {
        text.append("  \"extensions\": {\"KHR_lights_punctual\": {\"lights\": [");
        for (usize index = 0; index < mLights.size(); ++index) {
            const light_input &light = mLights[index];
            text.append(index == 0 ? "" : ", ");
            text.append(std::format(
              "{{\"type\": \"{}\", \"color\": {}, \"intensity\": {}, \"range\": {}",
              light.type,
              vec3_json(light.colour),
              number(light.intensity),
              number(light.range)
            ));
            if (light.type == "spot") {
                text.append(
                  std::format(
                    ", \"spot\": {{\"innerConeAngle\": {}, \"outerConeAngle\": {}}}",
                    number(light.innerCone),
                    number(light.outerCone)
                  )
                );
            }
            text.append("}");
        }
        text.append("]}},\n");
    }

    // Buffer, views, accessors.
    text.append(std::format("  \"buffers\": [{{\"byteLength\": {}", packed.buffer.size()));
    if (!bufferUri.empty()) {
        text.append(std::format(", \"uri\": \"{}\"", bufferUri));
    }
    text.append("}],\n  \"bufferViews\": [");
    for (usize index = 0; index < packed.views.size(); ++index) {
        const layout::view &view = packed.views[index];
        text.append(index == 0 ? "" : ", ");
        text.append(std::format("{{\"buffer\": 0, \"byteOffset\": {}, \"byteLength\": {}", view.offset, view.length));
        if (view.target.has_value()) {
            text.append(std::format(", \"target\": {}", *view.target));
        }
        text.append("}");
    }
    text.append("],\n  \"accessors\": [");
    for (usize index = 0; index < packed.accessors.size(); ++index) {
        const layout::accessor &accessor = packed.accessors[index];
        text.append(index == 0 ? "" : ", ");
        text.append(std::format(
          "{{\"bufferView\": {}, \"componentType\": {}, \"count\": {}, \"type\": \"{}\"{}}}",
          accessor.view,
          accessor.componentType,
          accessor.count,
          accessor.type,
          accessor.minMax
        ));
    }
    text.append("],\n");

    // Meshes.
    text.append("  \"meshes\": [");
    const auto appendPrimitive = [&text](const mesh_input &mesh, const layout::primitive &primitive) {
        text.append("{\"attributes\": {");
        bool first = true;
        const auto attribute = [&](const char *name, const std::optional<u32> &accessor) {
            if (accessor.has_value()) {
                text.append(first ? "" : ", ");
                text.append(std::format("\"{}\": {}", name, *accessor));
                first = false;
            }
        };
        attribute("POSITION", primitive.position);
        attribute("NORMAL", primitive.normal);
        attribute("TANGENT", primitive.tangent);
        attribute("TEXCOORD_0", primitive.texcoord);
        attribute("COLOR_0", primitive.colour);
        text.append("}");
        if (primitive.indices.has_value()) {
            text.append(std::format(", \"indices\": {}", *primitive.indices));
        }
        if (mesh.material.has_value()) {
            text.append(std::format(", \"material\": {}", *mesh.material));
        }
        text.append(std::format(", \"mode\": {}}}", mesh.mode));
    };
    for (usize index = 0; index < mMeshes.size(); ++index) {
        const std::vector<layout::primitive> &primitives = packed.meshes[index];
        text.append(index == 0 ? "" : ", ");
        text.append(std::format("{{\"name\": \"{}\", \"primitives\": [", mMeshes[index].name));
        appendPrimitive(mMeshes[index], primitives[0]);
        for (usize extra = 0; extra < mExtraPrimitives[index].size(); ++extra) {
            text.append(", ");
            appendPrimitive(mExtraPrimitives[index][extra], primitives[extra + 1]);
        }
        text.append("]}");
    }
    text.append("],\n");

    // Materials, textures, images.
    if (!mMaterials.empty()) {
        text.append("  \"materials\": [");
        for (usize index = 0; index < mMaterials.size(); ++index) {
            const material_input &material = mMaterials[index];
            text.append(index == 0 ? "" : ", ");
            text.append(std::format(
              "{{\"name\": \"{}\", \"pbrMetallicRoughness\": {{\"baseColorFactor\": {}, \"metallicFactor\": {}, "
              "\"roughnessFactor\": {}",
              material.name,
              vec4_json(material.baseColour),
              number(material.metallic),
              number(material.roughness)
            ));
            if (material.baseColourTexture.has_value()) {
                text.append(std::format(", \"baseColorTexture\": {{\"index\": {}", *material.baseColourTexture));
                if (material.textureTransform) {
                    text.append(", \"extensions\": {\"KHR_texture_transform\": {\"offset\": [0.5, 0.0]}}");
                }
                text.append("}");
            }
            if (material.metallicRoughnessTexture.has_value()) {
                text.append(
                  std::format(", \"metallicRoughnessTexture\": {{\"index\": {}}}", *material.metallicRoughnessTexture)
                );
            }
            text.append("}");
            if (material.normalTexture.has_value()) {
                text.append(
                  std::format(", \"normalTexture\": {{\"index\": {}, \"scale\": 0.75}}", *material.normalTexture)
                );
            }
            text.append(std::format(", \"emissiveFactor\": {}", vec3_json(material.emissive)));
            text.append(
              std::format(
                ", \"alphaMode\": \"{}\", \"alphaCutoff\": {}",
                material.alphaMode,
                number(material.alphaCutoff)
              )
            );
            text.append(std::format(", \"doubleSided\": {}", material.doubleSided ? "true" : "false"));
            if (material.unlit) {
                text.append(", \"extensions\": {\"KHR_materials_unlit\": {}}");
            }
            text.append("}");
        }
        text.append("],\n");
    }
    if (!mImages.empty()) {
        text.append("  \"textures\": [");
        for (usize index = 0; index < mImages.size(); ++index) {
            text.append(index == 0 ? "" : ", ");
            text.append(std::format("{{\"source\": {}, \"sampler\": 0}}", index));
        }
        text.append("],\n  \"images\": [");
        for (usize index = 0; index < mImages.size(); ++index) {
            text.append(index == 0 ? "" : ", ");
            if (mImageDataUris) {
                std::string encoded = base64_encode(mImages[index]);
                while (!mImageDataUriPadding && !encoded.empty() && encoded.back() == '=') {
                    encoded.pop_back();
                }
                text.append(
                  std::format("{{\"uri\": \"data:image/png;base64,{}\", \"mimeType\": \"image/png\"}}", encoded)
                );
            } else {
                text.append(
                  std::format("{{\"bufferView\": {}, \"mimeType\": \"image/png\"}}", packed.imageViews[index])
                );
            }
        }
        text.append(
          "],\n  \"samplers\": [{\"magFilter\": 9729, \"minFilter\": 9987, \"wrapS\": 10497, \"wrapT\": 10497}],\n"
        );
    }

    // Nodes and the scene.
    text.append("  \"nodes\": [");
    std::vector<u32> roots = {};
    for (usize index = 0; index < mNodes.size(); ++index) {
        const node_input &node = mNodes[index];
        text.append(index == 0 ? "" : ", ");
        text.append(std::format("{{\"name\": \"{}\"", node.name));
        if (node.mesh.has_value()) {
            text.append(std::format(", \"mesh\": {}", *node.mesh));
        }
        text.append(
          std::format(", \"translation\": {}, \"scale\": {}", vec3_json(node.translation), vec3_json(node.scale))
        );
        std::vector<u32> children = {};
        for (usize other = 0; other < mNodes.size(); ++other) {
            if (mNodes[other].parent.has_value() && *mNodes[other].parent == index) {
                children.push_back(static_cast<u32>(other));
            }
        }
        if (!children.empty()) {
            text.append(", \"children\": [");
            for (usize child = 0; child < children.size(); ++child) {
                text.append(child == 0 ? "" : ", ");
                text.append(std::to_string(children[child]));
            }
            text.append("]");
        }
        if (node.light.has_value()) {
            text.append(std::format(", \"extensions\": {{\"KHR_lights_punctual\": {{\"light\": {}}}}}", *node.light));
        }
        text.append("}");
        if (!node.parent.has_value()) {
            roots.push_back(static_cast<u32>(index));
        }
    }
    text.append("],\n  \"scenes\": [{\"nodes\": [");
    for (usize index = 0; index < roots.size(); ++index) {
        text.append(index == 0 ? "" : ", ");
        text.append(std::to_string(roots[index]));
    }
    text.append("]}],\n  \"scene\": 0");
    if (mSkin) {
        text.append(",\n  \"skins\": [{\"joints\": [0]}]");
    }
    if (mAnimation) {
        text.append(",\n  \"animations\": [{\"channels\": [], \"samplers\": []}]");
    }
    text.append("\n}\n");
    return text;
}

std::string gltf_test_builder::build_embedded() const {
    layout packed = {};
    pack(packed);
    const std::string uri = "data:application/octet-stream;base64," + base64_encode(packed.buffer);
    return json(packed, uri);
}

std::string gltf_test_builder::build_external(std::string_view binaryName) const {
    layout packed = {};
    pack(packed);
    return json(packed, binaryName);
}

std::vector<std::byte> gltf_test_builder::build_glb() const {
    layout packed = {};
    pack(packed);
    std::string text = json(packed, {});
    while (text.size() % 4 != 0) {
        text.push_back(' ');
    }
    std::vector<std::byte> out = {};
    const auto appendU32 = [&out](u32 value) {
        append_bytes(out, &value, sizeof(value));
    };
    const u32 totalLength = static_cast<u32>(12 + 8 + text.size() + 8 + packed.buffer.size());
    appendU32(0x46546C67u);   // "glTF"
    appendU32(2);
    appendU32(totalLength);
    appendU32(static_cast<u32>(text.size()));
    appendU32(0x4E4F534Au);   // "JSON"
    append_bytes(out, text.data(), text.size());
    appendU32(static_cast<u32>(packed.buffer.size()));
    appendU32(0x004E4942u);   // "BIN\0"
    out.insert(out.end(), packed.buffer.begin(), packed.buffer.end());
    return out;
}

std::vector<std::byte> gltf_test_builder::binary() const {
    layout packed = {};
    pack(packed);
    return packed.buffer;
}
