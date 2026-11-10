#pragma once

#include "conjure/assets/asset_ref.h"
#include "conjure/assets/asset_slot.h"
#include "conjure/assets/asset_types.h"
#include "conjure/assets/binary_asset.h"
#include "conjure/core/types.h"

#include <cstddef>
#include <type_traits>

// The untyped view of an assets::asset_ref<T> the bind pass writes through (docs/design/13_scene.md 3.10,
// R25): the uuid leads the object and the slot pointer follows. The static_asserts pin the contract with
// 05_assets.md 2.3; register_components() adds a runtime self-test against a constructed reference.

namespace conjure::scene::detail {

    struct asset_ref_raw {
        assets::asset_id id = {};
        const assets::asset_slot *slot = nullptr;
    };

    static_assert(std::is_standard_layout_v<asset_ref_raw>, "asset_ref_raw must be standard layout");
    static_assert(std::is_trivially_copyable_v<asset_ref_raw>, "asset_ref_raw must be trivially copyable");
    static_assert(
      sizeof(asset_ref_raw) == sizeof(assets::asset_ref<assets::binary_asset>),
      "asset_ref_raw must have the size of asset_ref<T>"
    );
    static_assert(
      alignof(asset_ref_raw) == alignof(assets::asset_ref<assets::binary_asset>),
      "asset_ref_raw must have the alignment of asset_ref<T>"
    );
    static_assert(sizeof(asset_ref_raw) == 24, "asset_ref is 24 bytes (05_assets.md 2.3)");
    static_assert(offsetof(asset_ref_raw, id) == 0, "the uuid leads an asset_ref");
    static_assert(offsetof(asset_ref_raw, slot) == 16, "the slot pointer follows the uuid");
    static_assert(
      std::is_standard_layout_v<assets::asset_ref<assets::binary_asset>>,
      "asset_ref<T> must be standard layout for the raw view"
    );
}
