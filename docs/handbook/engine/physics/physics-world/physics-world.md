---
kind: class
---

[Engine](../../../engine.md) → [Physics](../../physics.md) → [physics_world.h](../physics-world.md)

# physics_world
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::physics` |
| Include File Path | `engine/include/conjure/physics/physics_world.h` |
| Source File Path | `engine/src/physics/physics_world.cpp` |
| API ↗ | [Physics](../../../../api/physics/index.md), [The physics world and queries](../../../../api/physics/physics-world-and-queries.md) |

## Syntax

```cpp
class CONJURE_API physics_world
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">physics_world(
    <span class="k">const</span> <a href="../physics-settings/physics-settings.html">physics_settings</a> &amp;settings,
    <a href="../../core/jobs/job-system/jobs-job-system.html">core::jobs::job_system</a> &amp;jobs);</code></td><td>Builds the Jolt system from validated settings (<code>physics_settings::validate</code> first; an invalid value is clamped with one ERROR) and sizes every buffer from them. Never fails.</td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted. Move constructors have been deleted. Move assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~physics_world();</code></td><td>Detaches when attached, destroys every body and releases the process-wide Jolt state.</td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mState</code></td><td><code class="signature">std::unique_ptr&lt;<a href="../../core/event/event-state.html">state</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `active_body_count` {#function-active-body-count}

<div class="function-declaration" id="function-active-body-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> active_body_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The dynamic and kinematic bodies that are awake, the inner bodies of characters among them. Static bodies never count, although every static trigger costs the step what an awake body costs: Jolt keeps it active so that it sees sleeping bodies (<code>contact_events.h</code>).</p>

#### `add_angular_impulse` {#function-add-angular-impulse}

<div class="function-declaration" id="function-add-angular-impulse-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; add_angular_impulse(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target,
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;impulse);</code></pre></div>

<p>An angular impulse. <code>DYNAMIC</code> bodies only, as <code>add_force</code>.</p>

#### `add_force` {#function-add-force}

<div class="function-declaration" id="function-add-force-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; add_force(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target,
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;force);</code></pre></div>

<p><code>A</code> force at the centre of mass for the next fixed step. <code>DYNAMIC</code> bodies only: <code>WRONG_BODY_KIND</code> for a static or kinematic body and for a character.</p>

#### `add_force_at` {#function-add-force-at}

<div class="function-declaration" id="function-add-force-at-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; add_force_at(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target,
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;force,
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;point);</code></pre></div>

<p><code>A</code> force at a world point for the next fixed step. <code>DYNAMIC</code> bodies only, as <code>add_force</code>.</p>

#### `add_impulse` {#function-add-impulse}

<div class="function-declaration" id="function-add-impulse-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; add_impulse(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target,
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;impulse);</code></pre></div>

<p>An impulse at the centre of mass. <code>DYNAMIC</code> bodies only, as <code>add_force</code>.</p>

#### `add_impulse_at` {#function-add-impulse-at}

<div class="function-declaration" id="function-add-impulse-at-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; add_impulse_at(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target,
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;impulse,
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;point);</code></pre></div>

<p>An impulse at a world point. <code>DYNAMIC</code> bodies only, as <code>add_force</code>.</p>

#### `add_torque` {#function-add-torque}

<div class="function-declaration" id="function-add-torque-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; add_torque(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target,
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;torque);</code></pre></div>

<p><code>A</code> torque for the next fixed step. <code>DYNAMIC</code> bodies only, as <code>add_force</code>.</p>

#### `attach` {#function-attach}

<div class="function-declaration" id="function-attach-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; attach(
    <a href="../../ecs/world/world.html">ecs::world</a> &amp;target);</code></pre></div>

<p>Installs the component observers and queues every entity that already carries a body component. <code>ALREADY_ATTACHED</code>; <code>INVALID_STATE</code> when the registry lacks the physics or scene components (<code>register_components</code> / <code>scene::register_components</code> first); <code>WRONG_THREAD</code> (and an assert) off the owner thread.</p>

#### `body_count` {#function-body-count}

<div class="function-declaration" id="function-body-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> body_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `body_pose` {#function-body-pose}

<div class="function-declaration" id="function-body-pose-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="../../core/math/transform/transform.html">math::transform</a>&gt; body_pose(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The body&#x27;s pose straight from Jolt (unit scale), nullopt without a body or while a push, step or pull runs.</p>

#### `bounds` {#function-bounds}

<div class="function-declaration" id="function-bounds-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/math/aabb/aabb.html">math::aabb</a> bounds() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Bounds of every body (<code>PhysicsSystem::GetBounds</code>; invalid when empty).</p>

