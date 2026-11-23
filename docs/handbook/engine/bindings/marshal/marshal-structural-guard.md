---
kind: struct
---

[Engine](../../../engine.md) → [Bindings](../../bindings.md) → [marshal.h](../marshal.md)

# marshal::structural_guard
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::bindings::marshal` |
| Include File Path | `engine/include/conjure/bindings/marshal.h` |
| Source File Path | `engine/src/bindings/marshal.cpp` |
| API ↗ | [Scripting from C#: the bindings](../../../../api/bindings/index.md), [The native table](../../../../api/bindings/native-table.md) |

## Syntax

```cpp
struct structural_guard
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">runtime</code></td><td><code class="signature"><a href="../../scripting/script-runtime/script-runtime.html">scripting::<wbr>script_<wbr>runtime</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">world</code></td><td><code class="signature"><a href="../../ecs/world/world.html">ecs::world</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `can_defer` {#function-can-defer}

<div class="function-declaration" id="function-can-defer-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> can_defer() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when a deferred request has a buffer to go to (a runtime with the world attached).</p>

#### `commands` {#function-commands}

<div class="function-declaration" id="function-commands-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../ecs/command-buffer/command-buffer.html">ecs::command_buffer</a> &amp;commands() <span class="k">noexcept</span>;</code></pre></div>

<p>The buffer deferred requests are recorded into; only valid when <code><a href="#function-can-defer-1">can_defer</a>()</code>.</p>

#### `deferred` {#function-deferred}

<div class="function-declaration" id="function-deferred-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> deferred() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True while the world is locked and the request must be recorded instead of applied.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
