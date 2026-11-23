---
kind: header
---

[Engine](../../engine.md) → [ECS](../ecs.md)

# entity.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/ecs/entity.h` |
| API ↗ | [Entities and components](../../../api/ecs/entities-and-components.md) |

<p>Handle of one <code><a href="entity/entity.html">entity</a></code>: record index plus generation. Trivially copyable, 8 bytes, mirrored by the <code>C</code># Entity struct through <code>to_u64()</code>/<code>from_u64()</code>. Alignment is 4 ({<code><a href="../core/types.html#symbol-u32">u32</a></code>, <code><a href="../core/types.html#symbol-u32">u32</a></code>}); <code>core::reflect</code> caps a handle field&#x27;s expected alignment at the owning type&#x27;s alignment, so a reflected component may place <code><a href="entity/entity.html">entity</a></code> fields at any 4-<code><a href="../core/types.html#symbol-byte">byte</a></code> aligned offset without <code>alignas(8)</code>.</p>

## Structs

| File | Description |
|---|---|
| [`conjure::core::reflect::reflect_kind_override<conjure::ecs::entity>`](entity/conjure-core-reflect-reflect-kind-override-conjure-ecs-entity.md) |  |
| [`entity`](entity/entity.md) | Handle of one <code><a href="entity/entity.html">entity</a></code>: record index plus generation. Trivially copyable, 8 bytes, mirrored by the <code>C</code># Entity struct through <code>to_u64()</code>/<code>from_u64()</code>. Alignment is 4 ({<code><a href="../core/types.html#symbol-u32">u32</a></code>, <code><a href="../core/types.html#symbol-u32">u32</a></code>}); <code>core::reflect</code> caps a handle field&#x27;s expected alignment at the owning type&#x27;s alignment, so a reflected component may place <code><a href="entity/entity.html">entity</a></code> fields at any 4-<code><a href="../core/types.html#symbol-byte">byte</a></code> aligned offset without <code>alignas(8)</code>. |
| [`std::formatter<conjure::ecs::entity>`](entity/std-formatter-conjure-ecs-entity.md) |  |
| [`std::hash<conjure::ecs::entity>`](entity/std-hash-conjure-ecs-entity.md) |  |

## Classes

| File | Description |
|---|---|
| [`entity_remap`](entity/entity-remap.md) | Maps archive-local references to real entities: entry i of the table is the <code><a href="entity/entity.html">entity</a></code> created for archive entry i. Built once per instantiated copy of an archive and applied to every <code><a href="entity/entity.html">entity</a></code> field of the copy. |

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-ARCHIVE_LOCAL_GENERATION"></a><code>ARCHIVE_LOCAL_GENERATION</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">0</span>xFFFFFFFFu</code> | Generation that marks an archive-local reference: the index is an entry index inside the archive. The allocator never issues it. |
| <a id="variable-NULL_ENTITY"></a><code>NULL_ENTITY</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="entity/entity.html">entity</a></code> | <code class="signature">{}</code> | The <code>null</code> <code><a href="entity/entity.html">entity</a></code>. Never alive in any world. |

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="archive_local_generation"></a>
<a id="conjure"></a>
<a id="conjurecorereflectreflect_kind_overrideconjureecsentity"></a>
<a id="conjurecorereflectreflect_kind_overrideconjureecsentitykind"></a>
<a id="conjurecorereflectreflect_kind_overrideconjureecsentityname"></a>
<a id="conjurekind"></a>
<a id="conjurename"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="entity"></a>
<a id="entity_remap"></a>
<a id="entity_remapclear"></a>
<a id="entity_remapentity_remap"></a>
<a id="entity_remapentity_remap_1"></a>
<a id="entity_remapindex_targets"></a>
<a id="entity_remapmindex"></a>
<a id="entity_remapmtargets"></a>
<a id="entity_remapoperator"></a>
<a id="entity_remapremap"></a>
<a id="entity_remapresize"></a>
<a id="entity_remapresolve"></a>
<a id="entity_remapset"></a>
<a id="entity_remapsize"></a>
<a id="entity_remaptarget"></a>
<a id="entity_remaptargets"></a>
<a id="entityarchive_local"></a>
<a id="entityfrom_u64"></a>
<a id="entitygeneration"></a>
<a id="entityindex"></a>
<a id="entityis_archive_local"></a>
<a id="entityis_null"></a>
<a id="entityoperator"></a>
<a id="entityto_u64"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="null_entity"></a>
<a id="std"></a>
<a id="stdformat"></a>
<a id="stdformatterconjureecsentity"></a>
<a id="stdformatterconjureecsentityformat"></a>
<a id="stdhashconjureecsentity"></a>
<a id="stdhashconjureecsentityoperator"></a>
<a id="stdoperator"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
