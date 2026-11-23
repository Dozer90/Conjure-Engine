---
kind: class
---

[Editor](../../../editor.md) → [Commands and Undo](../../commands-and-undo.md) → [undo_stack.h](../undo-stack.md)

# transaction_scope
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/undo_stack.h` |
| Source File Path | `engine/src/editor/core/undo_stack.cpp` |
| API ↗ | [Headless mode and the command model](../../../../api/editor/headless-and-commands.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
class CONJURE_API transaction_scope
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">transaction_scope(
    <a href="undo-stack.html">undo_stack</a> &amp;stack,
    std::string_view label,
    <a href="../../../engine/core/types.html#symbol-u32">u32</a> clientId);</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~transaction_scope();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mActive</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mClientId</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mStack</code></td><td><code class="signature"><a href="undo-stack.html">undo_stack</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `active` {#function-active}

<div class="function-declaration" id="function-active-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> active() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `end` {#function-end}

<div class="function-declaration" id="function-end-1"><pre><code><span class="k">void</span> end();</code></pre></div>

<p>Ends the transaction now; the destructor then does nothing.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
