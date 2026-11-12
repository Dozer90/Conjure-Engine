#include "engine/subsystems/renderer_subsystem.h"

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER

#include "engine/subsystems/renderer_components.h"

#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_error.h"
#include "conjure/engine/engine_time.h"
#include "conjure/engine/log_category.h"

#include "conjure/core/log.h"
#include "conjure/core/math.h"
#include "conjure/core/reflect/type_registry.h"
#include "conjure/ecs/scheduler.h"
#include "conjure/ecs/view.h"
#include "conjure/renderer/environment_asset.h"
#include "conjure/renderer/material_asset.h"
#include "conjure/renderer/mesh_asset.h"
#include "conjure/renderer/renderer.h"
#include "conjure/renderer/texture_asset.h"
#include "conjure/scene/components.h"

#include <algorithm>
#include <format>
#include <utility>

using namespace conjure;
using namespace conjure::engine;

namespace {

    [[nodiscard]] core::error renderer_error(std::string_view what, const core::error &cause) {
        return make_error(engine_error::RENDERER_INIT_FAILED, std::format("{}: {}", what, cause.to_string()));
    }

    [[nodiscard]] math::vec3 rotate_direction(const math::mat4 &matrix, const math::vec3 &local) noexcept {
        const math::vec3 rotated = math::vec3(matrix * math::vec4(local, 0.0f));
        const f32 length = glm::length(rotated);
        return length > 1.0e-6f ? rotated / length : local;
    }

