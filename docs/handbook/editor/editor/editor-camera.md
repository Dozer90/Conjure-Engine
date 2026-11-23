---
kind: header
---

[Editor](../../editor.md) → [User Interface](../user-interface.md)

# editor_camera.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/editor_camera.h` |
| API ↗ | [Headless mode and the command model](../../../api/editor/headless-and-commands.md), [The editor](../../../api/editor/index.md) |

<p>The editor viewport camera: lives in core so <code>view.pick</code>, <code>view.set_camera</code> and <code>view.focus</code> work headless. An orbit camera (position, target, up) with perspective or orthographic projection; the reverse-<code>Z</code> convention of the renderer is used for the projection matrix so gizmos and picking agree with what the renderer draws.</p>



## Classes

| File | Description |
|---|---|
| [`editor_camera`](editor-camera/editor-camera.md) |  |

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
<a id="editor_camera"></a>
<a id="editor_cameraapply_json"></a>
<a id="editor_cameraeditor_camera"></a>
<a id="editor_cameraeditor_camera_1"></a>
<a id="editor_camerafar_plane"></a>
<a id="editor_camerafocus"></a>
<a id="editor_cameraforward"></a>
<a id="editor_camerais_orthographic"></a>
<a id="editor_cameramfarplane"></a>
<a id="editor_cameramnearplane"></a>
<a id="editor_cameramorthographic"></a>
<a id="editor_cameramorthographicheight"></a>
<a id="editor_cameramposition"></a>
<a id="editor_cameramtarget"></a>
<a id="editor_cameramup"></a>
<a id="editor_cameramverticalfovradians"></a>
<a id="editor_cameranear_plane"></a>
<a id="editor_cameraoperator"></a>
<a id="editor_cameraorbit"></a>
<a id="editor_cameraorthographic_height"></a>
<a id="editor_camerapan"></a>
<a id="editor_cameraposition"></a>
<a id="editor_cameraprojection_matrix"></a>
<a id="editor_camerascreen_ray"></a>
<a id="editor_cameraset_look_at"></a>
<a id="editor_cameraset_orthographic"></a>
<a id="editor_cameraset_orthographic_height"></a>
<a id="editor_cameraset_planes"></a>
<a id="editor_cameraset_vertical_fov"></a>
<a id="editor_cameratarget"></a>
<a id="editor_camerato_json"></a>
<a id="editor_cameravertical_fov"></a>
<a id="editor_cameraview_matrix"></a>
<a id="editor_cameraworld_from_view"></a>
<a id="editor_cameraworld_units_per_pixel"></a>
<a id="editor_camerazoom"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
