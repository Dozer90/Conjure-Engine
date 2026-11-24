---
kind: header
---

[Engine](../../engine.md) → [Scripting](../scripting.md)

# native_api.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/scripting/native_api.h` |
| API ↗ | [Hosting, the native API and hot reload](../../../api/scripting/hosting-and-hot-reload.md), [Scripting](../../../api/scripting/index.md) |

<p>The <code>C</code> ABI shared with <code>Conjure.Managed</code>.</p>

## Structs

| File | Description |
|---|---|
| [`conjure_asset_info`](native-api/conjure-asset-info.md) |  |
| [`conjure_chunk_view`](native-api/conjure-chunk-view.md) | Columns of one chunk inside a DOD query scope, valid until <code>query_end</code>. |
| [`conjure_collision_record`](native-api/conjure-collision-record.md) |  |
| [`conjure_create_request`](native-api/conjure-create-request.md) | One instance to create; filled by native, <code>out_slot</code> written by managed. |
| [`conjure_frame_block`](native-api/conjure-frame-block.md) | Written once per frame by the engine before <code>FRAME_BEGIN</code> and read directly by Time, Input and Screen in <code>C</code>#. The screen fields are <code>engine::game_screen_of(window)</code> (conjure/engine/<code><a href="../engine/game-screen/game-screen.html">game_screen</a>.h</code>): the framebuffer in pixels (0 x 0 while minimised or without a window), the framebuffer / logical ratio, the OS UI scale of the window&#x27;s monitor, and <code>window_state</code>, a set of the <code>FRAME_WINDOW_</code>* bits of <code>conjure::scripting</code>. |
| [`conjure_frame_stats_block`](native-api/conjure-frame-stats-block.md) | The last committed <code><a href="../engine/frame-stats/frame-sample.html">engine::frame_sample</a></code>. The <code>managed_</code>* fields are its managed-heap figures (the main thread&#x27;s allocations, the heap size and the collections of that frame), 0 in a frame without a ready script host. |
| [`conjure_gamepad_state`](native-api/conjure-gamepad-state.md) |  |
| [`conjure_input_block`](native-api/conjure-input-block.md) |  |
| [`conjure_managed_api`](native-api/conjure-managed-api.md) | Functions managed provides to native, filled by <code>Conjure.Interop.NativeBridge.Initialize</code>. <code>sample_memory</code> fills a <code><a href="native-api/conjure-managed-memory.html">conjure_managed_memory</a></code> and returns 0 (a negative status when it cannot). <code>set_active(slots, count, active)</code> tells the instances in <code>slots</code> that their entity became active (1) or inactive (0) and returns how many of them changed state; a free slot is skipped. Not yet carried out end to end: <code><a href="script-runtime/script-runtime.html">script_runtime</a></code> never calls <code>set_active</code>, and <code>Conjure.Managed</code> accepts the call and returns 0 without tracking activity; the <code>null</code> host tracks it for tests. |
| [`conjure_managed_component_desc`](native-api/conjure-managed-component-desc.md) | An [EcsComponent] struct: <code>schema_json</code> lists {&quot;fields&quot;:[{&quot;name&quot;,&quot;type&quot;,&quot;offset&quot;}]} with <code>core::reflect</code> type names. |
| [`conjure_managed_memory`](native-api/conjure-managed-memory.md) | What <code>conjure_managed_api::sample_memory</code> reports: <code>thread_allocated_bytes</code> is <code>GC.GetAllocatedBytesForCurrentThread()</code> on the calling (main) thread, <code>heap_bytes</code> <code>GC.GetTotalMemory(false)</code>, and the collections <code>GC.CollectionCount(0)</code> and <code>GC.CollectionCount(2)</code>. Everything but <code>heap_bytes</code> is cumulative over the life of the managed runtime. |
| [`conjure_mat4`](native-api/conjure-mat4.md) | Column-major 4x4 matrix. |
| [`conjure_module_record`](native-api/conjure-module-record.md) |  |
| [`conjure_native_api`](native-api/conjure-native-api.md) | Functions native provides to managed. Every entry defaults to <code>nullptr</code>; the handshake validates the header, and managed throws NotSupportedException for an entry nobody filled. |
| [`conjure_quat_xyzw`](native-api/conjure-quat-xyzw.md) | <code>A</code> quaternion in explicit x, y, z, w storage order (converted from <code><a href="../core/math/glm-types.html#symbol-quat">math::quat</a></code> by name, /). |
| [`conjure_query_scope`](native-api/conjure-query-scope.md) |  |
| [`conjure_ray_query`](native-api/conjure-ray-query.md) | <code>A</code> ray query; flags is a set of <code><a href="native-api.html#symbol-conjure-query-flags">conjure_query_flags</a></code>, and a bit outside it is refused with <code>INVALID_ARGUMENT</code>. |
| [`conjure_raycast_hit`](native-api/conjure-raycast-hit.md) | One query hit; <code>is_trigger</code> is 1 when the body hit is a trigger volume (hit only with <code>INCLUDE_TRIGGERS</code>). |
| [`conjure_script_class_desc`](native-api/conjure-script-class-desc.md) |  |
| [`conjure_script_field_desc`](native-api/conjure-script-field-desc.md) | One serialised field of a script class. <code>meta_json</code> is {&quot;tooltip&quot;,&quot;min&quot;,&quot;max&quot;,&quot;enumNames&quot;,&quot;enumValues&quot;, &quot;enumFlags&quot;,&quot;assetType&quot;,&quot;hidden&quot;}, each key present only when it applies: enumValues runs parallel to enumNames with the int32 the <code>ENUM</code> payload carries for each name, enumFlags is <code>true</code> for a [Flags] enum, and assetType is the registered asset type name of the AssetRef tag (&quot;<code>conjure.audio.clip</code>&quot;), absent for an untyped reference. <code>default_json</code> is the canonical JSON of the default value. |
| [`conjure_shape_query`](native-api/conjure-shape-query.md) |  |
| [`conjure_spawn_component`](native-api/conjure-spawn-component.md) | One component of an <code>entity_spawn</code> request: <code><a href="../core/reflect/type-id/reflect-type-id.html">type_id</a></code> is the id <code>component_lookup_type</code> or <code>component_register_managed</code> returned, stride the bytes between two entities&#x27; values (0: <code>data</code> is one value for every entity) and data the values (<code>nullptr</code>: the registered default value). |
| [`conjure_spawn_desc`](native-api/conjure-spawn-desc.md) | An <code>entity_spawn</code> request: <code>count</code> entities (1..65536) holding the <code>component_count</code> components (0..8) each. parent is read with <code>SCENE_ENTITIES</code> only (0: scene roots); flags is a set of <code><a href="native-api.html#symbol-conjure-spawn-flags">conjure_spawn_flags</a></code>. |
| [`conjure_transform`](native-api/conjure-transform.md) |  |
| [`conjure_uuid`](native-api/conjure-uuid.md) |  |
| [`conjure_vec2`](native-api/conjure-vec2.md) |  |
| [`conjure_vec3`](native-api/conjure-vec3.md) |  |
| [`conjure_vec4`](native-api/conjure-vec4.md) |  |
| [`native_entry_descriptor`](native-api/native-entry-descriptor.md) | One row of the table as text, as the generator normalised it: signature is &quot;&lt;return&gt; &lt;arguments&gt;&quot; with single spaces and &#x27;*&#x27; hugging the parameter name. |

## Enumerations

### `collision_kind` {#symbol-collision-kind}

```cpp
enum class collision_kind : uint8_t {
    COLLISION_ENTER,
    COLLISION_EXIT,
    TRIGGER_ENTER,
    TRIGGER_EXIT,
    COUNT
};
```

<code>conjure_collision_record::kind</code>.

### `conjure_asset_load_priority` {#symbol-conjure-asset-load-priority}

```cpp
enum class conjure_asset_load_priority : uint8_t {
    LOW,
    NORMAL,
    HIGH
};
```

### `conjure_button_action` {#symbol-conjure-button-action}

```cpp
enum class conjure_button_action : uint8_t {
    RELEASE,
    PRESS,
    REPEAT
};
```

### `conjure_cast_shape` {#symbol-conjure-cast-shape}

```cpp
enum class conjure_cast_shape : uint8_t {
    SPHERE,
    BOX,
    CAPSULE
};
```

### `conjure_cursor_mode` {#symbol-conjure-cursor-mode}

```cpp
enum class conjure_cursor_mode : uint8_t {
    NORMAL,
    HIDDEN,
    DISABLED,
    CAPTURED
};
```

### `conjure_force_mode` {#symbol-conjure-force-mode}

```cpp
enum class conjure_force_mode : uint8_t {
    FORCE,
    IMPULSE,
    VELOCITY_CHANGE,
    ACCELERATION
};
```

### `conjure_query_flags` {#symbol-conjure-query-flags}

```cpp
enum class conjure_query_flags : uint32_t {
    NONE = 0,
    INCLUDE_TRIGGERS = 1 << 0,
    EXCLUDE_STATIC = 1 << 1
};
```

<code>conjure_ray_query::flags</code> and the flags of the overlap entries, as bits: <code>NONE</code> (0) skips trigger volumes and hits static bodies; <code>INCLUDE_TRIGGERS</code> (1) also hits trigger volumes, <code>EXCLUDE_STATIC</code> (2) skips static bodies. The values are ABI, pinned by the <code>static_asserts</code> of the layout pins below; a new bit is appended with an explicit value of its own.

### `conjure_spawn_flags` {#symbol-conjure-spawn-flags}

```cpp
enum class conjure_spawn_flags : uint32_t {
    NONE = 0,
    SCENE_ENTITIES = 1 << 0
};
```

<code>conjure_spawn_desc::flags</code>, as bits: <code>NONE</code> (0) makes bare entities, as <code>entity_create</code> does; <code>SCENE_ENTITIES</code> (1) makes scene entities under <code>conjure_spawn_desc::parent</code>, as <code>entity_create_many</code> does. The values are ABI, pinned by the <code>static_asserts</code> of the layout pins below; a new bit is appended with an explicit value of its own.

### `script_callback` {#symbol-script-callback}

```cpp
enum class script_callback : uint8_t {
    CREATE,
    START,
    FIXED_UPDATE,
    UPDATE,
    LATE_UPDATE,
    DESTROY,
    COLLISION_ENTER,
    COLLISION_EXIT,
    TRIGGER_ENTER,
    TRIGGER_EXIT,
    RELOAD,
    COUNT
};
```

Bit index of <code>conjure_script_class_desc::callback_mask</code> and index of the fault counters. <code>RELOAD</code> (OnReload, which runs instead of OnCreate on an instance a hot reload recreated) comes last, so the older callbacks keep their bits.

### `script_field_kind` {#symbol-script-field-kind}

```cpp
enum class script_field_kind : uint8_t {
    BOOL,
    INT32,
    INT64,
    FLOAT,
    DOUBLE,
    STRING,
    VEC2,
    VEC3,
    VEC4,
    QUAT,
    COLOR,
    ENTITY,
    ASSET,
    ENUM,
    LIST,
    COUNT
};
```

Kind of a serialised script field (<code>field_blob</code> payload and inspector schema).

### `script_phase` {#symbol-script-phase}

```cpp
enum class script_phase : uint8_t {
    FIXED_UPDATE,
    UPDATE,
    LATE_UPDATE,
    COUNT
};
```

The three callback phases <code>run_phase</code> takes.

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-CREATE_FLAG_ALREADY_CREATED"></a><code>CREATE_FLAG_ALREADY_CREATED</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> uint8_t</code> | <code class="signature"><span class="mi">2</span></code> | <code>conjure_create_request::flags</code> bit: the entry&#x27;s OnCreate already returned in this entity&#x27;s lifetime, so the recreated instance runs OnReload instead of OnCreate, after its fields were applied (hot reload, and a snapshot restore or set replace from JSON whose entry says &quot;created&quot;). |
| <a id="variable-CREATE_FLAG_ALREADY_STARTED"></a><code>CREATE_FLAG_ALREADY_STARTED</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> uint8_t</code> | <code class="signature"><span class="mi">1</span></code> | <code>conjure_create_request::flags</code> bit: the entry already ran OnStart (reload, snapshot restore). |
| <a id="variable-CREATE_FLAG_INACTIVE"></a><code>CREATE_FLAG_INACTIVE</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> uint8_t</code> | <code class="signature"><span class="mi">4</span></code> | <code>conjure_create_request::flags</code> bit: the entity is inactive (it or an ancestor is disabled) when the instance is created, so the instance starts inactive; <code>conjure_managed_api::set_active</code> reports later changes. Only the <code>null</code> host honours it so far: <code><a href="script-runtime/script-runtime.html">script_runtime</a></code> never sets it, and <code>Conjure.Managed</code> ignores it. |
| <a id="variable-FRAME_WINDOW_FOCUSED"></a><code>FRAME_WINDOW_FOCUSED</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> uint8_t</code> | <code class="signature"><span class="mi">1</span></code> | <code>conjure_frame_block::window_state</code> bit: the window has the input focus. |
| <a id="variable-FRAME_WINDOW_FULLSCREEN"></a><code>FRAME_WINDOW_FULLSCREEN</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> uint8_t</code> | <code class="signature"><span class="mi">4</span></code> | <code>conjure_frame_block::window_state</code> bit: the window is (borderless) fullscreen. |
| <a id="variable-FRAME_WINDOW_MINIMISED"></a><code>FRAME_WINDOW_MINIMISED</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> uint8_t</code> | <code class="signature"><span class="mi">2</span></code> | <code>conjure_frame_block::window_state</code> bit: the window is minimised (the screen is then 0 x 0). |
| <a id="variable-NATIVE_API_CORE_ENTRY_COUNT"></a><code>NATIVE_API_CORE_ENTRY_COUNT</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="na">CONJURE_<wbr>NATIVE_<wbr>API_<wbr>CORE_<wbr>ENTRY_<wbr>COUNT</span></code> | Entries this module implements (everything before the bindings marker in <code>native_api.inl</code>). |
| <a id="variable-NATIVE_API_ENTRY_COUNT"></a><code>NATIVE_API_ENTRY_COUNT</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="na">CONJURE_<wbr>NATIVE_<wbr>API_<wbr>ENTRY_<wbr>COUNT</span></code> | Entries in the table, core and bindings together. |
| <a id="variable-QUERY_MAX_COMPONENTS"></a><code>QUERY_MAX_COMPONENTS</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> int32_t</code> | <code class="signature"><span class="mi">8</span></code> | Column slots of a <code><a href="native-api/conjure-chunk-view.html">conjure_chunk_view</a></code>, and the most component ids a <code>query_begin</code> exclusion list holds. |

## Macros

### `CONJURE_ENUM_FLAGS` {#symbol-conjure-enum-flags}

```cpp
CONJURE_ENUM_FLAGS(conjure_query_flags)
```

## Functions

### Public

#### `callback_bit` {#function-callback-bit}

<div class="function-declaration" id="function-callback-bit-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> uint16_t callback_bit(
    <a href="native-api.html#symbol-script-callback">script_callback</a> callback) <span class="k">noexcept</span>;</code></pre></div>

