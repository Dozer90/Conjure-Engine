---
kind: header
---

[Engine](../../engine.md) → [ECS](../ecs.md)

# world_hash.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/ecs/world_hash.h` |
| API ↗ | [Command buffers and observers](../../../api/ecs/command-buffers-and-observers.md) |

<p>Deterministic state hash.</p>

## Forward Declarations

### `world` {#symbol-world}

```cpp
class world;
```

## Functions

### Public

#### `world_hash` {#function-world-hash}

<div class="function-declaration" id="function-world-hash-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/types.html#symbol-u64">u64</a> world_hash(
    <span class="k">const</span> <a href="world/world.html">world</a> &amp;source);</code></pre></div>

<p>Hash of every alive entity and component value in the world. Equal worlds (same entities in the same row order, same bytes) hash equal regardless of archetype creation order; one flipped float bit changes it. Non-trivial components are serialised into one JSON writer reused for the whole call, so the writer allocates as it grows to the largest component, not once per component. An asset reference field of a described type takes part through its uuid alone, never through its bound slot, so a world hashes the same in every process and whether or not its references are bound yet. Reads the world without locking it: call it while no structural change is in flight.</p>

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
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<a id="world"></a>
<a id="world_hash"></a>
<!-- doc-legacy-anchors:end -->
