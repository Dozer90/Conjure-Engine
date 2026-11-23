---
kind: class
---

[Engine](../../../engine.md) → [Platform](../../platform.md) → [null_backend.h](../null-backend.md)

# null_backend
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::platform` |
| Include File Path | `engine/include/conjure/platform/null_backend.h` |
| Source File Path | `engine/src/platform/null_backend.cpp` |
| API ↗ | [Input](../../../../api/platform/input.md), [Windows and events](../../../../api/platform/windows-and-events.md) |

## Syntax

```cpp
class CONJURE_API null_backend final : public window
```

## Inheritance Hierarchy


[`window`](../window/window.md) → `null_backend`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> null_backend(
    <span class="k">const</span> <a href="../window/window-desc.html">window_desc</a> &amp;desc);</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~null_backend() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mClipboard</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Clipboard<wbr>Has<wbr>Text</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Content<wbr>Scale</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec2">math::vec2</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec2">math::vec2</a>(<span class="mi">1.0</span>f)</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mCursorMode</code></td><td><code class="signature"><a href="../window.html#symbol-cursor-mode">cursor_mode</a></code></td><td><code class="signature">cursor_<wbr>mode::<wbr>NORMAL</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Cursor<wbr>Position</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec2">math::vec2</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec2">math::vec2</a>(<span class="mi">0.0</span>f)</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mDelivered</code></td><td><code class="signature">std::vector&lt;<a href="../event.html#symbol-event">event</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Delivered<wbr>Drop<wbr>Paths</code></td><td><code class="signature">std::vector&lt;std::string&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Delivered<wbr>Drop<wbr>Ranges</code></td><td><code class="signature">std::vector&lt;<a href="null-backend-drop-range.html">drop_range</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFocused</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">true</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFullscreen</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Gamepad<wbr>Connected</code></td><td><code class="signature">std::<wbr>array&lt;<wbr><span class="k">bool</span>,<wbr> MAX_<wbr>GAMEPADS&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Gamepad<wbr>Names</code></td><td><code class="signature">std::<wbr>array&lt;<wbr>std::<wbr>string,<wbr> MAX_<wbr>GAMEPADS&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mIcons</code></td><td><code class="signature">std::vector&lt;<a href="../../core/image/image.html">core::image</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mLogicalSize</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-uvec2">math::uvec2</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-uvec2">math::uvec2</a>(<span class="mi">0</span>)</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mMinimised</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mMonitors</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="../monitor/monitor-info.html">monitor_<wbr>info</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPending</code></td><td><code class="signature">std::vector&lt;<a href="../event.html#symbol-event">event</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Pending<wbr>Drop<wbr>Paths</code></td><td><code class="signature">std::vector&lt;std::string&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Pending<wbr>Drop<wbr>Ranges</code></td><td><code class="signature">std::vector&lt;<a href="null-backend-drop-range.html">drop_range</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Pending<wbr>Resize</code></td><td><code class="signature">std::<wbr>optional&lt;<wbr><a href="../../core/math/glm-types.html#symbol-uvec2">math::<wbr>uvec2</a>&gt;</code></td><td><code class="signature">std::nullopt</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPosition</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-ivec2">math::ivec2</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-ivec2">math::ivec2</a>(<span class="mi">0</span>)</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Raw<wbr>Mouse<wbr>Motion</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mShouldClose</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mTitle</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mVideoModes</code></td><td><code class="signature">std::vector&lt;<a href="../monitor/video-mode.html">video_mode</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mVisible</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">true</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Windowed<wbr>Position</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-ivec2">math::ivec2</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-ivec2">math::ivec2</a>(<span class="mi">0</span>)</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Windowed<wbr>Position<wbr>Known</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">true</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Windowed<wbr>Size</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-uvec2">math::uvec2</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-uvec2">math::uvec2</a>(<span class="mi">0</span>)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">VIRTUAL_<wbr>MONITOR_<wbr>HEIGHT</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">1080</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">VIRTUAL_<wbr>MONITOR_<wbr>REFRESH_<wbr>HZ</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">60</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">VIRTUAL_<wbr>MONITOR_<wbr>WIDTH</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">1920</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `as_null` {#function-as-null}

<div class="function-declaration" id="function-as-null-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="null-backend.html">null_backend</a> *as_null() <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `backend` {#function-backend}

<div class="function-declaration" id="function-backend-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../backend.html#symbol-backend-kind">backend_kind</a> backend() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `clipboard_text` {#function-clipboard-text}

<div class="function-declaration" id="function-clipboard-text-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;std::string&gt; clipboard_text() <span class="k">const</span> <span class="k">override</span>;</code></pre></div>

#### `content_scale` {#function-content-scale}

<div class="function-declaration" id="function-content-scale-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/math/glm-types.html#symbol-vec2">math::vec2</a> content_scale() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `create` {#function-create}

<div class="function-declaration" id="function-create-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> std::unique_ptr&lt;<a href="null-backend.html">null_backend</a>&gt; create(
    <span class="k">const</span> <a href="../window/window-desc.html">window_desc</a> &amp;desc);</code></pre></div>

<p>Builds a <code>null</code> window from a description (what <code>create_window</code> does for <code>NULL_BACKEND</code>). The description is expected to pass <code>validate_window_desc</code> (asserted).</p>

#### `create_vulkan_surface` {#function-create-vulkan-surface}

<div class="function-declaration" id="function-create-vulkan-surface-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; create_vulkan_surface(
    <span class="k">void</span> *instance,
    <span class="k">void</span> **surface) <span class="k">const</span> <span class="k">override</span>;</code></pre></div>

#### `current_cursor_mode` {#function-current-cursor-mode}

<div class="function-declaration" id="function-current-cursor-mode-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../window.html#symbol-cursor-mode">cursor_mode</a> current_cursor_mode() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `cursor_position` {#function-cursor-position}

<div class="function-declaration" id="function-cursor-position-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/math/glm-types.html#symbol-vec2">math::vec2</a> cursor_position() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Where <code><a href="#function-set-cursor-position-1">set_cursor_position</a>()</code> last warped the cursor (logical coordinates).</p>

#### `framebuffer_size` {#function-framebuffer-size}

<div class="function-declaration" id="function-framebuffer-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/math/glm-types.html#symbol-uvec2">math::uvec2</a> framebuffer_size() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `gamepad_name` {#function-gamepad-name}

<div class="function-declaration" id="function-gamepad-name-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string_view gamepad_name(
    <a href="../../core/types.html#symbol-u32">u32</a> index) <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `hide` {#function-hide}

<div class="function-declaration" id="function-hide-1"><pre><code><span class="k">void</span> hide() <span class="k">override</span>;</code></pre></div>

#### `icons` {#function-icons}

<div class="function-declaration" id="function-icons-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="../../core/image/image.html">core::image</a>&gt; icons() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Copies of the images the last successful <code><a href="#function-set-icon-1">set_icon</a>()</code> received; empty for the default icon.</p>

#### `is_focused` {#function-is-focused}

<div class="function-declaration" id="function-is-focused-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_focused() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `is_fullscreen` {#function-is-fullscreen}

<div class="function-declaration" id="function-is-fullscreen-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_fullscreen() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `is_minimised` {#function-is-minimised}

<div class="function-declaration" id="function-is-minimised-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_minimised() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `is_visible` {#function-is-visible}

<div class="function-declaration" id="function-is-visible-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_visible() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `monitors` {#function-monitors}

<div class="function-declaration" id="function-monitors-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="../monitor/monitor-info.html">monitor_info</a>&gt; monitors() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `native_handle` {#function-native-handle}

<div class="function-declaration" id="function-native-handle-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../native-handle/native-window-handle.html">native_window_handle</a> native_handle() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `pending_event_count` {#function-pending-event-count}

<div class="function-declaration" id="function-pending-event-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-usize">usize</a> pending_event_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Events the next <code><a href="#function-pump-events-1">pump_events</a>()</code> will deliver, the coalesced resize included.</p>

#### `position` {#function-position}

<div class="function-declaration" id="function-position-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/math/glm-types.html#symbol-ivec2">math::ivec2</a> position() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `pump_events` {#function-pump-events}

<div class="function-declaration" id="function-pump-events-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="../event.html#symbol-event">event</a>&gt; pump_events() <span class="k">override</span>;</code></pre></div>

#### `push_event` {#function-push-event}

<div class="function-declaration" id="function-push-event-1"><pre><code><span class="k">void</span> push_event(
    <span class="k">const</span> <a href="../event.html#symbol-event">event</a> &amp;value);</code></pre></div>

<p>Queues any event verbatim. <code>A</code> <code><a href="../event/gamepad-connection-event.html">gamepad_connection_event</a></code> also updates <code><a href="#function-gamepad-name-1">gamepad_name</a>()</code> when it is delivered.</p>

#### `raw_mouse_motion` {#function-raw-mouse-motion}

<div class="function-declaration" id="function-raw-mouse-motion-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> raw_mouse_motion() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The raw mouse motion flag <code><a href="#function-set-raw-mouse-motion-1">set_raw_mouse_motion</a>()</code> stored.</p>

#### `request_close` {#function-request-close}

<div class="function-declaration" id="function-request-close-1"><pre><code><span class="k">void</span> request_close() <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `required_vulkan_instance_extensions` {#function-required-vulkan-instance-extensions}

<div class="function-declaration" id="function-required-vulkan-instance-extensions-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <span class="k">char</span> *<span class="k">const</span>&gt; required_vulkan_instance_extensions() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `set_clipboard_text` {#function-set-clipboard-text}

<div class="function-declaration" id="function-set-clipboard-text-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; set_clipboard_text(
    std::string_view text) <span class="k">override</span>;</code></pre></div>

#### `set_cursor_mode` {#function-set-cursor-mode}

<div class="function-declaration" id="function-set-cursor-mode-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; set_cursor_mode(
    <a href="../window.html#symbol-cursor-mode">cursor_mode</a> mode) <span class="k">override</span>;</code></pre></div>

#### `set_cursor_position` {#function-set-cursor-position}

<div class="function-declaration" id="function-set-cursor-position-1"><pre><code><span class="k">void</span> set_cursor_position(
    <a href="../../core/math/glm-types.html#symbol-vec2">math::vec2</a> logicalPosition) <span class="k">override</span>;</code></pre></div>

#### `set_fullscreen` {#function-set-fullscreen}

<div class="function-declaration" id="function-set-fullscreen-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; set_fullscreen(
    <span class="k">bool</span> fullscreen,
    <a href="../../core/types.html#symbol-u32">u32</a> monitorIndex = <span class="mi">0</span>) <span class="k">override</span>;</code></pre></div>

#### `set_icon` {#function-set-icon}

<div class="function-declaration" id="function-set-icon-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; set_icon(
    std::span&lt;<span class="k">const</span> <a href="../../core/image/image.html">core::image</a>&gt; images) <span class="k">override</span>;</code></pre></div>

#### `set_position` {#function-set-position}

<div class="function-declaration" id="function-set-position-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; set_position(
    <a href="../../core/math/glm-types.html#symbol-ivec2">math::ivec2</a> position) <span class="k">override</span>;</code></pre></div>

#### `set_raw_mouse_motion` {#function-set-raw-mouse-motion}

<div class="function-declaration" id="function-set-raw-mouse-motion-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; set_raw_mouse_motion(
    <span class="k">bool</span> enabled) <span class="k">override</span>;</code></pre></div>

#### `set_size` {#function-set-size}

<div class="function-declaration" id="function-set-size-1"><pre><code><span class="k">void</span> set_size(
    <a href="../../core/math/glm-types.html#symbol-uvec2">math::uvec2</a> logicalSize) <span class="k">override</span>;</code></pre></div>

#### `set_title` {#function-set-title}

<div class="function-declaration" id="function-set-title-1"><pre><code><span class="k">void</span> set_title(
    std::string_view title) <span class="k">override</span>;</code></pre></div>

#### `should_close` {#function-should-close}

<div class="function-declaration" id="function-should-close-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> should_close() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `show` {#function-show}

<div class="function-declaration" id="function-show-1"><pre><code><span class="k">void</span> show() <span class="k">override</span>;</code></pre></div>

#### `simulate_close` {#function-simulate-close}

<div class="function-declaration" id="function-simulate-close-1"><pre><code><span class="k">void</span> simulate_close();</code></pre></div>

<p>Queues a <code><a href="../event/close-event.html">close_event</a></code>; <code><a href="#function-should-close-1">should_close</a>()</code> is <code>true</code> immediately.</p>

#### `simulate_content_scale` {#function-simulate-content-scale}

<div class="function-declaration" id="function-simulate-content-scale-1"><pre><code><span class="k">void</span> simulate_content_scale(
    <a href="../../core/math/glm-types.html#symbol-vec2">math::vec2</a> scale);</code></pre></div>

<p>Changes the content scale: queues a <code><a href="../event/content-scale-changed-event.html">content_scale_changed_event</a></code> and, when the framebuffer size changes with it, the coalesced <code><a href="../event/framebuffer-resized-event.html">framebuffer_resized_event</a></code>.</p>

#### `simulate_drop_files` {#function-simulate-drop-files}

<div class="function-declaration" id="function-simulate-drop-files-1"><pre><code><span class="k">void</span> simulate_drop_files(
    std::span&lt;<span class="k">const</span> std::string&gt; paths);</code></pre></div>

<p>Copies the paths and queues one <code><a href="../event/drop-files-event.html">drop_files_event</a></code> spanning them; the span is valid until the pump after the one that delivered it.</p>

#### `simulate_focus` {#function-simulate-focus}

<div class="function-declaration" id="function-simulate-focus-1"><pre><code><span class="k">void</span> simulate_focus(
    <span class="k">bool</span> focused);</code></pre></div>

<p>Changes the focus state and queues a <code><a href="../event/focus-event.html">focus_event</a></code>.</p>

#### `simulate_minimise` {#function-simulate-minimise}

<div class="function-declaration" id="function-simulate-minimise-1"><pre><code><span class="k">void</span> simulate_minimise(
    <span class="k">bool</span> minimised);</code></pre></div>

<p>Changes the minimised state and queues a <code><a href="../event/minimise-event.html">minimise_event</a></code>; the framebuffer becomes 0 x 0 while minimised (delivered as a coalesced resize) and comes back on restore.</p>

#### `simulate_resize` {#function-simulate-resize}

<div class="function-declaration" id="function-simulate-resize-1"><pre><code><span class="k">void</span> simulate_resize(
    <a href="../../core/types.html#symbol-u32">u32</a> width,
    <a href="../../core/types.html#symbol-u32">u32</a> height);</code></pre></div>

<p>Changes the logical size; the next pump ends with one <code><a href="../event/framebuffer-resized-event.html">framebuffer_resized_event</a></code> carrying <code>round(size * content_scale)</code> in pixels, however many resizes were simulated.</p>

#### `title` {#function-title}

<div class="function-declaration" id="function-title-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string_view title() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `video_modes` {#function-video-modes}

<div class="function-declaration" id="function-video-modes-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="../monitor/video-mode.html">video_mode</a>&gt; video_modes(
    <a href="../../core/types.html#symbol-u32">u32</a> monitorIndex) <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `window_size` {#function-window-size}

<div class="function-declaration" id="function-window-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/math/glm-types.html#symbol-uvec2">math::uvec2</a> window_size() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

### Private

#### `centred_position` {#function-centred-position}

<div class="function-declaration" id="function-centred-position-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/math/glm-types.html#symbol-ivec2">math::ivec2</a> centred_position() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `compute_framebuffer_size` {#function-compute-framebuffer-size}

<div class="function-declaration" id="function-compute-framebuffer-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/math/glm-types.html#symbol-uvec2">math::uvec2</a> compute_framebuffer_size() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `queue_resize` {#function-queue-resize}

<div class="function-declaration" id="function-queue-resize-1"><pre><code><span class="k">void</span> queue_resize() <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
