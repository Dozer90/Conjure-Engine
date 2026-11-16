#include "conjure/ecs/serialization.h"

#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/entity_archive.h"
#include "conjure/ecs/log_category.h"
#include "conjure/ecs/world.h"

#include "conjure/core/json/json.h"
#include "conjure/core/log.h"
#include "conjure/core/reflect/field_info.h"
#include "conjure/core/reflect/json_conversion.h"
#include "conjure/core/string_utils.h"
#include "conjure/core/uuid.h"

#include <algorithm>
#include <cstring>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::ecs;
using core::reflect::field_info;
using core::reflect::field_kind;

namespace {

    /*=======================================================================*/
    /*=======================================================================*/
    // Helpers

    std::string join_path(std::string_view parent, std::string_view name) {
        if (parent.empty()) {
            return std::string(name);
        }
        return std::format("{}.{}", parent, name);
    }

    /// <summary>
    /// Turns core::reflect's "field '<path>': detail" into the ecs form "<path>: detail".
    /// </summary>
    std::string strip_field_prefix(std::string_view message) {
        constexpr std::string_view PREFIX = "field '";
        if (!message.starts_with(PREFIX)) {
            return std::string(message);
        }
        const usize close = message.find("': ", PREFIX.size());
        if (close == std::string_view::npos) {
            return std::string(message);
        }
        const std::string_view path = message.substr(PREFIX.size(), close - PREFIX.size());
        return std::format("{}: {}", path, message.substr(close + 3));
    }

    core::error failed(std::string_view path, std::string_view detail) {
        return make_error(ecs_error::SERIALIZATION_FAILED, std::format("{}: {}", path, detail));
    }

    core::error wrap_reflect_error(const core::error &failure) {
        return make_error(ecs_error::SERIALIZATION_FAILED, strip_field_prefix(failure.message()));
    }

    bool handled_here(field_kind kind) noexcept {
        return kind == field_kind::ENTITY || kind == field_kind::ASSET_REF || kind == field_kind::STRUCT;
    }

    core::reflect::field_ref make_ref(const field_info &field, void *data, const std::string &path) {
        core::reflect::field_ref ref = {};
        ref.data = data;
        ref.field = &field;
        ref.isElement = false;
        ref.path = path;
        return ref;
    }

    /*=======================================================================*/
    /*=======================================================================*/
    // Entity and asset references

    void write_entity(entity value, core::json::writer &out) {
        if (value.is_null()) {
            out.value(nullptr);
            return;
        }
        if (value.is_archive_local()) {
            out.value(value.index);
            return;
        }
        out.begin_object();
        out.key("external");
        out.value(std::format("{}:{}", value.index, value.generation));
        out.end_object();
    }

    core::result<entity> read_entity(const core::json::value &in, const std::string &path) {
        if (in.is_null()) {
            return NULL_ENTITY;
        }
        if (in.is_number()) {
            if (!in.is_integer()) {
                return failed(path, "entity reference must be an integer local index");
            }
            const u64 index = in.as_u64(0xFFFFFFFFFFFFFFFFull);
            if (index >= ARCHIVE_LOCAL_GENERATION) {
                return failed(path, "entity local index out of range");
            }
            return entity::archive_local(static_cast<u32>(index));
        }
        if (in.is_object()) {
            const std::string_view external = in["external"].as_string();
            const usize colon = external.find(':');
            if (colon == std::string_view::npos) {
                return failed(path, "entity reference object needs \"external\":\"<index>:<generation>\"");
            }
            const std::optional<u32> index = core::str::parse_integer<u32>(external.substr(0, colon));
            const std::optional<u32> generation = core::str::parse_integer<u32>(external.substr(colon + 1));
            if (!index.has_value() || !generation.has_value()) {
                return failed(path, "entity reference is not <index>:<generation>");
            }
            return entity{*index, *generation};
        }
        return failed(path, "expected an entity reference (integer, null or {\"external\":...})");
    }