<p>The <code>callback_mask</code> bit of a callback.</p>

#### `compute_native_api_hash` {#function-compute-native-api-hash}

<div class="function-declaration" id="function-compute-native-api-hash-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/types.html#symbol-u64">u64</a> compute_native_api_hash() <span class="k">noexcept</span>;</code></pre></div>

<p>FNV-1a 64 over &quot;&lt;group&gt;<code>_&lt;name&gt;</code> &lt;signature&gt;\n&quot; per descriptor, then over &quot;&lt;line&gt;\n&quot; per line of <code>CONJURE_NATIVE_API_FOR_EACH_RECORD_LINE</code> (the <code>conjure_</code>* records, enumerations and table members of this header): the generator&#x27;s algorithm run at runtime over the text it generated. It equals <code>CONJURE_NATIVE_API_HASH</code>, which proves both implementations agree; whether the generated text still matches this header is checked by tools/<code>generate_native_api.py</code> --check, not here.</p>

#### `find_native_entry` {#function-find-native-entry}

<div class="function-declaration" id="function-find-native-entry-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/types.html#symbol-i32">i32</a> find_native_entry(
    std::string_view fullName) <span class="k">noexcept</span>;</code></pre></div>

<p>Index of the entry named &quot;&lt;group&gt;<code>_&lt;name&gt;</code>&quot;, or -1.</p>

