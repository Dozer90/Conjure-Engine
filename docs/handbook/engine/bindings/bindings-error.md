---
kind: header
---

[Engine](../../engine.md) → [Bindings](../bindings.md)

# bindings_error.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/bindings/bindings_error.h` |
| API ↗ | [Scripting from C#: the bindings](../../../api/bindings/index.md), [The native table](../../../api/bindings/native-table.md) |

<p>The status codes (<code><a href="bindings-error.html#symbol-bindings-error">bindings_error</a>.inl</code>, append-only).</p>

## Enumerations

### `bindings_error` {#symbol-bindings-error}

```cpp
enum class bindings_error : u16 {
#define CONJURE_BINDINGS_STATUS_ENUMERATOR(name) name,
    CONJURE_BINDINGS_FOR_EACH_STATUS(CONJURE_BINDINGS_STATUS_ENUMERATOR)
#undef CONJURE_BINDINGS_STATUS_ENUMERATOR
    COUNT
};
```

The status codes (<code><a href="bindings-error.html#symbol-bindings-error">bindings_error</a>.inl</code>, append-only).

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-BINDINGS_ERROR_COUNT"></a><code>BINDINGS_ERROR_COUNT</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature">static_<wbr>cast&lt;<wbr><a href="../core/types.html#symbol-u32">u32</a>&gt;(bindings_<wbr>error::<wbr>COUNT)</code> | Number of status codes (<code>COUNT</code> itself excluded). |
| <a id="variable-LAST_ERROR_CAPACITY"></a><code>LAST_ERROR_CAPACITY</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">512</span></code> | Capacity of the last-error message buffer, terminator included (at most 511 message bytes). |

## Macros

### `CONJURE_DECLARE_ERROR_CATEGORY` {#symbol-conjure-declare-error-category}

```cpp
CONJURE_DECLARE_ERROR_CATEGORY(conjure::bindings::bindings_error, "bindings", conjure::bindings::bindings_error_name)
```

## Functions

### Public

#### `bindings_error_name` {#function-bindings-error-name}

<div class="function-declaration" id="function-bindings-error-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view bindings_error_name(
    <a href="bindings-error.html#symbol-bindings-error">bindings_error</a> code) <span class="k">noexcept</span>;</code></pre></div>

<p>The stable upper-case enumerator name of a code (&quot;<code>ENTITY_NOT_ALIVE</code>&quot;); &quot;&quot; for unknown values.</p>

#### `clear_last_error` {#function-clear-last-error}

<div class="function-declaration" id="function-clear-last-error-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> clear_last_error() <span class="k">noexcept</span>;</code></pre></div>

<p>Forgets the last failure.</p>

#### `fail` {#function-fail}

<div class="function-declaration" id="function-fail-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> int32_t fail(
    <a href="bindings-error.html#symbol-bindings-error">bindings_error</a> code,
    std::string_view entry,
    std::string_view detail = {}) <span class="k">noexcept</span>;</code></pre></div>

<p>Records &quot;&lt;entry&gt;: &lt;detail&gt;&quot; (detail may be empty) as the last error and returns <code><a href="#function-to-status-1">to_status</a>(code)</code>. Never allocates; a message longer than 511 bytes is truncated. Main thread only.</p>

<div class="function-declaration" id="function-fail-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> int32_t fail(
    <span class="k">const</span> <a href="../core/error/error.html">core::error</a> &amp;failure,
    std::string_view entry) <span class="k">noexcept</span>;</code></pre></div>

<p>Maps a <code><a href="../core/error/error.html">core::error</a></code> of another module (2.2: ecs <code>ENTITY_NOT_ALIVE</code> / <code>STRUCTURE_LOCKED</code> / <code>WRONG_THREAD</code> / <code>COMPONENT_NOT_PRESENT</code>, assets <code>NOT_FOUND</code> / <code>TYPE_MISMATCH</code>, physics <code>NO_BODY</code> / <code>NOT_A_CHARACTER</code>, physics <code>WRONG_BODY_KIND</code> (to <code>INVALID_ARGUMENT</code>), scene <code>INVALID_ENTITY</code> / <code>SCENE_NOT_LOADED</code> / <code>PREFAB_NOT_LOADED</code>, everything else <code>INTERNAL</code>) and records its <code>to_string()</code> as the detail.</p>

#### `from_status` {#function-from-status}

<div class="function-declaration" id="function-from-status-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="bindings-error.html#symbol-bindings-error">bindings_error</a> from_status(
    int32_t status) <span class="k">noexcept</span>;</code></pre></div>

<p>The code behind a negative status; <code>NONE</code> for a success value, <code>INTERNAL</code> for an unknown code (<code>INT32_MIN</code> included).</p>

#### `last_error_code` {#function-last-error-code}

<div class="function-declaration" id="function-last-error-code-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="bindings-error.html#symbol-bindings-error">bindings_error</a> last_error_code() <span class="k">noexcept</span>;</code></pre></div>

<p>The code of the last recorded failure (<code>NONE</code> after <code>clear_last_error</code>).</p>

#### `last_error_message` {#function-last-error-message}

<div class="function-declaration" id="function-last-error-message-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view last_error_message() <span class="k">noexcept</span>;</code></pre></div>

<p>The message of the last recorded failure (empty after <code>clear_last_error</code>). The view stays valid until the next <code><a href="#function-fail-1">fail</a>()</code> or <code><a href="#function-clear-last-error-1">clear_last_error</a>()</code>.</p>

#### `map_error` {#function-map-error}

<div class="function-declaration" id="function-map-error-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="bindings-error.html#symbol-bindings-error">bindings_error</a> map_error(
    <span class="k">const</span> <a href="../core/error/error.html">core::error</a> &amp;failure) <span class="k">noexcept</span>;</code></pre></div>

<p>The code a <code><a href="../core/error/error.html">core::error</a></code> maps to (the table of <code><a href="#function-fail-1">fail</a>(const <a href="../core/error/error.html">core::error</a> &amp;, ...)</code>).</p>

#### `to_status` {#function-to-status}

<div class="function-declaration" id="function-to-status-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> int32_t to_status(
    <a href="bindings-error.html#symbol-bindings-error">bindings_error</a> code) <span class="k">noexcept</span>;</code></pre></div>

<p>The ABI status of a code: -<code>static_cast&lt;int32_t&gt;(code)</code>, so <code>NONE</code> is 0 (B4).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="bindings_error"></a>
<a id="bindings_error_count"></a>
<a id="bindings_error_name"></a>
<a id="bindings_errorconjure_bindings_for_each_status"></a>
<a id="classes"></a>
<a id="clear_last_error"></a>
<a id="conjure_declare_error_category"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="fail"></a>
<a id="from_status"></a>
<a id="internals-and-algorithms"></a>
<a id="last_error_capacity"></a>
<a id="last_error_code"></a>
<a id="last_error_message"></a>
<a id="map_error"></a>
<a id="other-declarations"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_status"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