    void write_asset_ref(const void *data, core::json::writer &out) {
        core::uuid id = {};
        std::memcpy(&id, data, sizeof(core::uuid));
        if (id.is_null()) {
            out.value(nullptr);
            return;
        }
        out.value(id.to_string());
    }

    core::result<void> read_asset_ref(void *data, u32 size, const core::json::value &in, const std::string &path) {
        std::memset(data, 0, size);
        if (in.is_null()) {
            return core::result<void>::success();
        }
        if (!in.is_string()) {
            return failed(path, "asset reference must be a uuid string or null");
        }
        const std::optional<core::uuid> parsed = core::uuid::parse(in.as_string());
        if (!parsed.has_value()) {
            return failed(path, "asset reference is not a uuid");
        }
        const core::uuid id = *parsed;
        std::memcpy(data, &id, sizeof(core::uuid));
        return core::result<void>::success();
    }

    /*=======================================================================*/
    /*=======================================================================*/
    // Field walkers

    core::result<void> write_struct(
      const core::reflect::type_info &type,
      const void *object,
      core::json::writer &out,
      const std::string &path
    );

    core::result<void> write_value(
      field_kind kind,
      const field_info &field,
      u32 size,
      const void *data,
      core::json::writer &out,
      const std::string &path
    ) {
        switch (kind) {
            case field_kind::ENTITY: {
                entity value = {};
                std::memcpy(&value, data, sizeof(entity));
                write_entity(value, out);
                return core::result<void>::success();
            }
            case field_kind::ASSET_REF:
                write_asset_ref(data, out);
                return core::result<void>::success();
            case field_kind::STRUCT:
                if (field.structType == nullptr) {
                    return failed(path, "nested struct without a description");
                }
                return write_struct(*field.structType, data, out, path);
            default:
                break;
        }
        CONJURE_UNUSED(size);
        return core::result<void>::success();
    }

    core::result<void> write_field(
      const field_info &field,
      const void *data,
      core::json::writer &out,
      const std::string &path
    ) {
        const field_kind kind = field.value_kind();
        if (field.is_array() && handled_here(kind)) {
            const u32 elementSize = field.element_size();
            const std::byte *elements = static_cast<const std::byte *>(data);
            out.begin_array();
            for (u32 index = 0; index < field.arrayCount; ++index) {
                CONJURE_TRY(write_value(kind, field, elementSize, elements, out, std::format("{}[{}]", path, index)));
                elements += elementSize;
            }
            out.end_array();
            return core::result<void>::success();
        }
        if (!field.is_array() && handled_here(kind)) {
            return write_value(kind, field, field.size, data, out, path);
        }

        // Every other kind (and arrays of them) is core::reflect's business.
        core::result<void> written = core::reflect::write_json(make_ref(field, const_cast<void *>(data), path), out);
        if (!written.has_value()) {
            return wrap_reflect_error(written.get_error());
        }
        return core::result<void>::success();
    }

    core::result<void> write_struct(
      const core::reflect::type_info &type,
      const void *object,
      core::json::writer &out,
      const std::string &path
    ) {
        const std::byte *base = static_cast<const std::byte *>(object);
        out.begin_object();
        for (const field_info &field : type.fields) {
            out.key(field.name);
            CONJURE_TRY(write_field(field, base + field.offset, out, join_path(path, field.name)));
        }
        out.end_object();
        return core::result<void>::success();
    }

    core::result<void> read_struct(
      const core::reflect::type_info &type,
      void *object,
      const core::json::value &in,
      const std::string &path
    );

    core::result<void> read_value(
      field_kind kind,
      const field_info &field,
      u32 size,
      void *data,
      const core::json::value &in,
      const std::string &path
    ) {
        switch (kind) {
            case field_kind::ENTITY: {
                CONJURE_TRY_ASSIGN(const entity value, read_entity(in, path));
                std::memcpy(data, &value, sizeof(entity));
                return core::result<void>::success();
            }
            case field_kind::ASSET_REF:
                return read_asset_ref(data, size, in, path);
            case field_kind::STRUCT:
                if (field.structType == nullptr) {
                    return failed(path, "nested struct without a description");
                }
                return read_struct(*field.structType, data, in, path);
            default:
                break;
        }
        return core::result<void>::success();
    }

