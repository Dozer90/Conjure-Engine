---
kind: header
---

[Engine](../../engine.md) → [Platform](../platform.md)

# native_handle.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/platform/native_handle.h` |
| API ↗ | [Windows and events](../../../api/platform/windows-and-events.md) |

<p>Opaque per-OS window handles for the rhi: the <code>D3D12</code> swapchain needs the HWND, MoltenVK the NSWindow. No OS header is involved; the pointers are what the backend got from <code>GLFW</code>&#x27;s native access functions. kind == <code>NONE</code> on the <code>null</code> backend.</p>

## Structs

| File | Description |
|---|---|
| [`native_window_handle`](native-handle/native-window-handle.md) |  |

## Enumerations

### `native_handle_kind` {#symbol-native-handle-kind}

```cpp
enum class native_handle_kind : u8 {
    NONE = 0,
    WINDOWS,    // not WIN32: CMake defines that macro on every Windows build
    COCOA,
    X11,
    WAYLAND
};
```

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
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="native_handle_kind"></a>
<a id="native_handle_kindcocoa"></a>
<a id="native_handle_kindnone"></a>
<a id="native_handle_kindwayland"></a>
<a id="native_handle_kindwindows"></a>
<a id="native_handle_kindx11"></a>
<a id="native_window_handle"></a>
<a id="native_window_handledisplay"></a>
<a id="native_window_handlekind"></a>
<a id="native_window_handleoperator"></a>
<a id="native_window_handlewindow"></a>
<a id="native_window_handlex11window"></a>
<a id="other-declarations"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
