---
kind: header
---

[Engine](../../engine.md) → [Assets](../assets.md)

# asset_ref.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/assets/asset_ref.h` |
| API ↗ | [Assets](../../../api/assets/index.md), [Referencing and loading assets](../../../api/assets/referencing-and-loading.md) |

<p>Non-owning reference to an asset: the uuid plus the bound slot. Components store these by value in ECS chunks; copying is a memcpy. <code>get()</code> is one atomic load.</p>

## Structs

| File | Description |
|---|---|
| [`conjure::core::reflect::reflect_kind_override<conjure::assets::asset_ref<AssetType>>`](asset-ref/conjure-core-reflect-reflect-kind-override-conjure-assets-asset-ref-assettype.md) |  |

## Classes

| File | Description |
|---|---|
| [`asset_handle`](asset-ref/asset-handle.md) | Owning, type-erased reference: every live handle counts as one strong reference on the slot, which keeps the payload resident across <code>collect()</code>. Copy and move are atomics; the handle may be copied, moved and destroyed on any thread. |
| [`asset_ref`](asset-ref/asset-ref.md) | Non-owning reference to an asset: the uuid plus the bound slot. Components store these by value in ECS chunks; copying is a memcpy. <code>get()</code> is one atomic load. |

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-ASSET_REF_TYPE_NAME"></a><code>ASSET_REF_TYPE_NAME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.assets.asset_<wbr>ref"</span></code> | The name every <code><a href="asset-ref/asset-ref.html">asset_ref</a>&lt;T&gt;</code> field reflects under (<code><a href="../core/reflect/type-id.html">reflect_kind_override</a></code> below). The kind is type-erased on purpose: the uuid alone travels, the asset type is known to the binding pass. |

## Functions

### Public

#### `detail::forget_type_mismatches` {#function-detail-forget-type-mismatches}

<div class="function-declaration" id="function-detail-forget-type-mismatches-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> forget_type_mismatches() <span class="k">noexcept</span>;</code></pre></div>

<p>Forgets every reported mismatch so the next one logs again; called by ~<code><a href="asset-manager/asset-manager.html">asset_manager</a></code>.</p>

#### `detail::report_type_mismatch` {#function-detail-report-type-mismatch}

<div class="function-declaration" id="function-detail-report-type-mismatch-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> report_type_mismatch(<span class="k">const</span> <a href="asset-types.html#symbol-asset-id">asset_id</a> &amp;id, <a href="asset-types.html#symbol-asset-type-id">asset_type_id</a> requested, <a href="asset-types.html#symbol-asset-type-id">asset_type_id</a> actual);</code></pre></div>

<p>Logs the type mismatch of a typed access once per (uuid, requested type) pair until an <code><a href="asset-manager/asset-manager.html">asset_manager</a></code> is destroyed (the set is process-wide).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="asset_handle"></a>
<a id="asset_handleasset_handle"></a>
<a id="asset_handleasset_handle_1"></a>
<a id="asset_handleget"></a>
<a id="asset_handleid"></a>
<a id="asset_handleis_bound"></a>
<a id="asset_handleis_terminal"></a>
<a id="asset_handlemslot"></a>
<a id="asset_handleoperator"></a>
<a id="asset_handleoperator_1"></a>
<a id="asset_handleref"></a>
<a id="asset_handlereset"></a>
<a id="asset_handleslot"></a>
<a id="asset_handlestate"></a>
<a id="asset_handletype"></a>
<a id="asset_handleversion"></a>
<a id="asset_ref"></a>
<a id="asset_ref_type_name"></a>
<a id="asset_refasset_ref"></a>
<a id="asset_refget"></a>
<a id="asset_refid"></a>
<a id="asset_refis_bound"></a>
<a id="asset_refmid"></a>
<a id="asset_refmslot"></a>
<a id="asset_refoperator"></a>
<a id="asset_refslot"></a>
<a id="asset_refstate"></a>
<a id="asset_refversion"></a>
<a id="conjure"></a>
<a id="conjurecorereflectreflect_kind_overrideconjureassetsasset_refassettype"></a>
<a id="conjurecorereflectreflect_kind_overrideconjureassetsasset_refassettypekind"></a>
<a id="conjurecorereflectreflect_kind_overrideconjureassetsasset_refassettypename"></a>
<a id="conjurekind"></a>
<a id="conjurename"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="detailforget_type_mismatches"></a>
<a id="detailreport_type_mismatch"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="forget_type_mismatches"></a>
<a id="internals-and-algorithms"></a>
<a id="report_type_mismatch"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