    core::result<void> read_field(
      const field_info &field,
      void *data,
      const core::json::value &in,
      const std::string &path
    ) {
        const field_kind kind = field.value_kind();
        if (field.is_array() && handled_here(kind)) {
            if (!in.is_array()) {
                return failed(path, std::format("expected array, found {}", core::json::to_string(in.type())));
            }
            if (in.size() > field.arrayCount) {
                return failed(
                  path,
                  std::format("array has {} elements but the field holds {}", in.size(), field.arrayCount)
                );
            }
            const u32 elementSize = field.element_size();
            std::byte *elements = static_cast<std::byte *>(data);
            usize index = 0;
            for (const core::json::value item : in) {
                CONJURE_TRY(read_value(kind, field, elementSize, elements, item, std::format("{}[{}]", path, index)));
                elements += elementSize;
                ++index;
            }
            return core::result<void>::success();
        }
        if (!field.is_array() && handled_here(kind)) {
            return read_value(kind, field, field.size, data, in, path);
        }

        core::result<void> read = core::reflect::set_from_json(make_ref(field, data, path), in);
        if (!read.has_value()) {
            return wrap_reflect_error(read.get_error());
        }
        return core::result<void>::success();
    }

    core::result<void> read_struct(
      const core::reflect::type_info &type,
      void *object,
      const core::json::value &in,
      const std::string &path
    ) {
        if (!in.is_object()) {
            const std::string_view where = path.empty() ? std::string_view("<root>") : std::string_view(path);
            return failed(where, std::format("expected object, found {}", core::json::to_string(in.type())));
        }
        std::byte *base = static_cast<std::byte *>(object);
        for (const field_info &field : type.fields) {
            const std::optional<core::json::value> member = in.get(field.name);
            if (!member.has_value()) {
                continue;
            }
            CONJURE_TRY(read_field(field, base + field.offset, *member, join_path(path, field.name)));
        }
        return core::result<void>::success();
    }

    void remap_struct(const core::reflect::type_info &type, void *object, const entity_remap &map) {
        std::byte *base = static_cast<std::byte *>(object);
        for (const field_info &field : type.fields) {
            const field_kind kind = field.value_kind();
            if (kind != field_kind::ENTITY && kind != field_kind::STRUCT) {
                continue;
            }
            const u32 elementSize = field.is_array() ? field.element_size() : field.size;
            const u32 elementCount = field.is_array() ? field.arrayCount : 1u;
            for (u32 element = 0; element < elementCount; ++element) {
                std::byte *data = base + field.offset + element * elementSize;
                if (kind == field_kind::ENTITY) {
                    entity value = {};
                    std::memcpy(&value, data, sizeof(entity));
                    value = map.resolve(value);
                    std::memcpy(data, &value, sizeof(entity));
                } else if (field.structType != nullptr) {
                    remap_struct(*field.structType, data, map);
                }
            }
        }
    }

    /*=======================================================================*/
    /*=======================================================================*/
    // Archive helpers

    /// <summary>
    /// Constructs a default instance of a component into a fresh byte image.
    /// </summary>
    std::vector<std::byte> default_image(const component_info &info) {
        std::vector<std::byte> bytes(info.size, std::byte{0});
        if (info.size == 0) {
            return bytes;
        }
        if (info.ops.constructDefault != nullptr) {
            info.ops.constructDefault(bytes.data(), 1);
        } else if (!info.defaultValue.empty()) {
            std::memcpy(bytes.data(), info.defaultValue.data(), info.size);
        }
        return bytes;
    }

