---
kind: header
---

[Engine](../../engine.md) → [Platform](../platform.md)

# event.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/platform/event.h` |
| API ↗ | [Windows and events](../../../api/platform/windows-and-events.md) |

<p>The event stream a window produces once per frame. Every alternative is a trivially copyable struct; the one payload with external data (<code><a href="event/drop-files-event.html">drop_files_event</a></code>) points into a window-owned buffer that stays valid until the next <code>pump_events()</code>. <code><a href="event.html#symbol-input-event">input_event</a></code> lists the alternatives <code>input::apply</code> consumes; event adds the window-level ones. Both list the input alternatives first and in the same order, so the conversions below are index remaps.</p>

## Structs

| File | Description |
|---|---|
| [`close_event`](event/close-event.md) |  |
| [`content_scale_changed_event`](event/content-scale-changed-event.md) |  |
| [`cursor_enter_event`](event/cursor-enter-event.md) |  |
| [`drop_files_event`](event/drop-files-event.md) |  |
| [`focus_event`](event/focus-event.md) |  |
| [`framebuffer_resized_event`](event/framebuffer-resized-event.md) |  |
| [`gamepad_axis_event`](event/gamepad-axis-event.md) |  |
| [`gamepad_button_event`](event/gamepad-button-event.md) |  |
| [`gamepad_connection_event`](event/gamepad-connection-event.md) |  |
| [`key_event`](event/key-event.md) |  |
| [`minimise_event`](event/minimise-event.md) |  |
| [`mouse_button_event`](event/mouse-button-event.md) |  |
| [`mouse_move_event`](event/mouse-move-event.md) |  |
| [`mouse_scroll_event`](event/mouse-scroll-event.md) |  |
| [`text_event`](event/text-event.md) |  |

## Type Aliases

### `event` {#symbol-event}

```cpp
using event = std::variant< key_event, text_event, mouse_move_event, mouse_button_event, mouse_scroll_event, cursor_enter_event, gamepad_connection_event, gamepad_button_event, gamepad_axis_event, framebuffer_resized_event, content_scale_changed_event, focus_event, minimise_event, close_event, drop_files_event>;
```

### `input_event` {#symbol-input-event}

```cpp
using input_event = std::variant< key_event, text_event, mouse_move_event, mouse_button_event, mouse_scroll_event, cursor_enter_event, gamepad_connection_event, gamepad_button_event, gamepad_axis_event>;
```

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-EVENT_COUNT"></a><code>EVENT_COUNT</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-usize">usize</a></code> | <code class="signature">std::<wbr>variant_<wbr>size_<wbr>v&lt;<wbr><a href="event.html#symbol-event">event</a>&gt;</code> | Number of alternatives of event. |
| <a id="variable-INPUT_EVENT_COUNT"></a><code>INPUT_EVENT_COUNT</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-usize">usize</a></code> | <code class="signature">std::<wbr>variant_<wbr>size_<wbr>v&lt;<wbr><a href="event.html#symbol-input-event">input_<wbr>event</a>&gt;</code> | Number of input alternatives; they occupy the first <code>INPUT_EVENT_COUNT</code> indices of event. |

## Functions

### Public

#### `event_as` {#function-event-as}

<div class="function-declaration" id="function-event-as-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> EventType&gt; [[<span class="na">nodiscard</span>]] <span class="k">const</span> EventType *event_as(
    <span class="k">const</span> <a href="event.html#symbol-event">event</a> &amp;value) <span class="k">noexcept</span>;</code></pre></div>

<p>The payload when the event holds EventType, otherwise <code>nullptr</code> (<code>std::get_if</code>).</p>

#### `event_name` {#function-event-name}

<div class="function-declaration" id="function-event-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view event_name(
    <span class="k">const</span> <a href="event.html#symbol-event">event</a> &amp;value) <span class="k">noexcept</span>;</code></pre></div>

<p>The alternative name of an event (&quot;<code><a href="event/key-event.html">key_event</a></code>&quot;, &quot;<code><a href="event/close-event.html">close_event</a></code>&quot;, ...).</p>

