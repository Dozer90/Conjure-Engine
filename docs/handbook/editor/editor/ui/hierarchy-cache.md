---
kind: header
---

[Editor](../../../editor.md) → [User Interface](../../user-interface.md)

# hierarchy_cache.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/ui/hierarchy_cache.h` |
| API ↗ | [The workspace UI](../../../../api/editor/workspace-ui.md) |

<p>The hierarchy panel&#x27;s flat row list: one pre-order walk per structural change, expansion state (by scene guid) and name filter, drawn with a list clipper so 10,000 entities cost <code>O(visible rows)</code>. Names and enabled state, which change without a structural change, are refreshed for the visible rows only (<code>refresh_labels</code>).</p>

## Structs

| File | Description |
|---|---|
| [`ui::hierarchy_row`](hierarchy-cache/ui-hierarchy-row.md) |  |

## Classes

| File | Description |
|---|---|
| [`ui::hierarchy_cache`](hierarchy-cache/ui-hierarchy-cache.md) |  |

## Enumerations

### `ui::hierarchy_row_flags` {#symbol-ui-hierarchy-row-flags}

```cpp
enum class hierarchy_row_flags : u8 {
    NONE = 0,
    HAS_CHILDREN = 1 << 0,
    EXPANDED = 1 << 1,
    SELECTED = 1 << 2,
    PREFAB_INSTANCE = 1 << 3,
    DISABLED = 1 << 4
};
```

## Macros

### `ui::CONJURE_ENUM_FLAGS` {#symbol-ui-conjure-enum-flags}

```cpp
CONJURE_ENUM_FLAGS(hierarchy_row_flags)
```

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="conjure_enum_flags"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="hierarchy_cache"></a>
<a id="hierarchy_cachecompute_matches"></a>
<a id="hierarchy_cachefilter"></a>
<a id="hierarchy_cachehierarchy_cache"></a>
<a id="hierarchy_cachehierarchy_cache_1"></a>
<a id="hierarchy_cacheinvalidate"></a>
<a id="hierarchy_cacheis_expanded"></a>
<a id="hierarchy_cachemdirty"></a>
<a id="hierarchy_cachemexpanded"></a>
<a id="hierarchy_cachemfilter"></a>
<a id="hierarchy_cachemmatches"></a>
<a id="hierarchy_cachemrebuilds"></a>
<a id="hierarchy_cachemrows"></a>
<a id="hierarchy_cachemstructureversion"></a>
<a id="hierarchy_cacheoperator"></a>
<a id="hierarchy_cacherebuild"></a>
<a id="hierarchy_cacherebuild_count"></a>
<a id="hierarchy_cacherefresh_if_dirty"></a>
<a id="hierarchy_cacherefresh_labels"></a>
<a id="hierarchy_cachereveal"></a>
<a id="hierarchy_cacherows"></a>
<a id="hierarchy_cacheset_expanded"></a>
<a id="hierarchy_cacheset_filter"></a>
<a id="hierarchy_cachestructure_version"></a>
<a id="hierarchy_cachesubtree_matches"></a>
<a id="hierarchy_row"></a>
<a id="hierarchy_row_flags"></a>
<a id="hierarchy_row_flagsdisabled"></a>
<a id="hierarchy_row_flagsexpanded"></a>
<a id="hierarchy_row_flagshas_children"></a>
<a id="hierarchy_row_flagsnone"></a>
<a id="hierarchy_row_flagsprefab_instance"></a>
<a id="hierarchy_row_flagsselected"></a>
<a id="hierarchy_rowdepth"></a>
<a id="hierarchy_rowentity"></a>
<a id="hierarchy_rowflags"></a>
<a id="hierarchy_rownameid"></a>
<a id="hierarchy_rowpadding"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="uiconjure_enum_flags"></a>
<a id="uihierarchy_cache"></a>
<a id="uihierarchy_cachecompute_matches"></a>
<a id="uihierarchy_cachefilter"></a>
<a id="uihierarchy_cachehierarchy_cache"></a>
<a id="uihierarchy_cachehierarchy_cache_1"></a>
<a id="uihierarchy_cacheinvalidate"></a>
<a id="uihierarchy_cacheis_expanded"></a>
<a id="uihierarchy_cachemdirty"></a>
<a id="uihierarchy_cachemexpanded"></a>
<a id="uihierarchy_cachemfilter"></a>
<a id="uihierarchy_cachemmatches"></a>
<a id="uihierarchy_cachemrebuilds"></a>
<a id="uihierarchy_cachemrows"></a>
<a id="uihierarchy_cachemstructureversion"></a>
<a id="uihierarchy_cacheoperator"></a>
<a id="uihierarchy_cacherebuild"></a>
<a id="uihierarchy_cacherebuild_count"></a>
<a id="uihierarchy_cacherefresh_if_dirty"></a>
<a id="uihierarchy_cacherefresh_labels"></a>
<a id="uihierarchy_cachereveal"></a>
<a id="uihierarchy_cacherows"></a>
<a id="uihierarchy_cacheset_expanded"></a>
<a id="uihierarchy_cacheset_filter"></a>
<a id="uihierarchy_cachestructure_version"></a>
<a id="uihierarchy_cachesubtree_matches"></a>
<a id="uihierarchy_row"></a>
<a id="uihierarchy_row_flags"></a>
<a id="uihierarchy_row_flagsdisabled"></a>
<a id="uihierarchy_row_flagsexpanded"></a>
<a id="uihierarchy_row_flagshas_children"></a>
<a id="uihierarchy_row_flagsnone"></a>
<a id="uihierarchy_row_flagsprefab_instance"></a>
<a id="uihierarchy_row_flagsselected"></a>
<a id="uihierarchy_rowdepth"></a>
<a id="uihierarchy_rowentity"></a>
<a id="uihierarchy_rowflags"></a>
<a id="uihierarchy_rownameid"></a>
<a id="uihierarchy_rowpadding"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
