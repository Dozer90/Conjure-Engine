---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# type_registry.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/reflect/type_registry.h` |
| API ↗ | [Reflection](../../../../api/core/reflection.md) |

<p>The type registry.</p>



## Classes

| File | Description |
|---|---|
| [`reflect::type_registry`](type-registry/reflect-type-registry.md) | Registered struct and enum descriptions with stable addresses and lock-free lookup. |

## Forward Declarations

### `reflect::type_registry::state` {#symbol-reflect-type-registry-state}

```cpp
struct state;
```

## Functions

### Public

#### `reflect::detail::register_described_enum` {#function-reflect-detail-register-described-enum}

<div class="function-declaration" id="function-reflect-detail-register-described-enum-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">const</span> <a href="field-info/reflect-enum-info.html">enum_info</a> *register_described_enum(<a href="field-info/reflect-enum-info.html">enum_info</a> info) <span class="k">noexcept</span>;</code></pre></div>

<p>Registers a macro-built enumeration description in <code>registry()</code>; logs an ERROR and returns <code>nullptr</code> when the registry rejects it.</p>

#### `reflect::detail::register_described_type` {#function-reflect-detail-register-described-type}

<div class="function-declaration" id="function-reflect-detail-register-described-type-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">const</span> <a href="type-info/reflect-type-info.html">type_info</a> *register_described_type(<a href="type-info/reflect-type-info.html">type_info</a> info) <span class="k">noexcept</span>;</code></pre></div>

<p>Registers a macro-built struct description in <code>registry()</code>; logs an ERROR and returns <code>nullptr</code> when the registry rejects it.</p>

#### `reflect::enum_info_of` {#function-reflect-enum-info-of}

<div class="function-declaration" id="function-reflect-enum-info-of-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> T&gt; [[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="field-info/reflect-enum-info.html">enum_info</a> *enum_info_of() <span class="k">noexcept</span>;</code></pre></div>

<p>The registered description of a <code><a href="../../audio/components.html#symbol-conjure-reflect-enum">CONJURE_REFLECT_ENUM</a></code> type, registering it on first use. <code>nullptr</code> for enumerations without a description and when registration failed. Like <code>type_info_of()</code>, failures and removals are not cached.</p>

#### `reflect::registry` {#function-reflect-registry}

<div class="function-declaration" id="function-reflect-registry-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="type-registry/reflect-type-registry.html">type_registry</a> &amp;registry() <span class="k">noexcept</span>;</code></pre></div>

<p>The process-wide registry that <code>type_info_of()</code> and <code>enum_info_of()</code> fill.</p>

#### `reflect::type_info_of` {#function-reflect-type-info-of}

<div class="function-declaration" id="function-reflect-type-info-of-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> T&gt; [[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="type-info/reflect-type-info.html">type_info</a> *type_info_of() <span class="k">noexcept</span>;</code></pre></div>

<p>The registered description of a <code><a href="../../audio/components.html#symbol-conjure-reflect-struct">CONJURE_REFLECT_STRUCT</a></code> type, registering it in <code>registry()</code> on first use (thread-safe). <code>nullptr</code> for types without a description and when registration failed (a different type already owns the same name). Neither a failure nor a removal is cached: the cached pointer is re-validated against <code>registry()</code> on every call (one lock-free lookup), so the type registers again once the conflicting type is gone, and after <code>registry()</code>.<code>unregister_type()</code> (module unload) the next call returns the fresh registration.</p>

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
<a id="enum_info_of"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="reflectdetailregister_described_enum"></a>
<a id="reflectdetailregister_described_type"></a>
<a id="reflectenum_info_of"></a>
<a id="reflectregistry"></a>
<a id="reflecttype_info_of"></a>
<a id="reflecttype_registry"></a>
<a id="reflecttype_registryenum_count"></a>
<a id="reflecttype_registryenums"></a>
<a id="reflecttype_registryfind"></a>
<a id="reflecttype_registryfind_enum"></a>
<a id="reflecttype_registrymstate"></a>
<a id="reflecttype_registryon_registered"></a>
<a id="reflecttype_registryon_unregistered"></a>
<a id="reflecttype_registryoperator"></a>
<a id="reflecttype_registryregister_enum"></a>
<a id="reflecttype_registryregister_type"></a>
<a id="reflecttype_registrystate"></a>
<a id="reflecttype_registrytype_count"></a>
<a id="reflecttype_registrytype_registry"></a>
<a id="reflecttype_registrytype_registry_1"></a>
<a id="reflecttype_registrytypes"></a>
<a id="reflecttype_registryunregister_enum"></a>
<a id="reflecttype_registryunregister_type"></a>
<a id="register_described_enum"></a>
<a id="register_described_type"></a>
<a id="registry"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="type_info_of"></a>
<a id="type_registry"></a>
<a id="type_registryenum_count"></a>
<a id="type_registryenums"></a>
<a id="type_registryfind"></a>
<a id="type_registryfind_enum"></a>
<a id="type_registrymstate"></a>
<a id="type_registryon_registered"></a>
<a id="type_registryon_unregistered"></a>
<a id="type_registryoperator"></a>
<a id="type_registryregister_enum"></a>
<a id="type_registryregister_type"></a>
<a id="type_registrystate"></a>
<a id="type_registrytype_count"></a>
<a id="type_registrytype_registry"></a>
<a id="type_registrytype_registry_1"></a>
<a id="type_registrytypes"></a>
<a id="type_registryunregister_enum"></a>
<a id="type_registryunregister_type"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
