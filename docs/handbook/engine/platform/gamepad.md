---
kind: header
---

[Engine](../../engine.md) → [Platform](../platform.md)

# gamepad.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/platform/gamepad.h` |
| API ↗ | [Input](../../../api/platform/input.md) |

<p>Gamepad buttons and axes: the SDL game controller layout <code>GLFW</code> exposes as <code>GLFW_GAMEPAD_BUTTON_</code>* / <code>GLFW_GAMEPAD_AXIS_</code>*. Up to four gamepads are tracked; joysticks without a gamepad mapping are ignored.</p>

## Enumerations

### `gamepad_axis` {#symbol-gamepad-axis}

```cpp
enum class gamepad_axis : u8 {
    LEFT_X = 0,
    LEFT_Y,
    RIGHT_X,
    RIGHT_Y,
    LEFT_TRIGGER,       // reported in [0, 1] (GLFW's [-1, 1] is remapped by the backend)
    RIGHT_TRIGGER,      // reported in [0, 1]
    COUNT
};
```

### `gamepad_button` {#symbol-gamepad-button}

```cpp
enum class gamepad_button : u8 {
    A = 0,
    B,
    X,
    Y,
    LEFT_BUMPER,
    RIGHT_BUMPER,
    BACK,
    START,
    GUIDE,
    LEFT_THUMB,
    RIGHT_THUMB,
    DPAD_UP,
    DPAD_RIGHT,
    DPAD_DOWN,
    DPAD_LEFT,
    COUNT
};
```

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-GAMEPAD_AXIS_COUNT"></a><code>GAMEPAD_AXIS_COUNT</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">6</span></code> |  |
| <a id="variable-GAMEPAD_BUTTON_COUNT"></a><code>GAMEPAD_BUTTON_COUNT</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">15</span></code> |  |
| <a id="variable-MAX_GAMEPADS"></a><code>MAX_GAMEPADS</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">4</span></code> |  |

## Functions

### Public

#### `is_valid_gamepad_axis` {#function-is-valid-gamepad-axis}

<div class="function-declaration" id="function-is-valid-gamepad-axis-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_valid_gamepad_axis(
    <a href="gamepad.html#symbol-gamepad-axis">gamepad_axis</a> axis) <span class="k">noexcept</span>;</code></pre></div>

<p>True when the value names one of the six axes.</p>

#### `is_valid_gamepad_button` {#function-is-valid-gamepad-button}

<div class="function-declaration" id="function-is-valid-gamepad-button-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_valid_gamepad_button(
    <a href="gamepad.html#symbol-gamepad-button">gamepad_button</a> button) <span class="k">noexcept</span>;</code></pre></div>

<p>True when the value names one of the fifteen buttons.</p>

#### `parse_gamepad_axis` {#function-parse-gamepad-axis}

<div class="function-declaration" id="function-parse-gamepad-axis-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::optional&lt;<a href="gamepad.html#symbol-gamepad-axis">gamepad_axis</a>&gt; parse_gamepad_axis(
    std::string_view name) <span class="k">noexcept</span>;</code></pre></div>

<p>Parses an axis name case-insensitively; nullopt for anything else.</p>

#### `parse_gamepad_button` {#function-parse-gamepad-button}

<div class="function-declaration" id="function-parse-gamepad-button-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::optional&lt;<a href="gamepad.html#symbol-gamepad-button">gamepad_button</a>&gt; parse_gamepad_button(
    std::string_view name) <span class="k">noexcept</span>;</code></pre></div>

<p>Parses a button name case-insensitively; nullopt for anything else.</p>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="gamepad.html#symbol-gamepad-button">gamepad_button</a> button) <span class="k">noexcept</span>;</code></pre></div>

<p>The enumerator name (&quot;<code>LEFT_BUMPER</code>&quot;), or &quot;&quot; for unknown values.</p>

<div class="function-declaration" id="function-to-string-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="gamepad.html#symbol-gamepad-axis">gamepad_axis</a> axis) <span class="k">noexcept</span>;</code></pre></div>

<p>The enumerator name (&quot;<code>RIGHT_TRIGGER</code>&quot;), or &quot;&quot; for unknown values.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="gamepad_axis"></a>
<a id="gamepad_axis_count"></a>
<a id="gamepad_axiscount"></a>
<a id="gamepad_axisleft_trigger"></a>
<a id="gamepad_axisleft_x"></a>
<a id="gamepad_axisleft_y"></a>
<a id="gamepad_axisright_trigger"></a>
<a id="gamepad_axisright_x"></a>
<a id="gamepad_axisright_y"></a>
<a id="gamepad_button"></a>
<a id="gamepad_button_count"></a>
<a id="gamepad_buttona"></a>
<a id="gamepad_buttonb"></a>
<a id="gamepad_buttonback"></a>
<a id="gamepad_buttoncount"></a>
<a id="gamepad_buttondpad_down"></a>
<a id="gamepad_buttondpad_left"></a>
<a id="gamepad_buttondpad_right"></a>
<a id="gamepad_buttondpad_up"></a>
<a id="gamepad_buttonguide"></a>
<a id="gamepad_buttonleft_bumper"></a>
<a id="gamepad_buttonleft_thumb"></a>
<a id="gamepad_buttonright_bumper"></a>
<a id="gamepad_buttonright_thumb"></a>
<a id="gamepad_buttonstart"></a>
<a id="gamepad_buttonx"></a>
<a id="gamepad_buttony"></a>
<a id="internals-and-algorithms"></a>
<a id="is_valid_gamepad_axis"></a>
<a id="is_valid_gamepad_button"></a>
<a id="max_gamepads"></a>
<a id="other-declarations"></a>
<a id="parse_gamepad_axis"></a>
<a id="parse_gamepad_button"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
