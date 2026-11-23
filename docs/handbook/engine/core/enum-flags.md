---
kind: header
---

[Engine](../../engine.md) → [Core](../core.md)

# enum_flags.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/enum_flags.h` |
| API ↗ | [Data types](../../../api/core/data-types.md) |

<p>Bitwise operators and helpers for enumerations used as flag sets.</p>

## Functions

### Public

#### `has_any_flag` {#function-has-any-flag}

<div class="function-declaration" id="function-has-any-flag-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> EnumType&gt; <span class="k">requires</span> std::is_enum_v&lt;EnumType&gt; [[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> has_any_flag(
    EnumType value,
    EnumType flags) <span class="k">noexcept</span>;</code></pre></div>

<p>True when at least one bit of <code>flags</code> is set in <code>value</code>.</p>

#### `has_flag` {#function-has-flag}

<div class="function-declaration" id="function-has-flag-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> EnumType&gt; <span class="k">requires</span> std::is_enum_v&lt;EnumType&gt; [[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> has_flag(
    EnumType value,
    EnumType flag) <span class="k">noexcept</span>;</code></pre></div>

<p>True when every bit of <code>flag</code> is set in <code>value</code>.</p>

#### `to_underlying` {#function-to-underlying}

<div class="function-declaration" id="function-to-underlying-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> EnumType&gt; <span class="k">requires</span> std::is_enum_v&lt;EnumType&gt; [[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> std::underlying_type_t&lt;EnumType&gt; to_underlying(
    EnumType value) <span class="k">noexcept</span>;</code></pre></div>

<p>Converts an enumeration value to its underlying integer.</p>

#### `with_flag` {#function-with-flag}

<div class="function-declaration" id="function-with-flag-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> EnumType&gt; <span class="k">requires</span> std::is_enum_v&lt;EnumType&gt; [[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> EnumType with_flag(
    EnumType value,
    EnumType flag) <span class="k">noexcept</span>;</code></pre></div>

<p>Returns <code>value</code> with the bits of <code>flag</code> set.</p>

#### `without_flag` {#function-without-flag}

<div class="function-declaration" id="function-without-flag-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> EnumType&gt; <span class="k">requires</span> std::is_enum_v&lt;EnumType&gt; [[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> EnumType without_flag(
    EnumType value,
    EnumType flag) <span class="k">noexcept</span>;</code></pre></div>

<p>Returns <code>value</code> with the bits of <code>flag</code> cleared.</p>

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
<a id="extension-points"></a>
<a id="has_any_flag"></a>
<a id="has_flag"></a>
<a id="internals-and-algorithms"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_underlying"></a>
<a id="variables-and-properties"></a>
<a id="with_flag"></a>
<a id="without_flag"></a>
<!-- doc-legacy-anchors:end -->
