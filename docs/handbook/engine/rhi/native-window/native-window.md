---
kind: struct
---

[Engine](../../../engine.md) → [RHI](../../rhi.md) → [native_window.h](../native-window.md)

# native_window
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::rhi` |
| Include File Path | `engine/include/conjure/rhi/native_window.h` |
| Source File Path | `engine/src/rhi/native_window.cpp` |
| API ↗ | [Devices and swapchains](../../../../api/rhi/devices-and-swapchains.md) |

## Syntax

```cpp
struct native_window
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">display</code></td><td><code class="signature"><span class="k">void</span> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">kind</code></td><td><code class="signature"><a href="../native-window.html#symbol-native-window-kind">native_window_kind</a></code></td><td><code class="signature">native_<wbr>window_<wbr>kind::<wbr>NONE</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">window</code></td><td><code class="signature"><span class="k">void</span> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">x11Window</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `is_headless` {#function-is-headless}

<div class="function-declaration" id="function-is-headless-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_headless() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when no window was given (kind <code>NONE</code>).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
