---
kind: struct
---

[Engine](../../../engine.md) → [Scripting](../../scripting.md) → [script_component.h](../script-component.md)

# script_entry
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::scripting` |
| Include File Path | `engine/include/conjure/scripting/script_component.h` |
| Source File Path | `engine/src/scripting/script_component.cpp` |
| API ↗ | [Scripting](../../../../api/scripting/index.md), [Scripts, components and the runtime](../../../../api/scripting/scripts-and-components.md) |

## Syntax

```cpp
struct script_entry
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">classHash</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">fields</code></td><td><code class="signature"><a href="../../core/json/document/json-document.html">core::json::document</a></code></td><td><code class="signature">core::<wbr>json::<wbr>document::<wbr>object()</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">flags</code></td><td><code class="signature"><a href="../script-component.html#symbol-script-entry-flags">script_entry_flags</a></code></td><td><code class="signature">script_<wbr>entry_<wbr>flags::<wbr>NONE</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">managedSlot</code></td><td><code class="signature"><a href="../../core/types.html#symbol-i32">i32</a></code></td><td><code class="signature">INVALID_SLOT</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `has_flag` {#function-has-flag}

<div class="function-declaration" id="function-has-flag-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> has_flag(
    <a href="../script-component.html#symbol-script-entry-flags">script_entry_flags</a> flag) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `is_live` {#function-is-live}

<div class="function-declaration" id="function-is-live-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_live() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when a managed instance exists for the entry.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
