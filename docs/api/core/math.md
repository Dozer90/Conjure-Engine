# Math

📘 **Public API wiki** · 🛠 Handbook: [Core module](../../handbook/modules/core.md)

Vectors, quaternions, matrices and the geometry types every part of Conjure shares: `transform`, `aabb`, `plane`,
`frustum`, `ray`, `rect` and `urect`, plus scalar helpers and projection matrices. Include `conjure/core/math.h`
(the one-line umbrella) and everything lives in `namespace conjure::math`. The vector and matrix types are glm's,
so any glm function (`glm::normalize`, `glm::dot`, `glm::angleAxis`, ...) works on them directly.

## At a glance

| You want to... | Use |
|---|---|
| Position, rotate and scale something | `transform` (`conjure/core/math/transform.h`) |
| Point an object or camera at a target | `transform::look_at(eye, target, upHint)` |
| Build camera matrices | `perspective()`, `infinite_perspective()`, `orthographic()`, `look_at()` (`conjure/core/math/math.h`) |
| Cull objects against the camera | `frustum::from_view_projection(viewProjection).intersects(box)` |
| Pick what is under the mouse | `ray::from_viewport(pixel, viewportSize, inverseViewProjection)` then `ray::intersect*()` |
| Bound a set of points | `aabb` (`expand()`, `from_center_extents()`) |
| Work with screen or texture regions | `rect` (float, inclusive) or `urect` (pixels, exclusive edges) |
| Convert angles, interpolate, clamp | `radians()`, `degrees()`, `lerp()`, `clamp()`, `smoothstep()` (`conjure/core/math/math_utils.h`) |
| Compare floats or vectors with a tolerance | `nearly_equal(a, b)`; `same_rotation(q1, q2)` for quaternions |
| Round sizes to an alignment | `align_up()`, `align_down()`, `is_power_of_two()`, `next_power_of_two()` |
| Print a vector in a log message | `std::format("{}", value)` (vectors, quaternions, matrices, `transform`, `aabb`, `plane`, `rect`, `urect`; not `frustum` or `ray`) |

## Quick example

```cpp
#include "conjure/core/math.h"

using namespace conjure;
using namespace conjure::math;

bool box_is_visible(const aabb &worldBox) {
    const vec3 eye = vec3(0.0f, 2.0f, 5.0f);
    const vec3 target = vec3(0.0f);
    const vec3 worldUp = vec3(0.0f, 1.0f, 0.0f);

    const mat4 view = look_at(eye, target, worldUp);
    const mat4 projection = perspective(radians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f);
    const frustum visible = frustum::from_view_projection(projection * view);
    return visible.intersects(worldBox);
}

transform orbit(const transform &parent, f32 angleRadians) {
    transform child = transform::from_position(vec3(0.0f, 0.0f, -3.0f));
    child.rotation = glm::angleAxis(angleRadians, vec3(0.0f, 1.0f, 0.0f));
    return parent * child;   // child expressed in world space
}
```

## Coordinate conventions

Every module and the C# side agree on these; the types below assume them.

| Convention | Value |
|---|---|
| Handedness and axes | Right-handed, **Y up, -Z forward** (the glTF convention). `transform::forward()` is the rotated -Z axis, `right()` is +X, `up()` is +Y. |
| Matrices | Column-major, acting on column vectors: `matrix * vec4(point, 1.0f)`. Compose right to left: `projection * view * model`. |
| Angles | Radians everywhere. `radians(90.0f)` and `degrees(HALF_PI)` convert. |
| Depth | Clip-space depth is **0..1** (near plane 0, far plane 1), not -1..1. |
| Quaternions | Constructed as `quat(w, x, y, z)`; identity `quat(1.0f, 0.0f, 0.0f, 0.0f)`. Formatters print `(w, x, y, z)`. See the warning below for the memory layout. |
| Constructors | glm constructors are explicit: write `vec3(0.0f)`, never `vec3 value = 0.0f`. |

> [!NOTE]
> The depth row describes core math (`perspective()`, `frustum::from_view_projection`, `ray::from_viewport`). The
> renderer's camera matrices are **reverse-Z** (near plane at depth 1, far plane at 0, from
> `renderer::clip_from_view`); with those use `renderer::frustum_from_clip` and `renderer::ray_from_viewport`
> (`conjure/renderer/camera.h`, see [Rendering a frame](../renderer/rendering-a-frame.md)).

