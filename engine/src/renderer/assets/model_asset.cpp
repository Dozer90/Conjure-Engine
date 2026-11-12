#include "conjure/renderer/model_asset.h"

#include "conjure/core/assert.h"
#include "conjure/core/error.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/reflect/json_conversion.h"
#include "conjure/core/uuid.h"

#include <algorithm>
#include <limits>
#include <utility>

using namespace conjure;
using namespace conjure::renderer;

namespace {
    [[nodiscard]] core::error invalid_model(std::string_view detail) {
        return core::make_error(core::error_code::INVALID_ARGUMENT, "model_asset: {}", detail);
    }

    /// <summary>
    /// Reads an index member: a non-negative integer below INVALID_INDEX, or a negative one / missing
    /// for INVALID_INDEX. Larger integers fail instead of wrapping into a valid u32.
    /// </summary>
    [[nodiscard]] core::result<u32> read_index(const core::json::value &object, std::string_view key) {
        const core::json::value member = object[key];
        if (!member.is_valid() || member.is_null()) {
            return INVALID_INDEX;
        }
        if (!member.is_integer()) {
            return invalid_model(std::string(key) + " must be an integer");
        }
        // Integers beyond i64 (only u64 can hold them) take the fallback and fail the range check.
        const i64 value = member.as_i64(std::numeric_limits<i64>::max());
        if (value < 0) {
            return INVALID_INDEX;
        }
        if (value >= static_cast<i64>(INVALID_INDEX)) {
            return invalid_model(std::string(key) + " is out of range");
        }
        return static_cast<u32>(value);
    }

    [[nodiscard]] core::result<std::vector<assets::asset_id>> read_id_list(
      const core::json::value &root,
      std::string_view key
    ) {
        std::vector<assets::asset_id> ids = {};
        const core::json::value list = root[key];
        if (!list.is_valid()) {
            return ids;
        }
        if (!list.is_array()) {
            return invalid_model(std::string(key) + " must be an array");
        }
        for (const core::json::value entry : list) {
            if (!entry.is_string()) {
                return invalid_model(std::string(key) + " entries must be uuid strings");
            }
            const std::optional<core::uuid> id = core::uuid::parse(entry.as_string());
            if (!id.has_value()) {
                return invalid_model(std::string(key) + " entry is not a uuid");
            }
            ids.push_back(*id);
        }
        return ids;
    }

    [[nodiscard]] core::result<light> read_light(const core::json::value &object) {
        light value = {};
        const std::string_view typeName = object["type"].as_string("point");
        if (typeName == "directional") {
            value.type = light_type::DIRECTIONAL;
        } else if (typeName == "point") {
            value.type = light_type::POINT;
        } else if (typeName == "spot") {
            value.type = light_type::SPOT;
        } else {
            return invalid_model("light type must be directional, point or spot");
        }
        if (const core::json::value colour = object["colour"]; colour.is_valid()) {
            core::result<math::vec3> parsed = colour.to<math::vec3>();
            if (!parsed.has_value()) {
                return invalid_model("light colour must be three numbers");
            }
            value.colour = parsed.value();
        }
        value.intensity = object["intensity"].as_f32(value.intensity);
        value.range = object["range"].as_f32(value.range);
        value.innerConeRadians = object["innerConeRadians"].as_f32(value.innerConeRadians);
        value.outerConeRadians = object["outerConeRadians"].as_f32(value.outerConeRadians);
        value.sourceRadius = object["sourceRadius"].as_f32(value.sourceRadius);
        value.castsShadows = object["castsShadows"].as_bool(value.castsShadows);
        return value;
    }

    [[nodiscard]] core::result<camera> read_camera(const core::json::value &object) {
        camera value = {};
        const std::string_view projection = object["projection"].as_string("perspective");
        if (projection == "perspective") {
            value.projection = projection_type::PERSPECTIVE;
        } else if (projection == "orthographic") {
            value.projection = projection_type::ORTHOGRAPHIC;
        } else {
            return invalid_model("camera projection must be perspective or orthographic");
        }
        value.verticalFovRadians = object["verticalFovRadians"].as_f32(value.verticalFovRadians);
        value.orthographicHeight = object["orthographicHeight"].as_f32(value.orthographicHeight);
        value.nearPlane = object["nearPlane"].as_f32(value.nearPlane);
        value.farPlane = object["farPlane"].as_f32(value.farPlane);
        return value;
    }

    void write_light(const light &value, core::json::writer &out) {
        out.begin_object();
        out.key("castsShadows");
        out.value(value.castsShadows);
        out.key("colour");
        core::reflect::write_json(value.colour, out);
        out.key("innerConeRadians");
        out.value(value.innerConeRadians);
        out.key("intensity");
        out.value(value.intensity);
        out.key("outerConeRadians");
        out.value(value.outerConeRadians);
        out.key("range");
        out.value(value.range);
        out.key("sourceRadius");
        out.value(value.sourceRadius);
        out.key("type");
        out.value(
          value.type == light_type::DIRECTIONAL ? "directional" : (value.type == light_type::POINT ? "point" : "spot")
        );
        out.end_object();
    }

