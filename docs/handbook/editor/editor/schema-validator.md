---
kind: header
---

[Editor](../../editor.md) → [Commands and Undo](../commands-and-undo.md)

# schema_validator.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/schema_validator.h` |
| API ↗ | [Headless mode and the command model](../../../api/editor/headless-and-commands.md), [The editor](../../../api/editor/index.md) |

<p>The hand-written JSON Schema subset: type (one name or an array of names), properties, required, additionalProperties (default <code>false</code> at the top level of a command&#x27;s args, <code>true</code> below), items, enum, minimum, maximum, minItems, maxItems, oneOf (shallow), plus the documentation keywords description, title and default. Any other keyword fails compilation, so a typo in a descriptor is caught at registration rather than silently accepted.</p>



## Classes

| File | Description |
|---|---|
| [`compiled_schema`](schema-validator/compiled-schema.md) | <code>A</code> parsed and checked schema. |

## Functions

### Public

#### `compile_schema` {#function-compile-schema}

<div class="function-declaration" id="function-compile-schema-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../../engine/core/result/result.html">core::result</a>&lt;<a href="schema-validator/compiled-schema.html">compiled_schema</a>&gt; compile_schema(
    std::string_view schemaJson);</code></pre></div>

<p><code>compile()</code> alone, for callers that only need the check.</p>

#### `validate_args` {#function-validate-args}

<div class="function-declaration" id="function-validate-args-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../../engine/core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; validate_args(
    std::string_view schemaJson,
    <span class="k">const</span> <a href="../../engine/core/json/value/json-value.html">core::json::value</a> &amp;args);</code></pre></div>

<p><code>compile()</code> then <code>validate()</code>: for callers that validate once.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="compile_schema"></a>
<a id="compiled_schema"></a>
<a id="compiled_schemacompile"></a>
<a id="compiled_schemacompiled_schema"></a>
<a id="compiled_schemacompiled_schema_1"></a>
<a id="compiled_schemamschema"></a>
<a id="compiled_schemaoperator"></a>
<a id="compiled_schemaschema"></a>
<a id="compiled_schemavalidate"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="validate_args"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
