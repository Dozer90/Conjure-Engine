#include "conjure/ecs/component_registry.h"

#include "conjure/ecs/component_type_desc.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/log_category.h"
#include "conjure/ecs/serialization.h"
#include "conjure/ecs/world.h"

#include "conjure/core/assert.h"
#include "conjure/core/log.h"
#include "conjure/core/math/math_utils.h"
#include "conjure/core/reflect/field_info.h"
#include "conjure/core/reflect/type_id.h"
#include "conjure/core/uuid.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <string>
#include <utility>

using namespace conjure;
using namespace conjure::ecs;

namespace {
    std::atomic<u32> sNextSerial = 1;

    /// <summary>
    /// The field kind of a dynamic field's type id, or COUNT when the id is not a supported kind.
    /// Dynamic fields are primitives, math types or entity references (01_ecs.md 2.2).
    /// </summary>
    core::reflect::field_kind dynamic_field_kind(core::reflect::type_id type) noexcept {
        using core::reflect::field_kind;
        using core::reflect::type_id_of;
        struct kind_entry {
            core::reflect::type_id id = {};
            field_kind kind = field_kind::COUNT;
        };
        const kind_entry table[] = {
            {type_id_of<bool>(), field_kind::BOOL},
            {type_id_of<i8>(), field_kind::I8},
            {type_id_of<i16>(), field_kind::I16},
            {type_id_of<i32>(), field_kind::I32},
            {type_id_of<i64>(), field_kind::I64},
            {type_id_of<u8>(), field_kind::U8},
            {type_id_of<u16>(), field_kind::U16},
            {type_id_of<u32>(), field_kind::U32},
            {type_id_of<u64>(), field_kind::U64},
            {type_id_of<f32>(), field_kind::F32},
            {type_id_of<f64>(), field_kind::F64},
            {type_id_of<core::string_id>(), field_kind::STRING_ID},
            {type_id_of<core::uuid>(), field_kind::UUID},
            {type_id_of<math::vec2>(), field_kind::VEC2},
            {type_id_of<math::vec3>(), field_kind::VEC3},
            {type_id_of<math::vec4>(), field_kind::VEC4},
            {type_id_of<math::quat>(), field_kind::QUAT},
            {type_id_of<math::mat4>(), field_kind::MAT4},
            {type_id_of<math::transform>(), field_kind::TRANSFORM},
            {type_id_of<entity>(), field_kind::ENTITY},
        };
        for (const kind_entry &entry : table) {
            if (entry.id == type) {
                return entry.kind;
            }
        }
        return field_kind::COUNT;
    }

    u32 dynamic_field_size(core::reflect::field_kind kind) noexcept {
        if (kind == core::reflect::field_kind::ENTITY) {
            return static_cast<u32>(sizeof(entity));
        }
        return core::reflect::kind_size(kind);
    }

    /// <summary>
    /// True when two descriptions list the same fields in the same order: name, type, kind, offset and
    /// size. Two null descriptions match; a null and a non-null one do not.
    /// </summary>
    bool same_fields(const core::reflect::type_info *existing, const core::reflect::type_info *candidate) noexcept {
        if (existing == nullptr || candidate == nullptr) {
            return existing == candidate;
        }
        if (existing->fields.size() != candidate->fields.size()) {
            return false;
        }
        for (usize index = 0; index < existing->fields.size(); ++index) {
            const core::reflect::field_info &left = existing->fields[index];
            const core::reflect::field_info &right = candidate->fields[index];
            if (left.name != right.name || left.type != right.type || left.kind != right.kind ||
              left.offset != right.offset || left.size != right.size) {
                return false;
            }
        }
        return true;
    }

    bool same_layout(const component_info &existing, const component_info &candidate) noexcept {
        if (existing.size != candidate.size || existing.alignment != candidate.alignment ||
          existing.flags != candidate.flags) {
            return false;
        }
        // A runtime-described (C#) type is defined by its fields: the same name and size with reordered,
        // renamed or retyped fields is another layout, which redefine_dynamic replaces.
        const bool bothDynamic =
          has_flag(existing.flags, component_flags::DYNAMIC) && has_flag(candidate.flags, component_flags::DYNAMIC);
        if (bothDynamic && !same_fields(existing.reflection, candidate.reflection)) {
            return false;
        }
        return true;
    }

