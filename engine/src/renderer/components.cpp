#include "conjure/renderer/components.h"

#include "conjure/core/assert.h"
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

    /// <summary>
    /// A local direction carried into world space by a matrix and normalised; the local direction itself
    /// when the matrix collapses it.
    /// </summary>
    [[nodiscard]] math::vec3 rotate_direction(const math::mat4 &matrix, const math::vec3 &local) noexcept {
        const math::vec3 rotated = math::vec3(matrix * math::vec4(local, 0.0f));
        const f32 length = glm::length(rotated);
        return length > 1.0e-6f ? rotated / length : local;
    }

    /// <summary>
    /// Whether a scene component competes for a group.
    /// </summary>
    [[nodiscard]] bool overrides_group(const render_settings_component &component, usize group) noexcept {
        switch (static_cast<render_settings_group>(group)) {
            case render_settings_group::POST_PROCESS: {
                return component.overridePostProcess;
            }
            case render_settings_group::SHADOWS: {
                return component.overrideShadows;
            }
            case render_settings_group::SSAO: {
                return component.overrideSsao;
            }
            case render_settings_group::COUNT: {
                return false;
            }
        }
        return false;
    }

    /// <summary>
    /// The tie-break between two candidates of equal priority: the lower guid first, candidates without a
    /// guid after every candidate that has one, then the lower entity index.
    /// </summary>
    [[nodiscard]] bool wins_tie(const scene_settings_candidate &left, const scene_settings_candidate &right) noexcept {
        const bool leftHasGuid = !left.guid.is_null();
        const bool rightHasGuid = !right.guid.is_null();
        if (leftHasGuid != rightHasGuid) {
            return leftHasGuid;
        }
        if (leftHasGuid && left.guid != right.guid) {
            return left.guid < right.guid;
        }
        return left.entityIndex < right.entityIndex;
    }

    /// <summary>
    /// Copies one group of a scene component into resolved settings.
    /// </summary>
    void copy_group(render_settings &target, const render_settings_component &component, usize group) noexcept {
        switch (static_cast<render_settings_group>(group)) {
            case render_settings_group::POST_PROCESS: {
                target.postProcess = component.postProcess;
                return;
            }
            case render_settings_group::SHADOWS: {
                target.shadows = component.shadows;
                return;
            }
            case render_settings_group::SSAO: {
                target.ssao = component.ssao;
                return;
            }
            case render_settings_group::COUNT: {
                return;
            }
        }
    }
}

scene_settings_choice conjure::renderer::choose_scene_settings(
  std::span<const scene_settings_candidate> candidates
) noexcept {
    scene_settings_choice choice = {};
    for (usize index = 0; index < candidates.size(); ++index) {
        const scene_settings_candidate &candidate = candidates[index];
        if (candidate.component == nullptr) {
            continue;
        }
        for (usize group = 0; group < SETTINGS_GROUP_COUNT; ++group) {
            if (!overrides_group(*candidate.component, group)) {
                continue;
            }
            const u32 current = choice.winner[group];
            if (current == NO_SETTINGS_CANDIDATE) {
                choice.winner[group] = static_cast<u32>(index);
                continue;
            }
            const scene_settings_candidate &best = candidates[current];
            if (candidate.component->priority < best.component->priority) {
                continue;
            }
            if (candidate.component->priority > best.component->priority) {
                choice.winner[group] = static_cast<u32>(index);
                choice.ambiguous[group] = false;
                continue;
            }
            choice.ambiguous[group] = true;
            if (wins_tie(candidate, best)) {
                choice.winner[group] = static_cast<u32>(index);
            }
        }
    }
    return choice;
}

