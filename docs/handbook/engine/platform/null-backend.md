---
kind: header
---

[Engine](../../engine.md) → [Platform](../platform.md)

# null_backend.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/platform/null_backend.h` |
| API ↗ | [Input](../../../api/platform/input.md), [Windows and events](../../../api/platform/windows-and-events.md) |

<p>The virtual window: no OS call of any kind, constructible without a display server, deterministic. Tests and <code>headless_engine</code> drive it through <code>push_event()</code> and the <code>simulate_</code>*() controls, which queue events for the next <code>pump_events()</code>; every setter updates state so the getters round-trip. One virtual monitor: &quot;Null Monitor&quot;, 1920 x 1080 @ 60 Hz, content scale nullContentScale (<code>simulate_content_scale</code> changes window and monitor together). Error contracts match the glfw backend so a caller tested here sees the same codes there: <code>CLIPBOARD_UNAVAILABLE</code> until text (even empty text) has been copied, <code>INVALID_ARGUMENT</code> for <code>null</code> pointers passed to <code>create_vulkan_surface</code> before <code>VULKAN_UNSUPPORTED</code>. Leaving fullscreen restores the last windowed position, not one set while fullscreen, as on glfw; a window created fullscreen without <code>window_desc::position</code> is centred on the virtual monitor instead.</p>

## Structs

| File | Description |
|---|---|
| [`null_backend::drop_range`](null-backend/null-backend-drop-range.md) |  |

## Classes

| File | Description |
|---|---|
| [`null_backend`](null-backend/null-backend.md) |  |

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="null_backend"></a>
<a id="null_backendas_null"></a>
<a id="null_backendbackend"></a>
<a id="null_backendcentred_position"></a>
<a id="null_backendclipboard_text"></a>
<a id="null_backendcompute_framebuffer_size"></a>
<a id="null_backendcontent_scale"></a>
<a id="null_backendcreate"></a>
<a id="null_backendcreate_vulkan_surface"></a>
<a id="null_backendcurrent_cursor_mode"></a>
<a id="null_backendcursor_position"></a>
<a id="null_backenddrop_range"></a>
<a id="null_backenddrop_rangecount"></a>
<a id="null_backenddrop_rangeoffset"></a>
<a id="null_backendframebuffer_size"></a>
<a id="null_backendgamepad_name"></a>
<a id="null_backendhide"></a>
<a id="null_backendicons"></a>
<a id="null_backendis_focused"></a>
<a id="null_backendis_fullscreen"></a>
<a id="null_backendis_minimised"></a>
<a id="null_backendis_visible"></a>
<a id="null_backendmclipboard"></a>
<a id="null_backendmclipboardhastext"></a>
<a id="null_backendmcontentscale"></a>
<a id="null_backendmcursormode"></a>
<a id="null_backendmcursorposition"></a>
<a id="null_backendmdelivered"></a>
<a id="null_backendmdelivereddroppaths"></a>
<a id="null_backendmdelivereddropranges"></a>
<a id="null_backendmfocused"></a>
<a id="null_backendmfullscreen"></a>
<a id="null_backendmgamepadconnected"></a>
<a id="null_backendmgamepadnames"></a>
<a id="null_backendmicons"></a>
<a id="null_backendmlogicalsize"></a>
<a id="null_backendmminimised"></a>
<a id="null_backendmmonitors"></a>
<a id="null_backendmonitors"></a>
<a id="null_backendmpending"></a>
<a id="null_backendmpendingdroppaths"></a>
<a id="null_backendmpendingdropranges"></a>
<a id="null_backendmpendingresize"></a>
<a id="null_backendmposition"></a>
<a id="null_backendmrawmousemotion"></a>
<a id="null_backendmshouldclose"></a>
<a id="null_backendmtitle"></a>
<a id="null_backendmvideomodes"></a>
<a id="null_backendmvisible"></a>
<a id="null_backendmwindowedposition"></a>
<a id="null_backendmwindowedpositionknown"></a>
<a id="null_backendmwindowedsize"></a>
<a id="null_backendnative_handle"></a>
<a id="null_backendnull_backend"></a>
<a id="null_backendnull_backend_1"></a>
<a id="null_backendpending_event_count"></a>
<a id="null_backendposition"></a>
<a id="null_backendpump_events"></a>
<a id="null_backendpush_event"></a>
<a id="null_backendqueue_resize"></a>
<a id="null_backendraw_mouse_motion"></a>
<a id="null_backendrequest_close"></a>
<a id="null_backendrequired_vulkan_instance_extensions"></a>
<a id="null_backendset_clipboard_text"></a>
<a id="null_backendset_cursor_mode"></a>
<a id="null_backendset_cursor_position"></a>
<a id="null_backendset_fullscreen"></a>
<a id="null_backendset_icon"></a>
<a id="null_backendset_position"></a>
<a id="null_backendset_raw_mouse_motion"></a>
<a id="null_backendset_size"></a>
<a id="null_backendset_title"></a>
<a id="null_backendshould_close"></a>
<a id="null_backendshow"></a>
<a id="null_backendsimulate_close"></a>
<a id="null_backendsimulate_content_scale"></a>
<a id="null_backendsimulate_drop_files"></a>
<a id="null_backendsimulate_focus"></a>
<a id="null_backendsimulate_minimise"></a>
<a id="null_backendsimulate_resize"></a>
<a id="null_backendtitle"></a>
<a id="null_backendvideo_modes"></a>
<a id="null_backendvirtual_monitor_height"></a>
<a id="null_backendvirtual_monitor_refresh_hz"></a>
<a id="null_backendvirtual_monitor_width"></a>
<a id="null_backendwindow_size"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
