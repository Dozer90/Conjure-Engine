---
kind: header
---

[Editor](../../editor.md) → [User Interface](../user-interface.md)

# selection_set.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/selection_set.h` |
| API ↗ | [Headless mode and the command model](../../../api/editor/headless-and-commands.md), [The editor](../../../api/editor/index.md) |

<p>The selection: UI state, never undoable. <code>A</code> sorted guid list plus a dense bit set by entity index so the hierarchy panel asks &quot;is this row selected&quot; in <code>O(1)</code>, and the primary (active) entity: the one named last, as the inspector and the hierarchy&#x27;s &quot;Add&quot; parent use it.</p>



## Classes

| File | Description |
|---|---|
| [`selection_set`](selection-set/selection-set.md) |  |

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
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="selection_set"></a>
<a id="selection_setadd"></a>
<a id="selection_setclear"></a>
<a id="selection_setcontains"></a>
<a id="selection_setempty"></a>
<a id="selection_setguids"></a>
<a id="selection_setmbyentity"></a>
<a id="selection_setmguids"></a>
<a id="selection_setmonchanged"></a>
<a id="selection_setmprimary"></a>
<a id="selection_seton_changed"></a>
<a id="selection_setoperator"></a>
<a id="selection_setprimary"></a>
<a id="selection_setrebuild"></a>
<a id="selection_setrefresh_primary"></a>
<a id="selection_setremove"></a>
<a id="selection_setselection_set"></a>
<a id="selection_setselection_set_1"></a>
<a id="selection_setset"></a>
<a id="selection_setsize"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