    void write_camera(const camera &value, core::json::writer &out) {
        out.begin_object();
        out.key("farPlane");
        out.value(value.farPlane);
        out.key("nearPlane");
        out.value(value.nearPlane);
        out.key("orthographicHeight");
        out.value(value.orthographicHeight);
        out.key("projection");
        out.value(value.projection == projection_type::PERSPECTIVE ? "perspective" : "orthographic");
        out.key("verticalFovRadians");
        out.value(value.verticalFovRadians);
        out.end_object();
    }

    void write_index(u32 index, core::json::writer &out) {
        if (index == INVALID_INDEX) {
            out.value(static_cast<i32>(-1));
        } else {
            out.value(index);
        }
    }
}

/*===========================================================================*/
/*===========================================================================*/
// model_node

model_node::model_node() {
    materials.fill(INVALID_INDEX);
}

model_node::~model_node() {}

bool model_node::operator==(const model_node &other) const noexcept {
    return name == other.name && parent == other.parent && localTransform == other.localTransform
      && mesh == other.mesh &&
      materials == other.materials && hasLight == other.hasLight && lightData == other.lightData &&
      hasCamera == other.hasCamera && cameraData == other.cameraData;
}

/*===========================================================================*/
/*===========================================================================*/
// model_asset

model_asset::model_asset() {}

model_asset::~model_asset() {}

core::result<model_asset> model_asset::parse_json(std::string_view text) {
    core::result<core::json::document> parsed = core::json::document::parse(text);
    if (!parsed.has_value()) {
        return core::make_error(core::error_code::PARSE_FAILURE, "model_asset: {}", parsed.get_error().message());
    }
    const core::json::value root = parsed.value().root();
    if (!root.is_object()) {
        return invalid_model("the root must be an object");
    }
    if (root["version"].as_i64(MODEL_FORMAT_VERSION) != MODEL_FORMAT_VERSION) {
        return invalid_model("unsupported version");
    }

    model_asset result = {};
    result.mName = std::string(root["name"].as_string(""));
    CONJURE_TRY_ASSIGN(result.mMeshes, read_id_list(root, "meshes"));
    CONJURE_TRY_ASSIGN(result.mMaterials, read_id_list(root, "materials"));

    const core::json::value nodes = root["nodes"];
    if (nodes.is_valid() && !nodes.is_array()) {
        return invalid_model("nodes must be an array");
    }
    u32 nodeIndex = 0;
    for (const core::json::value entry : nodes) {
        if (!entry.is_object()) {
            return invalid_model("nodes entries must be objects");
        }
        model_node node = {};
        node.name = std::string(entry["name"].as_string(""));
        CONJURE_TRY_ASSIGN(node.parent, read_index(entry, "parent"));
        if (node.parent != INVALID_INDEX && node.parent >= nodeIndex) {
            return invalid_model("a node's parent must precede it");
        }
        if (const core::json::value transform = entry["transform"]; transform.is_valid()) {
            core::result<math::transform> value = transform.to<math::transform>();
            if (!value.has_value()) {
                return invalid_model("node transform is malformed");
            }
            node.localTransform = value.value();
        }
        CONJURE_TRY_ASSIGN(node.mesh, read_index(entry, "mesh"));
        if (node.mesh != INVALID_INDEX && node.mesh >= result.mMeshes.size()) {
            return invalid_model("node mesh index out of range");
        }
        if (const core::json::value materials = entry["materials"]; materials.is_valid()) {
            if (!materials.is_array() || materials.size() > MAX_MATERIAL_SLOTS) {
                return invalid_model("node materials must be an array of at most MAX_MATERIAL_SLOTS indices");
            }
            usize slot = 0;
            for (const core::json::value material : materials) {
                if (!material.is_integer()) {
                    return invalid_model("node material indices must be integers");
                }
                const i64 value = material.as_i64(std::numeric_limits<i64>::max());
                if (value >= 0 && static_cast<u64>(value) >= result.mMaterials.size()) {
                    return invalid_model("node material index out of range");
                }
                node.materials[slot] = value < 0 ? INVALID_INDEX : static_cast<u32>(value);
                ++slot;
            }
        }
        if (const core::json::value lightValue = entry["light"]; lightValue.is_valid() && !lightValue.is_null()) {
            if (!lightValue.is_object()) {
                return invalid_model("node light must be an object");
            }
            node.hasLight = true;
            CONJURE_TRY_ASSIGN(node.lightData, read_light(lightValue));
        }
        if (const core::json::value cameraValue = entry["camera"]; cameraValue.is_valid() && !cameraValue.is_null()) {
            if (!cameraValue.is_object()) {
                return invalid_model("node camera must be an object");
            }
            node.hasCamera = true;
            CONJURE_TRY_ASSIGN(node.cameraData, read_camera(cameraValue));
        }
        result.mNodes.push_back(std::move(node));
        ++nodeIndex;
    }
    return result;
}

