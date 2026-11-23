---
kind: header
---

[Engine](../../engine.md) → [Platform](../platform.md)

# input_state.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/platform/input_state.h` |
| API ↗ | [Input](../../../api/platform/input.md) |

<p>Everything a frame may ask about input. Published by <code>input::end_frame()</code>; readers never see a partially updated snapshot.</p>

## Structs

| File | Description |
|---|---|
| [`gamepad_state`](input-state/gamepad-state.md) | One gamepad slot: connection, button masks (bit i = <code><a href="gamepad.html#symbol-gamepad-button">gamepad_button</a>(i)</code>) and axes. |
| [`input_state`](input-state/input-state.md) | Everything a frame may ask about input. Published by <code>input::end_frame()</code>; readers never see a partially updated snapshot. |
| [`key_bitset`](input-state/key-bitset.md) | <code>A</code> fixed 512-bit set indexed by key code, laid out as eight <code><a href="../core/types.html#symbol-u64">u64</a></code> words (bit i of word w is key 64 * w + i). Codes outside [0, <code>KEY_COUNT</code>) are ignored by set/reset and test <code>false</code>. |

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-MAX_TEXT_CODEPOINTS_PER_FRAME"></a><code>MAX_TEXT_CODEPOINTS_PER_FRAME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">16</span></code> |  |

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
<a id="functions"></a>
<a id="gamepad_state"></a>
<a id="gamepad_stateaxes"></a>
<a id="gamepad_stateaxis"></a>
<a id="gamepad_statebuttonsdown"></a>
<a id="gamepad_statebuttonspressed"></a>
<a id="gamepad_statebuttonsreleased"></a>
<a id="gamepad_stateconnected"></a>
<a id="gamepad_stateis_down"></a>
<a id="gamepad_stateis_pressed"></a>
<a id="gamepad_stateis_released"></a>
<a id="gamepad_stateoperator"></a>
<a id="input_state"></a>
<a id="input_statecursorinwindow"></a>
<a id="input_stateframeindex"></a>
<a id="input_stategamepad"></a>
<a id="input_stategamepads"></a>
<a id="input_stateis_key_down"></a>
<a id="input_stateis_key_held"></a>
<a id="input_stateis_key_pressed"></a>
<a id="input_stateis_key_released"></a>
<a id="input_stateis_mouse_button_down"></a>
<a id="input_stateis_mouse_button_pressed"></a>
<a id="input_stateis_mouse_button_released"></a>
<a id="input_statekeysdown"></a>
<a id="input_statekeyspressed"></a>
<a id="input_statekeysreleased"></a>
<a id="input_statemodifiers"></a>
<a id="input_statemousebuttonsdown"></a>
<a id="input_statemousebuttonspressed"></a>
<a id="input_statemousebuttonsreleased"></a>
<a id="input_statemousedelta"></a>
<a id="input_statemouseposition"></a>
<a id="input_stateoperator"></a>
<a id="input_statescrolldelta"></a>
<a id="input_statetext"></a>
<a id="input_statetextcodepoints"></a>
<a id="input_statetextcount"></a>
<a id="internals-and-algorithms"></a>
<a id="key_bitset"></a>
<a id="key_bitsetany"></a>
<a id="key_bitsetclear"></a>
<a id="key_bitsetcount"></a>
<a id="key_bitsetoperator"></a>
<a id="key_bitsetreset"></a>
<a id="key_bitsetset"></a>
<a id="key_bitsettest"></a>
<a id="key_bitsetword_count"></a>
<a id="key_bitsetwords"></a>
<a id="max_text_codepoints_per_frame"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
