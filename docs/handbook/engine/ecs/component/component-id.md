---
kind: struct
---

[Engine](../../../engine.md) → [ECS](../../ecs.md) → [component.h](../component.md)

# component_id
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::core::json` |
| Include File Path | `engine/include/conjure/ecs/component.h` |
| Source File Path | N/A |
| API ↗ | [Entities and components](../../../../api/ecs/entities-and-components.md) |

## Syntax

```cpp
struct component_id
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">INVALID</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/types.html#symbol-u16">u16</a></code></td><td><code class="signature"><span class="mi">0</span>xFFFF</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">value</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u16">u16</a></code></td><td><code class="signature">INVALID</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `is_valid` {#function-is-valid}

<div class="function-declaration" id="function-is-valid-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_valid() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True for an id below <code>MAX_COMPONENTS</code> (not <code>INVALID</code>).</p>

## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="component-id.html">component_id</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
