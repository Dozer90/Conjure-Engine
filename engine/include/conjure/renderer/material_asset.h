#pragma once

#include "conjure/renderer/limits.h"
#include "conjure/renderer/texture_asset.h"

#include "conjure/assets/asset_importer.h"
#include "conjure/assets/asset_loader.h"
#include "conjure/assets/asset_ref.h"
#include "conjure/assets/asset_types.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/math.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The one PBR material model (docs/design/02_renderer.md R7, 4.2): glTF metallic-roughness with
// glTF defaults. Texture presence, double-sidedness, unlit and the UV set are uniform-driven; the
// alpha mode is pipeline state. The cooked form (.cmat) is JSON with texture uuids.

namespace conjure::renderer {

    inline constexpr std::string_view MATERIAL_ASSET_TYPE_NAME = "conjure.renderer.material";
    inline constexpr std::string_view MATERIAL_IMPORTER_ID = "conjure.renderer.material";
    inline constexpr u32 MATERIAL_IMPORTER_VERSION = 1;
    inline constexpr std::array<std::string_view, 1> MATERIAL_EXTENSIONS = {".cmat"};

    /// <summary>
    /// glTF alpha modes. SOLID is glTF's OPAQUE (a Windows macro name, CONVENTIONS.md 3).
    /// </summary>
    enum class alpha_mode : u8 {
        SOLID = 0,
        MASK,
        BLEND
    };

    /// <summary>
    /// Texture slots of a material, in binding order (t0..t4 of register space 1).
    /// </summary>
    enum class material_texture : u8 {
        BASE_COLOUR = 0,
        METALLIC_ROUGHNESS,
        NORMAL,
        OCCLUSION,
        EMISSIVE
    };

    struct material_desc {
        math::vec4 baseColourFactor = math::vec4(1.0f);
        math::vec3 emissiveFactor = math::vec3(0.0f);
        f32 emissiveStrength = 1.0f;
        f32 metallicFactor = 1.0f;
        f32 roughnessFactor = 1.0f;
        f32 normalScale = 1.0f;
        f32 occlusionStrength = 1.0f;
        f32 alphaCutoff = 0.5f;
        alpha_mode alphaMode = alpha_mode::SOLID;
        bool doubleSided = false;
        bool unlit = false;
        std::array<assets::asset_ref<texture_asset>, MATERIAL_TEXTURE_SLOTS> textures = {};
        std::array<u8, MATERIAL_TEXTURE_SLOTS> textureUvSets = {0, 0, 0, 0, 0};   // 0 = uv0, 1 = uv1

        [[nodiscard]] bool operator==(const material_desc &other) const noexcept;
    };

    class material_asset {
        private:
            material_desc mDesc = {};
            std::string mName = {};

        public:
            material_asset();
            explicit material_asset(material_desc desc, std::string name = {});
            ~material_asset();

            material_asset(const material_asset &) = default;
            material_asset &operator=(const material_asset &) = default;
            material_asset(material_asset &&) noexcept = default;
            material_asset &operator=(material_asset &&) noexcept = default;

        public:
            /// <summary>
            /// Parses the JSON form. Fails with PARSE_FAILURE for invalid JSON and INVALID_ARGUMENT for a
            /// wrong member type, an unknown enumerator or a malformed uuid.
            /// </summary>
            [[nodiscard]] static core::result<material_asset> parse_json(std::string_view text);

            /// <summary>
            /// parse_json over a blob's bytes.
            /// </summary>
            [[nodiscard]] static core::result<material_asset> parse_blob(std::span<const std::byte> bytes);

            /// <summary>
            /// The canonical JSON form (sorted keys, deterministic).
            /// </summary>
            [[nodiscard]] std::string to_json() const;

            /// <summary>
            /// The JSON form as blob bytes.
            /// </summary>
            [[nodiscard]] std::vector<std::byte> write_blob() const;

            /// <summary>
            /// The magenta fallback material.
            /// </summary>
            [[nodiscard]] static material_asset fallback();

            [[nodiscard]] const material_desc &desc() const noexcept {
                return mDesc;
            }

            [[nodiscard]] material_desc &desc() noexcept {
                return mDesc;
            }

            [[nodiscard]] std::string_view name() const noexcept {
                return mName;
            }

            void set_name(std::string name) {
                mName = std::move(name);
            }

            /// <summary>
            /// Bit i set when texture slot i references an asset (non-null uuid), bound or not.
            /// </summary>
            [[nodiscard]] u32 texture_flags() const noexcept;

            [[nodiscard]] bool operator==(const material_asset &other) const noexcept;
    };

}

CONJURE_DECLARE_ASSET_TYPE(conjure::renderer::material_asset, "conjure.renderer.material");

namespace conjure::renderer {

    /// <summary>
    /// The .cmat importer: validates the JSON and cooks it canonically; texture uuids become
    /// dependencies.
    /// </summary>
    class material_importer final : public assets::asset_importer {
        public:
            material_importer();
            ~material_importer() override;

        public:
            [[nodiscard]] std::string_view id() const noexcept override;
            [[nodiscard]] u32 version() const noexcept override;
            [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override;
            [[nodiscard]] core::result<assets::import_output> import(assets::import_context &context) const override;
    };

    /// <summary>
    /// The conjure.renderer.material loader; the fallback is the magenta material.
    /// </summary>
    class material_loader final : public assets::typed_asset_loader<material_asset, material_asset> {
        public:
            material_loader();
            ~material_loader() override;

        protected:
            [[nodiscard]] core::result<material_asset> load_cpu_typed(
              const assets::load_context &context,
              std::span<const std::byte> payloadBytes
            ) const override;

            [[nodiscard]] core::result<material_asset> finalise_typed(
              const assets::finalise_context &context,
              material_asset &&intermediate
            ) const override;

            [[nodiscard]] std::optional<material_asset> create_fallback_typed() const override;
    };

    [[nodiscard]] std::string_view to_string(alpha_mode value) noexcept;
    [[nodiscard]] std::string_view to_string(material_texture slot) noexcept;
}
