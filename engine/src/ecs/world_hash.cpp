#include "conjure/ecs/world_hash.h"

#include "conjure/ecs/archetype.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/serialization.h"
#include "conjure/ecs/world.h"

#include "conjure/core/containers/small_vector.h"
#include "conjure/core/hash.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/reflect/field_info.h"
#include "conjure/core/reflect/type_info.h"
#include "conjure/core/uuid.h"

#include <algorithm>
#include <cstddef>
#include <span>
#include <string>

using namespace conjure;
using namespace conjure::ecs;

namespace {
    /// <summary>
    /// FNV-1a over the bytes of every reflected field, nested structs walked recursively, so the
    /// padding between fields (which add/set copy from the caller's object) never reaches the hash.
    /// An ASSET_REF wider than a uuid (assets::asset_ref: the uuid, then the bound slot pointer, R25)
    /// contributes its leading uuid only, like its JSON form: the pointer is runtime binding state
    /// that differs between processes and between a bound and an unbound reference to one asset.
    /// </summary>
    u64 hash_fields(const core::reflect::type_info &type, const std::byte *base, u64 hash) {
        using core::reflect::field_kind;
        for (const core::reflect::field_info &field : type.fields) {
            const std::byte *fieldBytes = base + field.offset;
            const u32 elementSize = field.element_size();
            const u32 elementCount = field.is_array() ? field.arrayCount : 1u;
            if (field.value_kind() == field_kind::STRUCT && field.structType != nullptr) {
                for (u32 element = 0; element < elementCount; ++element) {
                    hash = hash_fields(*field.structType, fieldBytes + static_cast<usize>(elementSize) * element, hash);
                }
                continue;
            }
            if (field.value_kind() == field_kind::ASSET_REF && elementSize > sizeof(core::uuid)) {
                for (u32 element = 0; element < elementCount; ++element) {
                    const std::byte *identity = fieldBytes + static_cast<usize>(elementSize) * element;
                    hash = core::hash::fnv1a64(std::span<const std::byte>(identity, sizeof(core::uuid)), hash);
                }
                continue;
            }
            hash = core::hash::fnv1a64(std::span<const std::byte>(fieldBytes, field.size), hash);
        }
        return hash;
    }

    u64 hash_component(const component_info &info, const void *data) {
        if (info.is_tag()) {
            return info.name.hash();
        }
        u64 bytesHash = 0;
        if (info.is_trivial()) {
            const std::byte *bytes = static_cast<const std::byte *>(data);
            if (info.reflection != nullptr) {
                bytesHash = hash_fields(*info.reflection, bytes, core::hash::FNV1A64_OFFSET_BASIS);
            } else {
                bytesHash = core::hash::fnv1a64(std::span<const std::byte>(bytes, info.size));
            }
        } else {
            // Non-trivial types hash their serialised text (written in field order, so deterministic).
            core::json::writer out = {};
            const core::result<void> written = write_component_json(info, data, out);
            const std::string text = written.has_value() ? out.finish() : std::string("<unserialisable>");
            bytesHash = core::hash::fnv1a64(std::string_view(text));
        }
        return core::hash::combine(info.name.hash(), bytesHash);
    }
}

u64 conjure::ecs::world_hash(const world &source) {
    const component_registry &registry = source.registry();

    // Visiting order independent of creation order: ascending component-set hash, then index.
    const std::span<archetype *const> archetypes = source.archetypes();
    core::small_vector<const archetype *, 64> ordered = {};
    for (const archetype *owner : archetypes) {
        if (owner->entity_count() > 0) {
            ordered.push_back(owner);
        }
    }
    std::sort(ordered.begin(), ordered.end(), [](const archetype *left, const archetype *right) {
        const u64 leftHash = left->mask().hash();
        const u64 rightHash = right->mask().hash();
        if (leftHash != rightHash) {
            return leftHash < rightHash;
        }
        return left->index() < right->index();
    });

    u64 hash = core::hash::FNV1A64_OFFSET_BASIS;
    for (const archetype *owner : ordered) {
        const chunk_layout &layout = owner->layout();
        for (u32 chunkIndex = 0; chunkIndex < owner->chunk_count(); ++chunkIndex) {
            const std::span<const entity> entities = owner->chunk_entities(chunkIndex);
            for (u32 row = 0; row < entities.size(); ++row) {
                u64 entityHash = core::hash::fnv1a64(
                  std::span<const std::byte>(reinterpret_cast<const std::byte *>(&entities[row]), sizeof(entity))
                );
                owner->mask().for_each_set([&](component_id id) {
                    const component_info &info = registry.info(id);
                    const void *data = nullptr;
                    const u16 column = layout.column_of(id);
                    if (column != chunk_layout::NO_COLUMN) {
                        const std::byte *columnStart =
                          static_cast<const std::byte *>(owner->chunk_column(chunkIndex, id));
                        data = columnStart + static_cast<usize>(layout.columnSizes[column]) * row;
                    }
                    entityHash = core::hash::combine(entityHash, hash_component(info, data));
                });
                hash = core::hash::combine(hash, entityHash);
            }
        }
    }
    return hash;
}