    void destroy_image(const component_info &info, std::vector<std::byte> &bytes) noexcept {
        if (info.ops.destruct != nullptr && !bytes.empty()) {
            info.ops.destruct(bytes.data(), 1);
        }
        bytes.clear();
    }

    void destroy_entry(const component_registry &registry, archive_entry &entry) noexcept {
        for (archive_component &component : entry.components) {
            if (registry.is_registered(component.id)) {
                destroy_image(registry.info(component.id), component.bytes);
            }
        }
        entry.components.clear();
    }

    std::string display_name(const component_info &info) {
        return info.name.to_string();
    }

    /// <summary>
    /// One key of an entity's "components" object: a live component, written through its serialiser, or a
    /// preserved unknown one, written back verbatim.
    /// </summary>
    struct archive_key {
        std::string liveName = {};                              // the registered name of 'live'
        const archive_component *live = nullptr;
        const unresolved_component *preserved = nullptr;
        u32 order = 0;                                          // position in the archive entry

        [[nodiscard]] std::string_view name() const noexcept {
            if (preserved != nullptr) {
                return preserved->name;
            }
            return liveName;
        }
    };

    /// <summary>
    /// Gathers the keys write_archive_json emits for one entry and sorts them by name, in the byte order
    /// of the UTF-8 text (the order of core::json's canonical form), so the output does not depend on
    /// the ids the registry happened to assign. TRANSIENT components are left out. A preserved unknown
    /// component is left out as well when its name now resolves to a component the entity holds: that
    /// one is written under the same key, and a second key would make the file fail to load ("component
    /// listed twice"). Keys of the same name (an unknown component a file listed twice) keep the order of
    /// the entry. ARCHIVE_CORRUPT for an unregistered component id.
    /// </summary>
    core::result<void> collect_sorted_keys(
      const component_registry &registry,
      const archive_entry &entry,
      std::vector<archive_key> &keys
    ) {
        keys.clear();
        for (const archive_component &component : entry.components) {
            if (!registry.is_registered(component.id)) {
                return make_error(ecs_error::ARCHIVE_CORRUPT, "archive names an unregistered component id");
            }
            const component_info &info = registry.info(component.id);
            if (info.is_transient()) {
                continue;
            }
            if (component.id.value != 0) {
                archive_key key = {};
                key.liveName = display_name(info);
                key.live = &component;
                key.order = static_cast<u32>(keys.size());
                keys.push_back(std::move(key));
                continue;
            }

            const unresolved_components &unresolved =
              *reinterpret_cast<const unresolved_components *>(component.bytes.data());
            for (const unresolved_component &preserved : unresolved.entries) {
                const component_id resolved = registry.find(core::string_id(preserved.name));
                if (resolved.is_valid() && resolved.value != 0 && entry.mask.test(resolved) &&
                  !registry.info(resolved).is_transient()) {
                    continue;
                }
                archive_key key = {};
                key.preserved = &preserved;
                key.order = static_cast<u32>(keys.size());
                keys.push_back(std::move(key));
            }
        }

        std::sort(keys.begin(), keys.end(), [](const archive_key &left, const archive_key &right) {
            const std::string_view leftName = left.name();
            const std::string_view rightName = right.name();
            if (leftName != rightName) {
                return leftName < rightName;
            }
            return left.order < right.order;
        });
        return core::result<void>::success();
    }
}

/*===========================================================================*/
/*===========================================================================*/
// detail

core::result<void> conjure::ecs::detail::write_reflected_component(
  const core::reflect::type_info &type,
  const void *component,
  core::json::writer &out
) {
    return write_struct(type, component, out, std::string());
}

core::result<void> conjure::ecs::detail::read_reflected_component(
  const core::reflect::type_info &type,
  void *component,
  const core::json::value &in
) {
    return read_struct(type, component, in, std::string());
}

void conjure::ecs::detail::remap_reflected_entities(
  const core::reflect::type_info &type,
  void *component,
  const entity_remap &map
) {
    remap_struct(type, component, map);
}

