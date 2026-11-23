---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# field_info.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/reflect/field_info.h` |
| API ↗ | [Reflection](../../../../api/core/reflection.md) |

<p>Field descriptions.</p>

## Structs

| File | Description |
|---|---|
| [`reflect::detail::std_array_traits`](field-info/reflect-detail-std-array-traits.md) |  |
| [`reflect::detail::std_array_traits<std::array<ElementType, N>>`](field-info/reflect-detail-std-array-traits-std-array-elementtype-n.md) |  |
| [`reflect::enum_info`](field-info/reflect-enum-info.md) | <code>A</code> reflected enumeration: its enumerators by name and value plus the storage width. |
| [`reflect::field_attributes`](field-info/reflect-field-attributes.md) | Editor hints attached to a field. Set through the <code>CONJURE_FIELD</code> designated-initialiser list. |
| [`reflect::field_info`](field-info/reflect-field-info.md) | One reflected member of a struct. |
| [`reflect::reflect_traits<bool>`](field-info/reflect-reflect-traits-bool.md) |  |
| [`reflect::reflect_traits<f32>`](field-info/reflect-reflect-traits-f32.md) |  |
| [`reflect::reflect_traits<f64>`](field-info/reflect-reflect-traits-f64.md) |  |
| [`reflect::reflect_traits<i16>`](field-info/reflect-reflect-traits-i16.md) |  |
| [`reflect::reflect_traits<i32>`](field-info/reflect-reflect-traits-i32.md) |  |
| [`reflect::reflect_traits<i64>`](field-info/reflect-reflect-traits-i64.md) |  |
| [`reflect::reflect_traits<i8>`](field-info/reflect-reflect-traits-i8.md) |  |
| [`reflect::reflect_traits<math::mat4>`](field-info/reflect-reflect-traits-math-mat4.md) |  |
| [`reflect::reflect_traits<math::quat>`](field-info/reflect-reflect-traits-math-quat.md) |  |
| [`reflect::reflect_traits<math::transform>`](field-info/reflect-reflect-traits-math-transform.md) |  |
| [`reflect::reflect_traits<math::vec2>`](field-info/reflect-reflect-traits-math-vec2.md) |  |
| [`reflect::reflect_traits<math::vec3>`](field-info/reflect-reflect-traits-math-vec3.md) |  |
| [`reflect::reflect_traits<math::vec4>`](field-info/reflect-reflect-traits-math-vec4.md) |  |
| [`reflect::reflect_traits<std::string>`](field-info/reflect-reflect-traits-std-string.md) |  |
| [`reflect::reflect_traits<string_id>`](field-info/reflect-reflect-traits-string-id.md) |  |
| [`reflect::reflect_traits<T>`](field-info/reflect-reflect-traits-t.md) |  |
| [`reflect::reflect_traits<u16>`](field-info/reflect-reflect-traits-u16.md) |  |
| [`reflect::reflect_traits<u32>`](field-info/reflect-reflect-traits-u32.md) |  |
| [`reflect::reflect_traits<u64>`](field-info/reflect-reflect-traits-u64.md) |  |
| [`reflect::reflect_traits<u8>`](field-info/reflect-reflect-traits-u8.md) |  |
| [`reflect::reflect_traits<uuid>`](field-info/reflect-reflect-traits-uuid.md) |  |
| [`std::formatter<conjure::core::reflect::field_kind>`](field-info/std-formatter-conjure-core-reflect-field-kind.md) |  |

## Enumerations

### `reflect::field_kind` {#symbol-reflect-field-kind}

