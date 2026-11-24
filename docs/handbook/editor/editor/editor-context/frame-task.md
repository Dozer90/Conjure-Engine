---
kind: class
---

[Editor](../../../editor.md) → [Applications](../../applications.md) → [editor_context.h](../editor-context.md)

# frame_task
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core::reflect` |
| Include File Path | `engine/include/conjure/editor/editor_context.h` |
| Source File Path | `engine/src/editor/core/editor_context.cpp` |
| API ↗ | [Headless mode and the command model](../../../../api/editor/headless-and-commands.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
class CONJURE_API frame_task
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">frame_task();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">virtual</span> ~frame_task();</code></td><td></td></tr>
</tbody>
</table>



## Functions

### Public

#### `release_engine` {#function-release-engine}

<div class="function-declaration" id="function-release-engine-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> release_engine(
    <a href="editor-context.html">editor_context</a> &amp;context);</code></pre></div>

<p>The engine is shutting down (editor shutdown, the restart of <code>project.open</code>): release every handle into it, undo what the task left half done, fail a pending reply with <code>CANCELLED</code> and report finished from the next <code><a href="#function-tick-1">tick</a>()</code>. The default holds nothing and does nothing. Called at most once per engine, possibly again for a later engine the task never saw: an implementation is idempotent.</p>

#### `tick` {#function-tick}

<div class="function-declaration" id="function-tick-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <span class="k">bool</span> tick(
    <a href="editor-context.html">editor_context</a> &amp;context) = <span class="mi">0</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->
