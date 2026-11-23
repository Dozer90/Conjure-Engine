---
kind: header
---

[Editor](../../editor.md) → [Commands and Undo](../commands-and-undo.md)

# reflection_path.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/reflection_path.h` |
| API ↗ | [Headless mode and the command model](../../../api/editor/headless-and-commands.md), [The editor](../../../api/editor/index.md) |

<p>Property paths: &quot;<code>transform.position.x</code>&quot;, &quot;materials[1]&quot;, &quot;<code>script.fields.speed</code>&quot;. The first segment names the component type (resolved by the command through the component registry), the rest walks the component&#x27;s reflection description down to one value. Values move through JSON only: the component is serialised with <code>ecs::component_to_json</code>, patched at the JSON pointer the path resolves to, and written back with <code>ecs::component_from_json</code>.</p>

## Structs

| File | Description |
|---|---|
| [`field_ref`](reflection-path/field-ref.md) | <code>A</code> resolved property inside a component: the JSON pointer of the value in the component&#x27;s JSON form plus the reflection description of the field when the component is reflected. |
| [`path_segment`](reflection-path/path-segment.md) |  |

## Classes

| File | Description |
|---|---|
| [`reflection_path`](reflection-path/reflection-path.md) |  |

## Functions

### Public

#### `read_at` {#function-read-at}

<div class="function-declaration" id="function-read-at-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../../engine/core/result/result.html">core::result</a>&lt;<a href="../../engine/core/json/document/json-document.html">core::json::document</a>&gt; read_at(
    <span class="k">const</span> <a href="../../engine/core/json/value/json-value.html">core::json::value</a> &amp;componentJson,
    <span class="k">const</span> <a href="reflection-path/field-ref.html">field_ref</a> &amp;reference);</code></pre></div>

<p>The value at the reference inside a component&#x27;s JSON form. <code>PATH_NOT_FOUND</code> when the pointer does not resolve in the document (an unreflected component lacking the member).</p>

#### `write_at` {#function-write-at}

<div class="function-declaration" id="function-write-at-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../../engine/core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; write_at(
    <a href="../../engine/core/json/document/json-document.html">core::json::document</a> &amp;componentJson,
    <span class="k">const</span> <a href="reflection-path/field-ref.html">field_ref</a> &amp;reference,
    <span class="k">const</span> <a href="../../engine/core/json/value/json-value.html">core::json::value</a> &amp;value);</code></pre></div>

<p>Replaces the value at the reference inside a component&#x27;s JSON form (the whole document for a whole-component reference). <code>INVALID_ARGS</code> when the pointer cannot be set.</p>

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
<a id="extension-points"></a>
<a id="field_ref"></a>
<a id="field_reffield"></a>
<a id="field_refis_whole_component"></a>
<a id="field_refkind"></a>
<a id="field_refpath"></a>
<a id="field_refpointer"></a>
<a id="field_reftype"></a>
<a id="internals-and-algorithms"></a>
<a id="path_segment"></a>
<a id="path_segmentindex"></a>
<a id="path_segmentname"></a>
<a id="path_segmentoperator"></a>
<a id="read_at"></a>
<a id="reflection_path"></a>
<a id="reflection_pathcomponent"></a>
<a id="reflection_pathfields"></a>
<a id="reflection_pathmerge_component"></a>
<a id="reflection_pathmsegments"></a>
<a id="reflection_pathoperator"></a>
<a id="reflection_pathparse"></a>
<a id="reflection_pathreflection_path"></a>
<a id="reflection_pathreflection_path_1"></a>
<a id="reflection_pathresolve"></a>
<a id="reflection_pathsegments"></a>
<a id="reflection_pathto_string"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<a id="write_at"></a>
<!-- doc-legacy-anchors:end -->