    /// <summary>
    /// A view matrix from a camera's world matrix: the rigid inverse (scale is dropped so a scaled parent does
    /// not warp the projection).
    /// </summary>
    [[nodiscard]] math::mat4 view_from_world(const math::mat4 &worldFromCamera) noexcept {
        const math::vec3 position = math::vec3(worldFromCamera[3]);
        const math::vec3 forward = rotate_direction(worldFromCamera, math::vec3(0.0f, 0.0f, -1.0f));
        const math::vec3 up = rotate_direction(worldFromCamera, math::vec3(0.0f, 1.0f, 0.0f));
        return glm::lookAt(position, position + forward, up);
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

renderer_subsystem::renderer_subsystem(engine_services &services) :
    mServices(&services) {}

renderer_subsystem::~renderer_subsystem() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

std::string_view renderer_subsystem::name() const noexcept {
    return "renderer";
}

core::result<void> renderer_subsystem::startup(engine_context &context) {
    const engine_config &config = *context.config;
    mStats = {};

    // The component types exist whatever the rendering flag says: a headless server still loads the scene.
    CONJURE_TRY(renderer::register_components(core::reflect::registry()));
    ecs::component_registry &components = *mServices->components;
    CONJURE_TRY(components.register_component<renderer::mesh_renderer_component>());
    CONJURE_TRY(components.register_component<renderer::sprite_component>());
    CONJURE_TRY(components.register_component<renderer::camera_component>());
    CONJURE_TRY(components.register_component<renderer::light_component>());
    CONJURE_TRY(components.register_component<renderer::environment_component>());
    CONJURE_TRY(renderer::renderer::register_asset_types(
      mServices->assetManager->types(),
      mServices->importers,
      &mServices->jobs
    ));

    if (!config.rendering.enabled || mServices->device == nullptr) {
        CONJURE_LOG_INFO(LOG_ENGINE, "renderer: rendering disabled, no renderer created");
        return core::result<void>::success();
    }

    const std::filesystem::path shaderDir = shader_directory(config, mServices->engineDirectory);
    core::result<std::unique_ptr<rhi::shader_library>> shaders =
      rhi::shader_library::open(*mServices->device, shaderDir);
    if (!shaders.has_value()) {
        const std::string what = std::format("opening the shader library at '{}'", shaderDir.generic_string());
        return renderer_error(what, shaders.get_error());
    }
    mServices->shaders = std::move(shaders).value();

    renderer::renderer_desc desc = {};
    desc.device = mServices->device.get();
    desc.swapchain = mServices->swapchain.get();
    desc.shaders = mServices->shaders.get();
    desc.jobs = &mServices->jobs;
    desc.enableImGui = config.editor;
    core::result<std::unique_ptr<renderer::renderer>> created = renderer::renderer::create(desc);
    if (!created.has_value()) {
        mServices->shaders.reset();
        return renderer_error("creating the renderer", created.get_error());
    }
    mServices->renderer = std::move(created).value();
    mServices->debugDraw = std::make_unique<renderer::debug_draw>();

    mSubscription = mServices->assetManager->subscribe([this](const assets::asset_event &event) {
        this->handle_asset_event(event);
    });

    context.renderer = mServices->renderer.get();
    CONJURE_LOG_INFO(LOG_ENGINE, "renderer: ready (shaders from '{}')", shaderDir.generic_string());
    return core::result<void>::success();
}

void renderer_subsystem::register_systems(ecs::scheduler &scheduler, engine_context &context) {
    CONJURE_UNUSED(context);
    if (mServices->renderer == nullptr) {
        return;
    }

    ecs::system_desc sync = ecs::make_system<const renderer::mesh_renderer_component>(
      core::string_id::intern(SYSTEM_SYNC),
      ecs::system_phase::SYNC,
      [this](ecs::system_context &systemContext) {
          this->sync(systemContext);
      }
    );
    sync.flags = ecs::system_flags::MAIN_THREAD | ecs::system_flags::RUNS_WHILE_PAUSED;
    core::result<ecs::system_id> added = scheduler.add_system(std::move(sync));
    if (!added.has_value()) {
        CONJURE_LOG_ERROR(
          LOG_ENGINE,
          "renderer: {} could not be added: {}",
          SYSTEM_SYNC,
          added.get_error().to_string()
        );
    }

    ecs::system_desc extract = ecs::make_system<
      const renderer::mesh_renderer_component,
      const renderer::sprite_component,
      const renderer::camera_component,
      const renderer::light_component,
      const renderer::environment_component,
      const scene::render_world_transform,
      const scene::local_bounds>(
      core::string_id::intern(SYSTEM_EXTRACT),
      ecs::system_phase::EXTRACT,
      [this](ecs::system_context &systemContext) {
          if (mServices->extractTarget != nullptr) {
              this->extract(systemContext, *mServices->extractTarget);
          }
      }
    );
    extract.flags = ecs::system_flags::MAIN_THREAD | ecs::system_flags::RUNS_WHILE_PAUSED;
    added = scheduler.add_system(std::move(extract));
    if (!added.has_value()) {
        CONJURE_LOG_ERROR(
          LOG_ENGINE,
          "renderer: {} could not be added: {}",
          SYSTEM_EXTRACT,
          added.get_error().to_string()
        );
    }
}

void renderer_subsystem::begin_frame(engine_context &context) {
    mLastDeltaSeconds = context.time != nullptr ? context.time->deltaTime : 0.0f;
}

void renderer_subsystem::shutdown(engine_context &context) {
    context.renderer = nullptr;
    if (mSubscription.is_valid() && mServices->assetManager != nullptr) {
        mServices->assetManager->unsubscribe(mSubscription);
    }
    mSubscription = {};
    mServices->extractTarget = nullptr;
    mCameras.clear();
    mCameras.shrink_to_fit();
    mServices->debugDraw.reset();
    mServices->renderer.reset();
    mServices->shaders.reset();
}

std::filesystem::path renderer_subsystem::shader_directory(
  const engine_config &config,
  const std::filesystem::path &engineDirectory
) {
    if (!config.rendering.shaderDirectory.empty()) {
        return config.rendering.shaderDirectory;
    }
    return engineDirectory / "shaders";
}

void renderer_subsystem::extract(ecs::system_context &context, renderer::render_scene &target) {
    using render_transform = scene::render_world_transform;
    renderer::renderer &instance = *mServices->renderer;
    mStats.views = 0;
    mStats.meshes = 0;
    mStats.meshesSkipped = 0;
    mStats.lights = 0;
    mStats.sprites = 0;
    mStats.environmentSeen = false;

    target.frameIndex = context.frame().frameIndex;
    target.deltaSeconds = context.frame().deltaTime;

    // Cameras: the main cameras first in priority order; a scene without an isMain camera renders every camera.
    // The candidate list is a member so a steady-state frame allocates nothing (08_testing.md 7.1).
    std::vector<camera_candidate> &cameras = mCameras;
    cameras.clear();
    context.view<const renderer::camera_component, const scene::render_world_transform>()
      .without<scene::disabled>()
      .without<scene::inactive>()
      .for_each([&](ecs::entity, const renderer::camera_component &camera, const render_transform &transform) {
          camera_candidate candidate = {};
          candidate.view.cameraData = camera.cameraData;
          candidate.view.viewFromWorld = view_from_world(transform.matrix);
          candidate.view.cameraPosition = math::vec3(transform.matrix[3]);
          candidate.view.layerMask = camera.layerMask;
          candidate.view.flags = renderer::view_flags::NONE;
          if (!camera.postProcess) {
              candidate.view.flags |= renderer::view_flags::NO_POST_PROCESS;
          }
          if (mServices->debugDraw != nullptr) {
              candidate.view.flags |= renderer::view_flags::DRAW_DEBUG;
          }
          candidate.priority = camera.priority;
          candidate.isMain = camera.isMain;
          candidate.order = static_cast<u32>(cameras.size());
          cameras.push_back(candidate);
      });
    const bool anyMain = std::any_of(cameras.begin(), cameras.end(), [](const camera_candidate &c) {
        return c.isMain;
    });
    // The world-order tie-break makes the sort stable without std::stable_sort's temporary buffer.
    std::sort(cameras.begin(), cameras.end(), [](const camera_candidate &a, const camera_candidate &b) {
        if (a.priority != b.priority) {
            return a.priority > b.priority;
        }
        return a.order < b.order;
    });
    // Views target the swapchain; a renderer without one (headless tests) has nowhere to draw them, so they are
    // counted but not submitted instead of being warned about every frame.
    const bool hasSwapchain = mServices->swapchain != nullptr;
    for (const camera_candidate &candidate : cameras) {
        if (anyMain && !candidate.isMain) {
            continue;
        }
        ++mStats.views;
        if (hasSwapchain) {
            target.views.push_back(candidate.view);
        }
    }

    // Mesh instances from render_world_transform (R3): one fixed step of visual latency, never simulation truth.
    // Entities that share a mesh or a material in a row reuse the previous lookup instead of taking the resource
    // cache's lock again (the last-asset memo of 17_demo_games.md gap 9); one extract sees no upload or release,
    // so the memo cannot go stale.
    assets::asset_id lastMeshAsset = {};
    renderer::gpu_mesh_id lastMesh = {};
    bool haveMesh = false;
    assets::asset_id lastMaterialAsset = {};
    renderer::gpu_material_id lastMaterial = {};
    bool haveMaterial = false;
    context.view<
        const renderer::mesh_renderer_component,
        const scene::render_world_transform,
        ecs::maybe<const scene::local_bounds>>()
      .without<scene::disabled>()
      .without<scene::inactive>()
      .for_each([&](
        ecs::entity entity,
        const renderer::mesh_renderer_component &meshRenderer,
        const scene::render_world_transform &transform,
        const scene::local_bounds *bounds
      ) {
          const assets::asset_id &meshAsset = meshRenderer.mesh.id();
          if (!haveMesh || meshAsset != lastMeshAsset) {
              lastMesh = instance.find_mesh(meshAsset);
              lastMeshAsset = meshAsset;
              haveMesh = true;
          }
          const renderer::gpu_mesh_id mesh = lastMesh;
          if (!mesh.is_valid()) {
              ++mStats.meshesSkipped;
              return;
          }
          renderer::mesh_instance item = {};
          item.mesh = mesh;
          for (u32 slot = 0; slot < renderer::MAX_MATERIAL_SLOTS; ++slot) {
              const assets::asset_id &material = meshRenderer.materials[slot].id();
              if (material.is_null()) {
                  continue;
              }
              if (!haveMaterial || material != lastMaterialAsset) {
                  lastMaterial = instance.find_material(material);
                  lastMaterialAsset = material;
                  haveMaterial = true;
              }
              item.materials[slot] = lastMaterial;
          }
          item.worldFromLocal = transform.matrix;
          math::aabb local = {};
          if (bounds != nullptr) {
              local = bounds->box;
          } else if (const renderer::mesh_asset *cpu = meshRenderer.mesh.get(); cpu != nullptr) {
              local = cpu->bounds();
          } else {
              local = math::aabb{math::vec3(-0.5f), math::vec3(0.5f)};
          }
          item.worldBounds = local.transformed(transform.matrix);
          item.entityId = entity.index;
          item.layerMask = meshRenderer.layerMask;
          item.castsShadows = meshRenderer.castsShadows;
          item.receivesShadows = meshRenderer.receivesShadows;
          target.meshes.push_back(item);
          ++mStats.meshes;
      });

    // Lights: position and direction in world space, the entity as the stable shadow identity (02 R11).
    context.view<const renderer::light_component, const scene::render_world_transform>()
      .without<scene::disabled>()
      .without<scene::inactive>()
      .for_each([&](ecs::entity entity, const renderer::light_component &light, const render_transform &transform) {
          renderer::light item = light.lightData;
          item.position = math::vec3(transform.matrix[3]);
          item.direction = rotate_direction(transform.matrix, light.lightData.direction);
          item.entityId = entity.index;
          target.lights.push_back(item);
          ++mStats.lights;
      });

    // Sprites.
    context.view<const renderer::sprite_component, const scene::render_world_transform>()
      .without<scene::disabled>()
      .without<scene::inactive>()
      .for_each([&](ecs::entity, const renderer::sprite_component &sprite, const render_transform &transform) {
          renderer::sprite item = {};
          item.texture = instance.find_texture(sprite.texture.id());
          item.worldFromLocal = transform.matrix;
          item.uvRect = sprite.uvRect;
          item.colour = sprite.colour;
          item.size = sprite.size;
          item.space = sprite.space;
          item.layer = sprite.layer;
          item.order = sprite.order;
          target.sprites.push_back(item);
          ++mStats.sprites;
      });

    // Environment: the first enabled component wins.
    context.view<const renderer::environment_component>()
      .without<scene::disabled>()
      .without<scene::inactive>()
      .for_each([&](ecs::entity, const renderer::environment_component &environment) {
          if (mStats.environmentSeen) {
              return;
          }
          target.environment.environment = instance.find_environment(environment.environment.id());
          target.environment.intensity = environment.intensity;
          target.environment.rotationRadians = environment.rotationRadians;
          mStats.environmentSeen = true;
      });

    if (mServices->debugDraw != nullptr) {
        mServices->debugDraw->append_to(target.debugDraw);
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

void renderer_subsystem::handle_asset_event(const assets::asset_event &event) {
    renderer::renderer &instance = *mServices->renderer;
    assets::asset_manager &assets = *mServices->assetManager;

    const bool upload =
      event.kind == assets::asset_event_kind::LOADED || event.kind == assets::asset_event_kind::RELOADED;
    const bool release = event.kind == assets::asset_event_kind::UNLOADED;
    if (!upload && !release) {
        return;
    }

    if (event.type == assets::asset_type_traits<renderer::mesh_asset>::id()) {
        if (upload) {
            if (const renderer::mesh_asset *mesh = assets.bind<renderer::mesh_asset>(event.id).get(); mesh != nullptr) {
                CONJURE_UNUSED(instance.upload_mesh(event.id, *mesh));
                ++mStats.uploads;
            }
        } else if (const renderer::gpu_mesh_id id = instance.find_mesh(event.id); id.is_valid()) {
            instance.release_mesh(id);
            ++mStats.releases;
        }
    } else if (event.type == assets::asset_type_traits<renderer::texture_asset>::id()) {
        if (upload) {
            if (const renderer::texture_asset *texture = assets.bind<renderer::texture_asset>(event.id).get();
                texture != nullptr) {
                CONJURE_UNUSED(instance.upload_texture(event.id, *texture));
                ++mStats.uploads;
            }
        } else if (const renderer::gpu_texture_id id = instance.find_texture(event.id); id.is_valid()) {
            instance.release_texture(id);
            ++mStats.releases;
        }
    } else if (event.type == assets::asset_type_traits<renderer::material_asset>::id()) {
        if (upload) {
            if (const renderer::material_asset *material = assets.bind<renderer::material_asset>(event.id).get();
                material != nullptr) {
                CONJURE_UNUSED(instance.upload_material(event.id, *material));
                ++mStats.uploads;
            }
        } else if (const renderer::gpu_material_id id = instance.find_material(event.id); id.is_valid()) {
            instance.release_material(id);
            ++mStats.releases;
        }
    } else if (event.type == assets::asset_type_traits<renderer::environment_asset>::id()) {
        if (upload) {
            const renderer::environment_asset *environment = assets.bind<renderer::environment_asset>(event.id).get();
            if (environment != nullptr) {
                CONJURE_UNUSED(instance.upload_environment(event.id, *environment));
                ++mStats.uploads;
            }
        } else if (const renderer::gpu_environment_id id = instance.find_environment(event.id); id.is_valid()) {
            instance.release_environment(id);
            ++mStats.releases;
        }
    }
}

void renderer_subsystem::sync(ecs::system_context &context) {
    CONJURE_UNUSED(context);
    if (mServices->debugDraw != nullptr) {
        mServices->debugDraw->tick(mLastDeltaSeconds);
    }
}

#endif
