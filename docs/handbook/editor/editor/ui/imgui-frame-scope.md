---
kind: header
---

[Editor](../../../editor.md) → [User Interface](../../user-interface.md)

# imgui_frame_scope.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/ui/imgui_frame_scope.h` |
| API ↗ | [The workspace UI](../../../../api/editor/workspace-ui.md) |

<p><code>A</code> self-contained ImGui frame for code that runs without the engine&#x27;s <code><a href="../../../engine/engine/imgui-layer/imgui-layer.html">imgui_layer</a></code>: the UI tests construct every panel against the <code>null</code> RHI and draw one frame inside this scope. When an ImGui context already exists (the engine&#x27;s), the scope only brackets frames on it; otherwise it creates and later destroys one with the default font atlas built and docking enabled. Nothing is rendered.</p>



## Classes

| File | Description |
|---|---|
| [`ui::imgui_frame_scope`](imgui-frame-scope/ui-imgui-frame-scope.md) |  |

## Forward Declarations

### `ImGuiContext` {#symbol-imguicontext}

```cpp
struct ImGuiContext;
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
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="imgui_frame_scope"></a>
<a id="imgui_frame_scopebegin_frame"></a>
<a id="imgui_frame_scopecontext"></a>
<a id="imgui_frame_scopeend_frame"></a>
<a id="imgui_frame_scopeframe_count"></a>
<a id="imgui_frame_scopeimgui_frame_scope"></a>
<a id="imgui_frame_scopeimgui_frame_scope_1"></a>
<a id="imgui_frame_scopeis_frame_open"></a>
<a id="imgui_frame_scopemcontext"></a>
<a id="imgui_frame_scopemdisplaysize"></a>
<a id="imgui_frame_scopemframeopen"></a>
<a id="imgui_frame_scopemframes"></a>
<a id="imgui_frame_scopemownscontext"></a>
<a id="imgui_frame_scopemprevious"></a>
<a id="imgui_frame_scopeoperator"></a>
<a id="imgui_frame_scopeowns_context"></a>
<a id="imgui_frame_scopeset_mouse"></a>
<a id="imguicontext"></a>
<a id="internals-and-algorithms"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="uiimgui_frame_scope"></a>
<a id="uiimgui_frame_scopebegin_frame"></a>
<a id="uiimgui_frame_scopecontext"></a>
<a id="uiimgui_frame_scopeend_frame"></a>
<a id="uiimgui_frame_scopeframe_count"></a>
<a id="uiimgui_frame_scopeimgui_frame_scope"></a>
<a id="uiimgui_frame_scopeimgui_frame_scope_1"></a>
<a id="uiimgui_frame_scopeis_frame_open"></a>
<a id="uiimgui_frame_scopemcontext"></a>
<a id="uiimgui_frame_scopemdisplaysize"></a>
<a id="uiimgui_frame_scopemframeopen"></a>
<a id="uiimgui_frame_scopemframes"></a>
<a id="uiimgui_frame_scopemownscontext"></a>
<a id="uiimgui_frame_scopemprevious"></a>
<a id="uiimgui_frame_scopeoperator"></a>
<a id="uiimgui_frame_scopeowns_context"></a>
<a id="uiimgui_frame_scopeset_mouse"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
