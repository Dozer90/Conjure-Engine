---
kind: header
---

[Engine](../../engine.md) → [RHI](../rhi.md)

# native_window.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/rhi/native_window.h` |
| API ↗ | [Devices and swapchains](../../../api/rhi/devices-and-swapchains.md) |

<p>The operating-system window a swapchain presents to, as opaque handles. The fields mirror <code><a href="../platform/native-handle/native-window-handle.html">platform::native_window_handle</a></code> one to one so the engine copies <code>window.native_handle()</code> into this struct; the rhi module itself never includes a platform header. <code>A</code> handle of kind <code>NONE</code> means headless: the <code>null</code> device ignores it and the <code>D3D12</code> backend creates a composition swapchain that is never attached to a visual, which lets GPU tests exercise the real present path without a window.</p>

## Structs

| File | Description |
|---|---|
| [`native_window`](native-window/native-window.md) |  |

## Enumerations

### `native_window_kind` {#symbol-native-window-kind}

```cpp
enum class native_window_kind : u8 {
    NONE = 0,
    WINDOWS,
    COCOA,
    X11,
    WAYLAND
};
```

## Functions

### Public

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="native-window.html#symbol-native-window-kind">native_window_kind</a> kind) <span class="k">noexcept</span>;</code></pre></div>

<p>The enumerator name of a window kind (&quot;<code>WINDOWS</code>&quot;, &quot;<code>NONE</code>&quot;); &quot;&quot; for an out-of-range value.</p>

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
<a id="internals-and-algorithms"></a>
<a id="native_window"></a>
<a id="native_window_kind"></a>
<a id="native_window_kindcocoa"></a>
<a id="native_window_kindnone"></a>
<a id="native_window_kindwayland"></a>
<a id="native_window_kindwindows"></a>
<a id="native_window_kindx11"></a>
<a id="native_windowdisplay"></a>
<a id="native_windowis_headless"></a>
<a id="native_windowkind"></a>
<a id="native_windowwindow"></a>
<a id="native_windowx11window"></a>
<a id="other-declarations"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