#### `make_native_api` {#function-make-native-api}

<div class="function-declaration" id="function-make-native-api-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="native-api/conjure-native-api.html">conjure_native_api</a> make_native_api(
    <span class="k">const</span> <a href="native-api/conjure-frame-block.html">conjure_frame_block</a> *frame) <span class="k">noexcept</span>;</code></pre></div>

<p><code>A</code> fresh table: header filled, every entry <code>nullptr</code>, frame pointing at <code>frame</code> (may be <code>null</code>).</p>

#### `native_api_slots` {#function-native-api-slots}

<div class="function-declaration" id="function-native-api-slots-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::span&lt;<span class="k">void</span> *<span class="k">const</span>&gt; native_api_slots(
    <span class="k">const</span> <a href="native-api/conjure-native-api.html">conjure_native_api</a> &amp;api) <span class="k">noexcept</span>;</code></pre></div>

<p>The function pointer slots of a table as an array of void pointers (index = descriptor index).</p>

<div class="function-declaration" id="function-native-api-slots-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::span&lt;<span class="k">void</span> *&gt; native_api_slots(
    <a href="native-api/conjure-native-api.html">conjure_native_api</a> &amp;api) <span class="k">noexcept</span>;</code></pre></div>

<p>Mutable form of <code>native_api_slots</code> for fillers that address entries by index.</p>

