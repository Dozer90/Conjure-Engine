---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# type_info.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/reflect/type_info.h` |
| API ↗ | [Reflection](../../../../api/core/reflection.md) |

<p>Struct descriptions.</p>

## Structs

| File | Description |
|---|---|
| [`reflect::type_info`](type-info/reflect-type-info.md) | Layout and fields of a reflected struct. |

## Concepts

### `reflect::reflected_enum` {#symbol-reflect-reflected-enum}

```cpp
template <typename T> concept reflected_enum = has_reflect_kind<T> && requires { requires reflect_traits<std::remove_cvref_t<T>>::KIND == field_kind::ENUM; { reflect_traits<std::remove_cvref_t<T>>::describe() } -> std::same_as<enum_info>; };
```

True for enumerations described with <code><a href="../../audio/components.html#symbol-conjure-reflect-enum">CONJURE_REFLECT_ENUM</a></code> / <code><a href="../../physics/character-controller.html#symbol-conjure-reflect-flags">CONJURE_REFLECT_FLAGS</a></code>.

### `reflect::reflected_struct` {#symbol-reflect-reflected-struct}

```cpp
template <typename T> concept reflected_struct = has_reflect_kind<T> && requires { requires reflect_traits<std::remove_cvref_t<T>>::KIND == field_kind::STRUCT; { reflect_traits<std::remove_cvref_t<T>>::describe() } -> std::same_as<type_info>; };
```

True for structs described with <code><a href="../../audio/components.html#symbol-conjure-reflect-struct">CONJURE_REFLECT_STRUCT</a></code>.

## Functions

### Public

#### `reflect::build_type_info` {#function-reflect-build-type-info}

<div class="function-declaration" id="function-reflect-build-type-info-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../result/result.html">result</a>&lt;<a href="type-info/reflect-type-info.html">type_info</a>&gt; build_type_info( std::string name, <a href="../types.html#symbol-u32">u32</a> size, <a href="../types.html#symbol-u32">u32</a> alignment, std::span&lt;<span class="k">const</span> <a href="field-info/reflect-field-info.html">field_info</a>&gt; fields );</code></pre></div>

<p>Builds and validates a description at runtime (dynamic <code>C</code># components). The type id is derived from the name, <code>isTriviallyCopyable</code> from the fields. Fails with <code>INVALID_ARGUMENT</code> when the layout is inconsistent (see <code>validate_layout</code>).</p>

#### `reflect::describe_enum` {#function-reflect-describe-enum}

<div class="function-declaration" id="function-reflect-describe-enum-1"><pre><code><span class="k">template</span> &lt;<a href="type-info.html#symbol-reflect-reflected-enum">reflected_enum</a> T&gt; [[<span class="na">nodiscard</span>]] <a href="field-info/reflect-enum-info.html">enum_info</a> describe_enum();</code></pre></div>

<p><code>A</code> fresh description of a macro-reflected enumeration, without registering it.</p>

#### `reflect::describe_struct` {#function-reflect-describe-struct}

<div class="function-declaration" id="function-reflect-describe-struct-1"><pre><code><span class="k">template</span> &lt;<a href="type-info.html#symbol-reflect-reflected-struct">reflected_struct</a> T&gt; [[<span class="na">nodiscard</span>]] <a href="type-info/reflect-type-info.html">type_info</a> describe_struct();</code></pre></div>

<p><code>A</code> fresh description of a macro-reflected struct, without registering it. Nested struct and enum fields still point at the registered descriptions of their types.</p>

#### `reflect::validate_layout` {#function-reflect-validate-layout}

<div class="function-declaration" id="function-reflect-validate-layout-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../result/result.html">result</a>&lt;<span class="k">void</span>&gt; validate_layout(<span class="k">const</span> <a href="type-info/reflect-type-info.html">type_info</a> &amp;info);</code></pre></div>

<p>Checks a description for internal consistency: non-empty names usable in field paths, every field inside the struct, no overlapping fields, natural alignment of every field (and no field stricter than the type&#x27;s own alignment), a size that matches the kind (fixed kinds, enums, nested structs and arrays), struct/enum pointers present where the kind needs them, field ids matching the pointed-to description, enum widths of 1, 2, 4 or 8 bytes, and a unit attribute (<code><a href="field-info.html#symbol-reflect-field-unit">field_unit</a></code>) other than <code>NONE</code> only on <code>F32</code> and <code>F64</code> fields and arrays of them. Fails with <code>INVALID_ARGUMENT</code> naming the offending field.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="build_type_info"></a>
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="describe_enum"></a>
<a id="describe_struct"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="reflectbuild_type_info"></a>
<a id="reflectdescribe_enum"></a>
<a id="reflectdescribe_struct"></a>
<a id="reflected_enum"></a>
<a id="reflected_struct"></a>
<a id="reflectreflected_enum"></a>
<a id="reflectreflected_struct"></a>
<a id="reflecttype_info"></a>
<a id="reflecttype_infoalignment"></a>
<a id="reflecttype_infofields"></a>
<a id="reflecttype_infofind_field"></a>
<a id="reflecttype_infohas_same_layout"></a>
<a id="reflecttype_infoistriviallycopyable"></a>
<a id="reflecttype_infolayout_hash"></a>
<a id="reflecttype_infoname"></a>
<a id="reflecttype_infosize"></a>
<a id="reflecttype_infotype"></a>
<a id="reflectvalidate_layout"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="type_info"></a>
<a id="type_infoalignment"></a>
<a id="type_infofields"></a>
<a id="type_infofind_field"></a>
<a id="type_infohas_same_layout"></a>
<a id="type_infoistriviallycopyable"></a>
<a id="type_infolayout_hash"></a>
<a id="type_infoname"></a>
<a id="type_infosize"></a>
<a id="type_infotype"></a>
<a id="validate_layout"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
