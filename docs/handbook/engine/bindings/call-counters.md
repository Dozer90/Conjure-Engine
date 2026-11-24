---
kind: header
---

[Engine](../../engine.md) → [Bindings](../bindings.md)

# call_counters.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/bindings/call_counters.h` |
| API ↗ | [The C# API](../../../api/bindings/csharp-api.md), [Scripting from C#: the bindings](../../../api/bindings/index.md), [The native table](../../../api/bindings/native-table.md) |

<p>Call counters of the native table (3.5, B6). Every entry of <code>native_api.inl</code>, core and bindings alike, has an index (<code><a href="call-counters.html#symbol-native-function-id">native_function_id</a></code>, its .inl line order, the same value as the <code>C</code># NativeFunction enum and the manifest&#x27;s &quot;index&quot;) and a descriptor. When !<code>CONJURE_DISTRIBUTION</code> the subsystem replaces every filled slot with <code><a href="call-counters/counting-thunk-id-ret-args.html">counting_thunk</a>&lt;Id, Fn&gt;</code>::invoke, a forwarding function that increments sCounters[Id] and calls the original; in Distribution the raw pointers stay and <code>call_counters()</code> is empty. Counters are plain <code><a href="../core/types.html#symbol-u32">u32</a></code> because every entry is main-thread.</p>

## Structs

| File | Description |
|---|---|
| [`counting_thunk<Id, Ret(Args...)>`](call-counters/counting-thunk-id-ret-args.md) |  |
| [`native_entry_descriptor`](call-counters/native-entry-descriptor.md) | One row of the table: its id, group and name, the normalised &quot;&lt;return&gt; &lt;arguments&gt;&quot; signature of the generator and whether the bindings module owns the entry. |

## Enumerations

### `native_function_id` {#symbol-native-function-id}

```cpp
enum class native_function_id : u16 {
#define CONJURE_BINDINGS_FUNCTION_ID(group, name, signature) group##_##name,
    CONJURE_NATIVE_API_FOR_EACH_ENTRY(CONJURE_BINDINGS_FUNCTION_ID)
#undef CONJURE_BINDINGS_FUNCTION_ID
    COUNT
};
```

Index of every entry of the table (one enumerator per <code>CONJURE_NATIVE_FN</code> line, in table order).

## Type Aliases

### `counting_thunk<Id, Ret(Args...)>::function_pointer` {#symbol-counting-thunk-id-ret-args-function-pointer}

```cpp
using function_pointer = Ret (*)(Args...);
```

## Forward Declarations

### `counting_thunk` {#symbol-counting-thunk}

```cpp
template <native_function_id Id, typename Fn> struct counting_thunk;
```

The forwarding function installed over an entry&#x27;s slot (B6): one unique instantiation per entry, increments the entry&#x27;s counter and calls the original pointer kept in <code>sTarget</code>. Specialised on the function type <code>Ret(Args...)</code> that the .inl&#x27;s return and argument lists form.

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-NATIVE_FUNCTION_COUNT"></a><code>NATIVE_FUNCTION_COUNT</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature">static_<wbr>cast&lt;<wbr><a href="../core/types.html#symbol-u32">u32</a>&gt;(native_<wbr>function_<wbr>id::<wbr>COUNT)</code> | Entries in the table (core and bindings together). |

## Functions

### Public

#### `call_count` {#function-call-count}

<div class="function-declaration" id="function-call-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/types.html#symbol-u32">u32</a> call_count(
    <a href="call-counters.html#symbol-native-function-id">native_function_id</a> id) <span class="k">noexcept</span>;</code></pre></div>

<p>Calls recorded for one entry since the last reset; 0 in Distribution and for out-of-range ids.</p>

#### `call_counters` {#function-call-counters}

<div class="function-declaration" id="function-call-counters-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::span&lt;<span class="k">const</span> <a href="../core/types.html#symbol-u32">u32</a>&gt; call_counters() <span class="k">noexcept</span>;</code></pre></div>

<p>The counters, <code>NATIVE_FUNCTION_COUNT</code> entries indexed by <code><a href="call-counters.html#symbol-native-function-id">native_function_id</a></code>; empty in Distribution.</p>

#### `detail::counter_storage` {#function-detail-counter-storage}

<div class="function-declaration" id="function-detail-counter-storage-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::array&lt;u32, NATIVE_FUNCTION_COUNT + <span class="mi">1</span>&gt; &amp;counter_storage() <span class="k">noexcept</span>;</code></pre></div>

<p>The counter storage the thunks increment (one slot per entry plus one spare for <code>COUNT</code> so the index never needs a check).</p>

#### `find_function` {#function-find-function}

<div class="function-declaration" id="function-find-function-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="call-counters.html#symbol-native-function-id">native_function_id</a> find_function(
    std::string_view fullName) <span class="k">noexcept</span>;</code></pre></div>

<p>The id of the entry named &quot;&lt;group&gt;<code>_&lt;name&gt;</code>&quot;, or <code>COUNT</code>.</p>

#### `function_name` {#function-function-name}

<div class="function-declaration" id="function-function-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view function_name(
    <a href="call-counters.html#symbol-native-function-id">native_function_id</a> id) <span class="k">noexcept</span>;</code></pre></div>

