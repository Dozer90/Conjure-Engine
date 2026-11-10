#include "conjure/renderer/components.h"

#include "conjure/core/error.h"
#include "conjure/core/reflect/field_info.h"
#include "conjure/core/reflect/type_info.h"

#include <utility>

using namespace conjure;
using namespace conjure::renderer;

namespace {
    [[nodiscard]] core::result<void> ignore_duplicate(const core::error &failure) {
        if (failure.is(core::error_code::ALREADY_EXISTS)) {
            return core::result<void>();
        }
        return failure;
    }

    template <typename EnumType>
    [[nodiscard]] core::result<void> register_enum(core::reflect::type_registry &registry) {
        core::result<const core::reflect::enum_info *> registered = registry.register_enum(
          core::reflect::describe_enum<EnumType>()
        );
        if (!registered.has_value()) {
            return ignore_duplicate(registered.get_error());
        }
        return core::result<void>();
    }

    /// <summary>
    /// The registry keeps its own copies of the enum and struct descriptions, and a field must point at
    /// those copies (not at the static descriptions) to be accepted.
    /// </summary>
    void link_nested(core::reflect::type_info &description, const core::reflect::type_registry &registry) {
        for (core::reflect::field_info &field : description.fields) {
            const core::reflect::field_kind kind = field.value_kind();
            if (kind == core::reflect::field_kind::ENUM) {
                field.enumType = registry.find_enum(field.type);
            } else if (kind == core::reflect::field_kind::STRUCT) {
                field.structType = registry.find(field.type);
            }
        }
    }

    template <typename StructType>
    [[nodiscard]] core::result<void> register_struct(core::reflect::type_registry &registry) {
        core::reflect::type_info description = core::reflect::describe_struct<StructType>();
        link_nested(description, registry);
        core::result<const core::reflect::type_info *> registered = registry.register_type(std::move(description));
        if (!registered.has_value()) {
            return ignore_duplicate(registered.get_error());
        }
        return core::result<void>();
    }
}

core::result<void> conjure::renderer::register_components(core::reflect::type_registry &registry) {
    CONJURE_TRY(register_enum<projection_type>(registry));
    CONJURE_TRY(register_enum<light_type>(registry));
    CONJURE_TRY(register_enum<shadow_filter>(registry));
    CONJURE_TRY(register_enum<sprite_space>(registry));
    CONJURE_TRY(register_struct<camera>(registry));
    CONJURE_TRY(register_struct<light>(registry));
    CONJURE_TRY(register_struct<mesh_renderer_component>(registry));
    CONJURE_TRY(register_struct<sprite_component>(registry));
    CONJURE_TRY(register_struct<camera_component>(registry));
    CONJURE_TRY(register_struct<light_component>(registry));
    CONJURE_TRY(register_struct<environment_component>(registry));
    return core::result<void>();
}
