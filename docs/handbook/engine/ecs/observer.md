---
kind: header
---

[Engine](../../engine.md) → [ECS](../ecs.md)

# observer.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/ecs/observer.h` |
| API ↗ | [Command buffers and observers](../../../api/ecs/command-buffers-and-observers.md) |

<p>Observers. Observers run synchronously on the owner thread inside the structural operation that triggered them and may only enqueue follow-up work into the deferred command buffer they receive; the world drains that buffer once the operation has finished.</p>

## Structs

| File | Description |
|---|---|
| [`observer_id`](observer/observer-id.md) | Handle of a registered observer. <code>A</code> default-constructed id refers to nothing. |

## Enumerations

### `component_event` {#symbol-component-event}

```cpp
enum class component_event : u8 {
    ADDED,
    REMOVED
};
```

Component lifecycle events observers can subscribe to. <code>ADDED</code> fires after the component data exists, <code>REMOVED</code> before it is destroyed.

## Type Aliases

### `component_observer_fn` {#symbol-component-observer-fn}

```cpp
using component_observer_fn = std::function<void(world &owner, entity target, component_id id, command_buffer &deferred)>;
```

Callback for a component event on one entity.

### `entity_observer_fn` {#symbol-entity-observer-fn}

```cpp
using entity_observer_fn = std::function<void(world &owner, entity target, command_buffer &deferred)>;
```

Callback invoked when an entity is about to be destroyed (its components still exist).

## Forward Declarations

### `command_buffer` {#symbol-command-buffer}

```cpp
class command_buffer;
```

### `world` {#symbol-world}

```cpp
class world;
```

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
<a id="command_buffer"></a>
<a id="component_event"></a>
<a id="component_eventadded"></a>
<a id="component_eventremoved"></a>
<a id="component_observer_fn"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="entity_observer_fn"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="observer_id"></a>
<a id="observer_idgeneration"></a>
<a id="observer_idindex"></a>
<a id="observer_idis_valid"></a>
<a id="observer_idoperator"></a>
<a id="other-declarations"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<a id="world"></a>
<!-- doc-legacy-anchors:end -->