#### `native_entry_descriptors` {#function-native-entry-descriptors}

<div class="function-declaration" id="function-native-entry-descriptors-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::span&lt;<span class="k">const</span> <a href="native-api/native-entry-descriptor.html">native_entry_descriptor</a>&gt; native_entry_descriptors() <span class="k">noexcept</span>;</code></pre></div>

<p>Every entry of <code>native_api.inl</code> in table order; the bindings entries start at <code>NATIVE_API_CORE_ENTRY_COUNT</code>.</p>

#### `parse_script_callback` {#function-parse-script-callback}

<div class="function-declaration" id="function-parse-script-callback-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="native-api.html#symbol-script-callback">script_callback</a> parse_script_callback(
    std::string_view name) <span class="k">noexcept</span>;</code></pre></div>

<p>Parses a name produced by <code><a href="#function-to-string-2">to_string</a>(<a href="native-api.html#symbol-script-callback">script_callback</a>)</code>. <code>COUNT</code> when unknown.</p>

#### `parse_script_field_kind` {#function-parse-script-field-kind}

<div class="function-declaration" id="function-parse-script-field-kind-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="native-api.html#symbol-script-field-kind">script_field_kind</a> parse_script_field_kind(
    std::string_view name) <span class="k">noexcept</span>;</code></pre></div>

