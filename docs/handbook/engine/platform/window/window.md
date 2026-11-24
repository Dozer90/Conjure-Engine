---
kind: class
---

[Engine](../../../engine.md) → [Platform](../../platform.md) → [window.h](../window.md)

# window
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::platform` |
| Include File Path | `engine/include/conjure/platform/window.h` |
| Source File Path | N/A |
| API ↗ | [Input](../../../../api/platform/input.md), [Windows and events](../../../../api/platform/windows-and-events.md) |

## Syntax

```cpp
class CONJURE_API window
```

## Derived Classes


<details><summary>Classes that use this as a base</summary>

<ul>

<li><a href="../null-backend/null-backend.html">null_backend</a></li>

</ul>

</details>

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">window();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">virtual</span> ~window();</code></td><td></td></tr>
</tbody>
</table>



## Functions

### Public

#### `as_null` {#function-as-null}

<div class="function-declaration" id="function-as-null-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../null-backend/null-backend.html">null_backend</a> *as_null() <span class="k">noexcept</span>;</code></pre></div>

<p>This <code><a href="window.html">window</a></code> as the <code>null</code> backend, or <code>nullptr</code> for a real <code><a href="window.html">window</a></code>.</p>

#### `backend` {#function-backend}

<div class="function-declaration" id="function-backend-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../backend.html#symbol-backend-kind">backend_kind</a> backend() <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

<p>The backend behind this <code><a href="window.html">window</a></code>.</p>

#### `clipboard_text` {#function-clipboard-text}

<div class="function-declaration" id="function-clipboard-text-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../core/result/result.html">core::result</a>&lt;std::string&gt; clipboard_text() <span class="k">const</span> = <span class="mi">0</span>;</code></pre></div>

<p>The clipboard&#x27;s text (UTF-8). <code>CLIPBOARD_UNAVAILABLE</code> when it holds no text or the platform refused.</p>

#### `content_scale` {#function-content-scale}

<div class="function-declaration" id="function-content-scale-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../core/math/glm-types.html#symbol-vec2">math::vec2</a> content_scale() <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

<p><code><a href="#function-framebuffer-size-1">framebuffer_size</a>()</code> / <code><a href="#function-window-size-1">window_size</a>()</code> per axis: 1 on Windows and <code>X11</code>, where the framebuffer is as large as the <code><a href="window.html">window</a></code>, and, with <code>window_flags::SCALE_FRAMEBUFFER</code> (part of <code>DEFAULT</code>), the backing scale on a macOS Retina display or a scaled Wayland output; 1 there too without that flag. While either size has a zero axis (minimised) the last ratio the <code><a href="window.html">window</a></code> had is kept, 1 if it never had a size. It is not the OS UI scale a monitor is set to; <code><a href="#function-monitors-1">monitors</a>()</code>[i].contentScale reports that. The <code>null</code> backend reports <code>window_desc::nullContentScale</code>, then what <code>simulate_content_scale</code> set.</p>

#### `create_vulkan_surface` {#function-create-vulkan-surface}

<div class="function-declaration" id="function-create-vulkan-surface-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; create_vulkan_surface(
    <span class="k">void</span> *instance,
    <span class="k">void</span> **surface) <span class="k">const</span> = <span class="mi">0</span>;</code></pre></div>

<p>Creates a VkSurfaceKHR for VkInstance <code>instance</code> and stores it in *surface (both are 64-bit handles passed as opaque pointers; no Vulkan header is involved). <code>VULKAN_UNSUPPORTED</code> on the <code>null</code> backend or without a Vulkan loader; <code>SURFACE_CREATE_FAILED</code> with the VkResult in the message otherwise.</p>

#### `current_cursor_mode` {#function-current-cursor-mode}

<div class="function-declaration" id="function-current-cursor-mode-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../window.html#symbol-cursor-mode">cursor_mode</a> current_cursor_mode() <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

#### `framebuffer_size` {#function-framebuffer-size}

<div class="function-declaration" id="function-framebuffer-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../core/math/glm-types.html#symbol-uvec2">math::uvec2</a> framebuffer_size() <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

<p>Drawable size in pixels; 0 x 0 while minimised.</p>

#### `gamepad_name` {#function-gamepad-name}

<div class="function-declaration" id="function-gamepad-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> std::string_view gamepad_name(
    <a href="../../core/types.html#symbol-u32">u32</a> index) <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

<p>Human-readable name of a connected gamepad slot (glfwGetGamepadName); empty when the slot is not connected or the index is out of range.</p>

#### `hide` {#function-hide}

<div class="function-declaration" id="function-hide-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> hide() = <span class="mi">0</span>;</code></pre></div>

<p>Hides the <code><a href="window.html">window</a></code>, and the OS moves focus elsewhere (the <code>null</code> backend&#x27;s focus changes only through <code>simulate_focus</code>). <code>A</code> fullscreen <code><a href="window.html">window</a></code> too, except on Wayland (see <code><a href="#function-show-1">show</a>()</code>).</p>

#### `is_focused` {#function-is-focused}

<div class="function-declaration" id="function-is-focused-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <span class="k">bool</span> is_focused() <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

#### `is_fullscreen` {#function-is-fullscreen}

<div class="function-declaration" id="function-is-fullscreen-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <span class="k">bool</span> is_fullscreen() <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

#### `is_minimised` {#function-is-minimised}

<div class="function-declaration" id="function-is-minimised-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <span class="k">bool</span> is_minimised() <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

#### `is_visible` {#function-is-visible}

<div class="function-declaration" id="function-is-visible-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <span class="k">bool</span> is_visible() <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

#### `monitors` {#function-monitors}

<div class="function-declaration" id="function-monitors-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> std::span&lt;<span class="k">const</span> <a href="../monitor/monitor-info.html">monitor_info</a>&gt; monitors() <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

<p>The connected monitors, primary first; refreshed by <code><a href="#function-pump-events-1">pump_events</a>()</code>. <code>A</code> real <code><a href="window.html">window</a></code> rebuilds the list after a monitor connects or disconnects or the OS reports a new UI scale for the <code><a href="window.html">window</a></code>; a scale change on a monitor the <code><a href="window.html">window</a></code> is not on raises no such report, so that monitor&#x27;s contentScale stays stale until the next rebuild.</p>

#### `native_handle` {#function-native-handle}

<div class="function-declaration" id="function-native-handle-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../native-handle/native-window-handle.html">native_window_handle</a> native_handle() <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

<p>Opaque per-OS handles for rhi (<code>D3D12</code> swapchain, MoltenVK). kind == <code>NONE</code> on the <code>null</code> backend.</p>

#### `position` {#function-position}

<div class="function-declaration" id="function-position-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../core/math/glm-types.html#symbol-ivec2">math::ivec2</a> position() <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

<p>Position of the client area on the virtual desktop in logical coordinates.</p>

#### `pump_events` {#function-pump-events}

<div class="function-declaration" id="function-pump-events-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> std::span&lt;<span class="k">const</span> <a href="../event.html#symbol-event">event</a>&gt; pump_events() = <span class="mi">0</span>;</code></pre></div>

<p>Processes OS events and returns this frame&#x27;s batch; the span is valid until the next call. Several framebuffer size changes in one pump are coalesced into one <code><a href="../event/framebuffer-resized-event.html">framebuffer_resized_event</a></code> at the end of the batch. <code>A</code> <code><a href="../event/content-scale-changed-event.html">content_scale_changed_event</a></code> comes before it when a real <code><a href="window.html">window</a></code>&#x27;s framebuffer to <code><a href="window.html">window</a></code> ratio changed since the scale last announced (before any, the ratio the <code><a href="window.html">window</a></code> was created with, or 1 when it had no size then): a framebuffer that a fractional scale only rounds differently is no change. The <code>null</code> backend queues one per <code>simulate_content_scale</code> call.</p>

#### `request_close` {#function-request-close}

<div class="function-declaration" id="function-request-close-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> request_close() <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

<p>Marks the <code><a href="window.html">window</a></code> as asked to close; <code><a href="#function-should-close-1">should_close</a>()</code> is <code>true</code> afterwards.</p>

#### `required_vulkan_instance_extensions` {#function-required-vulkan-instance-extensions}

<div class="function-declaration" id="function-required-vulkan-instance-extensions-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> std::span&lt;<span class="k">const</span> <span class="k">char</span> *<span class="k">const</span>&gt; required_vulkan_instance_extensions() <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

<p>Instance extensions Vulkan needs to present to this <code><a href="window.html">window</a></code>; empty on the <code>null</code> backend and when Vulkan is unavailable.</p>

#### `set_clipboard_text` {#function-set-clipboard-text}

<div class="function-declaration" id="function-set-clipboard-text-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; set_clipboard_text(
    std::string_view text) = <span class="mi">0</span>;</code></pre></div>

<p>Replaces the clipboard&#x27;s text (UTF-8).</p>

#### `set_cursor_mode` {#function-set-cursor-mode}

<div class="function-declaration" id="function-set-cursor-mode-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; set_cursor_mode(
    <a href="../window.html#symbol-cursor-mode">cursor_mode</a> mode) = <span class="mi">0</span>;</code></pre></div>

<p>Changes how the cursor behaves over the <code><a href="window.html">window</a></code>. <code>FEATURE_UNAVAILABLE</code> (state unchanged) where the mode is not supported, e.g. <code>CAPTURED</code> on Wayland.</p>

#### `set_cursor_position` {#function-set-cursor-position}

<div class="function-declaration" id="function-set-cursor-position-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> set_cursor_position(
    <a href="../../core/math/glm-types.html#symbol-vec2">math::vec2</a> logicalPosition) = <span class="mi">0</span>;</code></pre></div>

<p>Warps the cursor to a position in logical <code><a href="window.html">window</a></code> coordinates.</p>

#### `set_fullscreen` {#function-set-fullscreen}

<div class="function-declaration" id="function-set-fullscreen-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; set_fullscreen(
    <span class="k">bool</span> fullscreen,
    <a href="../../core/types.html#symbol-u32">u32</a> monitorIndex = <span class="mi">0</span>) = <span class="mi">0</span>;</code></pre></div>

<p>Borderless fullscreen on <code><a href="#function-monitors-1">monitors</a>()</code>[<code>monitorIndex</code>] with its current video mode: the <code><a href="window.html">window</a></code> loses its decoration and covers the monitor without owning it, so losing focus neither minimises it nor keeps it above other applications (an <code>X11</code> <code><a href="window.html">window</a></code> manager decides itself whether its panels stay above). Called while fullscreen, the <code><a href="window.html">window</a></code> moves to that monitor. <code>false</code> returns to the previous windowed size, position and decoration; a <code><a href="window.html">window</a></code> created fullscreen without <code>window_desc::position</code> is centred on its monitor&#x27;s work area. Wayland, where a <code><a href="window.html">window</a></code> cannot be placed, uses the compositor&#x27;s fullscreen instead, which also stays on focus loss. While a fullscreen <code><a href="window.html">window</a></code> is shown and not minimised, the display stays awake on Windows and macOS hides the menu bar and the Dock whenever the application is active; on Wayland the display stays awake while fullscreen when the compositor supports the idle-inhibit protocol, and an <code>X11</code> screen saver may still start. <code>A</code> <code><a href="window.html">window</a></code> created fullscreen is shown only with <code>window_flags::VISIBLE</code>, except on Wayland, and <code>window_flags::MAXIMISED</code> maximises it only when it first leaves fullscreen (the <code>null</code> backend has no maximised state). <code>MONITOR_NOT_FOUND</code> leaves the <code><a href="window.html">window</a></code> unchanged.</p>

#### `set_icon` {#function-set-icon}

<div class="function-declaration" id="function-set-icon-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; set_icon(
    std::span&lt;<span class="k">const</span> <a href="../../core/image/image.html">core::image</a>&gt; images) = <span class="mi">0</span>;</code></pre></div>

<p>Sets the icon the OS shows for this <code><a href="window.html">window</a></code> (title bar, task switcher, taskbar) from RGBA8 images; the OS picks the size closest to each use, so pass several (16, 32 and 48 px at least). The pixels are copied. An empty span restores the default icon. <code>INVALID_ARGUMENT</code>, icon unchanged, when <code>validate_window_icon</code> rejects the images; <code>FEATURE_UNAVAILABLE</code> where windows have no icon of their own (macOS shows the application bundle&#x27;s, Wayland the desktop file&#x27;s).</p>

#### `set_position` {#function-set-position}

<div class="function-declaration" id="function-set-position-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; set_position(
    <a href="../../core/math/glm-types.html#symbol-ivec2">math::ivec2</a> position) = <span class="mi">0</span>;</code></pre></div>

<p>Moves the <code><a href="window.html">window</a></code> on the virtual desktop. <code>FEATURE_UNAVAILABLE</code> where the display server does not allow it (Wayland). Ignored while fullscreen (success, nothing moves): leaving fullscreen restores the windowed position from before.</p>

#### `set_raw_mouse_motion` {#function-set-raw-mouse-motion}

<div class="function-declaration" id="function-set-raw-mouse-motion-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; set_raw_mouse_motion(
    <span class="k">bool</span> enabled) = <span class="mi">0</span>;</code></pre></div>

<p>Raw (unaccelerated) mouse motion while the cursor is <code>DISABLED</code>. <code>FEATURE_UNAVAILABLE</code> when the system does not provide it.</p>

#### `set_size` {#function-set-size}

<div class="function-declaration" id="function-set-size-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> set_size(
    <a href="../../core/math/glm-types.html#symbol-uvec2">math::uvec2</a> logicalSize) = <span class="mi">0</span>;</code></pre></div>

<p>Resizes the <code><a href="window.html">window</a></code> in logical coordinates; the framebuffer change arrives as an event. Ignored while fullscreen: the <code><a href="window.html">window</a></code> keeps covering its monitor, and leaving fullscreen restores the windowed size from before.</p>

#### `set_title` {#function-set-title}

<div class="function-declaration" id="function-set-title-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> set_title(
    std::string_view title) = <span class="mi">0</span>;</code></pre></div>

<p>Changes the title shown by the OS (UTF-8).</p>

#### `should_close` {#function-should-close}

<div class="function-declaration" id="function-should-close-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <span class="k">bool</span> should_close() <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

<p>True once the user or <code><a href="#function-request-close-1">request_close</a>()</code> asked the <code><a href="window.html">window</a></code> to close.</p>

#### `show` {#function-show}

<div class="function-declaration" id="function-show-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> show() = <span class="mi">0</span>;</code></pre></div>

<p>Shows the <code><a href="window.html">window</a></code>; it takes focus when the OS allows it. <code>A</code> fullscreen <code><a href="window.html">window</a></code> too, except the compositor fullscreen of Wayland, which <code>GLFW</code> never shows or hides.</p>

#### `title` {#function-title}

<div class="function-declaration" id="function-title-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> std::string_view title() <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

<p>The current title.</p>

#### `video_modes` {#function-video-modes}

<div class="function-declaration" id="function-video-modes-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> std::span&lt;<span class="k">const</span> <a href="../monitor/video-mode.html">video_mode</a>&gt; video_modes(
    <a href="../../core/types.html#symbol-u32">u32</a> monitorIndex) <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

<p>The video modes a monitor supports; empty for an unknown index.</p>

#### `window_size` {#function-window-size}

<div class="function-declaration" id="function-window-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../core/math/glm-types.html#symbol-uvec2">math::uvec2</a> window_size() <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

<p>Size in logical screen coordinates.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->
