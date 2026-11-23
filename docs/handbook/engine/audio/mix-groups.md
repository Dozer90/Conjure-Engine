---
kind: header
---

[Engine](../../engine.md) → [Audio](../audio.md)

# mix_groups.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/audio/mix_groups.h` |
| API ↗ | [Playing sounds](../../../api/audio/playing-sounds.md) |

<p>Mix groups: a gain tree rooted at <code>MASTER_GROUP</code>. The main thread owns the tree and sends each group&#x27;s effective gain (the product up the chain) and effective paused flag (its own flag or any ancestor&#x27;s) to the mixer.</p>

## Structs

| File | Description |
|---|---|
| [`group_desc`](mix-groups/group-desc.md) | What <code>create_group()</code> needs. The parent is given by id, or by name through <code>parentName</code> when groups are declared in settings before any id exists (a <code>null</code> <code>parentName</code> means &quot;use parent&quot;). |
| [`group_id`](mix-groups/group-id.md) | Handle of a mix group: slot index plus the generation issued when the slot was last created. <code>remove_group()</code> retires the id at once and a reused slot gets a new generation, so stale ids are rejected. |
| [`group_info`](mix-groups/group-info.md) | Main-thread view of a group. <code>paused</code> is the group&#x27;s own flag (what <code>set_group_paused()</code> last stored); <code>effectivePaused</code> says whether its voices are frozen, which is also the case while any ancestor is paused. |

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-MASTER_GROUP"></a><code>MASTER_GROUP</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="mix-groups/group-id.html">group_<wbr>id</a></code> | <code class="signature">{<span class="mi">0</span>, <span class="mi">1</span>}</code> | The root group, always present, never removable. |

## Functions

### Public

#### `clamp_group_gain` {#function-clamp-group-gain}

<div class="function-declaration" id="function-clamp-group-gain-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/types.html#symbol-f32">f32</a> clamp_group_gain(
    <a href="../core/types.html#symbol-f32">f32</a> gain) <span class="k">noexcept</span>;</code></pre></div>

<p>Clamp rule of for group gains: [0, 2], NaN becomes 1.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="clamp_group_gain"></a>
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="group_desc"></a>
<a id="group_descgain"></a>
<a id="group_descname"></a>
<a id="group_descoperator"></a>
<a id="group_descparent"></a>
<a id="group_descparentname"></a>
<a id="group_id"></a>
<a id="group_idgeneration"></a>
<a id="group_idindex"></a>
<a id="group_idis_valid"></a>
<a id="group_idoperator"></a>
<a id="group_idoperator_1"></a>
<a id="group_info"></a>
<a id="group_infoeffectivegain"></a>
<a id="group_infoeffectivepaused"></a>
<a id="group_infogain"></a>
<a id="group_infoid"></a>
<a id="group_infoname"></a>
<a id="group_infooperator"></a>
<a id="group_infoparent"></a>
<a id="group_infopaused"></a>
<a id="internals-and-algorithms"></a>
<a id="master_group"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