<p>Parses a name produced by <code><a href="#function-to-string-1">to_string</a>(<a href="native-api.html#symbol-script-field-kind">script_field_kind</a>)</code>. <code>COUNT</code> when unknown.</p>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="native-api.html#symbol-script-field-kind">script_field_kind</a> kind) <span class="k">noexcept</span>;</code></pre></div>

<p>The stable upper-case name of a field kind (&quot;<code>VEC3</code>&quot;); &quot;<code>COUNT</code>&quot; for out-of-range values.</p>

<div class="function-declaration" id="function-to-string-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="native-api.html#symbol-script-callback">script_callback</a> callback) <span class="k">noexcept</span>;</code></pre></div>

<p>The upper-case name of a callback (&quot;<code>UPDATE</code>&quot;); &quot;<code>COUNT</code>&quot; for out-of-range values.</p>

<div class="function-declaration" id="function-to-string-3"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="native-api.html#symbol-script-phase">script_phase</a> phase) <span class="k">noexcept</span>;</code></pre></div>

<p>The upper-case name of a phase (&quot;<code>FIXED_UPDATE</code>&quot;); &quot;<code>COUNT</code>&quot; for out-of-range values.</p>

#### `validate_managed_api` {#function-validate-managed-api}

<div class="function-declaration" id="function-validate-managed-api-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; validate_managed_api(
    <span class="k">const</span> <a href="native-api/conjure-managed-api.html">conjure_managed_api</a> &amp;api);</code></pre></div>

<p>Checks a managed table a host received: size, version, hash and that every function is set.</p>

#### `validate_native_api` {#function-validate-native-api}

