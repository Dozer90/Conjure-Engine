#pragma once

#include "conjure/core/math.h"
#include "conjure/core/types.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Builds small glTF 2.0 files in memory for the importer tests (docs/design/02_renderer.md 10: "test
// glTF generated in-test, no binary assets"). Geometry, materials, PNG textures (encoded through
// core::image), a node tree and KHR_lights_punctual lights go into one binary buffer; the output is
// either a .gltf with the buffer embedded as a base64 data URI, a .gltf referencing an external
// .bin, or a .glb.

namespace conjure::testing::gltf_builder {

    /// <summary>
    /// Standard base64 (RFC 4648, padded).
    /// </summary>
    [[nodiscard]] std::string base64_encode(std::span<const std::byte> bytes);

    struct mesh_input {
        std::string name = "mesh";
        std::vector<math::vec3> positions = {};
        std::vector<math::vec3> normals = {};           // optional
        std::vector<math::vec4> tangents = {};          // optional
        std::vector<math::vec2> uv0 = {};               // optional
        std::vector<math::vec4> colours = {};           // optional
        std::vector<u32> indices = {};                  // triangle list; empty = non-indexed
        std::optional<u32> material = std::nullopt;
        u32 mode = 4;                                   // glTF primitive mode (4 = TRIANGLES)
    };

    struct material_input {
        std::string name = "material";
        math::vec4 baseColour = math::vec4(1.0f);
        f32 metallic = 1.0f;
        f32 roughness = 1.0f;
        std::optional<u32> baseColourTexture = std::nullopt;
        std::optional<u32> normalTexture = std::nullopt;
        std::optional<u32> metallicRoughnessTexture = std::nullopt;
        math::vec3 emissive = math::vec3(0.0f);
        std::string alphaMode = "OPAQUE";
        f32 alphaCutoff = 0.5f;
        bool doubleSided = false;
        bool unlit = false;
        bool textureTransform = false;                  // adds KHR_texture_transform to the base colour texture
    };

    struct node_input {
        std::string name = "node";
        std::optional<u32> mesh = std::nullopt;
        std::optional<u32> parent = std::nullopt;       // index of an earlier node
        math::vec3 translation = math::vec3(0.0f);
        math::vec3 scale = math::vec3(1.0f);
        std::optional<u32> light = std::nullopt;        // KHR_lights_punctual index
    };

    struct light_input {
        std::string type = "point";                     // "directional" | "point" | "spot"
        math::vec3 colour = math::vec3(1.0f);
        f32 intensity = 5.0f;
        f32 range = 8.0f;
        f32 innerCone = 0.3f;
        f32 outerCone = 0.6f;
    };

    class gltf_test_builder {
        private:
            std::vector<mesh_input> mMeshes = {};
            std::vector<material_input> mMaterials = {};
            std::vector<std::vector<std::byte>> mImages = {};   // PNG bytes
            std::vector<node_input> mNodes = {};
            std::vector<light_input> mLights = {};
            std::vector<std::string> mExtensionsUsed = {};
            bool mSkin = false;
            bool mAnimation = false;
            bool mImageDataUris = false;
            bool mImageDataUriPadding = true;

        public:
            gltf_test_builder();
            ~gltf_test_builder();

        public:
            u32 add_mesh(mesh_input mesh);
            u32 add_material(material_input material);

            /// <summary>
            /// Adds a width x height PNG of one colour; returns the texture index.
            /// </summary>
            u32 add_solid_texture(u32 width, u32 height, u8 r, u8 g, u8 b, u8 a = 255);

            u32 add_node(node_input node);
            u32 add_light(light_input light);

            /// <summary>
            /// Lists an extension in extensionsUsed without using it (unsupported ones warn).
            /// </summary>
            void use_extension(std::string_view name);

            /// <summary>
            /// Adds an (empty) skin and animation so the importer's warnings can be observed.
            /// </summary>
            void add_skin_and_animation();

            /// <summary>
            /// Emits the images as base64 data URIs instead of buffer views, with or without the '='
            /// padding (exporters differ).
            /// </summary>
            void use_image_data_uris(bool padded);

            /// <summary>
            /// A quad in the XY plane with normals and uvs, two triangles.
            /// </summary>
            [[nodiscard]] static mesh_input quad(f32 halfSize = 0.5f, std::optional<u32> material = std::nullopt);

            /// <summary>
            /// A cube of the given half size, 24 vertices with normals and uvs.
            /// </summary>
            [[nodiscard]] static mesh_input cube(f32 halfSize = 0.5f, std::optional<u32> material = std::nullopt);

            /// <summary>
            /// The .gltf text with the buffer embedded as a data URI.
            /// </summary>
            [[nodiscard]] std::string build_embedded() const;

            /// <summary>
            /// The .gltf text referencing 'binaryName'; binary() is that file.
            /// </summary>
            [[nodiscard]] std::string build_external(std::string_view binaryName) const;

            /// <summary>
            /// The .glb bytes (JSON chunk + BIN chunk).
            /// </summary>
            [[nodiscard]] std::vector<std::byte> build_glb() const;

            /// <summary>
            /// The binary buffer the external form references.
            /// </summary>
            [[nodiscard]] std::vector<std::byte> binary() const;

        private:
            struct layout;
            void pack(layout &out) const;
            [[nodiscard]] std::string json(const layout &packed, std::string_view bufferUri) const;
    };
}
