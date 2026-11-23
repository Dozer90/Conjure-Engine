---
kind: header
---

[Engine](../../engine.md) → [Physics](../physics.md)

# physics_error.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/physics/physics_error.h` |
| API ↗ | [Physics](../../../api/physics/index.md) |

<p>Failure conditions reported by the physics module.</p>

## Enumerations

### `physics_error` {#symbol-physics-error}

```cpp
enum class physics_error : u16 {
    NONE = 0,
    INVALID_SETTINGS,               // physics_settings::validate rejected a field (the message names it)
    LAYER_CONFIG_INVALID,           // extensions.physics_layers has no usable "names" array
    LAYER_LIMIT_REACHED,            // more than MAX_PHYSICS_LAYERS layers
    DUPLICATE_LAYER,                // a layer name appears twice
    UNKNOWN_LAYER,                  // an "ignore" pair or a body names a layer that does not exist
    BODY_LIMIT_REACHED,             // Jolt refused to create a body (maxBodies)
    NO_BODY,                        // the entity has no body (and is not pending)
    NOT_A_CHARACTER,                // the entity's body is not a character controller
    NOT_ATTACHED,                   // attach() was not called yet
    ALREADY_ATTACHED,               // attach() called twice
    SHAPE_CREATE_FAILED,            // a Jolt ShapeSettings::Create() reported an error
    INVALID_SHAPE_SCALE,            // the entity's world scale cannot be applied to its shape
    UNSUPPORTED_SHAPE_FOR_MOTION,   // mesh_collider on a DYNAMIC body
    MIXED_TRIGGER_FLAGS,            // colliders of one body disagree on isTrigger
    CONFLICTING_BODY_COMPONENTS,    // rigid_body and character_controller on one entity
    GEOMETRY_INVALID,               // collision mesh indices out of range or no triangles
    COLLISION_MESH_PARSE_FAILED,    // glTF / OBJ source could not be parsed
    STEP_IN_PROGRESS,               // a query or body call arrived while step() runs
    WRONG_THREAD,                   // an owner-thread call from another thread
    COMMAND_QUEUE_FULL,             // maxQueuedCommands reached; the command was dropped
    WRONG_BODY_KIND,                // the entity has a body, but the command does not apply to that kind of body
    COUNT
};
```

Failure conditions reported by the physics module.

## Macros

### `CONJURE_DECLARE_ERROR_CATEGORY` {#symbol-conjure-declare-error-category}

```cpp
CONJURE_DECLARE_ERROR_CATEGORY(conjure::physics::physics_error, "physics", conjure::physics::physics_error_name)
```

## Functions

### Public

#### `physics_error_name` {#function-physics-error-name}

<div class="function-declaration" id="function-physics-error-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view physics_error_name(
    <a href="physics-error.html#symbol-physics-error">physics_error</a> code) <span class="k">noexcept</span>;</code></pre></div>

<p>The stable upper-case enumerator name of a code (&quot;<code>NO_BODY</code>&quot;); &quot;&quot; for unknown values.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="classes"></a>
<a id="conjure_declare_error_category"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="physics_error"></a>
<a id="physics_error_name"></a>
<a id="physics_erroralready_attached"></a>
<a id="physics_errorbody_limit_reached"></a>
<a id="physics_errorcollision_mesh_parse_failed"></a>
<a id="physics_errorcommand_queue_full"></a>
<a id="physics_errorconflicting_body_components"></a>
<a id="physics_errorcount"></a>
<a id="physics_errorduplicate_layer"></a>
<a id="physics_errorgeometry_invalid"></a>
<a id="physics_errorinvalid_settings"></a>
<a id="physics_errorinvalid_shape_scale"></a>
<a id="physics_errorlayer_config_invalid"></a>
<a id="physics_errorlayer_limit_reached"></a>
<a id="physics_errormixed_trigger_flags"></a>
<a id="physics_errorno_body"></a>
<a id="physics_errornone"></a>
<a id="physics_errornot_a_character"></a>
<a id="physics_errornot_attached"></a>
<a id="physics_errorshape_create_failed"></a>
<a id="physics_errorstep_in_progress"></a>
<a id="physics_errorunknown_layer"></a>
<a id="physics_errorunsupported_shape_for_motion"></a>
<a id="physics_errorwrong_body_kind"></a>
<a id="physics_errorwrong_thread"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
