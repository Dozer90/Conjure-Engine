---
kind: header
---

[Engine](../../engine.md) → [Platform](../platform.md)

# window.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/platform/window.h` |
| API ↗ | [Input](../../../api/platform/input.md), [Windows and events](../../../api/platform/windows-and-events.md) |

<p>The abstract <code><a href="window/window.html">window</a></code>. One live <code><a href="window/window.html">window</a></code> per process in v1; created through <code>create_window()</code> for the backend named in <code><a href="window/window-desc.html">window_desc</a></code>. Every call is main thread only (asserted in Debug and Development); no call at frame time can fail.</p>

## Structs

| File | Description |
|---|---|
| [`window_desc`](window/window-desc.md) |  |

## Classes

| File | Description |
|---|---|
| [`window`](window/window.md) |  |

## Enumerations

### `cursor_mode` {#symbol-cursor-mode}

```cpp
enum class cursor_mode : u8 {           // GLFW_CURSOR_* (CAPTURED is 3.4)
    NORMAL = 0,
    HIDDEN,
    DISABLED,
    CAPTURED
};
```

### `display_server_hint` {#symbol-display-server-hint}

```cpp
enum class display_server_hint : u8 {   // Linux only; the GLFW_PLATFORM init hint
    ANY = 0,
    X11,
    WAYLAND
};
```

### `window_flags` {#symbol-window-flags}

```cpp
enum class window_flags : u8 {
    NONE = 0,
    RESIZABLE = 1 << 0,
    DECORATED = 1 << 1,
    VISIBLE = 1 << 2,
    FLOATING = 1 << 3,
    MAXIMISED = 1 << 4,
    SCALE_TO_MONITOR = 1 << 5,
    SCALE_FRAMEBUFFER = 1 << 6,
    DEFAULT = RESIZABLE | DECORATED | VISIBLE | SCALE_TO_MONITOR | SCALE_FRAMEBUFFER
};
```

## Forward Declarations

### `null_backend` {#symbol-null-backend}

```cpp
class null_backend;
```

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-MAX_WINDOW_ICON_DIMENSION"></a><code>MAX_WINDOW_ICON_DIMENSION</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">256</span></code> | Largest width or height of one <code><a href="window/window.html">window</a></code> icon image in pixels: the largest icon Windows draws, and small enough that a full set fits in one <code>X11</code> <code><a href="window/window.html">window</a></code> property. |
| <a id="variable-MAX_WINDOW_ICON_IMAGES"></a><code>MAX_WINDOW_ICON_IMAGES</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-usize">usize</a></code> | <code class="signature"><span class="mi">16</span></code> | Most images <code>window::set_icon</code> accepts in one call. |
| <a id="variable-MAX_WINDOW_TITLE_BYTES"></a><code>MAX_WINDOW_TITLE_BYTES</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-usize">usize</a></code> | <code class="signature"><span class="mi">255</span></code> | Longest title <code>create_window</code> accepts, in bytes. |

## Macros

### `CONJURE_ENUM_FLAGS` {#symbol-conjure-enum-flags}

```cpp
CONJURE_ENUM_FLAGS(window_flags)
```

## Functions

### Public

#### `create_window` {#function-create-window}

<div class="function-declaration" id="function-create-window-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;std::unique_ptr&lt;<a href="window/window.html">window</a>&gt;&gt; create_window(
    <span class="k">const</span> <a href="window/window-desc.html">window_desc</a> &amp;desc);</code></pre></div>

<p>Creates the <code><a href="window/window.html">window</a></code> for <code>desc.backend</code>. Main thread only. Exactly one live <code><a href="window/window.html">window</a></code> per process in v1: a second call asserts and returns <code>INVALID_DESC</code>. <code>BACKEND_NOT_COMPILED</code> when the glfw backend was not built; <code>INIT_FAILED</code> / <code>WINDOW_CREATE_FAILED</code> / <code>MONITOR_NOT_FOUND</code> from the glfw backend as described in.</p>

#### `live_window_count` {#function-live-window-count}

<div class="function-declaration" id="function-live-window-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/types.html#symbol-u32">u32</a> live_window_count() <span class="k">noexcept</span>;</code></pre></div>

<p>Number of windows alive in this process (0 or 1 in v1).</p>

#### `parse_cursor_mode` {#function-parse-cursor-mode}

<div class="function-declaration" id="function-parse-cursor-mode-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::optional&lt;<a href="window.html#symbol-cursor-mode">cursor_mode</a>&gt; parse_cursor_mode(
    std::string_view name) <span class="k">noexcept</span>;</code></pre></div>

<p>Parses a cursor mode name case-insensitively; nullopt for anything else.</p>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="window.html#symbol-cursor-mode">cursor_mode</a> mode) <span class="k">noexcept</span>;</code></pre></div>