#### `character_ground` {#function-character-ground}

<div class="function-declaration" id="function-character-ground-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="../character-controller.html#symbol-ground-state">ground_state</a>&gt; character_ground(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The ground state of a character, nullopt for anything else.</p>

#### `character_move` {#function-character-move}

<div class="function-declaration" id="function-character-move-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; character_move(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target,
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;velocity);</code></pre></div>

<p>Sets the desired velocity of a character for the next step (it replaces the component&#x27;s velocity for that step; the pull then writes the achieved velocity back). Like the component&#x27;s velocity it is the character&#x27;s own: the step adds the horizontal velocity of the ground that carries the character (<code><a href="../character-controller/character-controller.html">character_controller</a>.h</code>). <code>NOT_A_CHARACTER</code>, in place of <code>NO_BODY</code> and <code>WRONG_BODY_KIND</code>, when the entity has no body and is not pending or has a body that is no character; the other refusals are those of the body commands, in their order.</p>

#### `collect_contact_events` {#function-collect-contact-events}

<div class="function-declaration" id="function-collect-contact-events-1"><pre><code><span class="k">void</span> collect_contact_events();</code></pre></div>

<p>Merges and sorts the step&#x27;s raw contacts into <code><a href="#function-contact-events-1">contact_events</a>()</code> (3.6).</p>

#### `collect_shapes` {#function-collect-shapes}

<div class="function-declaration" id="function-collect-shapes-1"><pre><code><span class="k">void</span> collect_shapes();</code></pre></div>

<p>Drops cached shapes no body references.</p>

#### `contact_events` {#function-contact-events}

<div class="function-declaration" id="function-contact-events-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="../contact-events/contact-event.html">contact_event</a>&gt; contact_events() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The events of the last step, valid until the next <code>push_transforms</code>.</p>

#### `debug_draw` {#function-debug-draw}

<div class="function-declaration" id="function-debug-draw-1"><pre><code><span class="k">void</span> debug_draw(
    <a href="../physics-debug-draw/physics-debug-draw.html">physics_debug_draw</a> &amp;sink,
    <a href="../physics-debug-draw.html#symbol-physics-debug-draw-flags">physics_debug_draw_flags</a> flags) <span class="k">const</span>;</code></pre></div>

<p>Draws the bodies into the sink (3.10). Draws nothing when Jolt was built without its debug renderer (Distribution).</p>

#### `detach` {#function-detach}

<div class="function-declaration" id="function-detach-1"><pre><code><span class="k">void</span> detach();</code></pre></div>

<p>Removes the observers, destroys every body and removes the <code><a href="../../scene/components/interpolated-transform.html">interpolated_transform</a></code> components physics added; pending synthesised exits are discarded. The ECS world must not be locked.</p>

#### `gravity` {#function-gravity}

<div class="function-declaration" id="function-gravity-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> gravity() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The gravity in effect (after the last applied <code>set_gravity</code>, else <code><a href="#function-settings-1">settings</a>()</code>.gravity).</p>

#### `has_body` {#function-has-body}

<div class="function-declaration" id="function-has-body-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> has_body(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the entity has a body or a character now; <code>false</code> while it is still pending.</p>

#### `is_attached` {#function-is-attached}

<div class="function-declaration" id="function-is-attached-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_attached() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `is_sleeping` {#function-is-sleeping}

<div class="function-declaration" id="function-is-sleeping-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_sleeping(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the body is inactive, which a static body always is (a static trigger too); <code>false</code> when the entity has no body.</p>

#### `layers` {#function-layers}

<div class="function-declaration" id="function-layers-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../physics-layers/physics-layer-table.html">physics_layer_table</a> &amp;layers() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `move_kinematic` {#function-move-kinematic}

<div class="function-declaration" id="function-move-kinematic-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; move_kinematic(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target,
    <span class="k">const</span> <a href="../../core/math/transform/transform.html">math::transform</a> &amp;pose);</code></pre></div>

<p>MoveKinematic towards the pose over the next fixed step. <code>KINEMATIC</code> bodies without <code>KINEMATIC_MOVES_WITH_TRANSFORM</code> only: <code>WRONG_BODY_KIND</code> for a static or dynamic body, for a character, and for a kinematic body that follows its transform (write its <code><a href="../../scene/components/local-transform.html">local_transform</a></code>).</p>

#### `on_asset_reloaded` {#function-on-asset-reloaded}

<div class="function-declaration" id="function-on-asset-reloaded-1"><pre><code><span class="k">void</span> on_asset_reloaded(
    <a href="../../assets/asset-types.html#symbol-asset-id">assets::asset_id</a> id);</code></pre></div>

<p>Marks every body whose colliders reference the asset for a shape rebuild.</p>

#### `overlap_box` {#function-overlap-box}

<div class="function-declaration" id="function-overlap-box-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> overlap_box(
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;center,
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;halfExtents,
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-quat">math::quat</a> &amp;rotation,
    std::span&lt;<a href="../../ecs/entity/entity.html">ecs::entity</a>&gt; out,
    <span class="k">const</span> <a href="../queries/query-filter.html">query_filter</a> &amp;filter = {}) <span class="k">const</span>;</code></pre></div>

