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
// are warnings; parse errors, missing buffers, non-triangle topology and unsupported accessor
// layouts are failures.

namespace conjure::renderer {

    inline constexpr std::string_view GLTF_IMPORTER_ID = "conjure.renderer.gltf";
    inline constexpr u32 GLTF_IMPORTER_VERSION = 1;
    inline constexpr std::array<std::string_view, 2> GLTF_EXTENSIONS = {".gltf", ".glb"};

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
