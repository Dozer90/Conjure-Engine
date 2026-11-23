---
kind: class
---

[Editor](../../../editor.md) → [Applications](../../applications.md) → [editor_context.h](../editor-context.md)

# editor_host
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core::reflect` |
| Include File Path | `engine/include/conjure/editor/editor_context.h` |
| Source File Path | `engine/src/editor/core/editor_context.cpp` |
| API ↗ | [Headless mode and the command model](../../../../api/editor/headless-and-commands.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
class CONJURE_API editor_host
```

## Derived Classes


<details><summary>Classes that use this as a base</summary>

<ul>

<li><a href="../editor-application/editor-application.html">editor_application</a></li>

</ul>

</details>

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">editor_host();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">virtual</span> ~editor_host();</code></td><td></td></tr>
</tbody>
</table>



## Functions

### Public

#### `has_pending_project_open` {#function-has-pending-project-open}

<div class="function-declaration" id="function-has-pending-project-open-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <span class="k">bool</span> has_pending_project_open() <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

<p>True after <code>request_open_project</code> until the restart happened.</p>

#### `request_open_project` {#function-request-open-project}

<div class="function-declaration" id="function-request-open-project-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../../engine/core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; request_open_project(
    <span class="k">const</span> std::filesystem::path &amp;directory) = <span class="mi">0</span>;</code></pre></div>

<p>Validates the project directory now and schedules the engine restart over it for the next frame boundary (the current frame finishes on the old project).</p>

#### `request_quit` {#function-request-quit}

<div class="function-declaration" id="function-request-quit-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> request_quit(
    <span class="k">int</span> exitCode) = <span class="mi">0</span>;</code></pre></div>

<p>Asks the process to stop after the current frame with an exit code.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->
