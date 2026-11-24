---
kind: struct
---

[Engine](../../../engine.md) → [Platform](../../platform.md) → [input_state.h](../input-state.md)

# gamepad_state
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::platform` |
| Include File Path | `engine/include/conjure/platform/input_state.h` |
| Source File Path | `engine/src/platform/input_state.cpp` |
| API ↗ | [Input](../../../../api/platform/input.md) |

## Syntax

```cpp
struct gamepad_state
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">axes</code></td><td><code class="signature">std::<wbr>array&lt;<wbr>f32,<wbr> GAMEPAD_<wbr>AXIS_<wbr>COUNT&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">buttonsDown</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">buttons<wbr>Pressed</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">buttons<wbr>Released</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">connected</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `axis` {#function-axis}

<div class="function-declaration" id="function-axis-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-f32">f32</a> axis(
    <a href="../gamepad.html#symbol-gamepad-axis">gamepad_axis</a> axisId) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The axis value; 0 for an invalid axis.</p>

#### `is_down` {#function-is-down}

<div class="function-declaration" id="function-is-down-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_down(
    <a href="../gamepad.html#symbol-gamepad-button">gamepad_button</a> button) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True while the button is down, including the frame it was pressed in (<code>is_held</code> leaves that frame out); <code>false</code> for an invalid button.</p>

#### `is_held` {#function-is-held}

<div class="function-declaration" id="function-is-held-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_held(
    <a href="../gamepad.html#symbol-gamepad-button">gamepad_button</a> button) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True while the button is down and this is not the frame it was pressed in; <code>false</code> for an invalid button.</p>

#### `is_pressed` {#function-is-pressed}

<div class="function-declaration" id="function-is-pressed-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_pressed(
    <a href="../gamepad.html#symbol-gamepad-button">gamepad_button</a> button) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True in the frame the button went down; <code>false</code> for an invalid button.</p>

#### `is_released` {#function-is-released}

<div class="function-declaration" id="function-is-released-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_released(
    <a href="../gamepad.html#symbol-gamepad-button">gamepad_button</a> button) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True in the frame the button came up; <code>false</code> for an invalid button.</p>

## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="gamepad-state.html">gamepad_state</a> &amp;) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