core::result<model_asset> model_asset::parse_blob(std::span<const std::byte> bytes) {
    return parse_json(std::string_view(reinterpret_cast<const char *>(bytes.data()), bytes.size()));
}

std::string model_asset::to_json() const {
    core::json::writer out = {};
    out.begin_object();
    out.key("materials");
    out.begin_array();
    for (const assets::asset_id &id : mMaterials) {
        out.value(id.to_string());
    }
    out.end_array();
    out.key("meshes");
    out.begin_array();
    for (const assets::asset_id &id : mMeshes) {
        out.value(id.to_string());
    }
    out.end_array();
    out.key("name");
    out.value(mName);
    out.key("nodes");
    out.begin_array();
    for (const model_node &node : mNodes) {
        out.begin_object();
        if (node.hasCamera) {
            out.key("camera");
            write_camera(node.cameraData, out);
        }
        if (node.hasLight) {
            out.key("light");
            write_light(node.lightData, out);
        }
        out.key("materials");
        out.begin_array();
        for (const u32 material : node.materials) {
            write_index(material, out);
        }
        out.end_array();
        out.key("mesh");
        write_index(node.mesh, out);
        out.key("name");
        out.value(node.name);
        out.key("parent");
        write_index(node.parent, out);
        out.key("transform");
        core::reflect::write_json(node.localTransform, out);
        out.end_object();
    }
    out.end_array();
    out.key("version");
    out.value(MODEL_FORMAT_VERSION);
    out.end_object();
    return out.finish();
}

std::vector<std::byte> model_asset::write_blob() const {
    const std::string text = to_json();
    std::vector<std::byte> bytes(text.size());
    std::transform(text.begin(), text.end(), bytes.begin(), [](char character) {
        return static_cast<std::byte>(character);
    });
    return bytes;
}

u32 model_asset::add_node(model_node node) {
    const bool validParent = node.parent == INVALID_INDEX || node.parent < mNodes.size();
    CONJURE_ASSERT(validParent, "model_asset::add_node: parent {} does not exist", node.parent);
    if (!validParent) {
        return INVALID_INDEX;
    }
    mNodes.push_back(std::move(node));
    return static_cast<u32>(mNodes.size() - 1);
}

u32 model_asset::add_mesh(const assets::asset_id &id) {
    mMeshes.push_back(id);
    return static_cast<u32>(mMeshes.size() - 1);
}

u32 model_asset::add_material(const assets::asset_id &id) {
    mMaterials.push_back(id);
    return static_cast<u32>(mMaterials.size() - 1);
}

std::vector<u32> model_asset::root_nodes() const {
    std::vector<u32> roots = {};
    for (usize index = 0; index < mNodes.size(); ++index) {
        if (mNodes[index].parent == INVALID_INDEX) {
            roots.push_back(static_cast<u32>(index));
        }
    }
    return roots;
}

std::vector<u32> model_asset::children_of(u32 node) const {
    std::vector<u32> children = {};
    for (usize index = 0; index < mNodes.size(); ++index) {
        if (mNodes[index].parent == node) {
            children.push_back(static_cast<u32>(index));
        }
    }
    return children;
}

math::transform model_asset::world_transform(u32 node) const noexcept {
    if (node >= mNodes.size()) {
        return math::transform();
    }
    math::transform result = mNodes[node].localTransform;
    u32 parent = mNodes[node].parent;
    // Parents precede children, so the walk terminates after at most node steps.
    while (parent != INVALID_INDEX && parent < mNodes.size()) {
        result = mNodes[parent].localTransform * result;
        parent = mNodes[parent].parent;
    }
    return result;
}

bool model_asset::operator==(const model_asset &other) const noexcept {
    return mName == other.mName && mMeshes == other.mMeshes && mMaterials == other.mMaterials && mNodes == other.mNodes;
}

/*===========================================================================*/
/*===========================================================================*/
// model_loader

model_loader::model_loader() {}

model_loader::~model_loader() {}

core::result<model_asset> model_loader::load_cpu_typed(
  const assets::load_context &,
  std::span<const std::byte> payloadBytes
) const {
    return model_asset::parse_blob(payloadBytes);
}

core::result<model_asset> model_loader::finalise_typed(
  const assets::finalise_context &,
  model_asset &&intermediate
) const {
    return std::move(intermediate);
}
