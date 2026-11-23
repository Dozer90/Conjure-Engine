---
kind: struct
---

[Editor](../../../editor.md) → [User Interface](../../user-interface.md) → [picker.h](../picker.md)

# pick_result
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::core::jobs` |
| Include File Path | `engine/include/conjure/editor/picker.h` |
| Source File Path | `engine/src/editor/core/picker.cpp` |
| API ↗ | [Headless mode and the command model](../../../../api/editor/headless-and-commands.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
struct CONJURE_API pick_result
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">distance</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">0.0</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">guid</code></td><td><code class="signature"><a href="../../../engine/core/uuid/uuid.html">core::uuid</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">hit</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">normal</code></td><td><code class="signature"><a href="../../../engine/core/math/glm-types.html#symbol-vec3">math::vec3</a></code></td><td><code class="signature"><a href="../../../engine/core/math/glm-types.html#symbol-vec3">math::<wbr>vec3</a>(<span class="mi">0.0</span>f,<wbr> <span class="mi">1.0</span>f,<wbr> <span class="mi">0.0</span>f)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">point</code></td><td><code class="signature"><a href="../../../engine/core/math/glm-types.html#symbol-vec3">math::vec3</a></code></td><td><code class="signature"><a href="../../../engine/core/math/glm-types.html#symbol-vec3">math::vec3</a>(<span class="mi">0.0</span>f)</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `to_json` {#function-to-json}

<div class="function-declaration" id="function-to-json-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/json/document/json-document.html">core::json::document</a> to_json() <span class="k">const</span>;</code></pre></div>

<p>{hit, guid, point, normal, distance} as <code>view.pick</code> returns it.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