#### `is_input_event` {#function-is-input-event}

<div class="function-declaration" id="function-is-input-event-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">bool</span> is_input_event(
    <span class="k">const</span> <a href="event.html#symbol-event">event</a> &amp;value) <span class="k">noexcept</span>;</code></pre></div>

<p>True when the event is one of the input alternatives (what <code>input::apply</code> consumes).</p>

#### `to_event` {#function-to-event}

<div class="function-declaration" id="function-to-event-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="event.html#symbol-event">event</a> to_event(
    <span class="k">const</span> <a href="event.html#symbol-input-event">input_event</a> &amp;value) <span class="k">noexcept</span>;</code></pre></div>

<p>Widens an <code><a href="event.html#symbol-input-event">input_event</a></code> to an event (never fails).</p>

#### `to_input_event` {#function-to-input-event}

<div class="function-declaration" id="function-to-input-event-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::optional&lt;<a href="event.html#symbol-input-event">input_event</a>&gt; to_input_event(
    <span class="k">const</span> <a href="event.html#symbol-event">event</a> &amp;value) <span class="k">noexcept</span>;</code></pre></div>

<p>The event as an <code><a href="event.html#symbol-input-event">input_event</a></code>, or nullopt for a window event.</p>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string to_string(
    <span class="k">const</span> <a href="event.html#symbol-event">event</a> &amp;value);</code></pre></div>

<p>The alternative name and payload, e.g. &quot;<code><a href="event/key-event.html">key_event</a></code>{key=<code>SPACE</code>, scancode=57, action=<code>PRESS</code>, modifiers=<code>SHIFT</code>}&quot;, for the editor console and window diagnostics.</p>

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
<a id="close_event"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="content_scale_changed_event"></a>
<a id="content_scale_changed_eventscale"></a>
<a id="cursor_enter_event"></a>
<a id="cursor_enter_evententered"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="drop_files_event"></a>
<a id="drop_files_eventpaths"></a>
<a id="event"></a>
<a id="event_as"></a>
<a id="event_count"></a>
<a id="event_name"></a>
<a id="extension-points"></a>
<a id="focus_event"></a>
<a id="focus_eventfocused"></a>
<a id="framebuffer_resized_event"></a>
<a id="framebuffer_resized_eventheight"></a>
<a id="framebuffer_resized_eventwidth"></a>
<a id="gamepad_axis_event"></a>
<a id="gamepad_axis_eventaxis"></a>
<a id="gamepad_axis_eventgamepadindex"></a>
<a id="gamepad_axis_eventvalue"></a>
<a id="gamepad_button_event"></a>
<a id="gamepad_button_eventaction"></a>
<a id="gamepad_button_eventbutton"></a>
<a id="gamepad_button_eventgamepadindex"></a>
<a id="gamepad_connection_event"></a>
<a id="gamepad_connection_eventconnected"></a>
<a id="gamepad_connection_eventgamepadindex"></a>
<a id="input_event"></a>
<a id="input_event_count"></a>
<a id="internals-and-algorithms"></a>
<a id="is_input_event"></a>
<a id="key_event"></a>
<a id="key_eventaction"></a>
<a id="key_eventkeycode"></a>
<a id="key_eventmodifiers"></a>
<a id="key_eventscancode"></a>
<a id="minimise_event"></a>
<a id="minimise_eventminimised"></a>
<a id="mouse_button_event"></a>
<a id="mouse_button_eventaction"></a>
<a id="mouse_button_eventbutton"></a>
<a id="mouse_button_eventmodifiers"></a>
<a id="mouse_move_event"></a>
<a id="mouse_move_eventposition"></a>
<a id="mouse_scroll_event"></a>
<a id="mouse_scroll_eventoffset"></a>
<a id="other-declarations"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="text_event"></a>
<a id="text_eventcodepoint"></a>
<a id="threading-model"></a>
<a id="to_event"></a>
<a id="to_input_event"></a>
<a id="to_string"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
