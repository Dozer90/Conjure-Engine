---
kind: header
---

[Engine](../../engine.md) → [Scene](../scene.md)

# scene_error.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/scene/scene_error.h` |
| API ↗ | [Scenes, hierarchy and prefabs](../../../api/scene/index.md) |

<p>Failure conditions reported by the scene module.</p>

## Enumerations

### `scene_error` {#symbol-scene-error}

```cpp
enum class scene_error : u16 {
    NONE = 0,
    INVALID_ENTITY,           // dead, null or foreign entity handed to a mutator
    SELF_PARENT,              // set_parent(e, e)
    HIERARCHY_CYCLE,          // the new parent is a descendant of the child
    HIERARCHY_TOO_DEEP,       // the subtree would reach depth MAX_HIERARCHY_DEPTH
    INVALID_SIBLING_INDEX,    // the target has no sibling list (dead parent)
    NO_TRANSFORM,             // the entity has no local_transform
    DUPLICATE_GUID,           // a guid is already mapped to another entity
    GUID_NOT_FOUND,           // no entity carries the guid
    BAD_FORMAT,               // header "format" missing or wrong, or an entity without components
    UNSUPPORTED_VERSION,      // header "version" newer than SCENE_FORMAT_VERSION
    PREFAB_MULTIPLE_ROOTS,    // a prefab archive does not have exactly one root, at entry 0
    PREFAB_NOT_LOADED,        // the prefab asset of an instance is not resident
    NOT_A_PREFAB_INSTANCE,    // the entity carries no prefab_instance
    SCENE_NOT_LOADED,         // the scene_id names no loaded scene
    LOAD_FAILED,              // the scene asset FAILED while a request waited
    BUILDER_INVALID,          // prefab_builder without a single root, or an unknown node
    COUNT
};
```

Failure conditions reported by the scene module.

## Macros

### `CONJURE_DECLARE_ERROR_CATEGORY` {#symbol-conjure-declare-error-category}

```cpp
CONJURE_DECLARE_ERROR_CATEGORY(conjure::scene::scene_error, "scene", conjure::scene::scene_error_name)
```

## Functions

### Public

#### `scene_error_name` {#function-scene-error-name}

<div class="function-declaration" id="function-scene-error-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view scene_error_name(
    <a href="scene-error.html#symbol-scene-error">scene_error</a> code) <span class="k">noexcept</span>;</code></pre></div>

<p>The stable upper-case enumerator name of a code (&quot;<code>HIERARCHY_CYCLE</code>&quot;); &quot;&quot; for unknown values.</p>

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
<a id="scene_error"></a>
<a id="scene_error_name"></a>
<a id="scene_errorbad_format"></a>
<a id="scene_errorbuilder_invalid"></a>
<a id="scene_errorcount"></a>
<a id="scene_errorduplicate_guid"></a>
<a id="scene_errorguid_not_found"></a>
<a id="scene_errorhierarchy_cycle"></a>
<a id="scene_errorhierarchy_too_deep"></a>
<a id="scene_errorinvalid_entity"></a>
<a id="scene_errorinvalid_sibling_index"></a>
<a id="scene_errorload_failed"></a>
<a id="scene_errorno_transform"></a>
<a id="scene_errornone"></a>
<a id="scene_errornot_a_prefab_instance"></a>
<a id="scene_errorprefab_multiple_roots"></a>
<a id="scene_errorprefab_not_loaded"></a>
<a id="scene_errorscene_not_loaded"></a>
<a id="scene_errorself_parent"></a>
<a id="scene_errorunsupported_version"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
