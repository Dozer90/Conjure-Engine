---
kind: header
---

[Engine](../../engine.md) → [Engine](../engine.md)

# main_camera.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/engine/main_camera.h` |
| API ↗ | [Engine: the application and the frame](../../../api/engine/index.md) |

<p>The main-camera rule: which camera the game view shows. The renderer&#x27;s extraction keeps its own loop because it needs every camera, not only the first; the two agree on which camera comes first.</p>

## Forward Declarations

### `world` {#symbol-world}

```cpp
class world;
```

## Functions

### Public

#### `find_main_camera` {#function-find-main-camera}

<div class="function-declaration" id="function-find-main-camera-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../ecs/entity/entity.html">ecs::entity</a> find_main_camera(
    <span class="k">const</span> <a href="../ecs/world/world.html">ecs::world</a> &amp;world);</code></pre></div>

<p>The camera extraction shows first: among the enabled entities (neither <code>scene.disabled</code> nor <code>scene.inactive</code>) holding <code><a href="../renderer/components/camera-component.html">renderer::camera_component</a></code> and <code><a href="../scene/components/render-world-transform.html">scene::render_world_transform</a></code>, a main camera (isMain) before any other, then the highest priority, then the first in world order (archetype creation order, then row). <code>NULL_ENTITY</code> when there is none, when the world has never registered those components, and always in a build without the renderer. Reads the world without changing it; the caller keeps it from changing structurally meanwhile (main thread, outside a world flush).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->
