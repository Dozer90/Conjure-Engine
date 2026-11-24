#pragma once

#include "conjure/core/platform_detect.h"

// The ECS names of the renderer components (docs/design/02_renderer.md 2.4, R7): the renderer has no ecs
// dependency, so the engine's renderer_subsystem names and registers mesh_renderer, sprite, camera, light,
// environment, render_settings and post_process. Private to the engine module; tests include it through the
// engine/src include path.

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER

#include "conjure/ecs/component.h"
#include "conjure/renderer/components.h"

CONJURE_COMPONENT(conjure::renderer::mesh_renderer_component, "conjure.renderer.mesh_renderer");
CONJURE_COMPONENT(conjure::renderer::sprite_component, "conjure.renderer.sprite");
CONJURE_COMPONENT(conjure::renderer::camera_component, "conjure.renderer.camera");
CONJURE_COMPONENT(conjure::renderer::light_component, "conjure.renderer.light");
CONJURE_COMPONENT(conjure::renderer::environment_component, "conjure.renderer.environment");
CONJURE_COMPONENT(conjure::renderer::render_settings_component, "conjure.renderer.render_settings");
CONJURE_COMPONENT(conjure::renderer::post_process_component, "conjure.renderer.post_process");

#endif