<p>&quot;<code>transform_set_local</code>&quot;: the <code>C</code> name of an entry; &quot;&quot; for an out-of-range id.</p>

#### `is_bindings_owned` {#function-is-bindings-owned}

<div class="function-declaration" id="function-is-bindings-owned-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_bindings_owned(
    <a href="call-counters.html#symbol-native-function-id">native_function_id</a> id) <span class="k">noexcept</span>;</code></pre></div>

<p>True when the entry belongs to one of the eleven bindings groups (everything after the marker).</p>

#### `native_entry_descriptor_of` {#function-native-entry-descriptor-of}

<div class="function-declaration" id="function-native-entry-descriptor-of-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">const</span> <a href="call-counters/native-entry-descriptor.html">native_entry_descriptor</a> &amp;native_entry_descriptor_of(
    <a href="call-counters.html#symbol-native-function-id">native_function_id</a> id) <span class="k">noexcept</span>;</code></pre></div>

<p>The descriptor of an id; the <code>COUNT</code> descriptor (empty names) for an out-of-range id.</p>

#### `native_entry_descriptors` {#function-native-entry-descriptors}

<div class="function-declaration" id="function-native-entry-descriptors-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::span&lt;<span class="k">const</span> <a href="call-counters/native-entry-descriptor.html">native_entry_descriptor</a>&gt; native_entry_descriptors() <span class="k">noexcept</span>;</code></pre></div>

<p>Every entry in table order.</p>

#### `reset_call_counters` {#function-reset-call-counters}

<div class="function-declaration" id="function-reset-call-counters-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> reset_call_counters() <span class="k">noexcept</span>;</code></pre></div>

<p>Zeroes every counter (headless fixtures call it between cases).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="call_count"></a>
<a id="call_counters"></a>
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="counter_storage"></a>
<a id="counting_thunk"></a>
<a id="counting_thunk_1"></a>
<a id="counting_thunkfunction_pointer"></a>
<a id="counting_thunkid-retargs"></a>
<a id="counting_thunkid-retargsfunction_pointer"></a>
<a id="counting_thunkid-retargsinvoke"></a>
<a id="counting_thunkid-retargsstarget"></a>
<a id="counting_thunkinvoke"></a>
<a id="counting_thunkstarget"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="detailcounter_storage"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="find_function"></a>
<a id="function_name"></a>
<a id="internals-and-algorithms"></a>
<a id="is_bindings_owned"></a>
<a id="native_entry_descriptor"></a>
<a id="native_entry_descriptor_of"></a>
<a id="native_entry_descriptorgroup"></a>
<a id="native_entry_descriptorid"></a>
<a id="native_entry_descriptorname"></a>
<a id="native_entry_descriptorownedbybindings"></a>
<a id="native_entry_descriptors"></a>
<a id="native_entry_descriptorsignature"></a>
<a id="native_function_count"></a>
<a id="native_function_id"></a>
<a id="native_function_idconjure_native_api_for_each_entry"></a>
<a id="other-declarations"></a>
<a id="reset_call_counters"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
