---
kind: header
---

[Engine](../../engine.md) → [ECS](../ecs.md)

# ecs_error.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/ecs/ecs_error.h` |
| API ↗ | [Entities, components and systems](../../../api/ecs/index.md) |

<p>Failure conditions reported by the ecs module. Contract violations (a structural call while the world is locked, a call from the wrong thread) assert in Debug and Development and still return their code so Distribution builds stay defined.</p>

## Enumerations

### `ecs_error` {#symbol-ecs-error}

```cpp
enum class ecs_error : u16 {
    NONE = 0,
    ENTITY_NOT_ALIVE,
    ENTITY_LIMIT_REACHED,
    COMPONENT_ALREADY_PRESENT,
    COMPONENT_NOT_PRESENT,
    UNKNOWN_COMPONENT,
    COMPONENT_LIMIT_REACHED,
    COMPONENT_IN_USE,
    COMPONENT_TOO_LARGE,
    BAD_ALIGNMENT,
    NAME_CONFLICT,
    UNSUPPORTED_FIELD,
    ARCHETYPE_TOO_LARGE,
    STRUCTURE_LOCKED,
    WRONG_THREAD,
    OUT_OF_MEMORY,
    ARCHIVE_CORRUPT,
    SERIALIZATION_FAILED,
    SYSTEM_CYCLE,
    SYSTEM_NOT_FOUND,
    COUNT
};
```

Failure conditions reported by the ecs module. Contract violations (a structural call while the world is locked, a call from the wrong thread) assert in Debug and Development and still return their code so Distribution builds stay defined.

## Macros

### `CONJURE_DECLARE_ERROR_CATEGORY` {#symbol-conjure-declare-error-category}

```cpp
CONJURE_DECLARE_ERROR_CATEGORY(conjure::ecs::ecs_error, "ecs", conjure::ecs::ecs_error_name)
```

## Functions

### Public

#### `ecs_error_name` {#function-ecs-error-name}

<div class="function-declaration" id="function-ecs-error-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view ecs_error_name(
    <a href="ecs-error.html#symbol-ecs-error">ecs_error</a> code) <span class="k">noexcept</span>;</code></pre></div>

<p>The stable upper-case enumerator name of a code (&quot;<code>ENTITY_NOT_ALIVE</code>&quot;); &quot;&quot; for unknown values.</p>

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
<a id="ecs_error"></a>
<a id="ecs_error_name"></a>
<a id="ecs_errorarchetype_too_large"></a>
<a id="ecs_errorarchive_corrupt"></a>
<a id="ecs_errorbad_alignment"></a>
<a id="ecs_errorcomponent_already_present"></a>
<a id="ecs_errorcomponent_in_use"></a>
<a id="ecs_errorcomponent_limit_reached"></a>
<a id="ecs_errorcomponent_not_present"></a>
<a id="ecs_errorcomponent_too_large"></a>
<a id="ecs_errorcount"></a>
<a id="ecs_errorentity_limit_reached"></a>
<a id="ecs_errorentity_not_alive"></a>
<a id="ecs_errorname_conflict"></a>
<a id="ecs_errornone"></a>
<a id="ecs_errorout_of_memory"></a>
<a id="ecs_errorserialization_failed"></a>
<a id="ecs_errorstructure_locked"></a>
<a id="ecs_errorsystem_cycle"></a>
<a id="ecs_errorsystem_not_found"></a>
<a id="ecs_errorunknown_component"></a>
<a id="ecs_errorunsupported_field"></a>
<a id="ecs_errorwrong_thread"></a>
<a id="extension-points"></a>
<a id="function-make-error"></a>
<a id="function-make-error-1"></a>
<a id="internals-and-algorithms"></a>
<a id="make_error"></a>
<a id="other-declarations"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