```cpp
enum class field_kind : u8 {
    BOOL,
    I8,
    I16,
    I32,
    I64,
    U8,
    U16,
    U32,
    U64,
    F32,
    F64,
    STRING,         // std::string
    STRING_ID,      // core::string_id
    UUID,           // core::uuid
    VEC2,
    VEC3,
    VEC4,
    QUAT,           // stored x, y, z, w
    MAT4,           // column-major, 16 floats
    TRANSFORM,      // math::transform
    ENUM,           // enum described with CONJURE_REFLECT_ENUM (field_info::enumType)
    STRUCT,         // nested reflected struct (field_info::structType)
    ARRAY,          // std::array<T, N> of any other kind
    ENTITY,         // handle owned by the ecs module (reflect_kind_override)
    ASSET_REF,      // handle owned by the assets module (reflect_kind_override)
    COUNT
};
```

What a field holds. <code>ARRAY</code> fields describe their element in <code>field_info::elementKind</code>.

## Type Aliases

### `reflect::detail::std_array_traits<std::array<ElementType, N>>::element_type` {#symbol-reflect-detail-std-array-traits-std-array-elementtype-n-element-type}

```cpp
using element_type = ElementType;
```

## Concepts

### `reflect::detail::distinct_platform_integer` {#symbol-reflect-detail-distinct-platform-integer}

```cpp
template <typename T> concept distinct_platform_integer = (std::is_same_v<T, long> || std::is_same_v<T, unsigned long> || std::is_same_v<T, long long> || std::is_same_v<T, unsigned long long>) && !std::is_same_v<T, i32> && !std::is_same_v<T, u32> && !std::is_same_v<T, i64> && !std::is_same_v<T, u64>;
```

The platform integer types that are distinct from every fixed-width alias on the current target: <code>long</code> is 32 bits on Windows and 64 bits elsewhere, <code>long long</code> is <code><a href="../types.html#symbol-i64">i64</a></code> on Windows and macOS but not on Linux, <code><a href="../types.html#symbol-usize">usize</a></code> is <code>unsigned long</code> on macOS and Linux. Members of these types reflect as the fixed-width kind of the same width and signedness so a struct with a <code><a href="../types.html#symbol-usize">usize</a></code> or <code>long</code> member describes identically on every platform.

### `reflect::has_reflect_kind` {#symbol-reflect-has-reflect-kind}

```cpp
template <typename T> concept has_reflect_kind = requires { { reflect_traits<std::remove_cvref_t<T>>::KIND } -> std::convertible_to<field_kind>; };
```

True when <code><a href="field-info/reflect-reflect-traits-bool.html">reflect_traits</a>&lt;T&gt;</code> provides a <code>KIND</code> (built-in kinds, reflected structs and enums).

### `reflect::reflectable` {#symbol-reflect-reflectable}

```cpp
template <typename T> concept reflectable = has_kind_override<T> || has_reflect_kind<T> || (std_array<T> && (has_kind_override<typename detail::std_array_traits<std::remove_cvref_t<T>>::element_type> || has_reflect_kind<typename detail::std_array_traits<std::remove_cvref_t<T>>::element_type>));
```

True when <code>T</code> has a field kind: a kind override, a built-in or macro-described kind, or a <code>std::array</code> of such a type (arrays of arrays are not reflectable).

### `reflect::std_array` {#symbol-reflect-std-array}

```cpp
template <typename T> concept std_array = detail::std_array_traits<std::remove_cvref_t<T>>::IS_ARRAY;
```

True for <code>std::array&lt;T, N&gt;</code>.

## Forward Declarations

### `reflect::type_info` {#symbol-reflect-type-info}

```cpp
struct type_info;
```

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-reflect::detail::ALWAYS_FALSE"></a><code>reflect::detail::ALWAYS_FALSE</code> | <code class="signature"><span class="k">template</span> &lt;<wbr><span class="k">typename</span> T&gt; <span class="k">inline</span> <span class="k">constexpr</span> <span class="k">bool</span></code> | <code class="signature">false</code> |  |

## Functions

### Public

#### `reflect::detail::platform_integer_kind` {#function-reflect-detail-platform-integer-kind}

<div class="function-declaration" id="function-reflect-detail-platform-integer-kind-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> T&gt; [[<span class="na">nodiscard</span>]] <span class="k">consteval</span> <a href="field-info.html#symbol-reflect-field-kind">field_kind</a> platform_integer_kind() <span class="k">noexcept</span>;</code></pre></div>

