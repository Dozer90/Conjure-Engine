---
kind: struct
---

[Engine](../../../engine.md) → [Platform](../../platform.md) → [input_state.h](../input-state.md)

# input_state
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::platform` |
| Include File Path | `engine/include/conjure/platform/input_state.h` |
| Source File Path | `engine/src/platform/input_state.cpp` |
| API ↗ | [Input](../../../../api/platform/input.md) |

## Syntax

```cpp
struct input_state
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">cursor<wbr>In<wbr>Window</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">true</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">frameIndex</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">gamepads</code></td><td><code class="signature">std::<wbr>array&lt;<wbr>gamepad_<wbr>state,<wbr> MAX_<wbr>GAMEPADS&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">keysDown</code></td><td><code class="signature"><a href="key-bitset.html">key_bitset</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">keysPressed</code></td><td><code class="signature"><a href="key-bitset.html">key_bitset</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">keysReleased</code></td><td><code class="signature"><a href="key-bitset.html">key_bitset</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">modifiers</code></td><td><code class="signature"><a href="../key.html#symbol-key-modifier-flags">key_modifier_flags</a></code></td><td><code class="signature">key_<wbr>modifier_<wbr>flags::<wbr>NONE</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">mouse<wbr>Buttons<wbr>Down</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">mouse<wbr>Buttons<wbr>Pressed</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">mouse<wbr>Buttons<wbr>Released</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">mouseDelta</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec2">math::vec2</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec2">math::vec2</a>(<span class="mi">0.0</span>f)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">mouse<wbr>Position</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec2">math::vec2</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec2">math::vec2</a>(<span class="mi">0.0</span>f)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">scrollDelta</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec2">math::vec2</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec2">math::vec2</a>(<span class="mi">0.0</span>f)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">text<wbr>Codepoints</code></td><td><code class="signature">std::<wbr>array&lt;<wbr>u32,<wbr> MAX_<wbr>TEXT_<wbr>CODEPOINTS_<wbr>PER_<wbr>FRAME&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">textCount</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `gamepad` {#function-gamepad}

<div class="function-declaration" id="function-gamepad-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="gamepad-state.html">gamepad_state</a> &amp;gamepad(
    <a href="../../core/types.html#symbol-u32">u32</a> index) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The gamepad in a slot. Asserts on index &gt;= <code>MAX_GAMEPADS</code> and then returns the last slot.</p>

#### `is_key_down` {#function-is-key-down}

<div class="function-declaration" id="function-is-key-down-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_key_down(
    <a href="../key.html#symbol-key">key</a> code) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True while the key is down, including the frame it was pressed in (<code>is_key_held</code> leaves that frame out).</p>

#### `is_key_held` {#function-is-key-held}

<div class="function-declaration" id="function-is-key-held-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_key_held(
    <a href="../key.html#symbol-key">key</a> code) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True while the key is down and this is not the frame it was pressed in.</p>

#### `is_key_pressed` {#function-is-key-pressed}

<div class="function-declaration" id="function-is-key-pressed-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_key_pressed(
    <a href="../key.html#symbol-key">key</a> code) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True in the frame the key went down.</p>

#### `is_key_released` {#function-is-key-released}

<div class="function-declaration" id="function-is-key-released-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_key_released(
    <a href="../key.html#symbol-key">key</a> code) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True in the frame the key came up.</p>

#### `is_mouse_button_down` {#function-is-mouse-button-down}

<div class="function-declaration" id="function-is-mouse-button-down-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_mouse_button_down(
    <a href="../mouse-button.html#symbol-mouse-button">mouse_button</a> button) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True while the mouse button is down, including the frame it was pressed in (<code>is_mouse_button_held</code> leaves that frame out); <code>false</code> for an invalid button.</p>

#### `is_mouse_button_held` {#function-is-mouse-button-held}

<div class="function-declaration" id="function-is-mouse-button-held-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_mouse_button_held(
    <a href="../mouse-button.html#symbol-mouse-button">mouse_button</a> button) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True while the mouse button is down and this is not the frame it was pressed in; <code>false</code> for an invalid button.</p>

#### `is_mouse_button_pressed` {#function-is-mouse-button-pressed}

<div class="function-declaration" id="function-is-mouse-button-pressed-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_mouse_button_pressed(
    <a href="../mouse-button.html#symbol-mouse-button">mouse_button</a> button) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True in the frame the mouse button went down; <code>false</code> for an invalid button.</p>

#### `is_mouse_button_released` {#function-is-mouse-button-released}

<div class="function-declaration" id="function-is-mouse-button-released-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_mouse_button_released(
    <a href="../mouse-button.html#symbol-mouse-button">mouse_button</a> button) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True in the frame the mouse button came up; <code>false</code> for an invalid button.</p>

#### `text` {#function-text}

<div class="function-declaration" id="function-text-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="../../core/types.html#symbol-u32">u32</a>&gt; text() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The UTF-32 code points typed this frame, in order (at most <code>MAX_TEXT_CODEPOINTS_PER_FRAME</code>).</p>

## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="input-state.html">input_state</a> &amp;) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
