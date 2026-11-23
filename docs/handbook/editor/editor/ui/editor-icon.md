---
kind: header
---

[Editor](../../../editor.md) → [User Interface](../../user-interface.md)

# editor_icon.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/ui/editor_icon.h` |
| API ↗ | [The workspace UI](../../../../api/editor/workspace-ui.md) |

<p>The editor&#x27;s window icon (docs/handbook/<code>brand.md</code>): the official Conjure logo in the Midnight accent, compiled into <code>editor_ui</code> as one <code>PNG</code> per size by tools/<code>generate_brand_icons.py</code>. On Windows the executable also carries the icon as its <code>GLFW_ICON</code> resource, so Explorer, the taskbar and the first frame show it before the layer attaches.</p>

## Forward Declarations

### `window` {#symbol-window}

```cpp
class window;
```

## Functions

### Public

#### `ui::apply_editor_icon` {#function-ui-apply-editor-icon}

<div class="function-declaration" id="function-ui-apply-editor-icon-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../../../engine/core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; apply_editor_icon(<a href="../../../engine/platform/window/window.html">platform::window</a> &amp;window);</code></pre></div>

<p>Decodes the icon and sets it on a window (<code>window::set_icon</code>). <code>FEATURE_UNAVAILABLE</code> where windows have no icon of their own (macOS, Wayland); on any error the window keeps the icon it had.</p>

#### `ui::editor_icon_images` {#function-ui-editor-icon-images}

<div class="function-declaration" id="function-ui-editor-icon-images-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../../../engine/core/result/result.html">core::result</a>&lt;std::vector&lt;<a href="../../../engine/core/image/image.html">core::image</a>&gt;&gt; editor_icon_images();</code></pre></div>

<p>Decodes the icon: one square RGBA8 image per size, smallest first, from 16 to 256 px. <code>PARSE_FAILURE</code> naming the size when an embedded <code>PNG</code> does not decode, which only a hand-edited generated file can cause.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="apply_editor_icon"></a>
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="editor_icon_images"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="uiapply_editor_icon"></a>
<a id="uieditor_icon_images"></a>
<a id="variables-and-properties"></a>
<a id="window"></a>
<!-- doc-legacy-anchors:end -->
