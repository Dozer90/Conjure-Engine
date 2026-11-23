---
kind: component
---

[Editor](../editor.md)

# User Interface
<!-- doc-template:component -->

|---|---|
| Type | Component |
| Implementation | `conjure::editor_core`, `conjure::editor_ui` |
| Namespace | `conjure::editor` |
| Include Folder Path | `engine/include/conjure/editor` |
| Source Folder Path | `engine/src/editor` |

<!-- doc-content:purpose:start -->
The docking workspace, panels, selection, camera, picking, and property editing.
<!-- doc-content:purpose:end -->



## Headers

| File | Description | Provides |
|---|---|---|
| [ai_session_panel.h](editor/ui/panels/ai-session-panel.md) | The AI session panel: the server&#x27;s port and clients, a request line sent through an in-process loopback client, and the transcript of responses and events that came back. | [`ui::ai_session_panel`](editor/ui/panels/ai-session-panel/ui-ai-session-panel.md) |
| [asset_browser_panel.h](editor/ui/panels/asset-browser-panel.md) | The asset browser: the database&#x27;s records as a filterable table (path, type, status), import by path, reimport, delete, open scenes and instantiate prefabs; all through asset.*, <code>scene.open</code> and <code>prefab.instantiate</code>. Deleting the file behind the open scene asks first: a confirmation dialog closes the scene and deletes its file; every other asset is deleted at once and comes back with undo. | [`ui::asset_browser_panel`](editor/ui/panels/asset-browser-panel/ui-asset-browser-panel.md) |
| [console_buffer.h](editor/console-buffer.md) | The console ring: 65,536 entries and 16 MiB of text fed by a log sink that may be called from any thread, read on the main thread by the console panel, the <code>log</code> event and the <code>console.read</code> command. The mutex is never held across an allocation: the text ring is preallocated. | [`console_buffer`](editor/console-buffer/console-buffer.md)<br>[`console_buffer::entry`](editor/console-buffer/console-buffer-entry.md)<br>[`console_query`](editor/console-buffer/console-query.md)<br>[`console_record`](editor/console-buffer/console-record.md) |
| [console_panel.h](editor/ui/panels/console-panel.md) | The console: the newest records of the <code><a href="editor/console-buffer/console-buffer.html">console_buffer</a></code> ring with a level filter, a category filter, auto-scroll and clear. | [`ui::console_panel`](editor/ui/panels/console-panel/ui-console-panel.md) |
| [editor_camera.h](editor/editor-camera.md) | The editor viewport camera: lives in core so <code>view.pick</code>, <code>view.set_camera</code> and <code>view.focus</code> work headless. An orbit camera (position, target, up) with perspective or orthographic projection; the reverse-<code>Z</code> convention of the renderer is used for the projection matrix so gizmos and picking agree with what the renderer draws. | [`editor_camera`](editor/editor-camera/editor-camera.md) |
| [editor_icon.h](editor/ui/editor-icon.md) | The editor&#x27;s window icon (docs/handbook/<code>brand.md</code>): the official Conjure logo in the Midnight accent, compiled into <code>editor_ui</code> as one <code>PNG</code> per size by tools/<code>generate_brand_icons.py</code>. On Windows the executable also carries the icon as its <code>GLFW_ICON</code> resource, so Explorer, the taskbar and the first frame show it before the layer attaches. |  |
| [editor_layer.h](editor/ui/editor-layer.md) | The engine layer that draws the editor: <code>on_update</code> refreshes the panels&#x27; caches, <code>on_imgui</code> draws the workspace inside the frame the engine&#x27;s <code><a href="../engine/engine/imgui-layer/imgui-layer.html">imgui_layer</a></code> opened. It is pushed after <code><a href="../engine/engine/imgui-layer/imgui-layer.html">imgui_layer</a></code> by the executable through <code><a href="editor/editor-application/editor-application.html">editor_application</a></code>&#x27;s layer factory. | [`ui::editor_layer`](editor/ui/editor-layer/ui-editor-layer.md) |
| [editor_panel.h](editor/ui/editor-panel.md) | Panels: one ImGui window each, drawn by the workspace every frame while open. Panels own only UI state and submit <code>command_requests</code> through the context&#x27;s dispatcher. The registry lists the built-in panels and the ones loaded modules provide (<code><a href="editor/panel-desc/panel-desc.html">panel_desc</a></code> records). | [`ui::editor_panel`](editor/ui/editor-panel/ui-editor-panel.md)<br>[`ui::panel_descriptor`](editor/ui/editor-panel/ui-panel-descriptor.md)<br>[`ui::panel_registry`](editor/ui/editor-panel/ui-panel-registry.md) |
| [editor_workspace.h](editor/ui/editor-workspace.md) | The docking workspace: the full-window dockspace with a pass-through central node (the scene renders underneath), the main menu bar, the global shortcuts (Ctrl+<code>Z</code>, Ctrl+<code>Y</code>, Ctrl+<code>S</code>, <code>F5</code>, Shift+<code>F5</code>, handled here so they work whichever panels are open), the Save Scene As dialog, the project trust question (decision: &quot;Do you trust the authors of this project?&quot;, shown while the context&#x27;s <code>trust_prompt()</code> names a project), the panel instances and the layout file &lt;project&gt;/.conjure/<code>editor_layout.ini</code>. | [`ui::editor_workspace`](editor/ui/editor-workspace/ui-editor-workspace.md)<br>[`ui::editor_workspace::panel_instance`](editor/ui/editor-workspace/ui-editor-workspace-panel-instance.md)<br>[`ui::menu_request`](editor/ui/editor-workspace/ui-menu-request.md) |
| [export_dialog.h](editor/ui/panels/export-dialog.md) | The export dialog: the output directory, the <code>project.export</code> command and the progress events it emits. An export is always a Distribution export, so there is no configuration to choose. Each dialog owns its export&#x27;s state; the event client and response sink it registers are released when the export completes or the dialog is destroyed, whichever comes first. | [`ui::export_dialog`](editor/ui/panels/export-dialog/ui-export-dialog.md) |
| [gizmo_controller.h](editor/ui/gizmo-controller.md) | Transform gizmos over ImGuizmo: the first manipulated frame opens a transaction and caches the selected entities&#x27; world matrices; every frame with a delta dispatches <code>component.set_many</code> on the local transforms (through the parents&#x27; inverse world matrices, pivoting at the centroid) with the drag session as coalesce key; the release ends the transaction. Descendants of selected entities follow their ancestor instead of receiving the delta themselves. | [`ui::gizmo_controller`](editor/ui/gizmo-controller/ui-gizmo-controller.md)<br>[`ui::gizmo_controller::dragged_entity`](editor/ui/gizmo-controller/ui-gizmo-controller-dragged-entity.md) |
| [hierarchy_cache.h](editor/ui/hierarchy-cache.md) | The hierarchy panel&#x27;s flat row list: one pre-order walk per structural change, expansion state (by scene guid) and name filter, drawn with a list clipper so 10,000 entities cost <code>O(visible rows)</code>. Names and enabled state, which change without a structural change, are refreshed for the visible rows only (<code>refresh_labels</code>). | [`ui::hierarchy_cache`](editor/ui/hierarchy-cache/ui-hierarchy-cache.md)<br>[`ui::hierarchy_row`](editor/ui/hierarchy-cache/ui-hierarchy-row.md) |
| [hierarchy_panel.h](editor/ui/panels/hierarchy-panel.md) | The scene tree: flat rows from <code><a href="editor/ui/hierarchy-cache/ui-hierarchy-cache.html">hierarchy_cache</a></code> drawn with a list clipper, click, ctrl-click (toggle) and shift-click (range from the last clicked row) selection, drag and drop reparenting (<code>entity.set_parent</code>, dropping below the rows moves to the root), a context menu for create, duplicate, rename, enable and destroy, inline rename (<code>F2</code> or the menu; Enter or clicking away commits, Escape cancels) and a name filter. <code>A</code> selection made elsewhere (a viewport pick) expands its ancestors and scrolls its row into view. | [`ui::hierarchy_panel`](editor/ui/panels/hierarchy-panel/ui-hierarchy-panel.md) |
| [history_panel.h](editor/ui/panels/history-panel.md) | The undo history: every entry with its label and client, the cursor, undo / redo / clear. | [`ui::history_panel`](editor/ui/panels/history-panel/ui-history-panel.md) |
| [imgui_frame_scope.h](editor/ui/imgui-frame-scope.md) | <code>A</code> self-contained ImGui frame for code that runs without the engine&#x27;s <code><a href="../engine/engine/imgui-layer/imgui-layer.html">imgui_layer</a></code>: the UI tests construct every panel against the <code>null</code> RHI and draw one frame inside this scope. When an ImGui context already exists (the engine&#x27;s), the scope only brackets frames on it; otherwise it creates and later destroys one with the default font atlas built and docking enabled. Nothing is rendered. | [`ui::imgui_frame_scope`](editor/ui/imgui-frame-scope/ui-imgui-frame-scope.md) |
| [inspector_panel.h](editor/ui/panels/inspector-panel.md) | The inspector: one <code>component.get</code> document per component of the primary selection per frame, drawn through the property drawers; edits go back through <code>component.set</code> with a coalesce key per field so a drag is one undo entry. Add and remove component live in the panel&#x27;s menus. | [`ui::inspector_panel`](editor/ui/panels/inspector-panel/ui-inspector-panel.md) |
| [panel_desc.h](editor/panel-desc.md) | Module-provided panels: a trivially copyable extension record. The editor&#x27;s panel registry wraps the draw function into an <code><a href="editor/ui/editor-panel/ui-editor-panel.html">editor_panel</a></code> drawn inside the workspace with the one ImGui context the engine exports; the runtime ignores the record. | [`panel_desc`](editor/panel-desc/panel-desc.md) |
| [picker.h](editor/picker.md) | CPU picking: a slab test over every candidate&#x27;s world bounds sorted by entry distance, then, for mode <code>MESH</code>, the triangles of the mesh asset&#x27;s CPU geometry (sprites test their quad) until a hit is closer than the next candidate&#x27;s entry distance. Ties between bounds-only hits go to the smaller volume so a child inside its parent stays selectable. Mode <code>PHYSICS</code> delegates to Jolt. | [`pick_candidate`](editor/picker/pick-candidate.md)<br>[`pick_context`](editor/picker/pick-context.md)<br>[`pick_result`](editor/picker/pick-result.md) |
| [profiler_panel.h](editor/ui/panels/profiler-panel.md) | The profiler: frame time history from <code><a href="../engine/engine/frame-stats/frame-stats.html">engine::frame_stats</a></code> as a plot, the phase breakdown of the latest sample as a table, renderer and world counters. | [`ui::profiler_panel`](editor/ui/panels/profiler-panel/ui-profiler-panel.md) |
| [property_drawer.h](editor/ui/property-drawer.md) | Property drawers: one ImGui widget per reflection field kind. The inspector reads a component as JSON, hands each field&#x27;s value to the drawer for its kind and, when the widget reports a change, writes the new value back through <code>component.set</code>. Modules may replace the drawer of a kind for their own tooling. | [`ui::property_draw_context`](editor/ui/property-drawer/ui-property-draw-context.md)<br>[`ui::property_drawer_registry`](editor/ui/property-drawer/ui-property-drawer-registry.md) |
| [screenshot_service.h](editor/screenshot-service.md) | Screenshots: <code>view.screenshot</code> parks its request, the service waits the asked number of frames, captures an RGBA8 image through its backend on the main thread, then hands the image to a <code>core::jobs</code> job that encodes the file (<code>PNG</code>, or lossless JPEG XL through the renderer&#x27;s encoder) and writes it (a 16384 x 16384 capture must not stall the frame); the job posts the completion back to the main thread, which answers {path, width, height, frame, format}. Without a job system the encode runs inline. The renderer backend draws the scene through the requested camera (&quot;editor&quot;, &quot;game&quot; or a camera entity) into a render target of the requested size and captures it through <code>renderer::read_back</code> once a frame has been rendered into it; on the <code>null</code> RHI (every headless run) there is no backend and the request fails with <code>NOT_AVAILABLE</code>. Tests install a memory backend that paints a solid colour. | [`memory_screenshot_backend`](editor/screenshot-service/memory-screenshot-backend.md)<br>[`screenshot_backend`](editor/screenshot-service/screenshot-backend.md)<br>[`screenshot_request`](editor/screenshot-service/screenshot-request.md)<br>[`screenshot_service`](editor/screenshot-service/screenshot-service.md)<br>[`screenshot_service::pending`](editor/screenshot-service/screenshot-service-pending.md) |
| [selection_set.h](editor/selection-set.md) | The selection: UI state, never undoable. <code>A</code> sorted guid list plus a dense bit set by entity index so the hierarchy panel asks &quot;is this row selected&quot; in <code>O(1)</code>, and the primary (active) entity: the one named last, as the inspector and the hierarchy&#x27;s &quot;Add&quot; parent use it. | [`selection_set`](editor/selection-set/selection-set.md) |
| [toolbar_panel.h](editor/ui/panels/toolbar-panel.md) | The play bar: play / pause / resume / step / stop (play.*), the current play state and frame, undo and redo buttons (history.*), the scene path and dirty flag. | [`ui::toolbar_panel`](editor/ui/panels/toolbar-panel/ui-toolbar-panel.md) |
| [viewport_panel.h](editor/ui/panels/viewport-panel.md) | The viewport: the scene through the editor camera, drawn every frame into an off-screen target the size of the panel and shown as an image (without a window there is no ImGui renderer and the panel is the input surface alone), with the input surface over it: the gizmo for the selection, click picking through <code>view.pick</code>, right-drag orbit, middle-drag pan and wheel zoom of the editor camera, and the operation/space/snap toolbar. The picture, the pick ray and the gizmo all use the editor camera and the panel&#x27;s rectangle, so what is clicked is what is seen. | [`ui::viewport_panel`](editor/ui/panels/viewport-panel/ui-viewport-panel.md) |

## Breakdown

<!-- doc-content:breakdown:start -->
### Workspace

`editor_layer` connects the graphical editing interface to the application frame loop. `editor_workspace`
organises docking and panels, while property drawers, gizmos, and the hierarchy cache support scene editing.
Camera, selection, picking, and screenshot services provide the underlying editor operations.

### Panels

The panels under `ui/panels/` present assets, the hierarchy, properties, console output, history, profiling,
play controls, export, and the viewport. They use the editor context and command model to work with project data.
Panel extension records let registered extensions contribute interface functionality.

> [!IMPORTANT]
> Route undoable edits through the editor command model so validation, history, and notifications stay consistent.

The UI implementation is in `conjure::editor_ui`, with supporting services in `conjure::editor_core`.
See [Editor Architecture](editor.md) for workspace integration and extension records.
<!-- doc-content:breakdown:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:testing:start -->

<!-- doc-content:testing:end -->
