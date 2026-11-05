# core/math

The engine's geometry toolbox (`conjure::math`, static library `conjure::core_math`). It wraps glm
behind the single public header that is allowed to include it (`conjure/core/math/glm_types.h`) and
adds the value types every other module shares: `transform`, `aabb`, `plane`, `frustum`, `ray`,
`rect`/`urect`, scalar helpers and projection matrices. Design: `docs/design/10_core.md` 2.2.

## Conventions

- Right-handed, Y up, -Z forward (glTF). Matrices are column-major and multiply column vectors.
- Depth 0..1 (`GLM_FORCE_DEPTH_ZERO_TO_ONE`); `perspective`, `infinite_perspective` and
  `orthographic` call the `RH_ZO` glm variants explicitly, and `frustum::from_view_projection`
  extracts the near plane from clip `z >= 0`.
- Angles are radians everywhere (`GLM_FORCE_RADIANS`); `radians()` / `degrees()` convert.
- glm constructors are explicit (`GLM_FORCE_EXPLICIT_CTOR`): write `vec3(0.0f)`, never `vec3 v = 0.0f`.
- `quat` is constructed as `quat(w, x, y, z)` and stored as `x, y, z, w`; formatters print `(w, x, y, z)`.
- Containment and intersection tests on `aabb` and `rect` are inclusive (touching counts);
  `urect` is a pixel region with exclusive right and bottom edges.

## Headers

| Header | Contents |
|---|---|
| `conjure/core/math.h` | one-line include of the umbrella |
| `math/math.h` | umbrella + `perspective`, `infinite_perspective`, `orthographic`, `look_at` (view matrix), `project`, `unproject` |
| `math/glm_types.h` | glm includes, aliases `vec2/3/4`, `ivec2/3/4`, `uvec2/3/4`, `quat`, `mat3/4`, `nearly_equal` for all of them, `same_rotation`, `safe_normalize`, `std::formatter` specialisations |
| `math/math_utils.h` | `PI`, `EPSILON`, `radians`, `degrees`, `wrap_angle`, `nearly_equal`, `clamp`, `lerp`, `smoothstep`, `is_power_of_two`, `align_up/down`, `is_aligned`, `next/previous_power_of_two`, `log2_floor`, `divide_round_up` (no glm) |
| `math/transform.h` | `transform` (TRS, 40 bytes, trivially copyable, mirrored by C#) |
| `math/aabb.h` | `aabb` (inverted by default; `expand`, `transformed` by the 8-corner method, `surface_area`, ...) |
| `math/plane.h` | `plane` (`dot(normal, p) + distance = 0`, positive side = normal side) |
| `math/frustum.h` | `frustum_plane`, `frustum` (Gribb-Hartmann extraction, p-vertex box test, sphere and point tests, `corners`) |
| `math/ray.h` | `ray` (slab, sphere, plane and Moller-Trumbore triangle intersections, `from_viewport`) |
| `math/rect.h` | `rect` (float, min/max) and `urect` (u32 x/y/width/height) |

Two headers are not in the design document's list: `glm_types.h` holds the aliases so that
`transform.h`, `aabb.h`, ... can include them without a cycle through the umbrella, and `plane.h`
exists because both `frustum.h` and `ray.h` need `plane`.

## Behaviour worth knowing

- `transform` composition (`parent * child`) and `inverse()` are exact for uniform scale. A TRS
  cannot hold shear, so with non-uniform scale under rotation `parent * child` equals the matrix
  product only when the child rotation is the identity, and `inverse() * t` is the identity while
  `t * inverse()` is not. `inverse_transform_point` is exact for every scale. Zero scale components
  invert to zero (never infinity). `from_matrix` turns a negative determinant into a negative x scale,
  also when another axis is collapsed (the zero columns are repaired before the sign test).
  `transform::lerp` does not clamp `t` (like `math::lerp`), so it extrapolates for prediction/rewind.
- `transform::look_at` and `look_at` (view matrix) share one basis builder: a target equal to the eye
  gives the identity rotation, an up hint parallel to the view direction (or zero) is replaced by the
  world axis least aligned with it (ties prefer Y, then Z). The view matrix is exactly the inverse of
  the camera transform.
- `frustum::intersects(aabb)` is conservative: it never rejects a visible box but may accept a box that
  sits diagonally outside a far corner. `contains(aabb)` is exact. Infinite projections produce a
  degenerate far plane (zero normal, positive distance) that accepts everything; `frustum::corners`
  asserts for them because the far corners are at infinity.
- `ray` intersections return the smallest `t >= 0` at which the ray is on or inside the shape; a ray
  starting inside a box or sphere hits at `t = 0`, planes are hit from both sides, and the triangle
  test culls back faces by default. The triangle degeneracy test is relative to the triangle size and
  direction length, so millimetre triangles and grazing angles still hit. `ray::from_viewport` builds
  its direction from depth 0 and 0.5 so infinite projections work.
- `aabb::transformed` is for affine matrices only (no perspective divide). `closest_point` and
  `distance_squared` return the point itself and 0 for an invalid box, like `surface_area`/`volume`.
- `nearly_equal` treats exactly equal values (including equal infinities) as equal; NaN never is.
  `inverse_lerp` only collapses an exactly empty range to 0.
- `urect` requires `x + width` and `y + height` to fit in `u32`; `right()`/`bottom()` assert otherwise.
- The integer helpers (`align_up`, `align_down`, `is_aligned`, `next_power_of_two`, ...) are templates
  constrained on `unsigned_integer` (any unsigned width, not `bool`); callers `static_cast` signed
  indices. `align_up` asserts and returns the value unchanged when the rounded value would wrap.
- Every scalar helper that validates input (`align_up` with a non power-of-two alignment or an
  overflowing value, `clamp` with `low > high`, `divide_round_up` by zero, the projection factories)
  asserts and returns a safe value.

## Threading model

Everything in the area is a value type or a pure function with no global state, so every function
is safe to call from any thread concurrently. `tests/core/math/test_math.cpp` runs the heavier
functions from eight threads and checks bit-identical results.

## Testing

`tests/core/math/` builds `conjure_core_math_tests`, one `test_<header>.cpp` per public header plus
`math_test.h` (doctest string makers for the math types and `CHECK_NEARLY`). Frustum extraction is
verified against `glm::perspective`, transforms against glm matrix products, and the ray tests cover
the parallel, inside and behind edge cases. Build and run with:

```
python tools/build.py --build-dir build/core_math --target conjure_core_math_tests --test -R core_math
```
