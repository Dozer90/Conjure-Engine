---
kind: struct
---

[Engine](../../../engine.md) → [Modules](../../modules.md) → [module_version.h](../module-version.md)

# version_range
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::modules` |
| Include File Path | `engine/include/conjure/modules/module_version.h` |
| Source File Path | `engine/src/modules/module_version.cpp` |
| API ↗ | [Writing a module](../../../../api/modules/writing-a-module.md) |

## Syntax

```cpp
struct version_range
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">maxExclusive</code></td><td><code class="signature"><a href="semantic-version.html">semantic_version</a></code></td><td><code class="signature">MAX_<wbr>SEMANTIC_<wbr>VERSION</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">minInclusive</code></td><td><code class="signature"><a href="semantic-version.html">semantic_version</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `contains` {#function-contains}

<div class="function-declaration" id="function-contains-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> contains(
    <a href="semantic-version.html">semantic_version</a> version) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when <code>minInclusive</code> &lt;= version &lt; <code>maxExclusive</code>.</p>

#### `is_unbounded` {#function-is-unbounded}

<div class="function-declaration" id="function-is-unbounded-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_unbounded() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the range accepts every version.</p>

## Operator Overloads

### Public

#### `operator<=>` {#operator-3c--3d--3e}

<div class="function-declaration" id="operator-3c--3d--3e-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">auto</span> <span class="k">operator</span>&lt;=&gt;(
    <span class="k">const</span> <a href="version-range.html">version_range</a> &amp;) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