#### `reflect::detail::platform_integer_name` {#function-reflect-detail-platform-integer-name}

<div class="function-declaration" id="function-reflect-detail-platform-integer-name-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> T&gt; [[<span class="na">nodiscard</span>]] <span class="k">consteval</span> std::string_view platform_integer_name() <span class="k">noexcept</span>;</code></pre></div>

#### `reflect::is_custom_kind` {#function-reflect-is-custom-kind}

<div class="function-declaration" id="function-reflect-is-custom-kind-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_custom_kind(<a href="field-info.html#symbol-reflect-field-kind">field_kind</a> kind) <span class="k">noexcept</span>;</code></pre></div>

<p>True for the kinds supplied by other modules through <code><a href="../../assets/asset-ref/conjure-core-reflect-reflect-kind-override-conjure-assets-asset-ref-assettype.html">reflect_kind_override</a></code>.</p>

#### `reflect::is_fixed_size_kind` {#function-reflect-is-fixed-size-kind}

<div class="function-declaration" id="function-reflect-is-fixed-size-kind-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_fixed_size_kind(<a href="field-info.html#symbol-reflect-field-kind">field_kind</a> kind) <span class="k">noexcept</span>;</code></pre></div>

<p>True for the kinds whose size and alignment are fixed by the kind alone.</p>

#### `reflect::is_float_kind` {#function-reflect-is-float-kind}

<div class="function-declaration" id="function-reflect-is-float-kind-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_float_kind(<a href="field-info.html#symbol-reflect-field-kind">field_kind</a> kind) <span class="k">noexcept</span>;</code></pre></div>

<p>True for <code>F32</code> and <code>F64</code>.</p>

#### `reflect::is_integer_kind` {#function-reflect-is-integer-kind}

<div class="function-declaration" id="function-reflect-is-integer-kind-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_integer_kind(<a href="field-info.html#symbol-reflect-field-kind">field_kind</a> kind) <span class="k">noexcept</span>;</code></pre></div>

<p>True for the integer kinds <code>I8</code>..<code>U64</code>.</p>

#### `reflect::is_valid_enum_size` {#function-reflect-is-valid-enum-size}

<div class="function-declaration" id="function-reflect-is-valid-enum-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_valid_enum_size(<a href="../types.html#symbol-u32">u32</a> underlyingSize) <span class="k">noexcept</span>;</code></pre></div>

<p>True for the storage widths an enumeration may have: 1, 2, 4 or 8 bytes. Every consumer of an <code><a href="field-info/reflect-enum-info.html">enum_info</a></code> (registration, layout validation, JSON conversion) rejects other widths.</p>

#### `reflect::kind_alignment` {#function-reflect-kind-alignment}

<div class="function-declaration" id="function-reflect-kind-alignment-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../types.html#symbol-u32">u32</a> kind_alignment(<a href="field-info.html#symbol-reflect-field-kind">field_kind</a> kind) <span class="k">noexcept</span>;</code></pre></div>

<p>Natural alignment of a fixed-size kind; 0 for the kinds whose alignment depends on the field.</p>

#### `reflect::kind_of` {#function-reflect-kind-of}

<div class="function-declaration" id="function-reflect-kind-of-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> T&gt; [[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="field-info.html#symbol-reflect-field-kind">field_kind</a> kind_of() <span class="k">noexcept</span>;</code></pre></div>

<p>The field kind of <code>T</code>, decided at compile time. Fails to compile with a message naming the problem when <code>T</code> is not reflectable.</p>

#### `reflect::kind_size` {#function-reflect-kind-size}

<div class="function-declaration" id="function-reflect-kind-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../types.html#symbol-u32">u32</a> kind_size(<a href="field-info.html#symbol-reflect-field-kind">field_kind</a> kind) <span class="k">noexcept</span>;</code></pre></div>