    /// <summary>
    /// Copies a description and, recursively, the nested struct and enum descriptions its fields point
    /// at, so nothing in the copy refers to memory the describing module owns. Descriptions cannot
    /// contain themselves by value, so the recursion is finite.
    /// </summary>
    const core::reflect::type_info *deep_copy_type(
      const core::reflect::type_info &source,
      std::vector<std::unique_ptr<core::reflect::type_info>> &types,
      std::vector<std::unique_ptr<core::reflect::enum_info>> &enums
    ) {
        std::unique_ptr<core::reflect::type_info> copy = std::make_unique<core::reflect::type_info>(source);
        for (core::reflect::field_info &field : copy->fields) {
            if (field.structType != nullptr) {
                field.structType = deep_copy_type(*field.structType, types, enums);
            }
            if (field.enumType != nullptr) {
                enums.push_back(std::make_unique<core::reflect::enum_info>(*field.enumType));
                field.enumType = enums.back().get();
            }
        }
        types.push_back(std::move(copy));
        return types.back().get();
    }

    /// <summary>
    /// Recursively records the entity and asset-reference field offsets of a description and rejects
    /// field kinds the serializer cannot handle.
    /// </summary>
    core::result<void> walk_fields(const core::reflect::type_info &type, u32 baseOffset, component_info &info) {
        using core::reflect::field_kind;
        for (const core::reflect::field_info &field : type.fields) {
            const u32 offset = baseOffset + field.offset;
            const field_kind kind = field.value_kind();
            const u32 elementSize = field.is_array() ? field.element_size() : field.size;
            const u32 elementCount = field.is_array() ? field.arrayCount : 1u;
            if (field.is_array() && field.elementKind == field_kind::ARRAY) {
                return make_error(ecs_error::UNSUPPORTED_FIELD, "field '" + field.name + "': nested arrays");
            }

            for (u32 element = 0; element < elementCount; ++element) {
                const u32 elementOffset = offset + element * elementSize;
                switch (kind) {
                    case field_kind::ENTITY:
                        if (elementSize != sizeof(entity)) {
                            return make_error(
                              ecs_error::UNSUPPORTED_FIELD,
                              "field '" + field.name + "': ENTITY fields must be ecs::entity (8 bytes)"
                            );
                        }
                        info.entityFieldOffsets.push_back(elementOffset);
                        break;
                    case field_kind::ASSET_REF:
                        if (elementSize < sizeof(core::uuid)) {
                            return make_error(
                              ecs_error::UNSUPPORTED_FIELD,
                              "field '" + field.name + "': ASSET_REF fields must start with a uuid"
                            );
                        }
                        info.assetRefFieldOffsets.push_back(elementOffset);
                        break;
                    case field_kind::STRUCT:
                        if (field.structType == nullptr) {
                            return make_error(
                              ecs_error::UNSUPPORTED_FIELD,
                              "field '" + field.name + "': nested struct without a description"
                            );
                        }
                        CONJURE_TRY(walk_fields(*field.structType, elementOffset, info));
                        break;
                    case field_kind::ENUM:
                        if (field.enumType == nullptr) {
                            return make_error(
                              ecs_error::UNSUPPORTED_FIELD,
                              "field '" + field.name + "': enum without a description"
                            );
                        }
                        break;
                    case field_kind::ARRAY:
                    case field_kind::COUNT:
                        return make_error(ecs_error::UNSUPPORTED_FIELD, "field '" + field.name + "': unknown kind");
                    default:
                        break;
                }
            }
        }
        return core::result<void>::success();
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

component_registry::component_registry() :
    mSerial(sNextSerial.fetch_add(1, std::memory_order_relaxed)) {
    mTypes.reserve(MAX_COMPONENTS);

    // Id 0 preserves unknown component data across a load/save round trip (01_ecs.md D11).
    component_info unresolved = {};
    unresolved.name = core::string_id(UNRESOLVED_COMPONENTS_NAME);
    unresolved.size = static_cast<u32>(sizeof(unresolved_components));
    unresolved.alignment = static_cast<u32>(alignof(unresolved_components));
    unresolved.flags = component_flags::EDITOR_HIDDEN;
    unresolved.ops = make_component_ops<unresolved_components>();
    unresolved.serializer.writeJson = &detail::write_unresolved_components;
    unresolved.serializer.readJson = &detail::read_unresolved_components;
    const core::result<component_id> registered = register_info(std::move(unresolved));
    CONJURE_ASSERT(registered.has_value() && registered.value().value == 0, "unresolved_components must be id 0");
}

component_registry::~component_registry() {
    CONJURE_ASSERT(mWorlds.empty(), "component_registry destroyed while {} world(s) still use it", mWorlds.size());
}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

core::result<component_id> component_registry::register_described(const component_type_desc &desc) {
    if (desc.name == nullptr) {
        return core::error(core::error_code::INVALID_ARGUMENT, "component_type_desc has no name");
    }
    component_info info = {};
    info.name = core::string_id(std::string_view(desc.name));
    info.size = desc.size;
    info.alignment = desc.alignment;
    info.flags = desc.flags;
    info.ops = desc.ops;
    info.serializer = desc.serializer;

    // The module may unload: the registry keeps a private deep copy of the description. The copy is
    // owned locally until registration succeeds so a refused type leaves nothing behind.
    std::vector<std::unique_ptr<core::reflect::type_info>> copiedTypes = {};
    std::vector<std::unique_ptr<core::reflect::enum_info>> copiedEnums = {};
    if (desc.reflection != nullptr) {
        info.reflection = deep_copy_type(*desc.reflection, copiedTypes, copiedEnums);
    }
    const core::result<component_id> registered = register_info(std::move(info));
    if (!registered.has_value()) {
        return registered.get_error();
    }
    if (desc.reflection != nullptr && this->info(registered.value()).reflection == copiedTypes.back().get()) {
        // A re-registration of a known name returns the existing id and keeps its own description.
        const std::scoped_lock lock(mMutex);
        for (std::unique_ptr<core::reflect::type_info> &copy : copiedTypes) {
            mDynamicTypes.push_back(std::move(copy));
        }
        for (std::unique_ptr<core::reflect::enum_info> &copy : copiedEnums) {
            mDynamicEnums.push_back(std::move(copy));
        }
    }
    return registered.value();
}

core::result<component_id> component_registry::register_dynamic(const dynamic_component_desc &desc) {
    if (desc.name.is_null()) {
        return core::error(core::error_code::INVALID_ARGUMENT, "dynamic component has no name");
    }
    if (desc.alignment == 0 || desc.alignment > 16 || !math::is_power_of_two(desc.alignment)) {
        return make_error(ecs_error::BAD_ALIGNMENT, "dynamic component alignment must be 1, 2, 4, 8 or 16");
    }
    if (desc.size > MAX_COMPONENT_SIZE) {
        return make_error(ecs_error::COMPONENT_TOO_LARGE, "dynamic component exceeds MAX_COMPONENT_SIZE");
    }
    if (!desc.defaultValue.empty() && desc.defaultValue.size() != desc.size) {
        return core::error(core::error_code::INVALID_ARGUMENT, "dynamic default value size differs from the type size");
    }
    CONJURE_TRY_ASSIGN(std::unique_ptr<core::reflect::type_info> type, build_dynamic_type(desc));

    component_info info = {};
    info.name = desc.name;
    info.size = desc.size;
    info.alignment = desc.alignment;
    info.flags = desc.flags | component_flags::DYNAMIC | component_flags::TRIVIAL;
    if (desc.size == 0) {
        info.flags |= component_flags::TAG;
    }
    info.reflection = type.get();
    info.defaultValue.assign(desc.defaultValue.begin(), desc.defaultValue.end());
    const core::result<component_id> registered = register_info(std::move(info));
    if (!registered.has_value()) {
        return registered.get_error();
    }
    if (this->info(registered.value()).reflection == type.get()) {
        // A re-registration of a known name returns the existing id and keeps its own description.
        const std::scoped_lock lock(mMutex);
        mDynamicTypes.push_back(std::move(type));
    }
    return registered.value();
}

core::result<void> component_registry::redefine_dynamic(component_id id, const dynamic_component_desc &desc) {
    if (!is_registered(id) || !has_flag(info(id).flags, component_flags::DYNAMIC)) {
        return make_error(ecs_error::UNKNOWN_COMPONENT, "redefine_dynamic needs a registered dynamic component");
    }
    if (!desc.name.is_null() && desc.name != info(id).name) {
        return make_error(ecs_error::NAME_CONFLICT, "redefine_dynamic cannot rename a component");
    }
    if (desc.alignment == 0 || desc.alignment > 16 || !math::is_power_of_two(desc.alignment)) {
        return make_error(ecs_error::BAD_ALIGNMENT, "dynamic component alignment must be 1, 2, 4, 8 or 16");
    }
    if (desc.size > MAX_COMPONENT_SIZE) {
        return make_error(ecs_error::COMPONENT_TOO_LARGE, "dynamic component exceeds MAX_COMPONENT_SIZE");
    }
    if (!desc.defaultValue.empty() && desc.defaultValue.size() != desc.size) {
        return core::error(core::error_code::INVALID_ARGUMENT, "dynamic default value size differs from the type size");
    }
    if (has_instances(id)) {
        return make_error(ecs_error::COMPONENT_IN_USE, "redefine_dynamic while instances exist; remove_from_all first");
    }
    CONJURE_TRY_ASSIGN(std::unique_ptr<core::reflect::type_info> type, build_dynamic_type(desc));

    component_info replacement = {};
    replacement.id = id;
    replacement.name = info(id).name;
    replacement.size = desc.size;
    replacement.alignment = desc.alignment;
    replacement.flags = desc.flags | component_flags::DYNAMIC | component_flags::TRIVIAL;
    if (desc.size == 0) {
        replacement.flags |= component_flags::TAG;
    }
    replacement.reflection = type.get();
    replacement.defaultValue.assign(desc.defaultValue.begin(), desc.defaultValue.end());
    CONJURE_TRY(collect_field_offsets(replacement));

    std::vector<world *> worlds = {};
    {
        const std::scoped_lock lock(mMutex);
        mDynamicTypes.push_back(std::move(type));
        mTypes[id.value] = std::move(replacement);
        worlds = mWorlds;
    }
    // Empty archetypes that still carry the old layout must be laid out again before reuse.
    for (world *target : worlds) {
        target->relayout_archetypes(id);
    }
    return core::result<void>::success();
}

component_id component_registry::find(core::string_id name) const noexcept {
    const std::scoped_lock lock(mMutex);
    const auto found = mByName.find(name);
    if (found == mByName.end()) {
        return component_id{};
    }
    return found->second;
}

const component_info &component_registry::info(component_id id) const noexcept {
    if (!is_registered(id)) {
        CONJURE_ASSERT(false, "component_registry::info with an unregistered id {}", id.value);
        return mTypes[0];
    }
    return mTypes[id.value];
}

std::span<const component_info> component_registry::types() const noexcept {
    return std::span<const component_info>(mTypes.data(), count());
}

bool component_registry::has_instances(component_id id) const {
    if (!is_registered(id)) {
        return false;
    }
    const std::scoped_lock lock(mMutex);
    for (const world *target : mWorlds) {
        if (target->instance_count(id) > 0) {
            return true;
        }
    }
    return false;
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

core::result<component_id> component_registry::register_info(component_info info) {
    if (info.name.is_null()) {
        return core::error(core::error_code::INVALID_ARGUMENT, "component has no name");
    }
    CONJURE_TRY(validate_layout(info));
    CONJURE_TRY(collect_field_offsets(info));

    const component_info *registered = nullptr;
    {
        const std::scoped_lock lock(mMutex);
        const auto existing = mByName.find(info.name);
        if (existing != mByName.end()) {
            const component_info &current = mTypes[existing->second.value];
            if (!same_layout(current, info)) {
                return make_error(
                  ecs_error::NAME_CONFLICT,
                  "component '" + info.name.to_string() + "' is already registered with a different layout"
                );
            }
            return existing->second;
        }

        const u32 count = mCount.load(std::memory_order_relaxed);
        if (count >= MAX_COMPONENTS) {
            return make_error(ecs_error::COMPONENT_LIMIT_REACHED, "MAX_COMPONENTS types are registered");
        }
        info.id = component_id{static_cast<u16>(count)};
        mTypes.push_back(std::move(info));
        registered = &mTypes.back();
        mByName.emplace(registered->name, registered->id);
        mCount.store(count + 1, std::memory_order_release);
    }

    CONJURE_LOG_TRACE(LOG_ECS, "registered component '{}' as id {}", registered->name, registered->id.value);
    mOnRegistered.emit(*registered);
    return registered->id;
}

core::result<void> component_registry::validate_layout(const component_info &info) const {
    if (info.size > MAX_COMPONENT_SIZE) {
        return make_error(
          ecs_error::COMPONENT_TOO_LARGE,
          "component '" + info.name.to_string() + "' exceeds 4096 bytes"
        );
    }
    if (info.alignment == 0 || info.alignment > MAX_COMPONENT_ALIGNMENT || !math::is_power_of_two(info.alignment)) {
        return make_error(ecs_error::BAD_ALIGNMENT, "component '" + info.name.to_string() + "' has a bad alignment");
    }
    if (!info.is_trivial() && info.alignment > 16) {
        // Archive payloads live in std::vector<std::byte> storage, which only guarantees 16 bytes.
        return make_error(ecs_error::BAD_ALIGNMENT, "non-trivial components must not need more than 16 byte alignment");
    }
    if (info.is_tag() && info.size != 0) {
        return core::error(
          core::error_code::INVALID_ARGUMENT,
          "component '" + info.name.to_string() + "': a TAG component must have size 0"
        );
    }
    if (!info.is_tag() && info.size == 0) {
        return core::error(
          core::error_code::INVALID_ARGUMENT,
          "component '" + info.name.to_string() + "': a component with size 0 must be a TAG"
        );
    }
    if (!info.defaultValue.empty() && info.defaultValue.size() != info.size) {
        return core::error(core::error_code::INVALID_ARGUMENT, "default value size differs from the component size");
    }

    const bool hasCustomHooks = info.serializer.writeJson != nullptr && info.serializer.readJson != nullptr;
    if (info.reflection == nullptr && !info.is_tag() && !info.is_transient() && !hasCustomHooks) {
        return make_error(
          ecs_error::UNSUPPORTED_FIELD,
          "component '" + info.name.to_string() + "' has no reflection description and no serializer hooks"
        );
    }
    if (info.reflection != nullptr && !info.is_tag() && info.reflection->size != info.size) {
        return make_error(
          ecs_error::UNSUPPORTED_FIELD,
          "component '" + info.name.to_string() + "': reflection size differs from the component size"
        );
    }
    return core::result<void>::success();
}

core::result<void> component_registry::collect_field_offsets(component_info &info) const {
    info.entityFieldOffsets.clear();
    info.assetRefFieldOffsets.clear();
    if (info.reflection == nullptr) {
        return core::result<void>::success();
    }
    const core::result<void> walked = walk_fields(*info.reflection, 0, info);
    if (!walked.has_value()) {
        if (info.is_transient()) {
            // Never serialised: the description is informational only.
            info.entityFieldOffsets.clear();
            info.assetRefFieldOffsets.clear();
            return core::result<void>::success();
        }
        return make_error(
          ecs_error::UNSUPPORTED_FIELD,
          "component '" + info.name.to_string() + "': " + walked.get_error().message()
        );
    }
    return core::result<void>::success();
}

core::result<std::unique_ptr<core::reflect::type_info>> component_registry::build_dynamic_type(
  const dynamic_component_desc &desc
) const {
    std::vector<core::reflect::field_info> fields = {};
    fields.reserve(desc.fields.size());
    for (const dynamic_field_desc &field : desc.fields) {
        const core::reflect::field_kind kind = dynamic_field_kind(field.type);
        if (kind == core::reflect::field_kind::COUNT) {
            return make_error(
              ecs_error::UNSUPPORTED_FIELD,
              "dynamic field '" + field.name.to_string() + "' has an unsupported type"
            );
        }
        core::reflect::field_info described = {};
        described.name = field.name.to_string();
        described.type = field.type;
        described.kind = kind;
        described.offset = field.offset;
        described.size = dynamic_field_size(kind);
        fields.push_back(std::move(described));
    }

    core::result<core::reflect::type_info> built =
      core::reflect::build_type_info(desc.name.to_string(), desc.size, desc.alignment, fields);
    if (!built.has_value()) {
        return make_error(ecs_error::UNSUPPORTED_FIELD, built.get_error().message());
    }
    return std::make_unique<core::reflect::type_info>(std::move(built).value());
}

void component_registry::attach_world(world *target) const {
    const std::scoped_lock lock(mMutex);
    mWorlds.push_back(target);
}

void component_registry::detach_world(world *target) const {
    const std::scoped_lock lock(mMutex);
    std::erase(mWorlds, target);
}
