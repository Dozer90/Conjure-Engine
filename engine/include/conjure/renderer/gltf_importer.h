#pragma once

#include "conjure/renderer/limits.h"
#include "conjure/renderer/material_asset.h"
#include "conjure/renderer/mesh_asset.h"
#include "conjure/renderer/model_asset.h"
#include "conjure/renderer/texture_asset.h"

#include "conjure/assets/asset_importer.h"
#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <array>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// glTF 2.0 import (docs/design/02_renderer.md 4.2): .gltf / .glb -> CPU meshes, materials,
// textures and a model node tree. cgltf does the parsing in engine/src; this header carries only
// engine types. Unsupported features (skins, animations, KHR_texture_transform, unknown extensions)
// are warnings, and so is a normal-mapped mesh imported with generateTangents off (R20); parse
// errors, missing buffers, non-triangle topology and unsupported accessor layouts are failures.

namespace conjure::renderer {

    inline constexpr std::string_view GLTF_IMPORTER_ID = "conjure.renderer.gltf";

    /// <summary>
    /// Bumped whenever the importer's output changes for some input: a cached import made by another
    /// version is stale. 2: a light without a range gets one from its intensity. 3: generateTangents
    /// = false builds tangents from the normal alone, and with it a primitive keeps its TANGENT when
    /// other primitives of its mesh have none (sweep-2026-11-12 F391).
    /// </summary>
    inline constexpr u32 GLTF_IMPORTER_VERSION = 3;
    inline constexpr std::array<std::string_view, 2> GLTF_EXTENSIONS = {".gltf", ".glb"};

    /// <summary>
    /// A point or spot light without a glTF range gets the distance at which its inverse-square falloff,
    /// intensity / d^2, drops to GLTF_LIGHT_RANGE_CUTOFF lux: sqrt(intensity / GLTF_LIGHT_RANGE_CUTOFF)
    /// metres (about 32 m for 10 cd), clamped to [GLTF_MIN_LIGHT_RANGE, GLTF_MAX_LIGHT_RANGE] because the
    /// range is also the light's shadow far plane and bounds its light-culling tiles. Fixed at import.
    /// </summary>
    inline constexpr f32 GLTF_LIGHT_RANGE_CUTOFF = 0.01f;
    inline constexpr f32 GLTF_MIN_LIGHT_RANGE = 0.1f;
    inline constexpr f32 GLTF_MAX_LIGHT_RANGE = 100.0f;

    /// <summary>
    /// 'generateTangents' decides how a primitive gets its tangents when the file does not give them
    /// (docs/design/02_renderer.md R20): true builds them from the TEXCOORD_0 derivatives
    /// (tangent_generation::FROM_UVS), false from the normal alone (FROM_NORMAL), which is only
    /// right where no normal map is drawn. A mesh whose primitives all have a TANGENT (an element per
    /// vertex) keeps them under either value. In a mesh where only some primitives have one, false
    /// keeps those and builds the others from the normal, while true generates every tangent of the
    /// mesh from the UVs. One exception: in a mesh where some primitive lacks NORMAL the normals of
    /// all primitives are generated, and if only some of its primitives have a TANGENT, none of those
    /// is kept under either value. With false, a mesh gets one import warning when a lit material
    /// with a normal texture is drawn by a primitive whose tangents were built from the normal: the
    /// materials the file gives the mesh are looked at (a primitive after the first
    /// MAX_MATERIAL_SLOTS draws with the material of the last slot), a material assigned to the mesh
    /// after the import is not. With true nothing is reported, also not for a mesh without
    /// TEXCOORD_0, whose generated tangents come from the normal alone too.
    /// 'scale' multiplies the translation and the scale of the root nodes. 'importCameras' and
    /// 'importLights' put glTF cameras and KHR_lights_punctual lights on the model's nodes.
    /// </summary>
    struct gltf_import_options {
        bool generateTangents = true;
        f32 scale = 1.0f;
        bool importCameras = false;
        bool importLights = true;

