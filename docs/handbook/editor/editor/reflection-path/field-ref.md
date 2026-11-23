---
kind: struct
---

[Editor](../../../editor.md) → [Commands and Undo](../../commands-and-undo.md) → [reflection_path.h](../reflection-path.md)

# field_ref
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/reflection_path.h` |
| Source File Path | `engine/src/editor/core/reflection_path.cpp` |
| API ↗ | [Headless mode and the command model](../../../../api/editor/headless-and-commands.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
struct field_ref
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">field</code></td><td><code class="signature"><span class="k">const</span> <a href="../../../engine/core/reflect/field-info/reflect-field-info.html">core::<wbr>reflect::<wbr>field_<wbr>info</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">kind</code></td><td><code class="signature"><a href="../../../engine/core/reflect/field-info.html#symbol-reflect-field-kind">core::<wbr>reflect::<wbr>field_<wbr>kind</a></code></td><td><code class="signature">core::<wbr>reflect::<wbr>field_<wbr>kind::<wbr>COUNT</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">path</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">""</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">pointer</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">""</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">type</code></td><td><code class="signature"><span class="k">const</span> <a href="../../../engine/core/reflect/type-info/reflect-type-info.html">core::<wbr>reflect::<wbr>type_<wbr>info</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `is_whole_component` {#function-is-whole-component}

<div class="function-declaration" id="function-is-whole-component-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_whole_component() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
