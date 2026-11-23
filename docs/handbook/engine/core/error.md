---
kind: header
---

[Engine](../../engine.md) → [Core](../core.md)

# error.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/error.h` |
| API ↗ | [Errors and results](../../../api/core/errors-and-results.md) |

<p><code>A</code> recoverable failure: category, code and a human readable message. Engine code returns these through <code>result&lt;T&gt;</code> instead of throwing. <code>A</code> default-constructed <code><a href="error/error.html">error</a></code> means &quot;no <code><a href="error/error.html">error</a></code>&quot;.</p>

## Structs

| File | Description |
|---|---|
| [`error_category_traits<error_code>`](error/error-category-traits-error-code.md) |  |
| [`std::formatter<conjure::core::error>`](error/std-formatter-conjure-core-error.md) |  |
| [`std::formatter<conjure::core::error_code>`](error/std-formatter-conjure-core-error-code.md) |  |

## Classes

| File | Description |
|---|---|
| [`error`](error/error.md) | <code>A</code> recoverable failure: category, code and a human readable message. Engine code returns these through <code>result&lt;T&gt;</code> instead of throwing. <code>A</code> default-constructed <code><a href="error/error.html">error</a></code> means &quot;no <code><a href="error/error.html">error</a></code>&quot;. |

## Enumerations

### `error_code` {#symbol-error-code}

```cpp
enum class error_code : u16 {
    NONE = 0,
    UNKNOWN,
    INVALID_ARGUMENT,
    INVALID_STATE,
    NOT_FOUND,
    ALREADY_EXISTS,
    OUT_OF_RANGE,
    OUT_OF_MEMORY,
    UNSUPPORTED,
    NOT_IMPLEMENTED,
    IO_FAILURE,
    PARSE_FAILURE,
    TIMEOUT,
    PERMISSION_DENIED,
    BACKEND_FAILURE,
    DEPENDENCY_FAILURE,
    CANCELLED,
    COUNT
};
```

Generic failures that any module may return. Module-specific conditions get their own enum.

## Type Aliases

### `error_code_name_fn` {#symbol-error-code-name-fn}

```cpp
using error_code_name_fn = std::string_view (*)(u32 code) noexcept;
```

Function that names a code of one category. Returns &quot;&quot; for unknown values.

## Concepts

### `error_enum` {#symbol-error-enum}

```cpp
template <typename EnumType> concept error_enum = std::is_enum_v<EnumType> && requires { { error_category_traits<EnumType>::NAME } -> std::convertible_to<std::string_view>; { error_category_traits<EnumType>::ID } -> std::convertible_to<u32>; { error_category_traits<EnumType>::code_name(u32{}) } -> std::convertible_to<std::string_view>; };
```

## Forward Declarations

### `error_category_traits` {#symbol-error-category-traits}

```cpp
template <typename EnumType> struct error_category_traits;
```

Binds an <code><a href="error/error.html">error</a></code> enumeration to a category. Specialised through <code><a href="../assets/assets-error.html#symbol-conjure-declare-error-category">CONJURE_DECLARE_ERROR_CATEGORY</a></code>.

## Functions

### Public

#### `detail::core_error_code_name` {#function-detail-core-error-code-name}

<div class="function-declaration" id="function-detail-core-error-code-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view core_error_code_name(<a href="types.html#symbol-u32">u32</a> code) <span class="k">noexcept</span>;</code></pre></div>

#### `make_error` {#function-make-error}

<div class="function-declaration" id="function-make-error-1"><pre><code><span class="k">template</span> &lt;<a href="error.html#symbol-error-enum">error_enum</a> EnumType, <span class="k">typename</span>... Args&gt; [[<span class="na">nodiscard</span>]] <a href="error/error.html">error</a> make_error(
    EnumType code,
    std::format_string&lt;Args...&gt; format,
    Args &amp;&amp;...args);</code></pre></div>

<p>Builds an <code><a href="error/error.html">error</a></code> from an enumerator with a <code>std::format</code> style message.</p>

<div class="function-declaration" id="function-make-error-2"><pre><code><span class="k">template</span> &lt;<a href="error.html#symbol-error-enum">error_enum</a> EnumType&gt; [[<span class="na">nodiscard</span>]] <a href="error/error.html">error</a> make_error(
    EnumType code,
    std::string_view detail = {});</code></pre></div>

<p>Builds an <code><a href="error/error.html">error</a></code> from an enumerator with a fixed detail message (possibly empty).</p>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="error.html#symbol-error-code">error_code</a> code) <span class="k">noexcept</span>;</code></pre></div>

<p>Returns the stable, upper-case name of a generic <code><a href="error/error.html">error</a></code> code (e.g. &quot;<code>NOT_FOUND</code>&quot;).</p>

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
<a id="core_error_code_name"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="detailcore_error_code_name"></a>
<a id="direct-header-dependencies"></a>
<a id="error"></a>
<a id="error_category_traits"></a>
<a id="error_category_traits_1"></a>
<a id="error_category_traitscode_name"></a>
<a id="error_category_traitserror_code"></a>
<a id="error_category_traitserror_codecode_name"></a>
<a id="error_category_traitserror_codeid"></a>
<a id="error_category_traitserror_codename"></a>
<a id="error_category_traitsid"></a>
<a id="error_category_traitsname"></a>
<a id="error_code"></a>
<a id="error_code_name_fn"></a>
<a id="error_codealready_exists"></a>
<a id="error_codebackend_failure"></a>
<a id="error_codecancelled"></a>
<a id="error_codecount"></a>
<a id="error_codedependency_failure"></a>
<a id="error_codeinvalid_argument"></a>
<a id="error_codeinvalid_state"></a>
<a id="error_codeio_failure"></a>
<a id="error_codenone"></a>
<a id="error_codenot_found"></a>
<a id="error_codenot_implemented"></a>
<a id="error_codeout_of_memory"></a>
<a id="error_codeout_of_range"></a>
<a id="error_codeparse_failure"></a>
<a id="error_codepermission_denied"></a>
<a id="error_codetimeout"></a>
<a id="error_codeunknown"></a>
<a id="error_codeunsupported"></a>
<a id="error_enum"></a>
<a id="errorcategory"></a>
<a id="errorcategory_name"></a>
<a id="errorcode"></a>
<a id="errorcode_name"></a>
<a id="errorerror"></a>
<a id="errorerror_1"></a>
<a id="errorfrom"></a>
<a id="errorgeneric_code"></a>
<a id="erroris"></a>
<a id="erroris_category"></a>
<a id="errormcategory"></a>
<a id="errormcategoryname"></a>
<a id="errormcode"></a>
<a id="errormcodename"></a>
<a id="errormessage"></a>
<a id="errormmessage"></a>
<a id="erroroperator"></a>
<a id="erroroperator_1"></a>
<a id="errorto_string"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="make_error"></a>
<a id="other-declarations"></a>
<a id="std"></a>
<a id="stdformat"></a>
<a id="stdformatterconjurecoreerror"></a>
<a id="stdformatterconjurecoreerror_code"></a>
<a id="stdformatterconjurecoreerror_codeformat"></a>
<a id="stdformatterconjurecoreerrorformat"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
