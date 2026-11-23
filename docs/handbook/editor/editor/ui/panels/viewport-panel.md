---
kind: header
---

[Editor](../../../../editor.md) → [User Interface](../../../user-interface.md)

# viewport_panel.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/ui/panels/viewport_panel.h` |
| API ↗ | [The workspace UI](../../../../../api/editor/workspace-ui.md) |

<p>The viewport: the scene through the editor camera, drawn every frame into an off-screen target the size of the panel and shown as an image (without a window there is no ImGui renderer and the panel is the input surface alone), with the input surface over it: the gizmo for the selection, click picking through <code>view.pick</code>, right-drag orbit, middle-drag pan and wheel zoom of the editor camera, and the operation/space/snap toolbar. The picture, the pick ray and the gizmo all use the editor camera and the panel&#x27;s rectangle, so what is clicked is what is seen.</p>



## Classes

| File | Description |
|---|---|
| [`ui::viewport_panel`](viewport-panel/ui-viewport-panel.md) |  |

## Forward Declarations

### `scene_view` {#symbol-scene-view}

```cpp
class scene_view;
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
<a id="internals-and-algorithms"></a>
<a id="scene_view"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="uiviewport_panel"></a>
<a id="uiviewport_paneldraw_contents"></a>
<a id="uiviewport_paneldraw_scene_picture"></a>
<a id="uiviewport_paneldraw_snap_controls"></a>
<a id="uiviewport_paneldraw_toolbar"></a>
<a id="uiviewport_panelgizmo"></a>
<a id="uiviewport_panelhandle_camera_input"></a>
<a id="uiviewport_panelmgizmo"></a>
<a id="uiviewport_panelmpicks"></a>
<a id="uiviewport_panelmpicturesdrawn"></a>
<a id="uiviewport_panelmrectmin"></a>
<a id="uiviewport_panelmrectsize"></a>
<a id="uiviewport_panelmscenetexture"></a>
<a id="uiviewport_panelmsceneview"></a>
<a id="uiviewport_panelmtargetfailurelogged"></a>
<a id="uiviewport_panelmtextureowner"></a>
<a id="uiviewport_panelpick_at"></a>
<a id="uiviewport_panelpick_count"></a>
<a id="uiviewport_panelpictures_drawn"></a>
<a id="uiviewport_panelrect_min"></a>
<a id="uiviewport_panelrect_size"></a>
<a id="uiviewport_panelviewport_panel"></a>
<a id="uiviewport_panelviewport_panel_1"></a>
<a id="variables-and-properties"></a>
<a id="viewport_panel"></a>
<a id="viewport_paneldraw_contents"></a>
<a id="viewport_paneldraw_scene_picture"></a>
<a id="viewport_paneldraw_snap_controls"></a>
<a id="viewport_paneldraw_toolbar"></a>
<a id="viewport_panelgizmo"></a>
<a id="viewport_panelhandle_camera_input"></a>
<a id="viewport_panelmgizmo"></a>
<a id="viewport_panelmpicks"></a>
<a id="viewport_panelmpicturesdrawn"></a>
<a id="viewport_panelmrectmin"></a>
<a id="viewport_panelmrectsize"></a>
<a id="viewport_panelmscenetexture"></a>
<a id="viewport_panelmsceneview"></a>
<a id="viewport_panelmtargetfailurelogged"></a>
<a id="viewport_panelmtextureowner"></a>
<a id="viewport_panelpick_at"></a>
<a id="viewport_panelpick_count"></a>
<a id="viewport_panelpictures_drawn"></a>
<a id="viewport_panelrect_min"></a>
<a id="viewport_panelrect_size"></a>
<a id="viewport_panelviewport_panel"></a>
<a id="viewport_panelviewport_panel_1"></a>
<!-- doc-legacy-anchors:end -->