<p>Entities whose bodies overlap the oriented box, one entry per body, at most <code>out.size()</code>.</p>

#### `overlap_sphere` {#function-overlap-sphere}

<div class="function-declaration" id="function-overlap-sphere-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> overlap_sphere(
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;center,
    <a href="../../core/types.html#symbol-f32">f32</a> radius,
    std::span&lt;<a href="../../ecs/entity/entity.html">ecs::entity</a>&gt; out,
    <span class="k">const</span> <a href="../queries/query-filter.html">query_filter</a> &amp;filter = {}) <span class="k">const</span>;</code></pre></div>

<p>Entities whose bodies overlap the sphere, one entry per body, at most <code>out.size()</code>.</p>

#### `pull_transforms` {#function-pull-transforms}

<div class="function-declaration" id="function-pull-transforms-1"><pre><code><span class="k">void</span> pull_transforms(
    <a href="../../ecs/system/system-context.html">ecs::system_context</a> *context = nullptr);</code></pre></div>

<p>Writes Jolt poses and velocities into <code><a href="../../scene/components/local-transform.html">local_transform</a></code>, <code><a href="../../scene/components/interpolated-transform.html">interpolated_transform</a></code>, <code><a href="../components/rigid-body.html">rigid_body</a></code> and <code><a href="../character-controller/character-controller.html">character_controller</a></code> (3.7). With a <code><a href="../../ecs/system/system-context.html">system_context</a></code> the chunks run through <code>parallel_for_chunks</code>; <code>nullptr</code> runs the loop inline on the caller.</p>

#### `push_transforms` {#function-push-transforms}

<div class="function-declaration" id="function-push-transforms-1"><pre><code><span class="k">void</span> push_transforms(
    <a href="../../core/types.html#symbol-f32">f32</a> fixedDeltaTime,
    <a href="../../ecs/command-buffer/command-buffer.html">ecs::command_buffer</a> *deferred = nullptr);</code></pre></div>

<p>Clears the previous step&#x27;s events, creates pending bodies, applies rebuilds, pushes changed components and transforms into Jolt and applies the queued commands (3.4). <code>deferred</code> receives the <code><a href="../../scene/components/interpolated-transform.html">interpolated_transform</a></code> add/remove of bodies whose motion type changed; <code>nullptr</code> uses an internal buffer flushed when the world is unlocked.</p>

#### `raycast` {#function-raycast}

<div class="function-declaration" id="function-raycast-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="../queries/raycast-hit.html">raycast_hit</a>&gt; raycast(
    <span class="k">const</span> <a href="../../core/math/ray/ray.html">math::ray</a> &amp;ray,
    <a href="../../core/types.html#symbol-f32">f32</a> maxDistance,
    <span class="k">const</span> <a href="../queries/query-filter.html">query_filter</a> &amp;filter = {}) <span class="k">const</span>;</code></pre></div>

<p>Closest hit along the ray within <code>maxDistance</code>. <code>maxDistance</code> is clamped to 1e6 m (infinity means unlimited); a non-finite origin or direction hits nothing.</p>

#### `raycast_all` {#function-raycast-all}

<div class="function-declaration" id="function-raycast-all-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> raycast_all(
    <span class="k">const</span> <a href="../../core/math/ray/ray.html">math::ray</a> &amp;ray,
    <a href="../../core/types.html#symbol-f32">f32</a> maxDistance,
    std::span&lt;<a href="../queries/raycast-hit.html">raycast_hit</a>&gt; out,
    <span class="k">const</span> <a href="../queries/query-filter.html">query_filter</a> &amp;filter = {}) <span class="k">const</span>;</code></pre></div>

<p>The <code>out.size()</code> nearest hits along the ray, sorted by distance; returns the count written. <code>maxDistance</code> is clamped to 1e6 m (infinity means unlimited); a non-finite origin or direction hits nothing.</p>