<p>The enumerator name of a cursor mode (&quot;<code>DISABLED</code>&quot;), or &quot;&quot; for unknown values.</p>

#### `validate_window_desc` {#function-validate-window-desc}

<div class="function-declaration" id="function-validate-window-desc-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; validate_window_desc(
    <span class="k">const</span> <a href="window/window-desc.html">window_desc</a> &amp;desc);</code></pre></div>

<p>Checks a description the way <code>create_window</code> does: a non-fullscreen <code><a href="window/window.html">window</a></code> needs a non-zero size, the title is at most <code>MAX_WINDOW_TITLE_BYTES</code> bytes and the backend is a known kind. Fails with <code>INVALID_DESC</code> naming the field.</p>

#### `validate_window_icon` {#function-validate-window-icon}

<div class="function-declaration" id="function-validate-window-icon-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; validate_window_icon(
    std::span&lt;<span class="k">const</span> <a href="../core/image/image.html">core::image</a>&gt; images);</code></pre></div>

<p>Checks icon images the way <code>window::set_icon</code> does: at most <code>MAX_WINDOW_ICON_IMAGES</code> of them, none empty and none wider or taller than <code>MAX_WINDOW_ICON_DIMENSION</code>. An empty span is valid (the default icon). Fails with <code>INVALID_ARGUMENT</code> naming the first offending image.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="conjure_enum_flags"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="create_window"></a>
<a id="cursor_mode"></a>
<a id="cursor_modecaptured"></a>
<a id="cursor_modedisabled"></a>
<a id="cursor_modehidden"></a>
<a id="cursor_modenormal"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="display_server_hint"></a>
<a id="display_server_hintany"></a>
<a id="display_server_hintwayland"></a>
<a id="display_server_hintx11"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="live_window_count"></a>
<a id="max_window_icon_dimension"></a>
<a id="max_window_icon_images"></a>
<a id="max_window_title_bytes"></a>
<a id="null_backend"></a>
<a id="other-declarations"></a>
<a id="parse_cursor_mode"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="validate_window_desc"></a>
<a id="validate_window_icon"></a>
<a id="variables-and-properties"></a>
<a id="window"></a>
<a id="window_desc"></a>
<a id="window_descbackend"></a>
<a id="window_descdisplayserver"></a>
<a id="window_descflags"></a>
<a id="window_descfullscreen"></a>
<a id="window_descgamepadmappings"></a>
<a id="window_descheight"></a>
<a id="window_descmonitorindex"></a>
<a id="window_descnullcontentscale"></a>
<a id="window_descposition"></a>
<a id="window_desctitle"></a>
<a id="window_descwidth"></a>
<a id="window_flags"></a>
<a id="window_flagsdecorated"></a>
<a id="window_flagsdefault"></a>
<a id="window_flagsfloating"></a>
<a id="window_flagsmaximised"></a>
<a id="window_flagsnone"></a>
<a id="window_flagsresizable"></a>
<a id="window_flagsscale_framebuffer"></a>
<a id="window_flagsscale_to_monitor"></a>
<a id="window_flagsvisible"></a>
<a id="windowas_null"></a>
<a id="windowbackend"></a>
<a id="windowclipboard_text"></a>
<a id="windowcontent_scale"></a>
<a id="windowcreate_vulkan_surface"></a>
<a id="windowcurrent_cursor_mode"></a>
<a id="windowframebuffer_size"></a>
<a id="windowgamepad_name"></a>
<a id="windowhide"></a>
<a id="windowis_focused"></a>
<a id="windowis_fullscreen"></a>
<a id="windowis_minimised"></a>
<a id="windowis_visible"></a>
<a id="windowmonitors"></a>
<a id="windownative_handle"></a>
<a id="windowoperator"></a>
<a id="windowposition"></a>
<a id="windowpump_events"></a>
<a id="windowrequest_close"></a>
<a id="windowrequired_vulkan_instance_extensions"></a>
<a id="windowset_clipboard_text"></a>
<a id="windowset_cursor_mode"></a>
<a id="windowset_cursor_position"></a>
<a id="windowset_fullscreen"></a>
<a id="windowset_icon"></a>
<a id="windowset_position"></a>
<a id="windowset_raw_mouse_motion"></a>
<a id="windowset_size"></a>
<a id="windowset_title"></a>
<a id="windowshould_close"></a>
<a id="windowshow"></a>
<a id="windowtitle"></a>
<a id="windowvideo_modes"></a>
<a id="windowwindow"></a>
<a id="windowwindow_1"></a>
<a id="windowwindow_size"></a>
<!-- doc-legacy-anchors:end -->
