---
kind: class
---

[Editor](../../../editor.md) → [Commands and Undo](../../commands-and-undo.md) → [entity_ref.h](../entity-ref.md)

# entity_ref
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/entity_ref.h` |
| Source File Path | `engine/src/editor/core/entity_ref.cpp` |
| API ↗ | [Headless mode and the command model](../../../../api/editor/headless-and-commands.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
class CONJURE_API entity_ref
```









## Functions

### Public

#### `guid_of` {#function-guid-of}

<div class="function-declaration" id="function-guid-of-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../../engine/core/result/result.html">core::result</a>&lt;<a href="../../../engine/core/uuid/uuid.html">core::uuid</a>&gt; guid_of(
    scene::scene &amp;target,
    <a href="../../../engine/ecs/entity/entity.html">ecs::entity</a> entity);</code></pre></div>

<p>The guid of a live entity, generated when missing. <code>ENTITY_NOT_FOUND</code> for a dead entity.</p>

#### `looks_like_guid` {#function-looks-like-guid}

<div class="function-declaration" id="function-looks-like-guid-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <span class="k">bool</span> looks_like_guid(
    std::string_view text) <span class="k">noexcept</span>;</code></pre></div>

<p>True when the text parses as a uuid.</p>

#### `path_of` {#function-path-of}

<div class="function-declaration" id="function-path-of-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> std::string path_of(
    <span class="k">const</span> scene::scene &amp;target,
    <a href="../../../engine/ecs/entity/entity.html">ecs::entity</a> entity);</code></pre></div>

<p>The hierarchy path of an entity (&quot;Root/Child&quot;); unnamed entities print as &quot;#index&quot;.</p>

#### `resolve` {#function-resolve}

<div class="function-declaration" id="function-resolve-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../../engine/core/result/result.html">core::result</a>&lt;<a href="../../../engine/ecs/entity/entity.html">ecs::entity</a>&gt; resolve(
    <span class="k">const</span> <a href="../../../engine/core/json/value/json-value.html">core::json::value</a> &amp;reference,
    <span class="k">const</span> scene::scene &amp;target,
    <a href="../../../engine/core/json/document/json-document.html">core::json::document</a> *details = nullptr);</code></pre></div>

<p>Resolves a reference: a uuid string is a guid (<code>ENTITY_NOT_FOUND</code> when no live entity carries it); any other string is a path resolved from the roots by name (<code>ENTITY_NOT_FOUND</code> when a segment does not match, <code>INVALID_ARGS</code> with <code>details.candidates</code> when a segment matches several siblings); a non-string is <code>INVALID_ARGS</code>. <code>details</code> receives the candidate guids when given.</p>

#### `resolve_many` {#function-resolve-many}

<div class="function-declaration" id="function-resolve-many-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../../engine/core/result/result.html">core::result</a>&lt;std::vector&lt;<a href="../../../engine/ecs/entity/entity.html">ecs::entity</a>&gt;&gt; resolve_many(
    <span class="k">const</span> <a href="../../../engine/core/json/value/json-value.html">core::json::value</a> &amp;references,
    <span class="k">const</span> scene::scene &amp;target,
    <a href="../../../engine/core/json/document/json-document.html">core::json::document</a> *details = nullptr);</code></pre></div>

<p>Resolves every element of an array of references, in order. <code>INVALID_ARGS</code> for a non-array.</p>

#### `to_json` {#function-to-json}

<div class="function-declaration" id="function-to-json-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> std::string to_json(
    <span class="k">const</span> <a href="../../../engine/core/uuid/uuid.html">core::uuid</a> &amp;guid);</code></pre></div>

<p>The canonical string form of a guid.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->
