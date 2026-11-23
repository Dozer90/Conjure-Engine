---
kind: header
---

[Engine](../../engine.md) → [Scripting](../scripting.md)

# script_class_registry.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/scripting/script_class_registry.h` |
| API ↗ | [Scripting](../../../api/scripting/index.md), [Scripts, components and the runtime](../../../api/scripting/scripts-and-components.md) |

<p>The native copy of the script class schema. Managed reports every ScriptBehaviour subclass and its serialised fields through <code>script_register_class</code>/field into the staging half while an assembly loads; <code>commit_staging()</code> swaps it in once the load succeeded, so a failed load never disturbs the active schema the inspector and the blob codecs read.</p>

## Structs

| File | Description |
|---|---|
| [`script_class_info`](script-class-registry/script-class-info.md) |  |
| [`script_class_registry::table`](script-class-registry/script-class-registry-table.md) |  |
| [`script_field_info`](script-class-registry/script-field-info.md) |  |

## Classes

| File | Description |
|---|---|
| [`script_class_registry`](script-class-registry/script-class-registry.md) |  |

## Functions

### Public

#### `script_name_hash` {#function-script-name-hash}

<div class="function-declaration" id="function-script-name-hash-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/types.html#symbol-u64">u64</a> script_name_hash(
    std::string_view fullName) <span class="k">noexcept</span>;</code></pre></div>

<p>Hash of a <code>C</code># type or field name as the managed side computes it (<code>core::hash::fnv1a64</code> of UTF-8).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="script_class_info"></a>
<a id="script_class_infocallbackmask"></a>
<a id="script_class_infofields"></a>
<a id="script_class_infofind_field"></a>
<a id="script_class_infofullname"></a>
<a id="script_class_infohas_callback"></a>
<a id="script_class_infohash"></a>
<a id="script_class_infosort_fields"></a>
<a id="script_class_registry"></a>
<a id="script_class_registrybegin_staging"></a>
<a id="script_class_registryclasses"></a>
<a id="script_class_registryclear"></a>
<a id="script_class_registrycommit_staging"></a>
<a id="script_class_registrydiscard_staging"></a>
<a id="script_class_registryfind"></a>
<a id="script_class_registryfind_in"></a>
<a id="script_class_registryfind_staged"></a>
<a id="script_class_registryindex_of"></a>
<a id="script_class_registryis_staging_open"></a>
<a id="script_class_registrymactive"></a>
<a id="script_class_registrymstaging"></a>
<a id="script_class_registrymstagingopen"></a>
<a id="script_class_registryoperator"></a>
<a id="script_class_registryscript_class_registry"></a>
<a id="script_class_registryscript_class_registry_1"></a>
<a id="script_class_registrysize"></a>
<a id="script_class_registrystage_class"></a>
<a id="script_class_registrystage_field"></a>
<a id="script_class_registrystaged_classes"></a>
<a id="script_class_registrytable"></a>
<a id="script_class_registrytableclasses"></a>
<a id="script_class_registrytableindexbyhash"></a>
<a id="script_field_info"></a>
<a id="script_field_infodefaultvalue"></a>
<a id="script_field_infoelementkind"></a>
<a id="script_field_infohash"></a>
<a id="script_field_infokind"></a>
<a id="script_field_infometa"></a>
<a id="script_field_infoname"></a>
<a id="script_name_hash"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
