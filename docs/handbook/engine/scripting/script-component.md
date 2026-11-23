---
kind: header
---

[Engine](../../engine.md) → [Scripting](../scripting.md)

# script_component.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/scripting/script_component.h` |
| API ↗ | [Scripting](../../../api/scripting/index.md), [Scripts, components and the runtime](../../../api/scripting/scripts-and-components.md) |

<p>The script component and its store.</p>

## Structs

| File | Description |
|---|---|
| [`script_component`](script-component/script-component.md) |  |
| [`script_entry`](script-component/script-entry.md) |  |
| [`script_entry_id`](script-component/script-entry-id.md) | Public handle to one script of one entity: the set handle plus the entry&#x27;s index in the set. |
| [`script_set`](script-component/script-set.md) |  |
| [`script_store::set_slot`](script-component/script-store-set-slot.md) |  |

## Classes

| File | Description |
|---|---|
| [`script_store`](script-component/script-store.md) | Generational storage of script sets plus the managed slot -&gt; entry reverse map. Handles pack a 20-bit index and a 12-bit generation (1..4095, 0 never issued) into the component&#x27;s 32 bits. |

## Enumerations

### `script_entry_flags` {#symbol-script-entry-flags}

```cpp
enum class script_entry_flags : u8 {
    NONE = 0,
    STARTED = 1 << 0,           // OnStart ran (or the entry was created ALREADY_STARTED)
    FAULTED = 1 << 1,           // a callback was disabled (three consecutive faults, or OnCreate threw)
    CLASS_MISSING = 1 << 2      // no class for the hash in the active registry; JSON preserved
};
```

## Forward Declarations

### `script_runtime` {#symbol-script-runtime}

```cpp
class script_runtime;
```

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-INVALID_SET_HANDLE"></a><code>INVALID_SET_HANDLE</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">0</span></code> | The <code>null</code> store handle: zero-filled component memory refers to no set. |
| <a id="variable-INVALID_SLOT"></a><code>INVALID_SLOT</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-i32">i32</a></code> | <code class="signature">-<span class="mi">1</span></code> | Managed slot of an entry without a live instance (edit mode, missing class, disabled runtime). |
| <a id="variable-MAX_SCRIPTS_PER_ENTITY"></a><code>MAX_SCRIPTS_PER_ENTITY</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">8</span></code> | Scripts one entity may carry. |
| <a id="variable-SCRIPT_COMPONENT_NAME"></a><code>SCRIPT_COMPONENT_NAME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.scripting.script_<wbr>component"</span></code> | Registered component name. |

## Macros

### `CONJURE_COMPONENT` {#symbol-conjure-component}

```cpp
CONJURE_COMPONENT(conjure::scripting::script_component, "conjure.scripting.script_component")
```

### `CONJURE_ENUM_FLAGS` {#symbol-conjure-enum-flags}

```cpp
CONJURE_ENUM_FLAGS(script_entry_flags)
```

## Functions

### Public

#### `register_script_component` {#function-register-script-component}

<div class="function-declaration" id="function-register-script-component-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;ecs::component_id&gt; register_script_component(
    <a href="../ecs/component-registry/component-registry.html">ecs::component_registry</a> &amp;registry);</code></pre></div>

<p>Registers <code><a href="script-component/script-component.html">script_component</a></code> (name <code>SCRIPT_COMPONENT_NAME</code>, 4 bytes, custom copy/destruct, memcpy relocate) and returns its id. Re-registration returns the existing id.</p>

#### `script_component_ops` {#function-script-component-ops}

<div class="function-declaration" id="function-script-component-ops-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> ecs::component_ops script_component_ops() <span class="k">noexcept</span>;</code></pre></div>

<p>The lifecycle hooks the component registers: copy deep-copies the set, destruct frees it; both go through <code>script_runtime::active()</code> and do nothing when no runtime is alive.</p>

#### `script_component_serializer` {#function-script-component-serializer}

<div class="function-declaration" id="function-script-component-serializer-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> ecs::component_serializer script_component_serializer() <span class="k">noexcept</span>;</code></pre></div>

<p>The serializer hooks of the component: {&quot;scripts&quot;:[{&quot;class&quot;,&quot;hash&quot;,&quot;fields&quot;}]} through the active runtime; remapEntities rewrites <code>ENTITY</code> fields of every entry with the class schema. Without a live runtime writeJson writes {&quot;scripts&quot;:[]} for a component without a set and fails with <code>INVALID_STATE</code> for one with a set (its scripts would be lost), readJson fails with <code>INVALID_STATE</code>, remapEntities does nothing.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="conjure_component"></a>
<a id="conjure_enum_flags"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="invalid_set_handle"></a>
<a id="invalid_slot"></a>
<a id="max_scripts_per_entity"></a>
<a id="other-declarations"></a>
<a id="register_script_component"></a>
<a id="script_component"></a>
<a id="script_component_name"></a>
<a id="script_component_ops"></a>
<a id="script_component_serializer"></a>
<a id="script_componentoperator"></a>
<a id="script_componentsethandle"></a>
<a id="script_entry"></a>
<a id="script_entry_flags"></a>
<a id="script_entry_flagsclass_missing"></a>
<a id="script_entry_flagsfaulted"></a>
<a id="script_entry_flagsnone"></a>
<a id="script_entry_flagsstarted"></a>
<a id="script_entry_id"></a>
<a id="script_entry_identryindex"></a>
<a id="script_entry_idis_valid"></a>
<a id="script_entry_idoperator"></a>
<a id="script_entry_idsethandle"></a>
<a id="script_entryclasshash"></a>
<a id="script_entryfields"></a>
<a id="script_entryflags"></a>
<a id="script_entryhas_flag"></a>
<a id="script_entryis_live"></a>
<a id="script_entrymanagedslot"></a>
<a id="script_runtime"></a>
<a id="script_set"></a>
<a id="script_setentries"></a>
<a id="script_setowner"></a>
<a id="script_store"></a>
<a id="script_storeadd_entry"></a>
<a id="script_storeclear"></a>
<a id="script_storeclone_set"></a>
<a id="script_storecontains"></a>
<a id="script_storecreate_set"></a>
<a id="script_storefind"></a>
<a id="script_storefind_entry"></a>
<a id="script_storefor_each_set"></a>
<a id="script_storefree_set"></a>
<a id="script_storegeneration_mask"></a>
<a id="script_storehandle_generation"></a>
<a id="script_storehandle_index"></a>
<a id="script_storeindex_bits"></a>
<a id="script_storelive_slot"></a>
<a id="script_storemake_handle"></a>
<a id="script_storemap_slot"></a>
<a id="script_storemapped_slot_count"></a>
<a id="script_storemax_sets"></a>
<a id="script_storemfreelist"></a>
<a id="script_storemlivecount"></a>
<a id="script_storemmappedcount"></a>
<a id="script_storemslots"></a>
<a id="script_storemslottoentry"></a>
<a id="script_storeoperator"></a>
<a id="script_storeremove_entry"></a>
<a id="script_storeresolve_slot"></a>
<a id="script_storescript_store"></a>
<a id="script_storescript_store_1"></a>
<a id="script_storeset_slot"></a>
<a id="script_storeset_slotgeneration"></a>
<a id="script_storeset_slotlive"></a>
<a id="script_storeset_slotset"></a>
<a id="script_storesize"></a>
<a id="script_storeunmap_slot"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
