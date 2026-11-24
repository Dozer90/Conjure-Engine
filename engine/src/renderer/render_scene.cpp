#include "conjure/renderer/render_scene.h"

#include "conjure/core/assert.h"

using namespace conjure;
using namespace conjure::renderer;

/*===========================================================================*/
/*===========================================================================*/
// render_view

view_render_settings conjure::renderer::effective_view_settings(
  const post_process_settings &scenePostProcess,
  const ssao_settings &sceneSsao,
  const render_view &view
) noexcept {
    const bool ownPostProcess = has_flag(view.settingsOverride, view_settings_override_flags::POST_PROCESS);
    const bool ownSsao = has_flag(view.settingsOverride, view_settings_override_flags::SSAO);
    view_render_settings settings = {};
    settings.postProcess = ownPostProcess ? view.postProcess : scenePostProcess;
    settings.ssao = ownSsao ? view.ssao : sceneSsao;
    return settings;
}

/*===========================================================================*/
/*===========================================================================*/
// render_scene

std::span<mesh_instance> conjure::renderer::render_scene::allocate_meshes(u32 count) {
    meshes.clear();
    meshes.resize(count);
    return std::span<mesh_instance>(meshes.data(), meshes.size());
}

void conjure::renderer::render_scene::clear() noexcept {
    frameIndex = 0;
    deltaSeconds = 0.0f;
    views.clear();
    meshes.clear();
    lights.clear();
    sprites.clear();
    environment = environment_settings();
    postProcess = post_process_settings();
    shadows = shadow_settings();
    ssao = ssao_settings();
    debugDraw.clear();
}

/*===========================================================================*/
/*===========================================================================*/
// render_scene_pair

render_scene_pair::render_scene_pair() {}

render_scene_pair::~render_scene_pair() {}

render_scene &render_scene_pair::begin_extract() {
    CONJURE_ASSERT(!mExtracting, "render_scene_pair: begin_extract while an extraction is open");
    if (mOwner == std::thread::id()) {
        mOwner = std::this_thread::get_id();
    }
    CONJURE_ASSERT(mOwner == std::this_thread::get_id(), "render_scene_pair: begin_extract from a foreign thread");
    mExtracting = true;
    render_scene &slot = mScenes[mWriteIndex];
    slot.clear();
    return slot;
}

void render_scene_pair::end_extract() {
    CONJURE_ASSERT(mExtracting, "render_scene_pair: end_extract without begin_extract");
    CONJURE_ASSERT(
      mOwner == std::thread::id() || mOwner == std::this_thread::get_id(),
      "render_scene_pair: end_extract from a foreign thread"
    );
    if (!mExtracting) {
        return;
    }
    mExtracting = false;
    mWriteIndex ^= 1u;
}

const render_scene &render_scene_pair::read() const noexcept {
    return mScenes[mWriteIndex ^ 1u];
}

render_scene &render_scene_pair::write() noexcept {
    return mScenes[mWriteIndex];
}
