---
kind: header
---

[Engine](../../engine.md) → [ECS](../ecs.md)

# view.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/ecs/view.h` |
| API ↗ | [Queries, views and systems](../../../api/ecs/queries-views-and-systems.md) |

<p>Typed views.</p>

## Structs

| File | Description |
|---|---|
| [`detail::view_arg_traits`](view/detail-view-arg-traits.md) |  |
| [`detail::view_arg_traits<maybe<ComponentType>>`](view/detail-view-arg-traits-maybe-componenttype.md) |  |
| [`maybe`](view/maybe.md) | Marks an optional component in a <code><a href="view/view.html">view</a></code> pack. The callback receives <code>T</code> * (<code>null</code> when absent). |
| [`view_access`](view/view-access.md) | The component sets a <code><a href="view/view.html">view</a></code> reads and writes, derived from its pack. |

## Classes

| File | Description |
|---|---|
| [`view`](view/view.md) |  |

## Type Aliases

### `detail::view_arg_traits::callback_type` {#symbol-detail-view-arg-traits-callback-type}

```cpp
using callback_type = PackType &;
```

### `detail::view_arg_traits::component_type` {#symbol-detail-view-arg-traits-component-type}

```cpp
using component_type = std::remove_cv_t<PackType>;
```

### `detail::view_arg_traits::span_type` {#symbol-detail-view-arg-traits-span-type}

```cpp
using span_type = std::span<PackType>;
```

### `detail::view_arg_traits<maybe<ComponentType>>::callback_type` {#symbol-detail-view-arg-traits-maybe-componenttype-callback-type}

```cpp
using callback_type = ComponentType *;
```

### `detail::view_arg_traits<maybe<ComponentType>>::component_type` {#symbol-detail-view-arg-traits-maybe-componenttype-component-type}

```cpp
using component_type = std::remove_cv_t<ComponentType>;
```

### `detail::view_arg_traits<maybe<ComponentType>>::span_type` {#symbol-detail-view-arg-traits-maybe-componenttype-span-type}

```cpp
using span_type = std::span<ComponentType>;
```

## Forward Declarations

### `command_buffer` {#symbol-command-buffer}

```cpp
class command_buffer;
```

### `system_context` {#symbol-system-context}

```cpp
class system_context;
```

### `world` {#symbol-world}

```cpp
class world;
```

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-detail::HAS_COLUMN"></a><code>detail::HAS_COLUMN</code> | <code class="signature"><span class="k">template</span> &lt;<wbr><span class="k">typename</span> Pack<wbr>Type&gt; <span class="k">inline</span> <span class="k">constexpr</span> <span class="k">bool</span></code> | <code class="signature">!std::<wbr>is_<wbr>empty_<wbr>v&lt;<wbr><span class="k">typename</span> <a href="view/detail-view-arg-traits.html">view_<wbr>arg_<wbr>traits</a>&lt;<wbr>Pack<wbr>Type&gt;::<wbr><a href="view.html#symbol-detail-view-arg-traits-component-type">component_<wbr>type</a>&gt;</code> | False for a tag (an empty type, plain or in <code><a href="view/maybe.html">maybe</a>&lt;&gt;</code>): it has no column a <code><a href="view/view.html">view</a></code> could hand to the callback, so it is required with <code>view::with&lt;Tag&gt;()</code> instead. |

## Functions

### Public

#### `detail::fetch_column` {#function-detail-fetch-column}

<div class="function-declaration" id="function-detail-fetch-column-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> PackType&gt; [[<span class="na">nodiscard</span>]] <span class="k">typename</span> <a href="view/detail-view-arg-traits.html">view_arg_traits</a>&lt;PackType&gt;::<a href="view.html#symbol-detail-view-arg-traits-span-type">span_type</a> fetch_column(<a href="chunk-view/chunk-view.html">chunk_view</a> &amp;view) <span class="k">noexcept</span>;</code></pre></div>

#### `detail::row_argument` {#function-detail-row-argument}

<div class="function-declaration" id="function-detail-row-argument-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> PackType&gt; [[<span class="na">nodiscard</span>]] <span class="k">typename</span> <a href="view/detail-view-arg-traits.html">view_arg_traits</a>&lt;PackType&gt;::<a href="view.html#symbol-detail-view-arg-traits-callback-type">callback_type</a> row_argument( <span class="k">typename</span> <a href="view/detail-view-arg-traits.html">view_arg_traits</a>&lt;PackType&gt;::<a href="view.html#symbol-detail-view-arg-traits-span-type">span_type</a> column, <a href="../core/types.html#symbol-u32">u32</a> row ) <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="command_buffer"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="detailfetch_column"></a>
<a id="detailhas_column"></a>
<a id="detailrow_argument"></a>
<a id="detailview_arg_traits"></a>
<a id="detailview_arg_traitscallback_type"></a>
<a id="detailview_arg_traitscomponent_type"></a>
<a id="detailview_arg_traitsis_const"></a>
<a id="detailview_arg_traitsis_optional"></a>
<a id="detailview_arg_traitsmaybecomponenttype"></a>
<a id="detailview_arg_traitsmaybecomponenttypecallback_type"></a>
<a id="detailview_arg_traitsmaybecomponenttypecomponent_type"></a>
<a id="detailview_arg_traitsmaybecomponenttypeis_const"></a>
<a id="detailview_arg_traitsmaybecomponenttypeis_optional"></a>
<a id="detailview_arg_traitsmaybecomponenttypespan_type"></a>
<a id="detailview_arg_traitsspan_type"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="fetch_column"></a>
<a id="has_column"></a>
<a id="internals-and-algorithms"></a>
<a id="maybe"></a>
<a id="other-declarations"></a>
<a id="row_argument"></a>
<a id="structs-and-types"></a>
<a id="system_context"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<a id="view"></a>
<a id="view_access"></a>
<a id="view_accessreads"></a>
<a id="view_accesswrites"></a>
<a id="view_arg_traits"></a>
<a id="view_arg_traits_1"></a>
<a id="view_arg_traitscallback_type"></a>
<a id="view_arg_traitscallback_type_1"></a>
<a id="view_arg_traitscomponent_type"></a>
<a id="view_arg_traitscomponent_type_1"></a>
<a id="view_arg_traitsis_const"></a>
<a id="view_arg_traitsis_const_1"></a>
<a id="view_arg_traitsis_optional"></a>
<a id="view_arg_traitsis_optional_1"></a>
<a id="view_arg_traitsspan_type"></a>
<a id="view_arg_traitsspan_type_1"></a>
<a id="viewaccess"></a>
<a id="viewchanged"></a>
<a id="viewcollect_access"></a>
<a id="viewentity_count"></a>
<a id="viewfill_slot"></a>
<a id="viewfor_each"></a>
<a id="viewfor_each_chunk"></a>
<a id="viewinvoke_row"></a>
<a id="viewis_valid"></a>
<a id="viewmatch_nothing"></a>
<a id="viewmids"></a>
<a id="viewmquery"></a>
<a id="viewmvalid"></a>
<a id="viewoperator"></a>
<a id="viewpack_size"></a>
<a id="viewparallel_for_each_chunk"></a>
<a id="viewrequire_unregistered"></a>
<a id="viewunderlying"></a>
<a id="viewview"></a>
<a id="viewview_1"></a>
<a id="viewwith"></a>
<a id="viewwithout"></a>
<a id="world"></a>
<!-- doc-legacy-anchors:end -->