#### `set_angular_velocity` {#function-set-angular-velocity}

<div class="function-declaration" id="function-set-angular-velocity-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; set_angular_velocity(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target,
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;velocity);</code></pre></div>

<p>Sets the angular velocity. <code>DYNAMIC</code> and <code>KINEMATIC</code> bodies, as <code>set_linear_velocity</code>.</p>

#### `set_gravity` {#function-set-gravity}

<div class="function-declaration" id="function-set-gravity-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; set_gravity(
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;gravity);</code></pre></div>

<p>Queued <code>PhysicsSystem::SetGravity</code> at the next push.</p>

#### `set_linear_velocity` {#function-set-linear-velocity}

<div class="function-declaration" id="function-set-linear-velocity-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; set_linear_velocity(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target,
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;velocity);</code></pre></div>

<p>Sets the linear velocity. <code>DYNAMIC</code> and <code>KINEMATIC</code> bodies: <code>WRONG_BODY_KIND</code> for a static body and for a character (<code>character_move</code> sets a character&#x27;s velocity). <code>A</code> kinematic body that follows its transform takes it for one step: the next push moves it back to its <code><a href="../../scene/components/local-transform.html">local_transform</a></code>.</p>

#### `settings` {#function-settings}

<div class="function-declaration" id="function-settings-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../physics-settings/physics-settings.html">physics_settings</a> &amp;settings() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `shape_cast` {#function-shape-cast}

<div class="function-declaration" id="function-shape-cast-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="../queries/shape-cast-hit.html">shape_cast_hit</a>&gt; shape_cast(
    <span class="k">const</span> <a href="../queries.html#symbol-cast-shape-desc">cast_shape_desc</a> &amp;shape,
    <span class="k">const</span> <a href="../../core/math/transform/transform.html">math::transform</a> &amp;start,
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;direction,
    <span class="k">const</span> <a href="../queries/query-filter.html">query_filter</a> &amp;filter = {}) <span class="k">const</span>;</code></pre></div>

<p>Sweeps the shape from start along direction (which includes the length); the closest hit. <code>A</code> non-finite start or direction hits nothing.</p>

#### `sleep` {#function-sleep}

<div class="function-declaration" id="function-sleep-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; sleep(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target);</code></pre></div>

<p>Puts a body to sleep at the next push. Every kind of body takes it; it does nothing on a static body, which is never awake, and on a character, which never sleeps.</p>

#### `stats` {#function-stats}

<div class="function-declaration" id="function-stats-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="physics-stats.html">physics_stats</a> stats() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `step` {#function-step}

<div class="function-declaration" id="function-step-1"><pre><code><span class="k">void</span> step(
    <a href="../../core/types.html#symbol-f32">f32</a> fixedDeltaTime);</code></pre></div>

<p>Updates the characters, then <code>PhysicsSystem::Update</code> once (3.5). Owner thread.</p>

#### `teleport` {#function-teleport}

<div class="function-declaration" id="function-teleport-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; teleport(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target,
    <span class="k">const</span> <a href="../../core/math/transform/transform.html">math::transform</a> &amp;pose);</code></pre></div>

<p>Moves the body instantly; scale is ignored, velocities and the interpolated pose are reset. The entity&#x27;s <code><a href="../../scene/components/local-transform.html">local_transform</a></code> follows: at the next pull for bodies the pull writes, at the push that applies the command for static and <code>KINEMATIC_MOVES_WITH_TRANSFORM</code> bodies. Every kind of body takes it, characters included. <code>A</code> character keeps its own velocity (its component&#x27;s) and loses what moving ground carried it with; the step after the teleport adds no horizontal ground velocity. The vertical part of that velocity is the ground&#x27;s while it stands (<code><a href="../character-controller/character-controller.html">character_controller</a>.h</code>), so a character teleported off ground that moves vertically takes that vertical velocity along.</p>

#### `validate` {#function-validate}

<div class="function-declaration" id="function-validate-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; validate() <span class="k">const</span>;</code></pre></div>

<p>Checks that rows, entities and Jolt agree; the message names the first violated invariant. <code>UNKNOWN_LAYER</code> for a body whose component names a missing layer.</p>

#### `wake` {#function-wake}

<div class="function-declaration" id="function-wake-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; wake(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target);</code></pre></div>

<p>Wakes a sleeping body at the next push. Every kind of body takes it; it does nothing on a static body, which is never awake (<code>is_sleeping</code> is <code>true</code> for it), and on a character, which never sleeps.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