core::result<void> conjure::ecs::detail::write_unresolved_components(const void *component, core::json::writer &out) {
    const unresolved_components &value = *static_cast<const unresolved_components *>(component);
    out.begin_object();
    for (const unresolved_component &entry : value.entries) {
        out.key(entry.name);
        out.write_raw_json(entry.json);
    }
    out.end_object();
    return core::result<void>::success();
}

core::result<void> conjure::ecs::detail::read_unresolved_components(void *component, const core::json::value &in) {
    if (!in.is_object()) {
        return failed("<root>", "unresolved components must be an object");
    }
    unresolved_components &value = *static_cast<unresolved_components *>(component);
    value.entries.clear();
    for (const core::json::member member : in.members()) {
        value.entries.push_back(unresolved_component{std::string(member.key), core::json::to_string(member.item)});
    }
    return core::result<void>::success();
}


/*===========================================================================*/
/*===========================================================================*/
// Single component

core::result<void> conjure::ecs::write_component_json(
  const component_info &info,
  const void *component,
  core::json::writer &out
) {
    if (info.is_tag() || info.is_transient()) {
        out.begin_object();
        out.end_object();
        return core::result<void>::success();
    }
    if (info.serializer.writeJson != nullptr) {
        return info.serializer.writeJson(component, out);
    }
    if (info.reflection != nullptr) {
        return write_struct(*info.reflection, component, out, std::string());
    }
    return make_error(ecs_error::SERIALIZATION_FAILED, "component '" + display_name(info) + "' cannot be serialised");
}

core::result<void> conjure::ecs::read_component_json(
  const component_info &info,
  void *component,
  const core::json::value &in
) {
    if (info.is_tag() || info.is_transient()) {
        return core::result<void>::success();
    }
    if (info.serializer.readJson != nullptr) {
        return info.serializer.readJson(component, in);
    }
    if (info.reflection != nullptr) {
        return read_struct(*info.reflection, component, in, std::string());
    }
    return make_error(ecs_error::SERIALIZATION_FAILED, "component '" + display_name(info) + "' cannot be deserialised");
}


/*===========================================================================*/
/*===========================================================================*/
// Archives

core::result<void> conjure::ecs::write_archive_json(const entity_archive &archive, core::json::writer &out) {
    const component_registry &registry = archive.registry();
    // One key buffer serves every entry, so a large archive does not allocate one per entity.
    std::vector<archive_key> keys = {};
    out.begin_object();
    out.key("entities");
    out.begin_array();
    for (const archive_entry &entry : archive.entries()) {
        out.begin_object();
        out.key("components");
        out.begin_object();
        // Like a failed component write below, a failure here returns with the scopes still open: the
        // writer keeps the partial output (see the summary).
        CONJURE_TRY(collect_sorted_keys(registry, entry, keys));
        for (const archive_key &key : keys) {
            out.key(key.name());
            if (key.preserved != nullptr) {
                // Unresolved data goes back under its original name, exactly as it was read.
                out.write_raw_json(key.preserved->json);
                continue;
            }
            const archive_component &component = *key.live;
            const void *data = component.bytes.empty() ? nullptr : static_cast<const void *>(component.bytes.data());
            CONJURE_TRY(write_component_json(registry.info(component.id), data, out));
        }
        out.end_object();
        out.end_object();
    }
    out.end_array();
    out.end_object();
    return core::result<void>::success();
}

