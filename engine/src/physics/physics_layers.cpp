#include "conjure/physics/physics_layers.h"

#include "conjure/physics/physics_error.h"

#include "conjure/core/assert.h"

#include <string>
#include <utility>

namespace conjure::physics {

namespace {

    [[nodiscard]] constexpr u32 layer_bit(u8 layer) noexcept {
        return u32{1} << layer;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

physics_layer_table::physics_layer_table() {
    mNames.push_back(core::string_id::intern("default"));
    mCollides[0] = layer_bit(0);
}

physics_layer_table::~physics_layer_table() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

core::result<physics_layer_table> physics_layer_table::from_json(const core::json::value &section) {
    if (!section.is_object()) {
        return core::make_error(physics_error::LAYER_CONFIG_INVALID, "physics_layers must be an object");
    }
    const core::json::value names = section["names"];
    if (!names.is_array() || names.size() == 0) {
        return core::make_error(physics_error::LAYER_CONFIG_INVALID, "physics_layers.names must be a non-empty array");
    }
    if (names.size() > MAX_PHYSICS_LAYERS) {
        return core::make_error(
          physics_error::LAYER_LIMIT_REACHED,
          "physics_layers.names holds {} layers; at most {} are supported",
          names.size(),
          MAX_PHYSICS_LAYERS
        );
    }

    physics_layer_table table = {};
    table.mNames.clear();
    table.mCollides.fill(0);
    for (const core::json::value item : names) {
        if (!item.is_string() || item.as_string().empty()) {
            return core::make_error(
              physics_error::LAYER_CONFIG_INVALID,
              "physics_layers.names entries must be strings"
            );
        }
        CONJURE_TRY(table.add_layer(core::string_id(item.as_string())));
    }

    const core::json::value ignore = section["ignore"];
    if (ignore.is_valid() && !ignore.is_array()) {
        return core::make_error(physics_error::LAYER_CONFIG_INVALID, "physics_layers.ignore must be an array of pairs");
    }
    for (const core::json::value pair : ignore) {
        if (!pair.is_array() || pair.size() != 2 || !pair[0].is_string() || !pair[1].is_string()) {
            return core::make_error(
              physics_error::LAYER_CONFIG_INVALID,
              "physics_layers.ignore entries must be [a, b]"
            );
        }
        const std::optional<u8> first = table.find(core::string_id(pair[0].as_string()));
        const std::optional<u8> second = table.find(core::string_id(pair[1].as_string()));
        if (!first.has_value()) {
            return core::make_error(
              physics_error::UNKNOWN_LAYER,
              "physics_layers.ignore names '{}'",
              pair[0].as_string()
            );
        }
        if (!second.has_value()) {
            return core::make_error(
              physics_error::UNKNOWN_LAYER,
              "physics_layers.ignore names '{}'",
              pair[1].as_string()
            );
        }
        table.set_collides(*first, *second, false);
    }
    return table;
}

core::result<u8> physics_layer_table::add_layer(core::string_id layerName) {
    if (mNames.size() >= MAX_PHYSICS_LAYERS) {
        return core::make_error(physics_error::LAYER_LIMIT_REACHED, "at most {} layers", MAX_PHYSICS_LAYERS);
    }
    if (layerName.is_null()) {
        return core::make_error(physics_error::DUPLICATE_LAYER, "the null name cannot be a layer");
    }
    if (find(layerName).has_value()) {
        return core::make_error(physics_error::DUPLICATE_LAYER, "layer '{}' already exists", layerName);
    }

    const u8 index = static_cast<u8>(mNames.size());
    mNames.push_back(layerName);

    // A new layer collides with every existing layer and with itself.
    for (u8 other = 0; other <= index; ++other) {
        mCollides[other] |= layer_bit(index);
        mCollides[index] |= layer_bit(other);
    }
    return index;
}

void physics_layer_table::set_collides(u8 a, u8 b, bool collidesFlag) noexcept {
    if (a >= mNames.size() || b >= mNames.size()) {
        return;
    }
    if (collidesFlag) {
        mCollides[a] |= layer_bit(b);
        mCollides[b] |= layer_bit(a);
        return;
    }
    mCollides[a] &= ~layer_bit(b);
    mCollides[b] &= ~layer_bit(a);
}

bool physics_layer_table::collides(u8 a, u8 b) const noexcept {
    if (a >= mNames.size() || b >= mNames.size()) {
        return false;
    }
    return (mCollides[a] & layer_bit(b)) != 0;
}

physics_layer_mask physics_layer_table::collision_mask(u8 layer) const noexcept {
    if (layer >= mNames.size()) {
        return physics_layer_mask::none();
    }
    return physics_layer_mask{mCollides[layer]};
}

std::optional<u8> physics_layer_table::find(core::string_id layerName) const noexcept {
    for (usize index = 0; index < mNames.size(); ++index) {
        if (mNames[index] == layerName) {
            return static_cast<u8>(index);
        }
    }
    return std::nullopt;
}

core::string_id physics_layer_table::name(u8 layer) const noexcept {
    if (layer >= mNames.size()) {
        return core::string_id::null();
    }
    return mNames[layer];
}

u32 physics_layer_table::count() const noexcept {
    return static_cast<u32>(mNames.size());
}

core::result<void> physics_layer_table::validate() const {
    if (mNames.empty()) {
        return core::make_error(physics_error::LAYER_CONFIG_INVALID, "a layer table needs at least one layer");
    }
    const u32 layerCount = count();
    const u32 validBits = layerCount == MAX_PHYSICS_LAYERS ? 0xFFFFFFFFu : (u32{1} << layerCount) - 1;
    for (u8 a = 0; a < layerCount; ++a) {
        if ((mCollides[a] & ~validBits) != 0) {
            return core::make_error(
              physics_error::LAYER_CONFIG_INVALID,
              "layer {} collides with a layer outside the table",
              a
            );
        }
        for (u8 b = 0; b < layerCount; ++b) {
            if (collides(a, b) != collides(b, a)) {
                return core::make_error(
                  physics_error::LAYER_CONFIG_INVALID,
                  "collision matrix is not symmetric at ({}, {})",
                  a,
                  b
                );
            }
        }
        for (u8 b = static_cast<u8>(a + 1); b < layerCount; ++b) {
            if (mNames[a] == mNames[b]) {
                return core::make_error(physics_error::DUPLICATE_LAYER, "layers {} and {} share a name", a, b);
            }
        }
    }
    return core::result<void>::success();
}

core::result<void> physics_layer_table::to_json(core::json::writer &out) const {
    const u32 layerCount = count();
    for (u8 layer = 0; layer < layerCount; ++layer) {
        if (mNames[layer].view().empty()) {
            return core::make_error(physics_error::LAYER_CONFIG_INVALID, "layer {} has no interned name", layer);
        }
    }

    // Keys in sorted order so the text equals document::to_canonical_string of its parse.
    out.begin_object();
    out.key("ignore");
    out.begin_array();
    for (u8 a = 0; a < layerCount; ++a) {
        for (u8 b = a; b < layerCount; ++b) {
            if (collides(a, b)) {
                continue;
            }
            out.begin_array();
            out.value(mNames[a].view());
            out.value(mNames[b].view());
            out.end_array();
        }
    }
    out.end_array();
    out.key("names");
    out.begin_array();
    for (u8 layer = 0; layer < layerCount; ++layer) {
        out.value(mNames[layer].view());
    }
    out.end_array();
    out.end_object();
    return core::result<void>::success();
}

bool physics_layer_table::operator==(const physics_layer_table &other) const noexcept {
    return mNames == other.mNames && mCollides == other.mCollides;
}
}
