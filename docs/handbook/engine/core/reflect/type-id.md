---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# type_id.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/reflect/type_id.h` |
| API ↗ | [Reflection](../../../../api/core/reflection.md) |

<p>Type identifiers.</p>

## Structs

| File | Description |
|---|---|
| [`reflect::type_id`](type-id/reflect-type-id.md) | Identity of a reflected type: the FNV-1a 64 hash of its registered name. The <code>null</code> id (0) means &quot;no type&quot;. |
| [`std::formatter<conjure::core::reflect::type_id>`](type-id/std-formatter-conjure-core-reflect-type-id.md) |  |
| [`std::hash<conjure::core::reflect::type_id>`](type-id/std-hash-conjure-core-reflect-type-id.md) |  |

## Concepts

### `reflect::has_kind_override` {#symbol-reflect-has-kind-override}

```cpp
template <typename T> concept has_kind_override = requires { { reflect_kind_override<std::remove_cvref_t<T>>::NAME } -> std::convertible_to<std::string_view>; };
```

True when <code><a href="../../assets/asset-ref/conjure-core-reflect-reflect-kind-override-conjure-assets-asset-ref-assettype.html">reflect_kind_override</a></code> is specialised for <code>T</code> with a <code>NAME</code>.

### `reflect::has_reflect_name` {#symbol-reflect-has-reflect-name}

```cpp
template <typename T> concept has_reflect_name = requires { { reflect_traits<std::remove_cvref_t<T>>::NAME } -> std::convertible_to<std::string_view>; };
```

True when <code><a href="field-info/reflect-reflect-traits-bool.html">reflect_traits</a></code> is specialised for <code>T</code> with a <code>NAME</code>.

### `reflect::reflect_named` {#symbol-reflect-reflect-named}

```cpp
template <typename T> concept reflect_named = has_reflect_name<T> || has_kind_override<T>;
```

True when a registered name is known for <code>T</code> through either customisation point.

## Forward Declarations

### `reflect::reflect_kind_override` {#symbol-reflect-reflect-kind-override}

```cpp
template <typename T> struct reflect_kind_override;
```

Customisation point for handle types owned by other modules: specialise it with <code>static constexpr <a href="field-info.html#symbol-reflect-field-kind">field_kind</a> KIND</code> (<code>ENTITY</code> or <code>ASSET_REF</code>) and <code>static constexpr std::string_view NAME</code> and the type becomes a reflectable field of that kind without core knowing the type.

### `reflect::reflect_traits` {#symbol-reflect-reflect-traits}

```cpp
template <typename T> struct reflect_traits;
```

Compile-time description of a reflected type. Specialisations provide at least <code>static constexpr std::string_view NAME</code> and <code>static constexpr <a href="field-info.html#symbol-reflect-field-kind">field_kind</a> KIND</code>; structs and enums described with the <code>reflect.h</code> macros also provide <code>describe()</code>.

## Functions

### Public

#### `reflect::reflect_name_of` {#function-reflect-reflect-name-of}

<div class="function-declaration" id="function-reflect-reflect-name-of-1"><pre><code><span class="k">template</span> &lt;<a href="type-id.html#symbol-reflect-reflect-named">reflect_named</a> T&gt; [[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> std::string_view reflect_name_of() <span class="k">noexcept</span>;</code></pre></div>

<p>The registered name of <code>T</code>. The kind override wins so a module may rename a type it reflects.</p>

#### `reflect::type_id_of` {#function-reflect-type-id-of}

<div class="function-declaration" id="function-reflect-type-id-of-1"><pre><code><span class="k">template</span> &lt;<a href="type-id.html#symbol-reflect-reflect-named">reflect_named</a> T&gt; [[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="type-id/reflect-type-id.html">type_id</a> type_id_of() <span class="k">noexcept</span>;</code></pre></div>

<p>The <code><a href="type-id/reflect-type-id.html">type_id</a></code> of <code>T</code>, computed at compile time from its registered name.</p>

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
<a id="has_kind_override"></a>
<a id="has_reflect_name"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="reflect_kind_override"></a>
<a id="reflect_name_of"></a>
<a id="reflect_named"></a>
<a id="reflect_traits"></a>
<a id="reflecthas_kind_override"></a>
<a id="reflecthas_reflect_name"></a>
<a id="reflectreflect_kind_override"></a>
<a id="reflectreflect_name_of"></a>
<a id="reflectreflect_named"></a>
<a id="reflectreflect_traits"></a>
<a id="reflecttype_id"></a>
<a id="reflecttype_id_of"></a>
<a id="reflecttype_idfrom_name"></a>
<a id="reflecttype_idis_null"></a>
<a id="reflecttype_idnull"></a>
<a id="reflecttype_idoperator"></a>
<a id="reflecttype_idoperator_1"></a>
<a id="reflecttype_idvalue"></a>
<a id="std"></a>
<a id="stdformat"></a>
<a id="stdformatterconjurecorereflecttype_id"></a>
<a id="stdformatterconjurecorereflecttype_idformat"></a>
<a id="stdhashconjurecorereflecttype_id"></a>
<a id="stdhashconjurecorereflecttype_idoperator"></a>
<a id="stdoperator"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="type_id"></a>
<a id="type_id_of"></a>
<a id="type_idfrom_name"></a>
<a id="type_idis_null"></a>
<a id="type_idnull"></a>
<a id="type_idoperator"></a>
<a id="type_idoperator_1"></a>
<a id="type_idvalue"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
