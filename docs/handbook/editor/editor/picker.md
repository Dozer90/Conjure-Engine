---
kind: header
---

[Editor](../../editor.md) → [User Interface](../user-interface.md)

# picker.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/picker.h` |
| API ↗ | [Headless mode and the command model](../../../api/editor/headless-and-commands.md), [The editor](../../../api/editor/index.md) |

<p>CPU picking: a slab test over every candidate&#x27;s world bounds sorted by entry distance, then, for mode <code>MESH</code>, the triangles of the mesh asset&#x27;s CPU geometry (sprites test their quad) until a hit is closer than the next candidate&#x27;s entry distance. Ties between bounds-only hits go to the smaller volume so a child inside its parent stays selectable. Mode <code>PHYSICS</code> delegates to Jolt.</p>

## Structs

| File | Description |
|---|---|
| [`pick_candidate`](picker/pick-candidate.md) |  |
| [`pick_context`](picker/pick-context.md) | What the narrow phase needs: the scene (components and transforms) and, for mode <code>PHYSICS</code>, the physics world. Jobs parallelise the broad phase above 4096 candidates when given. |
| [`pick_result`](picker/pick-result.md) |  |

## Enumerations

### `pick_mode` {#symbol-pick-mode}

```cpp
enum class pick_mode : u8 {
    MESH = 0,
    BOUNDS,
    PHYSICS
};
```

## Forward Declarations

### `jobs::job_system` {#symbol-jobs-job-system}

```cpp
class job_system;
```

### `physics_world` {#symbol-physics-world}

```cpp
class physics_world;
```

## Functions

### Public

#### `gather_pick_candidates` {#function-gather-pick-candidates}

<div class="function-declaration" id="function-gather-pick-candidates-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::vector&lt;<a href="picker/pick-candidate.html">pick_candidate</a>&gt; gather_pick_candidates(
    <span class="k">const</span> scene::scene &amp;scene);</code></pre></div>

<p>Every active entity (<code>scene::is_active</code>: it and every ancestor enabled) with renderable or framing bounds: mesh renderers and sprites use <code>scene::world_bounds</code> (their <code><a href="../../engine/scene/components/local-bounds.html">local_bounds</a></code>), lights and cameras a fixed 0.5 m box around their position. Entities without a transform are skipped.</p>

#### `parse_pick_mode` {#function-parse-pick-mode}

<div class="function-declaration" id="function-parse-pick-mode-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::optional&lt;<a href="picker.html#symbol-pick-mode">pick_mode</a>&gt; parse_pick_mode(
    std::string_view text) <span class="k">noexcept</span>;</code></pre></div>

<p>Parses a wire name; nullopt for anything else.</p>

#### `pick` {#function-pick}

<div class="function-declaration" id="function-pick-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="picker/pick-result.html">pick_result</a> pick(
    <span class="k">const</span> <a href="../../engine/core/math/ray/ray.html">math::ray</a> &amp;ray,
    std::span&lt;<span class="k">const</span> <a href="picker/pick-candidate.html">pick_candidate</a>&gt; candidates,
    <a href="picker.html#symbol-pick-mode">pick_mode</a> mode,
    <span class="k">const</span> <a href="picker/pick-context.html">pick_context</a> &amp;context = {});</code></pre></div>

<p>Picks among the candidates (see the header comment). With mode <code>PHYSICS</code> the candidates are ignored and <code>context.physics</code> is queried (a miss when it is <code>null</code>).</p>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="picker.html#symbol-pick-mode">pick_mode</a> mode) <span class="k">noexcept</span>;</code></pre></div>

<p>The wire name of a mode (&quot;mesh&quot;).</p>

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
<a id="gather_pick_candidates"></a>
<a id="internals-and-algorithms"></a>
<a id="job_system"></a>
<a id="jobsjob_system"></a>
<a id="other-declarations"></a>
<a id="parse_pick_mode"></a>
<a id="physics_world"></a>
<a id="pick"></a>
<a id="pick_candidate"></a>
<a id="pick_candidateentity"></a>
<a id="pick_candidateguid"></a>
<a id="pick_candidateworldbounds"></a>
<a id="pick_context"></a>
<a id="pick_contextjobs"></a>
<a id="pick_contextmaxdistance"></a>
<a id="pick_contextphysics"></a>
<a id="pick_contextscene"></a>
<a id="pick_mode"></a>
<a id="pick_modebounds"></a>
<a id="pick_modemesh"></a>
<a id="pick_modephysics"></a>
<a id="pick_result"></a>
<a id="pick_resultdistance"></a>
<a id="pick_resultguid"></a>
<a id="pick_resulthit"></a>
<a id="pick_resultnormal"></a>
<a id="pick_resultpoint"></a>
<a id="pick_resultto_json"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
