#pragma once

#include "conjure/renderer/camera.h"
#include "conjure/renderer/light.h"
#include "conjure/renderer/limits.h"

#include "conjure/assets/asset_loader.h"
#include "conjure/assets/asset_types.h"
#include "conjure/core/api.h"
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

// The glTF node tree (docs/design/02_renderer.md 4.2): what the scene module turns into a prefab.
// Nodes reference meshes and materials by index into the asset id lists, so the model is
// self-describing once its dependencies are loaded. Cooked as JSON (.cmodel).

namespace conjure::renderer {

    inline constexpr std::string_view MODEL_ASSET_TYPE_NAME = "conjure.renderer.model";
    inline constexpr u32 MODEL_FORMAT_VERSION = 1;

    struct CONJURE_API model_node {
        std::string name = {};
        u32 parent = INVALID_INDEX;                     // index into model_asset::nodes(), INVALID_INDEX = root
        math::transform localTransform = {};
        u32 mesh = INVALID_INDEX;                       // index into model_asset::meshes()
        std::array<u32, MAX_MATERIAL_SLOTS> materials = {};   // indices into materials(); INVALID_INDEX = none
        bool hasLight = false;
        light lightData = {};
        bool hasCamera = false;
        camera cameraData = {};

        model_node();
        ~model_node();

        model_node(const model_node &) = default;
        model_node &operator=(const model_node &) = default;
        model_node(model_node &&) noexcept = default;
        model_node &operator=(model_node &&) noexcept = default;

        [[nodiscard]] bool operator==(const model_node &other) const noexcept;
    };

    class CONJURE_API model_asset {
        private:
            std::vector<model_node> mNodes = {};
            std::vector<assets::asset_id> mMeshes = {};
            std::vector<assets::asset_id> mMaterials = {};
            std::string mName = {};

        public:
            model_asset();
            ~model_asset();

            model_asset(const model_asset &) = default;
            model_asset &operator=(const model_asset &) = default;
            model_asset(model_asset &&) noexcept = default;
            model_asset &operator=(model_asset &&) noexcept = default;

        public:
            /// <summary>
            /// Parses the JSON form. Fails with PARSE_FAILURE for invalid JSON and INVALID_ARGUMENT for
            /// a wrong member type, a parent index that is not smaller than the node's own index (the
            /// list is parent-first), or a mesh / material index out of range.
            /// </summary>
            [[nodiscard]] static core::result<model_asset> parse_json(std::string_view text);
            [[nodiscard]] static core::result<model_asset> parse_blob(std::span<const std::byte> bytes);

            /// <summary>
            /// The canonical JSON form.
            /// </summary>
            [[nodiscard]] std::string to_json() const;
            [[nodiscard]] std::vector<std::byte> write_blob() const;

            /// <summary>
            /// Appends a node; the parent must already exist (or be INVALID_INDEX). Returns the index, or
            /// INVALID_INDEX (asserting) for a bad parent.
            /// </summary>
            u32 add_node(model_node node);

            /// <summary>
            /// Appends a mesh id and returns its index.
            /// </summary>
            u32 add_mesh(const assets::asset_id &id);

            /// <summary>
            /// Appends a material id and returns its index.
            /// </summary>
            u32 add_material(const assets::asset_id &id);

            [[nodiscard]] std::span<const model_node> nodes() const noexcept {
                return mNodes;
            }

            [[nodiscard]] std::span<const assets::asset_id> meshes() const noexcept {
                return mMeshes;
            }

            [[nodiscard]] std::span<const assets::asset_id> materials() const noexcept {
                return mMaterials;
            }

            [[nodiscard]] std::string_view name() const noexcept {
                return mName;
            }

            void set_name(std::string name) {
                mName = std::move(name);
            }

            /// <summary>
            /// Indices of the nodes without a parent, in order.
            /// </summary>
            [[nodiscard]] std::vector<u32> root_nodes() const;

            /// <summary>
            /// Indices of the direct children of a node, in order.
            /// </summary>
            [[nodiscard]] std::vector<u32> children_of(u32 node) const;

            /// <summary>
            /// The world transform of a node: the product of its ancestors' local transforms.
            /// </summary>
            [[nodiscard]] math::transform world_transform(u32 node) const noexcept;

            [[nodiscard]] bool operator==(const model_asset &other) const noexcept;
    };

}

CONJURE_DECLARE_ASSET_TYPE(conjure::renderer::model_asset, "conjure.renderer.model");

namespace conjure::renderer {

    /// <summary>
    /// The conjure.renderer.model loader. No fallback: a missing model instantiates nothing.
    /// </summary>
    class CONJURE_API model_loader final : public assets::typed_asset_loader<model_asset, model_asset> {
        public:
            model_loader();
            ~model_loader() override;

        protected:
            [[nodiscard]] core::result<model_asset> load_cpu_typed(
              const assets::load_context &context,
              std::span<const std::byte> payloadBytes
            ) const override;

            [[nodiscard]] core::result<model_asset> finalise_typed(
              const assets::finalise_context &context,
              model_asset &&intermediate
            ) const override;
    };
}