core::result<entity_archive> conjure::ecs::read_archive_json(
  const core::json::value &in,
  const component_registry &registry
) {
    if (!in.is_object()) {
        return make_error(ecs_error::SERIALIZATION_FAILED, "archive root must be an object");
    }
    const core::json::value entities = in["entities"];
    if (!entities.is_array()) {
        return make_error(ecs_error::SERIALIZATION_FAILED, "archive needs an \"entities\" array");
    }

    entity_archive archive(registry);
    const component_info &unresolvedInfo = registry.info(component_id{0});
    usize entityIndex = 0;
    for (const core::json::value entityValue : entities) {
        if (!entityValue.is_object()) {
            return make_error(
              ecs_error::SERIALIZATION_FAILED,
              std::format("{}: entity must be an object", entityIndex)
            );
        }
        const core::json::value components = entityValue["components"];
        if (components.is_valid() && !components.is_object()) {
            return make_error(
              ecs_error::SERIALIZATION_FAILED,
              std::format("{}/components: must be an object", entityIndex)
            );
        }

        archive_entry entry = {};
        unresolved_components unresolved = {};
        for (const core::json::member member : components.members()) {
            const component_id id = registry.find(core::string_id(member.key));
            if (!id.is_valid() || id.value == 0) {
                CONJURE_LOG_INFO(LOG_ECS, "entity {}: unknown component '{}' preserved", entityIndex, member.key);
                unresolved.entries.push_back(
                  unresolved_component{std::string(member.key), core::json::to_string(member.item)}
                );
                continue;
            }
            const component_info &info = registry.info(id);
            if (info.is_transient()) {
                continue;
            }
            if (entry.mask.test(id)) {
                destroy_entry(registry, entry);
                return make_error(
                  ecs_error::SERIALIZATION_FAILED,
                  std::format("{}/{}: component listed twice", entityIndex, member.key)
                );
            }

            archive_component component = {};
            component.id = id;
            if (!info.is_tag()) {
                component.bytes = default_image(info);
                const core::result<void> read = read_component_json(info, component.bytes.data(), member.item);
                if (!read.has_value()) {
                    destroy_image(info, component.bytes);
                    destroy_entry(registry, entry);
                    return make_error(
                      ecs_error::SERIALIZATION_FAILED,
                      std::format("{}/{}/{}", entityIndex, member.key, read.get_error().message())
                    );
                }
            }
            entry.mask.set(id);
            entry.components.push_back(std::move(component));
        }

        if (!unresolved.entries.empty()) {
            archive_component component = {};
            component.id = component_id{0};
            component.bytes = default_image(unresolvedInfo);
            *reinterpret_cast<unresolved_components *>(component.bytes.data()) = std::move(unresolved);
            entry.mask.set(component.id);
            entry.components.push_back(std::move(component));
        }
        archive.push_entry(std::move(entry));
        ++entityIndex;
    }

    CONJURE_TRY(archive.validate());
    return archive;
}


/*===========================================================================*/
/*===========================================================================*/
// Live components (R40)

core::result<void> conjure::ecs::component_to_json(
  const world &source,
  entity target,
  component_id id,
  core::json::writer &out
) {
    const component_registry &registry = source.registry();
    if (!registry.is_registered(id)) {
        return make_error(ecs_error::UNKNOWN_COMPONENT, "component id is not registered");
    }
    if (!source.is_alive(target)) {
        return make_error(ecs_error::ENTITY_NOT_ALIVE, "component_to_json on a dead entity");
    }
    if (!source.has_raw(target, id)) {
        return make_error(ecs_error::COMPONENT_NOT_PRESENT, "entity does not hold the component");
    }
    return write_component_json(registry.info(id), source.get_raw(target, id), out);
}

core::result<void> conjure::ecs::component_from_json(
  world &target,
  entity owner,
  component_id id,
  const core::json::value &in
) {
    const component_registry &registry = target.registry();
    if (!registry.is_registered(id)) {
        return make_error(ecs_error::UNKNOWN_COMPONENT, "component id is not registered");
    }
    if (!target.is_alive(owner)) {
        return make_error(ecs_error::ENTITY_NOT_ALIVE, "component_from_json on a dead entity");
    }
    if (!target.has_raw(owner, id)) {
        return make_error(ecs_error::COMPONENT_NOT_PRESENT, "entity does not hold the component");
    }
    return read_component_json(registry.info(id), target.get_raw(owner, id, true), in);
}
