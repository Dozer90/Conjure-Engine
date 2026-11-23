---
kind: header
---

[Engine](../../engine.md) → [Bindings](../bindings.md)

# marshal.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/bindings/marshal.h` |
| API ↗ | [Scripting from C#: the bindings](../../../api/bindings/index.md), [The native table](../../../api/bindings/native-table.md) |

<p>Marshalling rules of the <code>C</code> ABI (B5): everything crosses blittable or as (pointer, length). Math records are memcpy-compatible with the core types (the <code>static_asserts</code> below pin that, including the x, y, z, w storage of <code><a href="../core/math/glm-types.html#symbol-quat">math::quat</a></code>); entities travel as the packed <code><a href="../core/types.html#symbol-u64">u64</a></code> of <code><a href="../ecs/entity/entity.html">ecs::entity</a></code>; uuids as two <code><a href="../core/types.html#symbol-u64">u64</a></code> halves; strings in as UTF-8 pointer + <code><a href="../core/types.html#symbol-byte">byte</a></code> length (never terminated); strings and arrays out follow the needed-length / count protocol.</p>

## Structs

| File | Description |
|---|---|
| [`marshal::structural_guard`](marshal/marshal-structural-guard.md) | Decides where a structural request goes: straight to the world while it is unlocked, into <code>script_runtime::deferred_commands()</code> while a DOD query scope holds the lock. Constructed per structural entry; <code>runtime</code> may be <code>null</code> (then a locked request has no buffer and reports <code>STRUCTURE_LOCKED</code> through <code>can_defer()</code>). |

## Functions

### Public

#### `marshal::from_entity` {#function-marshal-from-entity}

<div class="function-declaration" id="function-marshal-from-entity-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> uint64_t from_entity(<a href="../ecs/entity/entity.html">ecs::entity</a> value) <span class="k">noexcept</span>;</code></pre></div>

#### `marshal::from_mat4` {#function-marshal-from-mat4}

<div class="function-declaration" id="function-marshal-from-mat4-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">inline</span> <a href="../scripting/native-api/conjure-mat4.html">conjure_mat4</a> from_mat4(<span class="k">const</span> <a href="../core/math/glm-types.html#symbol-mat4">math::mat4</a> &amp;value) <span class="k">noexcept</span>;</code></pre></div>

#### `marshal::from_quat` {#function-marshal-from-quat}

<div class="function-declaration" id="function-marshal-from-quat-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">inline</span> <a href="../scripting/native-api/conjure-quat-xyzw.html">conjure_quat_xyzw</a> from_quat(<span class="k">const</span> <a href="../core/math/glm-types.html#symbol-quat">math::quat</a> &amp;value) <span class="k">noexcept</span>;</code></pre></div>

#### `marshal::from_transform` {#function-marshal-from-transform}

<div class="function-declaration" id="function-marshal-from-transform-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">inline</span> <a href="../scripting/native-api/conjure-transform.html">conjure_transform</a> from_transform(<span class="k">const</span> <a href="../core/math/transform/transform.html">math::transform</a> &amp;value) <span class="k">noexcept</span>;</code></pre></div>

#### `marshal::from_uuid` {#function-marshal-from-uuid}

<div class="function-declaration" id="function-marshal-from-uuid-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../scripting/native-api/conjure-uuid.html">conjure_uuid</a> from_uuid(<span class="k">const</span> <a href="../core/uuid/uuid.html">core::uuid</a> &amp;value) <span class="k">noexcept</span>;</code></pre></div>

#### `marshal::from_vec2` {#function-marshal-from-vec2}

<div class="function-declaration" id="function-marshal-from-vec2-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">inline</span> <a href="../scripting/native-api/conjure-vec2.html">conjure_vec2</a> from_vec2(<span class="k">const</span> <a href="../core/math/glm-types.html#symbol-vec2">math::vec2</a> &amp;value) <span class="k">noexcept</span>;</code></pre></div>

#### `marshal::from_vec3` {#function-marshal-from-vec3}

<div class="function-declaration" id="function-marshal-from-vec3-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">inline</span> <a href="../scripting/native-api/conjure-vec3.html">conjure_vec3</a> from_vec3(<span class="k">const</span> <a href="../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;value) <span class="k">noexcept</span>;</code></pre></div>

#### `marshal::from_vec4` {#function-marshal-from-vec4}

<div class="function-declaration" id="function-marshal-from-vec4-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">inline</span> <a href="../scripting/native-api/conjure-vec4.html">conjure_vec4</a> from_vec4(<span class="k">const</span> <a href="../core/math/glm-types.html#symbol-vec4">math::vec4</a> &amp;value) <span class="k">noexcept</span>;</code></pre></div>

#### `marshal::quat_storage_is_xyzw` {#function-marshal-quat-storage-is-xyzw}

<div class="function-declaration" id="function-marshal-quat-storage-is-xyzw-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">bool</span> quat_storage_is_xyzw() <span class="k">noexcept</span>;</code></pre></div>

<p>True when <code><a href="../core/math/glm-types.html#symbol-quat">math::quat</a></code> stores its components x, y, z, w in memory (glm 1.0.1 without <code>GLM_FORCE_QUAT_DATA_WXYZ</code>), which is what makes <code>to_transform</code> / <code>from_transform</code> a memcpy. glm&#x27;s quaternion is a union of anonymous structs on MSVC, so the check is a runtime <code><a href="../core/types.html#symbol-byte">byte</a></code> comparison rather than an offsetof.</p>

#### `marshal::read_utf8` {#function-marshal-read-utf8}

<div class="function-declaration" id="function-marshal-read-utf8-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;std::string_view&gt; read_utf8(<span class="k">const</span> <span class="k">char</span> *utf8, int32_t length) <span class="k">noexcept</span>;</code></pre></div>

<p><code>A</code> UTF-8 (pointer, <code><a href="../core/types.html#symbol-byte">byte</a></code> length) argument as a view. <code>INVALID_ARGUMENT</code> (bindings category) for a <code>null</code> pointer with a positive length or a negative length; (<code>nullptr</code>, 0) and (anything, 0) are &quot;&quot;.</p>

#### `marshal::rgba_to_color` {#function-marshal-rgba-to-color}

<div class="function-declaration" id="function-marshal-rgba-to-color-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../core/math/glm-types.html#symbol-vec4">math::vec4</a> rgba_to_color(uint32_t rgba) <span class="k">noexcept</span>;</code></pre></div>

<p>0xRRGGBBAA to a linear float colour with channels in [0, 1].</p>

#### `marshal::to_entity` {#function-marshal-to-entity}

<div class="function-declaration" id="function-marshal-to-entity-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../ecs/entity/entity.html">ecs::entity</a> to_entity(uint64_t packed) <span class="k">noexcept</span>;</code></pre></div>

#### `marshal::to_mat4` {#function-marshal-to-mat4}

<div class="function-declaration" id="function-marshal-to-mat4-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">inline</span> <a href="../core/math/glm-types.html#symbol-mat4">math::mat4</a> to_mat4(<span class="k">const</span> <a href="../scripting/native-api/conjure-mat4.html">conjure_mat4</a> &amp;value) <span class="k">noexcept</span>;</code></pre></div>

#### `marshal::to_quat` {#function-marshal-to-quat}

<div class="function-declaration" id="function-marshal-to-quat-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">inline</span> <a href="../core/math/glm-types.html#symbol-quat">math::quat</a> to_quat(<span class="k">const</span> <a href="../scripting/native-api/conjure-quat-xyzw.html">conjure_quat_xyzw</a> &amp;value) <span class="k">noexcept</span>;</code></pre></div>

<p>By-name conversion: <code><a href="../core/math/glm-types.html#symbol-quat">math::quat</a></code> is constructed (w, x, y, z) but stored x, y, z, w.</p>

#### `marshal::to_transform` {#function-marshal-to-transform}

<div class="function-declaration" id="function-marshal-to-transform-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">inline</span> <a href="../core/math/transform/transform.html">math::transform</a> to_transform(<span class="k">const</span> <a href="../scripting/native-api/conjure-transform.html">conjure_transform</a> &amp;value) <span class="k">noexcept</span>;</code></pre></div>

#### `marshal::to_uuid` {#function-marshal-to-uuid}

<div class="function-declaration" id="function-marshal-to-uuid-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../core/uuid/uuid.html">core::uuid</a> to_uuid(<span class="k">const</span> <a href="../scripting/native-api/conjure-uuid.html">conjure_uuid</a> &amp;value) <span class="k">noexcept</span>;</code></pre></div>

#### `marshal::to_vec2` {#function-marshal-to-vec2}

<div class="function-declaration" id="function-marshal-to-vec2-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">inline</span> <a href="../core/math/glm-types.html#symbol-vec2">math::vec2</a> to_vec2(<span class="k">const</span> <a href="../scripting/native-api/conjure-vec2.html">conjure_vec2</a> &amp;value) <span class="k">noexcept</span>;</code></pre></div>

#### `marshal::to_vec3` {#function-marshal-to-vec3}

<div class="function-declaration" id="function-marshal-to-vec3-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">inline</span> <a href="../core/math/glm-types.html#symbol-vec3">math::vec3</a> to_vec3(<span class="k">const</span> <a href="../scripting/native-api/conjure-vec3.html">conjure_vec3</a> &amp;value) <span class="k">noexcept</span>;</code></pre></div>

#### `marshal::to_vec4` {#function-marshal-to-vec4}

<div class="function-declaration" id="function-marshal-to-vec4-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">inline</span> <a href="../core/math/glm-types.html#symbol-vec4">math::vec4</a> to_vec4(<span class="k">const</span> <a href="../scripting/native-api/conjure-vec4.html">conjure_vec4</a> &amp;value) <span class="k">noexcept</span>;</code></pre></div>

#### `marshal::write_entities` {#function-marshal-write-entities}

<div class="function-declaration" id="function-marshal-write-entities-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> int32_t write_entities( std::span&lt;<span class="k">const</span> <a href="../ecs/entity/entity.html">ecs::entity</a>&gt; entities, uint64_t *out, int32_t capacity ) <span class="k">noexcept</span>;</code></pre></div>

<p>The count protocol of entity arrays: writes <code>min(entities.size(), capacity)</code> packed ids and returns the full count; capacity 0 (or a <code>null</code> buffer) is a count query.</p>

#### `marshal::write_utf8` {#function-marshal-write-utf8}

<div class="function-declaration" id="function-marshal-write-utf8-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> int32_t write_utf8(std::string_view text, <span class="k">char</span> *utf8, int32_t capacity) <span class="k">noexcept</span>;</code></pre></div>

<p>The needed-length protocol of string outputs: writes <code>min(text.size(), capacity)</code> bytes into <code>utf8</code> (no terminator) and returns the full length; capacity 0 (or a <code>null</code> buffer) is a length query, a negative capacity writes nothing.</p>

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
<a id="from_entity"></a>
<a id="from_mat4"></a>
<a id="from_quat"></a>
<a id="from_transform"></a>
<a id="from_uuid"></a>
<a id="from_vec2"></a>
<a id="from_vec3"></a>
<a id="from_vec4"></a>
<a id="internals-and-algorithms"></a>
<a id="marshalfrom_entity"></a>
<a id="marshalfrom_mat4"></a>
<a id="marshalfrom_quat"></a>
<a id="marshalfrom_transform"></a>
<a id="marshalfrom_uuid"></a>
<a id="marshalfrom_vec2"></a>
<a id="marshalfrom_vec3"></a>
<a id="marshalfrom_vec4"></a>
<a id="marshalquat_storage_is_xyzw"></a>
<a id="marshalread_utf8"></a>
<a id="marshalrgba_to_color"></a>
<a id="marshalstructural_guard"></a>
<a id="marshalstructural_guardcan_defer"></a>
<a id="marshalstructural_guardcommands"></a>
<a id="marshalstructural_guarddeferred"></a>
<a id="marshalstructural_guardruntime"></a>
<a id="marshalstructural_guardworld"></a>
<a id="marshalto_entity"></a>
<a id="marshalto_mat4"></a>
<a id="marshalto_quat"></a>
<a id="marshalto_transform"></a>
<a id="marshalto_uuid"></a>
<a id="marshalto_vec2"></a>
<a id="marshalto_vec3"></a>
<a id="marshalto_vec4"></a>
<a id="marshalwrite_entities"></a>
<a id="marshalwrite_utf8"></a>
<a id="quat_storage_is_xyzw"></a>
<a id="read_utf8"></a>
<a id="rgba_to_color"></a>
<a id="structs-and-types"></a>
<a id="structural_guard"></a>
<a id="structural_guardcan_defer"></a>
<a id="structural_guardcommands"></a>
<a id="structural_guarddeferred"></a>
<a id="structural_guardruntime"></a>
<a id="structural_guardworld"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_entity"></a>
<a id="to_mat4"></a>
<a id="to_quat"></a>
<a id="to_transform"></a>
<a id="to_uuid"></a>
<a id="to_vec2"></a>
<a id="to_vec3"></a>
<a id="to_vec4"></a>
<a id="variables-and-properties"></a>
<a id="write_entities"></a>
<a id="write_utf8"></a>
<!-- doc-legacy-anchors:end -->