> [!WARNING]
> `quat(1.0f, 0.0f, 0.0f, 0.0f)` is the identity because the constructor takes `w` first, but the same four
> floats laid out in memory are `x, y, z, w` (what the C# mirror and `memcpy` see). Never build a quaternion from
> a raw float array without checking which order you hold; when in doubt, assign `.x`, `.y`, `.z` and `.w` by name.

> [!TIP]
> `nearly_equal(q1, q2)` compares the four components, so `q` and `-q` compare different although they rotate
> identically. Use `same_rotation(q1, q2)` when you mean "the same orientation".

## Vectors, matrices and formatting

`conjure/core/math/glm_types.h` is the single header through which glm reaches the engine. It defines the aliases
`vec2`, `vec3`, `vec4`, `ivec2`, `ivec3`, `ivec4`, `uvec2`, `uvec3`, `uvec4`, `quat`, `mat3` and `mat4`, all
unpadded (`vec3` is 12 bytes, `mat4` is 64), and adds:

- `nearly_equal(a, b, epsilon = EPSILON)` for the float aliases (`vec2`, `vec3`, `vec4`, `quat`, `mat3`, `mat4`;
  component-wise), `nearly_zero(v)` for `vec2`/`vec3`, and `same_rotation(a, b)` for unit quaternions. Integer
  vectors compare with `==`.
- `safe_normalize(value, fallback)`: like `glm::normalize` but returns `fallback` instead of NaN for a zero
  vector. The default fallback is `-Z` for `vec3` and `+X` for `vec2`.
- `std::formatter` specialisations for every alias, so `std::format("{:.2f}", position)` prints `(1.00, 2.00, 3.00)`
  and `CONJURE_LOG_INFO(category, "camera at {}", eye)` just works. Matrices print their columns in order. The
  geometry types `transform`, `aabb`, `plane`, `rect` and `urect` format the same way; `frustum` and `ray` have no
  formatter, so print their members.

## Scalar helpers

`conjure/core/math/math_utils.h` needs no glm and is `constexpr` wherever the standard library allows.

| Helper | Behaviour |
|---|---|
| `PI`, `TWO_PI`, `HALF_PI`, `QUARTER_PI`, `INVERSE_PI`, `EPSILON` (1e-6), `F32_MAX`, `F32_INFINITY` | Constants (`f32`). `EPSILON_F64` is 1e-12. |
| `radians(deg)`, `degrees(rad)`, `wrap_angle(rad)` | Conversions; `wrap_angle` maps into `[-PI, PI)`. |
| `nearly_equal(a, b, epsilon)`, `nearly_zero(v)` | Absolute tolerance. Exactly equal values (even infinities) are equal; NaN never is. |
| `absolute(v)`, `sign(v)`, `square(v)` | `sign` returns -1, 0 or 1 (0 for NaN). |
| `clamp(v, low, high)`, `saturate(v)` | `clamp` asserts when `low > high`. `saturate` clamps to `[0, 1]`. |
| `lerp(a, b, t)`, `inverse_lerp(a, b, v)`, `remap(v, inLow, inHigh, outLow, outHigh)` | None of them clamp `t`; `inverse_lerp` returns 0 when `a == b`. |
| `smoothstep(edge0, edge1, v)` | Hermite step, clamped to `[0, 1]`. |

The integer helpers accept **unsigned** types only (`u8` ... `u64`, `usize`), because sizes, offsets and alignments
are unsigned in the engine. Hold a signed index? `static_cast` it first.

| Helper | Behaviour |
|---|---|
| `is_power_of_two(v)` | False for 0. |
| `align_up(v, alignment)`, `align_down(v, alignment)`, `is_aligned(v, alignment)` | `alignment` must be a power of two (asserted; `align_up`/`align_down` then return `v` unchanged and `is_aligned` returns false). `align_up` also asserts instead of wrapping to 0 when the result does not fit the type. `is_aligned(pointer, alignment)` exists for addresses. |
| `next_power_of_two(v)`, `previous_power_of_two(v)` | `next_power_of_two(0) == 1` and returns 0 when the result does not fit; `previous_power_of_two(0) == 0`. |
| `log2_floor(v)`, `divide_round_up(n, d)` | `log2_floor(0)` and `divide_round_up(n, 0)` assert and return 0; `divide_round_up(0, d)` is simply 0. |

```cpp
const usize bytes = align_up(usize{1000}, 256);   // 1024
const u32 chunks = divide_round_up(u32{10}, 3u);   // 4
```

## Transforms

`transform` (`conjure/core/math/transform.h`) is position, rotation and scale: 10 floats, trivially copyable, the
same layout the C# `Transform` uses. The default value is the identity.

```cpp
transform pose = {};
pose.position = vec3(1.0f, 0.0f, -2.0f);
pose.rotation = glm::angleAxis(HALF_PI, vec3(0.0f, 1.0f, 0.0f));   // yaw 90 degrees left
pose.scale = vec3(2.0f);

const vec3 worldPoint = pose.transform_point(vec3(0.0f, 0.0f, -1.0f));
const vec3 localPoint = pose.inverse_transform_point(worldPoint);    // back to (0, 0, -1)
const mat4 model = pose.to_matrix();                                 // translate * rotate * scale
const transform camera = transform::look_at(vec3(0.0f, 5.0f, 5.0f), pose.position, vec3(0.0f, 1.0f, 0.0f));
```

- `parent * child` moves a child expressed in the parent's space into the parent's parent space (world space for a
  root parent). The identity is neutral and rigid transforms compose associatively.
- `transform_point` applies scale, rotation then translation; `transform_direction` applies only the rotation.
  `inverse_transform_point` is the exact inverse for any scale, including non-uniform and zero (a zero scale axis
  maps to 0, never infinity).
- `look_at(eye, target, upHint)` always yields a valid rotation: a target equal to the eye keeps the identity, and an
  up hint parallel to the view direction (or zero) is replaced by the world axis least aligned with it.
- `lerp(a, b, t)` interpolates position and scale linearly and the rotation by shortest-path slerp. Like
  `math::lerp`, `t` is **not** clamped, so `t = 2` extrapolates (handy for prediction and rewind).
- `from_matrix` decomposes a TRS matrix; mirroring comes back as a negative x scale.

> [!WARNING]
> A TRS cannot hold shear. With a **non-uniform** parent scale and a **rotated** child, `parent * child` is not the
> matrix product `parent.to_matrix() * child.to_matrix()`, and `t * t.inverse()` is not the identity (only
> `t.inverse() * t` is). With uniform scale, or an unrotated child, every identity holds exactly. When you need the
> sheared result, multiply the matrices instead.

## Bounding boxes

`aabb` (`conjure/core/math/aabb.h`) is an axis-aligned box given by `min` and `max`. The default box is **inverted**
(`min = +F32_MAX`, `max = -F32_MAX`) so that expanding it with the first point makes it valid; `is_valid()` is false
until then and every query on an invalid box returns the "nothing" answer (`contains` false, `volume` 0).

```cpp
aabb world_bounds(std::span<const vec3> vertices, const mat4 &model) {
    aabb bounds = {};
    for (const vec3 &vertex : vertices) {
        bounds.expand(vertex);
    }
    return bounds.transformed(model);   // encloses the 8 transformed corners
}

bool touches_unit_cube(const aabb &worldBounds) {
    const aabb unit = aabb::from_center_extents(vec3(0.0f), vec3(0.5f));
    return worldBounds.intersects(unit);   // inclusive: touching counts
}
```

Containment and intersection are inclusive: boxes that merely touch intersect. `corners()` returns the eight corners
in the order `min, +x, +y, +xy, +z, +xz, +yz, max`.

> [!WARNING]
> `aabb::transformed` is for affine matrices (model, view) only: it ignores the perspective divide, so a projection
> matrix produces a meaningless box. For view volumes use `frustum::corners`.

## Planes

`plane` (`conjure/core/math/plane.h`) stores `normal` and `distance` with the equation `dot(normal, p) + distance = 0`;
the positive side is where the normal points. `signed_distance(point)` is a metric distance only when the normal has
unit length, which `from_point_normal`, `from_points` (counter-clockwise `a, b, c`) and frustum extraction guarantee;
call `normalize()` on a plane you built by hand. `project(point)` drops a point onto the plane.

## Frustums

`frustum` (`conjure/core/math/frustum.h`) holds six inward-facing, normalised planes indexed by `frustum_plane`
(`LEFT`, `RIGHT`, `BOTTOM`, `TOP`, `NEAR_PLANE`, `FAR_PLANE`). Build it from a view-projection matrix (or a bare
projection for camera space) and test against it:

```cpp
void draw_mesh(const aabb &worldBox);
void draw_point_light(vec3 position, f32 radius);

void cull_and_draw(const mat4 &viewProjection, const aabb &worldBox, vec3 lightPosition, f32 radius) {
    const frustum visible = frustum::from_view_projection(viewProjection);
    if (!visible.intersects(worldBox)) {
        return;   // fast, conservative: never rejects something visible
    }
    draw_mesh(worldBox);
    if (visible.intersects_sphere(lightPosition, radius)) {
        draw_point_light(lightPosition, radius);
    }
}
```

```cpp
// The eight world-space corners of the view volume: debug drawing, fitting shadow cascades.
const std::array<vec3, 8> corners = frustum::corners(glm::inverse(viewProjection));
```

> [!NOTE]
> `intersects(aabb)` and `intersects_sphere` are conservative: they never reject something visible, but a box sitting
> diagonally outside a far corner can be accepted. That is the right trade for culling; use `contains(box)` (every
> corner inside) or `contains(point)` when you need an exact answer.

> [!WARNING]
> An infinite projection has no far plane: `from_view_projection` produces a degenerate far plane that accepts
> everything (fine for culling), but `frustum::corners` asserts for it because the far corners are at infinity.

> [!NOTE]
> `from_view_projection` and `corners` assume forward depth (near plane at depth 0). Given a reverse-Z matrix from
> the renderer, the planes still bound the same volume but `NEAR_PLANE` and `FAR_PLANE` swap labels, and `corners`
> lists the far corners first. `renderer::frustum_from_clip` corrects the labels for the renderer's matrices.

## Rays and picking

`ray` (`conjure/core/math/ray.h`) is `origin + t * direction` for `t >= 0`. Keep `direction` normalised (the
factories do) so that `t` is a distance. Every `intersect*` returns the smallest `t >= 0` at which the ray is on or
inside the shape, or `std::nullopt` on a miss; a ray that starts inside a box or sphere hits at `t = 0`.

```cpp
std::optional<vec3> pick(vec2 mousePixel, uvec2 viewportSize, const mat4 &viewProjection, const aabb &target) {
    const ray pickRay = ray::from_viewport(mousePixel, viewportSize, glm::inverse(viewProjection));
    const std::optional<f32> hit = pickRay.intersect(target);
    if (!hit.has_value()) {
        return std::nullopt;
    }
    return pickRay.at(*hit);
}
```

- `from_viewport` takes pixel coordinates with the origin at the top-left and y pointing down (pixel centres are at
  +0.5); the ray starts on the near plane and works for perspective, infinite and orthographic projections built
  with forward depth (`perspective()` and friends). For the renderer's reverse-Z camera matrices use
  `renderer::ray_from_viewport` instead.
- `intersect_plane` hits from both sides and misses when parallel; `intersect_triangle` culls back faces
  (counter-clockwise front) unless you pass `cullBackFaces = false`. Edges and vertices count as hits.
- `from_points(a, b)` normalises the direction; `a == b` gives the default `-Z` ray.

## Rectangles

`conjure/core/math/rect.h` has two rectangle types for two jobs:

| Type | Fields | Edges | Use for |
|---|---|---|---|
| `rect` | `vec2 min`, `vec2 max` | inclusive (touching counts) | UI layout, 2D bounds, anything in float units |
| `urect` | `u32 x, y, width, height` | right and bottom **exclusive** | viewports, scissors, texture regions |

`rect` offers the same vocabulary as `aabb` (`contains`, `intersects`, `intersection`, `expand`, `inflated`,
`closest_point`, `center`, `size`) plus `translated`, `from_position_size`, `from_center_extents` and `from_points`.
The default `rect` is empty at the origin and valid; `is_empty()` is true for zero width or height. Like `aabb`,
`closest_point` returns the point unchanged for an invalid rectangle. `from_position_size(position, extent)` spans
`position` and `position + extent`, so a negative extent extends to the other side of `position`
(`from_position_size(vec2(10.0f), vec2(-4.0f, 2.0f))` is min `(6, 10)`, max `(10, 12)`).

`urect` covers pixel `(px, py)` when `x <= px < right()` and `y <= py < bottom()`. `intersection` of disjoint
rectangles is empty (zero size), `area()` returns a `u64`, and `to_rect()` converts to the float form.

> [!CAUTION]
> `urect` requires `x + width` and `y + height` to fit in `u32`; `right()` and `bottom()` assert when they would
> wrap. Clamp sizes coming from untrusted data before building one.

## Projection and view matrices

The free functions in `conjure/core/math/math.h` build the camera matrices with the engine's conventions baked in
(right-handed, depth 0..1). All of them assert and return a safe matrix on invalid input (non-positive field of view,
aspect or near plane; `farPlane <= nearPlane`; an empty orthographic range).

| Function | Result |
|---|---|
| `perspective(fovYRadians, aspect, nearPlane, farPlane)` | Perspective projection; `-nearPlane` maps to depth 0, `-farPlane` to depth 1. |
| `infinite_perspective(fovYRadians, aspect, nearPlane)` | No far plane; depth tends to 1 at infinity. |
| `orthographic(left, right, bottom, top, nearPlane, farPlane)` | Maps the box to clip space. |
| `look_at(eye, target, up)` | View matrix (world to camera): exactly `glm::inverse(transform::look_at(eye, target, up).to_matrix())`. |
| `project(point, viewProjection)` | World point to normalised device coordinates (`x, y` in `[-1, 1]`, depth in `[0, 1]`); `nullopt` on the camera plane. |
| `unproject(ndc, inverseViewProjection)` | The reverse; `nullopt` at infinity (depth 1 of an infinite projection). |

> [!TIP]
> Need the camera as an object in the scene as well as a view matrix? Keep a `transform` for the camera (so it can be
> parented, moved and interpolated) and derive the view matrix with `glm::inverse(cameraTransform.to_matrix())` or
> `look_at(...)`; both agree exactly.

## Threading

Everything on this page is a value type or a pure function with no global state. Every call is safe from any thread,
including job workers.

## API summary

| Signature (short) | Purpose |
|---|---|
| `transform{position, rotation, scale}` | TRS value type; `to_matrix()`, `from_matrix(m)`, `operator*`, `inverse()` |
| `transform::transform_point(p)` / `inverse_transform_point(p)` | Local to parent space and back (exact) |
| `transform::transform_direction(d)` / `inverse_transform_direction(d)` | Rotation only |
| `transform::forward()`, `right()`, `up()` | Rotated -Z, +X, +Y |
| `transform::look_at(eye, target, upHint)`, `lerp(a, b, t)` | Aim at a target; interpolate (unclamped) |
| `transform::from_position(v)`, `from_rotation(q)`, `from_scale(v)` | Single-component factories |
| `aabb{min, max}`: `is_valid()`, `center()`, `extents()`, `size()` | Box geometry |
| `aabb::expand(point \| box)`, `inflated(amount)` | Grow |
| `aabb::contains(point \| box)`, `intersects(box)`, `intersection(box)` | Inclusive tests |
| `aabb::closest_point(p)`, `distance_squared(p)`, `corners()`, `transformed(m)` | Queries |
| `aabb::surface_area()`, `volume()`, `from_center_extents(c, e)`, `from_points(a, b)` | Measures and factories |
| `plane{normal, distance}`: `signed_distance(p)`, `project(p)`, `normalize()`, `normalized()` | Plane queries |
| `plane::from_point_normal(p, n)`, `from_points(a, b, c)` | Normalised factories |
| `frustum::from_view_projection(m)`, `corners(inverseM)` | Build from a camera |
| `frustum::intersects(box)`, `intersects_sphere(c, r)`, `contains(box \| point)`, `get(frustum_plane)` | Culling tests |
| `ray{origin, direction}`: `at(t)` | Point along the ray |
| `ray::intersect(box)`, `intersect_sphere(c, r)`, `intersect_plane(p)`, `intersect_triangle(a, b, c, cull)` | `std::optional<f32>` hit distance |
| `ray::from_points(a, b)`, `from_viewport(pixel, size, inverseViewProjection)` | Factories |
| `rect{min, max}`, `urect{x, y, width, height}` | Float and pixel rectangles |
| `perspective(...)`, `infinite_perspective(...)`, `orthographic(...)`, `look_at(...)` | Camera matrices |
| `project(p, vp)`, `unproject(ndc, inverseVp)` | `std::optional<vec3>` conversions |
| `radians()`, `degrees()`, `wrap_angle()`, `lerp()`, `clamp()`, `smoothstep()`, ... | Scalar helpers |
| `nearly_equal(a, b)`, `same_rotation(q1, q2)`, `safe_normalize(v)` | Comparisons and safe normalisation |
| `align_up()`, `align_down()`, `is_aligned()`, `is_power_of_two()`, `next_power_of_two()` | Unsigned integer helpers |

## See also

- 📘 API: [Data types](data-types.md) for `f32`, `u32`, `usize` and friends
- 📘 API: [Time, events and random](time-and-events.md) for `random::unit_vector3<vec3>()`
- 🛠 Handbook: [Core module](../../handbook/modules/core.md)