<div class="function-declaration" id="function-validate-native-api-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; validate_native_api(
    <span class="k">const</span> <a href="native-api/conjure-native-api.html">conjure_native_api</a> &amp;api);</code></pre></div>

<p>Checks the header (size, version, hash) and that every core entry is filled. <code>ABI_MISMATCH</code> or <code>NOT_INITIALISED</code> naming the first missing entry.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="callback_bit"></a>
<a id="classes"></a>
<a id="collision_kind"></a>
<a id="collision_kindcollision_enter"></a>
<a id="collision_kindcollision_exit"></a>
<a id="collision_kindcount"></a>
<a id="collision_kindtrigger_enter"></a>
<a id="collision_kindtrigger_exit"></a>
<a id="compute_native_api_hash"></a>
<a id="conjure_asset_info"></a>
<a id="conjure_asset_infopad"></a>
<a id="conjure_asset_infostate"></a>
<a id="conjure_asset_infotype_id"></a>
<a id="conjure_asset_infoversion"></a>
<a id="conjure_asset_load_priority"></a>
<a id="conjure_asset_load_priorityhigh"></a>
<a id="conjure_asset_load_prioritylow"></a>
<a id="conjure_asset_load_prioritynormal"></a>
<a id="conjure_button_action"></a>
<a id="conjure_button_actionpress"></a>
<a id="conjure_button_actionrelease"></a>
<a id="conjure_button_actionrepeat"></a>
<a id="conjure_cast_shape"></a>
<a id="conjure_cast_shapebox"></a>
<a id="conjure_cast_shapecapsule"></a>
<a id="conjure_cast_shapesphere"></a>
<a id="conjure_chunk_view"></a>
<a id="conjure_chunk_viewcolumns"></a>
<a id="conjure_chunk_viewcount"></a>
<a id="conjure_chunk_viewentities"></a>
<a id="conjure_collision_record"></a>
<a id="conjure_collision_recordimpulse"></a>
<a id="conjure_collision_recordkind"></a>
<a id="conjure_collision_recordnormal"></a>
<a id="conjure_collision_recordother"></a>
<a id="conjure_collision_recordpoint"></a>
<a id="conjure_collision_recordslot"></a>
<a id="conjure_create_request"></a>
<a id="conjure_create_requestclass_hash"></a>
<a id="conjure_create_requestentity"></a>
<a id="conjure_create_requestfields"></a>
<a id="conjure_create_requestfields_size"></a>
<a id="conjure_create_requestflags"></a>
<a id="conjure_create_requestout_slot"></a>
<a id="conjure_cursor_mode"></a>
<a id="conjure_cursor_modecaptured"></a>
<a id="conjure_cursor_modedisabled"></a>
<a id="conjure_cursor_modehidden"></a>
<a id="conjure_cursor_modenormal"></a>
<a id="conjure_force_mode"></a>
<a id="conjure_force_modeacceleration"></a>
<a id="conjure_force_modeforce"></a>
<a id="conjure_force_modeimpulse"></a>
<a id="conjure_force_modevelocity_change"></a>
<a id="conjure_frame_block"></a>
<a id="conjure_frame_blockcurrent_phase"></a>
<a id="conjure_frame_blockdelta_time"></a>
<a id="conjure_frame_blockfixed_delta_time"></a>
<a id="conjure_frame_blockfixed_step_index"></a>
<a id="conjure_frame_blockframe_index"></a>
<a id="conjure_frame_blockin_play_mode"></a>
<a id="conjure_frame_blockinput"></a>
<a id="conjure_frame_blockpadding"></a>
<a id="conjure_frame_blocktime_scale"></a>
<a id="conjure_frame_blocktime_seconds"></a>
<a id="conjure_frame_blockunscaled_time_seconds"></a>
<a id="conjure_frame_stats_block"></a>
<a id="conjure_frame_stats_blockfixed_steps"></a>
<a id="conjure_frame_stats_blockframe_memory_high_water"></a>
<a id="conjure_frame_stats_blockframe_ms"></a>
<a id="conjure_frame_stats_blockjobs_executed"></a>
<a id="conjure_frame_stats_blockphase_ms"></a>
<a id="conjure_frame_stats_blocksubmit_ms"></a>
<a id="conjure_gamepad_state"></a>
<a id="conjure_gamepad_stateaxes"></a>
<a id="conjure_gamepad_statebuttons_down"></a>
<a id="conjure_gamepad_statebuttons_pressed"></a>
<a id="conjure_gamepad_statebuttons_released"></a>
<a id="conjure_gamepad_stateconnected"></a>
<a id="conjure_input_block"></a>
<a id="conjure_input_blockgamepads"></a>
<a id="conjure_input_blockkeys_down"></a>
<a id="conjure_input_blockkeys_pressed"></a>
<a id="conjure_input_blockkeys_released"></a>
<a id="conjure_input_blockmouse_buttons_down"></a>
<a id="conjure_input_blockmouse_buttons_pressed"></a>
<a id="conjure_input_blockmouse_buttons_released"></a>
<a id="conjure_input_blockmouse_dx"></a>
<a id="conjure_input_blockmouse_dy"></a>
<a id="conjure_input_blockmouse_x"></a>
<a id="conjure_input_blockmouse_y"></a>
<a id="conjure_input_blockscroll_x"></a>
<a id="conjure_input_blockscroll_y"></a>
<a id="conjure_managed_api"></a>
<a id="conjure_managed_apiapply_fields"></a>
<a id="conjure_managed_apicollect_fields"></a>
<a id="conjure_managed_apicollect_garbage"></a>
<a id="conjure_managed_apicreate_instances"></a>
<a id="conjure_managed_apidestroy_instances"></a>
<a id="conjure_managed_apidispatch_collisions"></a>
<a id="conjure_managed_apilayout_hash"></a>
<a id="conjure_managed_apiload_game_assembly"></a>
<a id="conjure_managed_apirun_phase"></a>
<a id="conjure_managed_apishutdown"></a>
<a id="conjure_managed_apisize"></a>
<a id="conjure_managed_apiunload_game_assembly"></a>
<a id="conjure_managed_apiversion"></a>
<a id="conjure_managed_component_desc"></a>
<a id="conjure_managed_component_descalignment"></a>
<a id="conjure_managed_component_descfull_name"></a>
<a id="conjure_managed_component_desclayout_hash"></a>
<a id="conjure_managed_component_descname_hash"></a>
<a id="conjure_managed_component_descname_length"></a>
<a id="conjure_managed_component_descschema_json"></a>
<a id="conjure_managed_component_descschema_length"></a>
<a id="conjure_managed_component_descsize"></a>
<a id="conjure_mat4"></a>
<a id="conjure_mat4m"></a>
<a id="conjure_module_record"></a>
<a id="conjure_module_recordfeature_flags"></a>
<a id="conjure_module_recordkind"></a>
<a id="conjure_module_recordpad"></a>
<a id="conjure_module_recordstate"></a>
<a id="conjure_module_recordversion_major"></a>
<a id="conjure_module_recordversion_minor"></a>
<a id="conjure_module_recordversion_patch"></a>
<a id="conjure_native_api"></a>
<a id="conjure_native_apiframe"></a>
<a id="conjure_native_apilayout_hash"></a>
<a id="conjure_native_apisize"></a>
<a id="conjure_native_apiversion"></a>
<a id="conjure_quat_xyzw"></a>
<a id="conjure_quat_xyzww"></a>
<a id="conjure_quat_xyzwx"></a>
<a id="conjure_quat_xyzwy"></a>
<a id="conjure_quat_xyzwz"></a>
<a id="conjure_query_scope"></a>
<a id="conjure_query_scopechunk_count"></a>
<a id="conjure_query_scopechunks"></a>
<a id="conjure_query_scopescope_id"></a>
<a id="conjure_ray_query"></a>
<a id="conjure_ray_querydirection"></a>
<a id="conjure_ray_queryignore_entity"></a>
<a id="conjure_ray_querylayer_mask"></a>
<a id="conjure_ray_querymax_distance"></a>
<a id="conjure_ray_queryorigin"></a>
<a id="conjure_raycast_hit"></a>
<a id="conjure_raycast_hitdistance"></a>
<a id="conjure_raycast_hitentity"></a>
<a id="conjure_raycast_hitlayer"></a>
<a id="conjure_raycast_hitnormal"></a>
<a id="conjure_raycast_hitpoint"></a>
<a id="conjure_script_class_desc"></a>
<a id="conjure_script_class_desccallback_mask"></a>
<a id="conjure_script_class_descfield_count"></a>
<a id="conjure_script_class_descfull_name"></a>
<a id="conjure_script_class_deschash"></a>
<a id="conjure_script_class_descname_length"></a>
<a id="conjure_script_field_desc"></a>
<a id="conjure_script_field_descdefault_json"></a>
<a id="conjure_script_field_descdefault_length"></a>
<a id="conjure_script_field_descelement_kind"></a>
<a id="conjure_script_field_deschash"></a>
<a id="conjure_script_field_desckind"></a>
<a id="conjure_script_field_descmeta_json"></a>
<a id="conjure_script_field_descmeta_length"></a>
<a id="conjure_script_field_descname"></a>
<a id="conjure_script_field_descname_length"></a>
<a id="conjure_shape_query"></a>
<a id="conjure_shape_queryhalf_extents"></a>
<a id="conjure_shape_querypad"></a>
<a id="conjure_shape_queryradius"></a>
<a id="conjure_shape_queryray"></a>
<a id="conjure_shape_queryrotation"></a>
<a id="conjure_shape_queryshape"></a>
<a id="conjure_transform"></a>
<a id="conjure_transformposition"></a>
<a id="conjure_transformrotation"></a>
<a id="conjure_transformscale"></a>
<a id="conjure_uuid"></a>
<a id="conjure_uuidhigh"></a>
<a id="conjure_uuidlow"></a>
<a id="conjure_vec2"></a>
<a id="conjure_vec2x"></a>
<a id="conjure_vec2y"></a>
<a id="conjure_vec3"></a>
<a id="conjure_vec3x"></a>
<a id="conjure_vec3y"></a>
<a id="conjure_vec3z"></a>
<a id="conjure_vec4"></a>
<a id="conjure_vec4w"></a>
<a id="conjure_vec4x"></a>
<a id="conjure_vec4y"></a>
<a id="conjure_vec4z"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="create_flag_already_started"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="find_native_entry"></a>
<a id="internals-and-algorithms"></a>
<a id="make_native_api"></a>
<a id="native_api_core_entry_count"></a>
<a id="native_api_entry_count"></a>
<a id="native_api_slots"></a>
<a id="native_entry_descriptor"></a>
<a id="native_entry_descriptorgroup"></a>
<a id="native_entry_descriptorindex"></a>
<a id="native_entry_descriptorname"></a>
<a id="native_entry_descriptors"></a>
<a id="native_entry_descriptorsignature"></a>
<a id="other-declarations"></a>
<a id="parse_script_callback"></a>
<a id="parse_script_field_kind"></a>
<a id="query_max_components"></a>
<a id="script_callback"></a>
<a id="script_callbackcollision_enter"></a>
<a id="script_callbackcollision_exit"></a>
<a id="script_callbackcount"></a>
<a id="script_callbackcreate"></a>
<a id="script_callbackdestroy"></a>
<a id="script_callbackfixed_update"></a>
<a id="script_callbacklate_update"></a>
<a id="script_callbackstart"></a>
<a id="script_callbacktrigger_enter"></a>
<a id="script_callbacktrigger_exit"></a>
<a id="script_callbackupdate"></a>
<a id="script_field_kind"></a>
<a id="script_field_kindasset"></a>
<a id="script_field_kindbool"></a>
<a id="script_field_kindcolor"></a>
<a id="script_field_kindcount"></a>
<a id="script_field_kinddouble"></a>
<a id="script_field_kindentity"></a>
<a id="script_field_kindenum"></a>
<a id="script_field_kindfloat"></a>
<a id="script_field_kindint32"></a>
<a id="script_field_kindint64"></a>
<a id="script_field_kindlist"></a>
<a id="script_field_kindquat"></a>
<a id="script_field_kindstring"></a>
<a id="script_field_kindvec2"></a>
<a id="script_field_kindvec3"></a>
<a id="script_field_kindvec4"></a>
<a id="script_phase"></a>
<a id="script_phasecount"></a>
<a id="script_phasefixed_update"></a>
<a id="script_phaselate_update"></a>
<a id="script_phaseupdate"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="validate_managed_api"></a>
<a id="validate_native_api"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