<p>Size in bytes of a fixed-size kind; 0 for <code>ENUM</code>, <code>STRUCT</code>, <code>ARRAY</code>, <code>ENTITY</code> and <code>ASSET_REF</code> whose size depends on the field.</p>

#### `reflect::parse_field_kind` {#function-reflect-parse-field-kind}

<div class="function-declaration" id="function-reflect-parse-field-kind-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::optional&lt;<a href="field-info.html#symbol-reflect-field-kind">field_kind</a>&gt; parse_field_kind(std::string_view name) <span class="k">noexcept</span>;</code></pre></div>

<p>Parses an enumerator name produced by <code>to_string()</code> (exact match). Returns nullopt otherwise.</p>

#### `reflect::to_string` {#function-reflect-to-string}

<div class="function-declaration" id="function-reflect-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(<a href="field-info.html#symbol-reflect-field-kind">field_kind</a> kind) <span class="k">noexcept</span>;</code></pre></div>

<p>Returns the stable upper-case enumerator name of a kind (&quot;<code>VEC3</code>&quot;); &quot;<code>UNKNOWN</code>&quot; for <code>COUNT</code> and out-of-range values.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="always_false"></a>
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="distinct_platform_integer"></a>
<a id="enum_info"></a>
<a id="enum_infofind_name"></a>
<a id="enum_infofind_value"></a>
<a id="enum_infoformat_value"></a>
<a id="enum_infois_representable"></a>
<a id="enum_infoisflags"></a>
<a id="enum_infoissigned"></a>
<a id="enum_infoname"></a>
<a id="enum_infooperator"></a>
<a id="enum_infoparse_value"></a>
<a id="enum_infotype"></a>
<a id="enum_infounderlyingsize"></a>
<a id="enum_infovalues"></a>
<a id="extension-points"></a>
<a id="field_attributes"></a>
<a id="field_attributescategory"></a>
<a id="field_attributeshidden"></a>
<a id="field_attributesmax"></a>
<a id="field_attributesmin"></a>
<a id="field_attributesoperator"></a>
<a id="field_attributesreadonly"></a>
<a id="field_attributesstep"></a>
<a id="field_attributestooltip"></a>
<a id="field_info"></a>
<a id="field_infoarraycount"></a>
<a id="field_infoattributes"></a>
<a id="field_infodefaultvaluejson"></a>
<a id="field_infoelement_size"></a>
<a id="field_infoelementkind"></a>
<a id="field_infoenumtype"></a>
<a id="field_infois_array"></a>
<a id="field_infokind"></a>
<a id="field_infoname"></a>
<a id="field_infooffset"></a>
<a id="field_infosize"></a>
<a id="field_infostructtype"></a>
<a id="field_infotype"></a>
<a id="field_infovalue_kind"></a>
<a id="field_kind"></a>
<a id="field_kindarray"></a>
<a id="field_kindasset_ref"></a>
<a id="field_kindbool"></a>
<a id="field_kindcount"></a>
<a id="field_kindentity"></a>
<a id="field_kindenum"></a>
<a id="field_kindf32"></a>
<a id="field_kindf64"></a>
<a id="field_kindi16"></a>
<a id="field_kindi32"></a>
<a id="field_kindi64"></a>
<a id="field_kindi8"></a>
<a id="field_kindmat4"></a>
<a id="field_kindquat"></a>
<a id="field_kindstring"></a>
<a id="field_kindstring_id"></a>
<a id="field_kindstruct"></a>
<a id="field_kindtransform"></a>
<a id="field_kindu16"></a>
<a id="field_kindu32"></a>
<a id="field_kindu64"></a>
<a id="field_kindu8"></a>
<a id="field_kinduuid"></a>
<a id="field_kindvec2"></a>
<a id="field_kindvec3"></a>
<a id="field_kindvec4"></a>
<a id="has_reflect_kind"></a>
<a id="internals-and-algorithms"></a>
<a id="is_custom_kind"></a>
<a id="is_fixed_size_kind"></a>
<a id="is_float_kind"></a>
<a id="is_integer_kind"></a>
<a id="is_valid_enum_size"></a>
<a id="kind_alignment"></a>
<a id="kind_of"></a>
<a id="kind_size"></a>
<a id="other-declarations"></a>
<a id="parse_field_kind"></a>
<a id="platform_integer_kind"></a>
<a id="platform_integer_name"></a>
<a id="reflect_traits"></a>
<a id="reflect_traits_1"></a>
<a id="reflect_traits_10"></a>
<a id="reflect_traits_11"></a>
<a id="reflect_traits_12"></a>
<a id="reflect_traits_13"></a>
<a id="reflect_traits_14"></a>
<a id="reflect_traits_15"></a>
<a id="reflect_traits_16"></a>
<a id="reflect_traits_17"></a>
<a id="reflect_traits_18"></a>
<a id="reflect_traits_19"></a>
<a id="reflect_traits_2"></a>
<a id="reflect_traits_3"></a>
<a id="reflect_traits_4"></a>
<a id="reflect_traits_5"></a>
<a id="reflect_traits_6"></a>
<a id="reflect_traits_7"></a>
<a id="reflect_traits_8"></a>
<a id="reflect_traits_9"></a>
<a id="reflect_traitskind"></a>
<a id="reflect_traitskind_1"></a>
<a id="reflect_traitskind_10"></a>
<a id="reflect_traitskind_11"></a>
<a id="reflect_traitskind_12"></a>
<a id="reflect_traitskind_13"></a>
<a id="reflect_traitskind_14"></a>
<a id="reflect_traitskind_15"></a>
<a id="reflect_traitskind_16"></a>
<a id="reflect_traitskind_17"></a>
<a id="reflect_traitskind_18"></a>
<a id="reflect_traitskind_19"></a>
<a id="reflect_traitskind_2"></a>
<a id="reflect_traitskind_3"></a>
<a id="reflect_traitskind_4"></a>
<a id="reflect_traitskind_5"></a>
<a id="reflect_traitskind_6"></a>
<a id="reflect_traitskind_7"></a>
<a id="reflect_traitskind_8"></a>
<a id="reflect_traitskind_9"></a>
<a id="reflect_traitsname"></a>
<a id="reflect_traitsname_1"></a>
<a id="reflect_traitsname_10"></a>
<a id="reflect_traitsname_11"></a>
<a id="reflect_traitsname_12"></a>
<a id="reflect_traitsname_13"></a>
<a id="reflect_traitsname_14"></a>
<a id="reflect_traitsname_15"></a>
<a id="reflect_traitsname_16"></a>
<a id="reflect_traitsname_17"></a>
<a id="reflect_traitsname_18"></a>
<a id="reflect_traitsname_19"></a>
<a id="reflect_traitsname_2"></a>
<a id="reflect_traitsname_3"></a>
<a id="reflect_traitsname_4"></a>
<a id="reflect_traitsname_5"></a>
<a id="reflect_traitsname_6"></a>
<a id="reflect_traitsname_7"></a>
<a id="reflect_traitsname_8"></a>
<a id="reflect_traitsname_9"></a>
<a id="reflectable"></a>
<a id="reflectdetailalways_false"></a>
<a id="reflectdetaildistinct_platform_integer"></a>
<a id="reflectdetailplatform_integer_kind"></a>
<a id="reflectdetailplatform_integer_name"></a>
<a id="reflectdetailstd_array_traits"></a>
<a id="reflectdetailstd_array_traitsis_array"></a>
<a id="reflectdetailstd_array_traitsstdarrayelementtype-n"></a>
<a id="reflectdetailstd_array_traitsstdarrayelementtype-ncount"></a>
<a id="reflectdetailstd_array_traitsstdarrayelementtype-nelement_type"></a>
<a id="reflectdetailstd_array_traitsstdarrayelementtype-nis_array"></a>
<a id="reflectenum_info"></a>
<a id="reflectenum_infofind_name"></a>
<a id="reflectenum_infofind_value"></a>
<a id="reflectenum_infoformat_value"></a>
<a id="reflectenum_infois_representable"></a>
<a id="reflectenum_infoisflags"></a>
<a id="reflectenum_infoissigned"></a>
<a id="reflectenum_infoname"></a>
<a id="reflectenum_infooperator"></a>
<a id="reflectenum_infoparse_value"></a>
<a id="reflectenum_infotype"></a>
<a id="reflectenum_infounderlyingsize"></a>
<a id="reflectenum_infovalues"></a>
<a id="reflectfield_attributes"></a>
<a id="reflectfield_attributescategory"></a>
<a id="reflectfield_attributeshidden"></a>
<a id="reflectfield_attributesmax"></a>
<a id="reflectfield_attributesmin"></a>
<a id="reflectfield_attributesoperator"></a>
<a id="reflectfield_attributesreadonly"></a>
<a id="reflectfield_attributesstep"></a>
<a id="reflectfield_attributestooltip"></a>
<a id="reflectfield_info"></a>
<a id="reflectfield_infoarraycount"></a>
<a id="reflectfield_infoattributes"></a>
<a id="reflectfield_infodefaultvaluejson"></a>
<a id="reflectfield_infoelement_size"></a>
<a id="reflectfield_infoelementkind"></a>
<a id="reflectfield_infoenumtype"></a>
<a id="reflectfield_infois_array"></a>
<a id="reflectfield_infokind"></a>
<a id="reflectfield_infoname"></a>
<a id="reflectfield_infooffset"></a>
<a id="reflectfield_infosize"></a>
<a id="reflectfield_infostructtype"></a>
<a id="reflectfield_infotype"></a>
<a id="reflectfield_infovalue_kind"></a>
<a id="reflectfield_kind"></a>
<a id="reflectfield_kindarray"></a>
<a id="reflectfield_kindasset_ref"></a>
<a id="reflectfield_kindbool"></a>
<a id="reflectfield_kindcount"></a>
<a id="reflectfield_kindentity"></a>
<a id="reflectfield_kindenum"></a>
<a id="reflectfield_kindf32"></a>
<a id="reflectfield_kindf64"></a>
<a id="reflectfield_kindi16"></a>
<a id="reflectfield_kindi32"></a>
<a id="reflectfield_kindi64"></a>
<a id="reflectfield_kindi8"></a>
<a id="reflectfield_kindmat4"></a>
<a id="reflectfield_kindquat"></a>
<a id="reflectfield_kindstring"></a>
<a id="reflectfield_kindstring_id"></a>
<a id="reflectfield_kindstruct"></a>
<a id="reflectfield_kindtransform"></a>
<a id="reflectfield_kindu16"></a>
<a id="reflectfield_kindu32"></a>
<a id="reflectfield_kindu64"></a>
<a id="reflectfield_kindu8"></a>
<a id="reflectfield_kinduuid"></a>
<a id="reflectfield_kindvec2"></a>
<a id="reflectfield_kindvec3"></a>
<a id="reflectfield_kindvec4"></a>
<a id="reflecthas_reflect_kind"></a>
<a id="reflectis_custom_kind"></a>
<a id="reflectis_fixed_size_kind"></a>
<a id="reflectis_float_kind"></a>
<a id="reflectis_integer_kind"></a>
<a id="reflectis_valid_enum_size"></a>
<a id="reflectkind_alignment"></a>
<a id="reflectkind_of"></a>
<a id="reflectkind_size"></a>
<a id="reflectparse_field_kind"></a>
<a id="reflectreflect_traitsbool"></a>
<a id="reflectreflect_traitsboolkind"></a>
<a id="reflectreflect_traitsboolname"></a>
<a id="reflectreflect_traitsf32"></a>
<a id="reflectreflect_traitsf32kind"></a>
<a id="reflectreflect_traitsf32name"></a>
<a id="reflectreflect_traitsf64"></a>
<a id="reflectreflect_traitsf64kind"></a>
<a id="reflectreflect_traitsf64name"></a>
<a id="reflectreflect_traitsi16"></a>
<a id="reflectreflect_traitsi16kind"></a>
<a id="reflectreflect_traitsi16name"></a>
<a id="reflectreflect_traitsi32"></a>
<a id="reflectreflect_traitsi32kind"></a>
<a id="reflectreflect_traitsi32name"></a>
<a id="reflectreflect_traitsi64"></a>
<a id="reflectreflect_traitsi64kind"></a>
<a id="reflectreflect_traitsi64name"></a>
<a id="reflectreflect_traitsi8"></a>
<a id="reflectreflect_traitsi8kind"></a>
<a id="reflectreflect_traitsi8name"></a>
<a id="reflectreflect_traitsmathmat4"></a>
<a id="reflectreflect_traitsmathmat4kind"></a>
<a id="reflectreflect_traitsmathmat4name"></a>
<a id="reflectreflect_traitsmathquat"></a>
<a id="reflectreflect_traitsmathquatkind"></a>
<a id="reflectreflect_traitsmathquatname"></a>
<a id="reflectreflect_traitsmathtransform"></a>
<a id="reflectreflect_traitsmathtransformkind"></a>
<a id="reflectreflect_traitsmathtransformname"></a>
<a id="reflectreflect_traitsmathvec2"></a>
<a id="reflectreflect_traitsmathvec2kind"></a>
<a id="reflectreflect_traitsmathvec2name"></a>
<a id="reflectreflect_traitsmathvec3"></a>
<a id="reflectreflect_traitsmathvec3kind"></a>
<a id="reflectreflect_traitsmathvec3name"></a>
<a id="reflectreflect_traitsmathvec4"></a>
<a id="reflectreflect_traitsmathvec4kind"></a>
<a id="reflectreflect_traitsmathvec4name"></a>
<a id="reflectreflect_traitsstdstring"></a>
<a id="reflectreflect_traitsstdstringkind"></a>
<a id="reflectreflect_traitsstdstringname"></a>
<a id="reflectreflect_traitsstring_id"></a>
<a id="reflectreflect_traitsstring_idkind"></a>
<a id="reflectreflect_traitsstring_idname"></a>
<a id="reflectreflect_traitst"></a>
<a id="reflectreflect_traitstkind"></a>
<a id="reflectreflect_traitstname"></a>
<a id="reflectreflect_traitsu16"></a>
<a id="reflectreflect_traitsu16kind"></a>
<a id="reflectreflect_traitsu16name"></a>
<a id="reflectreflect_traitsu32"></a>
<a id="reflectreflect_traitsu32kind"></a>
<a id="reflectreflect_traitsu32name"></a>
<a id="reflectreflect_traitsu64"></a>
<a id="reflectreflect_traitsu64kind"></a>
<a id="reflectreflect_traitsu64name"></a>
<a id="reflectreflect_traitsu8"></a>
<a id="reflectreflect_traitsu8kind"></a>
<a id="reflectreflect_traitsu8name"></a>
<a id="reflectreflect_traitsuuid"></a>
<a id="reflectreflect_traitsuuidkind"></a>
<a id="reflectreflect_traitsuuidname"></a>
<a id="reflectreflectable"></a>
<a id="reflectstd_array"></a>
<a id="reflectto_string"></a>
<a id="reflecttype_info"></a>
<a id="std"></a>
<a id="std_array"></a>
<a id="std_array_traits"></a>
<a id="std_array_traits_1"></a>
<a id="std_array_traitscount"></a>
<a id="std_array_traitselement_type"></a>
<a id="std_array_traitsis_array"></a>
<a id="std_array_traitsis_array_1"></a>
<a id="stdformat"></a>
<a id="stdformatterconjurecorereflectfield_kind"></a>
<a id="stdformatterconjurecorereflectfield_kindformat"></a>
<a id="structs-and-types"></a>
<a id="t"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="type_info"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