resolved_render_settings conjure::renderer::resolve_scene_settings(
  const render_settings &projectDefaults,
  std::span<const scene_settings_candidate> candidates,
  const scene_settings_choice &choice
) noexcept {
    resolved_render_settings resolved = {};
    resolved.settings = projectDefaults;
    for (usize group = 0; group < SETTINGS_GROUP_COUNT; ++group) {
        resolved.ambiguous[group] = choice.ambiguous[group];
        const u32 winner = choice.winner[group];
        if (winner == NO_SETTINGS_CANDIDATE) {
            continue;
        }
        const bool validWinner = winner < candidates.size() && candidates[winner].component != nullptr;
        CONJURE_ASSERT(validWinner, "resolve_scene_settings: winner {} is not a candidate with a component", winner);
        if (!validWinner) {
            continue;
        }
        copy_group(resolved.settings, *candidates[winner].component, group);
        resolved.source[group] = render_settings_source::SCENE;
        resolved.sceneWinner[group] = winner;
    }
    return resolved;
}

resolved_render_settings conjure::renderer::resolve_camera_settings(
  const resolved_render_settings &scene,
  const camera_component &camera,
  const post_process_component *overrides
) noexcept {
    resolved_render_settings resolved = scene;
    resolved.postProcessDisabled = !camera.postProcess;
    if (overrides == nullptr) {
        return resolved;
    }
    if (overrides->overridePostProcess) {
        resolved.settings.postProcess = overrides->postProcess;
        resolved.source[static_cast<usize>(render_settings_group::POST_PROCESS)] = render_settings_source::CAMERA;
    }
    if (overrides->overrideSsao) {
        resolved.settings.ssao = overrides->ssao;
        resolved.source[static_cast<usize>(render_settings_group::SSAO)] = render_settings_source::CAMERA;
    }
    return resolved;
}

render_view conjure::renderer::make_camera_view(
  const camera_component &camera,
  const math::mat4 &worldFromCamera,
  render_target_id target,
  const post_process_component *overrides
) noexcept {
    const math::vec3 position = math::vec3(worldFromCamera[3]);
    const math::vec3 forward = rotate_direction(worldFromCamera, math::vec3(0.0f, 0.0f, -1.0f));
    const math::vec3 up = rotate_direction(worldFromCamera, math::vec3(0.0f, 1.0f, 0.0f));
    render_view view = {};
    view.cameraData = camera.cameraData;
    view.viewFromWorld = glm::lookAt(position, position + forward, up);
    view.cameraPosition = position;
    view.target = target;
    view.layerMask = camera.layerMask;
    view.flags = camera.postProcess ? view_flags::NONE : view_flags::NO_POST_PROCESS;
    if (overrides == nullptr) {
        return view;
    }
    if (overrides->overridePostProcess) {
        view.postProcess = overrides->postProcess;
        view.settingsOverride |= view_settings_override_flags::POST_PROCESS;
    }
    if (overrides->overrideSsao) {
        view.ssao = overrides->ssao;
        view.settingsOverride |= view_settings_override_flags::SSAO;
    }
    return view;
}

core::result<void> conjure::renderer::register_components(core::reflect::type_registry &registry) {
    CONJURE_TRY(register_enum<projection_type>(registry));
    CONJURE_TRY(register_enum<light_type>(registry));
    CONJURE_TRY(register_enum<shadow_filter>(registry));
    CONJURE_TRY(register_enum<sprite_space>(registry));
    CONJURE_TRY(register_enum<exposure_mode>(registry));
    CONJURE_TRY(register_enum<tonemap_operator>(registry));
    CONJURE_TRY(register_enum<agx_look>(registry));
    CONJURE_TRY(register_struct<camera>(registry));
    CONJURE_TRY(register_struct<light>(registry));
    CONJURE_TRY(register_struct<post_process_settings>(registry));
    CONJURE_TRY(register_struct<shadow_settings>(registry));
    CONJURE_TRY(register_struct<ssao_settings>(registry));
    CONJURE_TRY(register_struct<render_settings>(registry));
    CONJURE_TRY(register_struct<mesh_renderer_component>(registry));
    CONJURE_TRY(register_struct<sprite_component>(registry));
    CONJURE_TRY(register_struct<camera_component>(registry));
    CONJURE_TRY(register_struct<light_component>(registry));
    CONJURE_TRY(register_struct<environment_component>(registry));
    CONJURE_TRY(register_struct<render_settings_component>(registry));
    CONJURE_TRY(register_struct<post_process_component>(registry));
    return core::result<void>();
}
