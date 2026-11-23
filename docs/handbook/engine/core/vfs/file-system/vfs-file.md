---
kind: class
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [file_system.h](../file-system.md)

# vfs::file
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core::vfs` |
| Include File Path | `engine/include/conjure/core/vfs/file_system.h` |
| Source File Path | `engine/src/core/vfs/file_system.cpp` |
| API ↗ | [Files and the virtual file system](../../../../../api/core/files.md) |

## Syntax

```cpp
class CONJURE_API file
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">file();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Additional constructor overloads have been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">virtual</span> ~file();</code></td><td></td></tr>
</tbody>
</table>



## Functions

### Public

#### `read_at` {#function-read-at}

<div class="function-declaration" id="function-read-at-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../result/result.html">result</a>&lt;<a href="../../types.html#symbol-usize">usize</a>&gt; read_at(
    <a href="../../types.html#symbol-u64">u64</a> offset,
    std::span&lt;std::byte&gt; out) = <span class="mi">0</span>;</code></pre></div>

<p>Reads up to <code>out.<a href="#function-size-1">size</a>()</code> bytes starting at <code>offset</code> and returns how many were read, which is fewer than requested only at the end of the file. An offset past the end is <code>OUT_OF_RANGE</code>; an offset equal to the size reads zero bytes.</p>

#### `size` {#function-size}

<div class="function-declaration" id="function-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../types.html#symbol-u64">u64</a> size() <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

<p>Size of the file in bytes when it was opened.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->