        [[nodiscard]] bool operator==(const gltf_import_options &other) const noexcept = default;
    };

    /// <summary>
    /// How a material of the result references the result's textures: an index into
    /// gltf_import_result::textures per slot, or INVALID_INDEX.
    /// </summary>
    using material_texture_indices = std::array<u32, MATERIAL_TEXTURE_SLOTS>;

    struct gltf_import_result {
        std::vector<mesh_asset> meshes = {};
        std::vector<material_asset> materials = {};
        std::vector<material_texture_indices> materialTextures = {};   // one per material
        std::vector<texture_asset> textures = {};
        // mesh / material indices into the vectors above
        model_asset model = {};
        std::vector<std::string> warnings = {};
    };

    /// <summary>
    /// Reads a file next to the glTF (external buffers and images) by its relative URI.
    /// </summary>
    using gltf_file_reader = std::function<core::result<std::vector<std::byte>>(std::string_view relativePath)>;

    class CONJURE_API gltf_importer {
        public:
            gltf_importer();
            ~gltf_importer();

            gltf_importer(const gltf_importer &) = delete;
            gltf_importer &operator=(const gltf_importer &) = delete;

        public:
            /// <summary>
            /// Imports .gltf/.glb into CPU assets. Unsupported extensions are warned and skipped. Buffers
            /// and images are read only from the model's directory and below: a URI that is absolute or
            /// climbs out of it with '..' fails with NOT_FOUND. Fails with NOT_FOUND when the file does
            /// not exist, IO_FAILURE when it cannot be read, then as import_bytes.
            /// </summary>
            [[nodiscard]] core::result<gltf_import_result> import(
              const std::filesystem::path &sourcePath,
              const gltf_import_options &options
            ) const;

            /// <summary>
            /// Imports from memory; external buffers and images are read through 'reader' (a null
            /// reader fails them with NOT_FOUND). Fails with PARSE_FAILURE for malformed glTF (also an
            /// accessor or buffer view reaching past its buffer, a byteStride above 252, a node cycle,
            /// or a COLOR_0 that is not VEC3 or VEC4), NOT_FOUND for a missing buffer or image and
            /// UNSUPPORTED for points / lines topology or an accessor layout the importer cannot read.
            /// Node hierarchies of any depth import without recursion.
            /// </summary>
            [[nodiscard]] core::result<gltf_import_result> import_bytes(
              std::span<const std::byte> bytes,
              const gltf_import_options &options,
              const gltf_file_reader &reader
            ) const;
    };

    /// <summary>
    /// The assets-module adapter: the primary asset is the model (conjure.renderer.model); meshes,
    /// materials and textures are sub-assets ("mesh/<index>", "material/<index>", "texture/<index>")
    /// with stable uuids. Settings mirror gltf_import_options as
    /// { "generateTangents": true, "scale": 1.0, "importCameras": false, "importLights": true }.
    /// The importer's warnings become the import's warnings.
    /// </summary>
    class CONJURE_API gltf_asset_importer final : public assets::asset_importer {
        public:
            gltf_asset_importer();
            ~gltf_asset_importer() override;

        public:
            [[nodiscard]] std::string_view id() const noexcept override;
            [[nodiscard]] u32 version() const noexcept override;
            [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override;
            [[nodiscard]] core::json::document default_settings() const override;
            [[nodiscard]] core::result<assets::import_output> import(assets::import_context &context) const override;
    };

    /// <summary>
    /// Reads the options object; missing keys keep their defaults. Fails with INVALID_ARGUMENT for a
    /// non-object or a wrong member type.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<gltf_import_options> gltf_import_options_from_json(
      const core::json::value &source
    );

    /// <summary>
    /// Writes the options object with sorted keys.
    /// </summary>
    [[nodiscard]] CONJURE_API core::json::document gltf_import_options_to_document(const gltf_import_options &options);
}
