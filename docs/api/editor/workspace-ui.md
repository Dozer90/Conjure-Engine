# The workspace UI

📘 **Public API wiki** · 🛠 Handbook: [Editor module](../../handbook/modules/editor.md)

The Dear ImGui side of the editor (`conjure::editor::ui`, library `conjure_editor_ui`): the layer that draws
(`conjure/editor/ui/editor_layer.h`), the docking workspace (`conjure/editor/ui/editor_workspace.h`), panels
(`conjure/editor/ui/editor_panel.h` and `conjure/editor/ui/panels/*.h`), the inspector's property drawers
(`conjure/editor/ui/property_drawer.h`), the transform gizmo (`conjure/editor/ui/gizmo_controller.h`), the
hierarchy's row cache (`conjure/editor/ui/hierarchy_cache.h`) and the frame scope the UI tests draw in
(`conjure/editor/ui/imgui_frame_scope.h`). The UI is a client of the command model: panels own only UI state and
submit `command_request`s through the context's dispatcher: every edit is validated, undoable and announced like a
protocol request. Selection, camera navigation, the gizmo operation and space, and the debug-draw toggles are UI
state the panels write directly; they are not commands.

## At a glance

| You want to... | Use |
|---|---|
| Put the editor UI on an application | `create_editor_layer(context)` as the `layer_factory` of `editor_application::initialise` |
| Add a panel from a module | a `panel_desc { title, draw, userData, openByDefault }` extension record; the workspace lists it in the View menu |
| Add a panel from C++ (the editor itself) | derive from `editor_panel`, register a `panel_descriptor` in the `panel_registry` |
| Draw a field kind differently | `property_drawer_registry::set(field_kind, drawer)` |
| Draw panels in a test without the engine's ImGui layer | `imgui_frame_scope` around `begin_frame()` ... `end_frame()` on the null device |

## The layer and the workspace

`editor_layer` is pushed after the engine's `imgui_layer` (which owns the one `ImGuiContext`): `on_update`
refreshes the panels' caches, `on_imgui` draws the workspace inside the frame the engine opened, `on_event` lets
ImGui consume input first. `on_attach` also gives the window the editor's icon through
`conjure/editor/ui/editor_icon.h`: `editor_icon_images()` decodes the official logo, compiled in as one PNG per
size from 16 to 256 px, and `apply_editor_icon(window)` hands it to `window::set_icon` (`FEATURE_UNAVAILABLE` on
macOS and Wayland, which is logged once at INFO and never fails the attach). `editor_workspace` is the
full-window dockspace with a pass-through central node (the
scene renders underneath), the main menu bar, the panel instances and the layout file
`<project>/.conjure/editor_layout.ini`: `create_default_panels(context)`, `sync_module_panels(context)` (adds
and removes module panels as modules load and unload), `update`, `draw`, `set_layout_file`, `save_layout`,
`open_panel_count()`, `menu_requests()`.

Ctrl+Z, Ctrl+Y, Ctrl+S, F5 and Shift+F5 are handled by `editor_workspace::draw` whether or not the Toolbar is open.
File > Save Scene and Ctrl+S call `request_save_scene(context)`: `scene.save` when the scene has a path, otherwise
the Save Scene As dialog, because a scene made with New Scene has none yet. File > Save Scene As... opens that dialog
directly (`request_save_scene_as`); its path is relative to `assets://` and `.scene.json` is added, and its Save
button calls `save_scene_as(context, path)`, which sends `scene.save_as` and keeps the error text on failure
(`save_as_pending()`, `save_as_path()`, `save_as_error()`).

## Panels

`editor_panel` is a titled ImGui window with an open flag; `draw(context)` brackets the window and calls the
subclass. The built-in panels:

| Panel (header) | Shows and does |
|---|---|
| `hierarchy_panel` (`conjure/editor/ui/panels/hierarchy_panel.h`) | the scene tree as flat rows from `hierarchy_cache` with a list clipper (10,000 entities cost the visible rows); click, Ctrl-click and Shift-click (range) selection; inline rename with F2 or the menu (`begin_rename`, `is_renaming`), committed by Enter or clicking away and cancelled by Escape; reparent by drag, or drop below the rows to move to the root; filter. A selection made elsewhere expands its ancestors and scrolls to its row |
| `inspector_panel` (`conjure/editor/ui/panels/inspector_panel.h`) | one `component.get` document per component of the primary selection; edits go back through `component.set` with the drag session as coalesce key |
| `viewport_panel` (`conjure/editor/ui/panels/viewport_panel.h`) | the scene through the editor camera, drawn every frame into an off-screen target the size of the panel and shown as an image, with an input surface over it for camera orbit, pan, zoom, picking (`view.pick`) and the gizmo, under a toolbar for the gizmo operation, space and snapping. The picture, the pick ray and the gizmo use the same camera and rectangle, so what is clicked is what is seen. The 2D view mode keeps authored colours (no post-processing). `pictures_drawn()` counts the frames with a picture; headless there is no ImGui renderer and the panel is the input surface alone |
| `asset_browser_panel` (`conjure/editor/ui/panels/asset_browser_panel.h`) | the database's records as a filterable table (path, type, status), import by path, reimport, delete |
| `console_panel` (`conjure/editor/ui/panels/console_panel.h`) | the `console_buffer` ring with level and category filters, auto-scroll and clear |
| `history_panel` (`conjure/editor/ui/panels/history_panel.h`) | the undo entries with label and client, the cursor, undo / redo / clear |
| `toolbar_panel` (`conjure/editor/ui/panels/toolbar_panel.h`) | play / pause / resume / step / stop (`play.*`), the play state and frame, undo and redo (the keyboard shortcuts belong to the workspace) |
| `profiler_panel` (`conjure/editor/ui/panels/profiler_panel.h`) | frame-time history from `engine::frame_stats` as a plot, the phase breakdown of the latest frame |
| `ai_session_panel` (`conjure/editor/ui/panels/ai_session_panel.h`) | the server's port and clients, a request line sent through an in-process loopback client, the responses and events |
| `export_dialog` (`conjure/editor/ui/panels/export_dialog.h`) | output directory and configuration, the `project.export` command and its `progress` events |

```cpp
#include "conjure/editor/ui/editor_panel.h"

class counter_panel final : public conjure::editor::ui::editor_panel {
    public:
        counter_panel() : editor_panel("Counter") {}

    protected:
        void draw_contents(conjure::editor::editor_context &context) override {
            ImGui::Text("%u entities", context.world().entity_count());
            if (ImGui::Button("Create one")) {
                conjure::editor::command_request request = {};
                request.name = "entity.create";
                request.args = conjure::core::json::object({{"name", "FromPanel"}});
                CONJURE_UNUSED(context.dispatcher().execute(request));   // undoable like any other edit
            }
        }
};
```

> [!TIP]
> A module's panel uses the `panel_desc` record and the `IMGUI_API` interface target (`EDITOR_UI` in
> `conjure_add_game_module`), so it draws into the same context as the editor without owning one.

## Property drawers, gizmo, hierarchy cache

`property_drawer_registry` maps every `core::reflect::field_kind` to a widget: the inspector reads a component
as JSON, hands each field to the drawer for its kind and writes the new value back through `component.set` when
the widget reports a change; `install_defaults()` sets the built-in widgets (number fields, colour pickers, enum
combos, entity and asset pickers, nested structs and arrays) and `set(kind, drawer)` replaces one. Numbers clamp to
whichever of the field's `min` and `max` attributes are set: f32 and integer fields are a slider when both are set
and fit ImGui's slider range, and a clamped drag otherwise; f64 fields are an input box clamped when the edit is
committed. Integers are edited in their own type's range, and a rotation keeps the Euler angles it shows until the
quaternion changes elsewhere, so an angle can be dragged past 90 degrees.

`gizmo_controller` drives ImGuizmo: the first manipulated frame opens an undo transaction and caches the selected
entities' world matrices, every frame with a delta dispatches `component.set_many` on the local transforms
(through the parents' inverse world matrices, pivoting at the centroid), the release ends the transaction. A
selected entity whose ancestor is also selected follows that ancestor instead of receiving the delta itself.
`set_operation` (translate, rotate, scale), `set_space` (world, local), `set_snap(enabled, translate,
rotateDegrees, scale)` (defaults 0.5, 15 and 0.1, off; the viewport toolbar's Snap toggle and step field drive it,
`snap_enabled()` and the step getters read it back), `is_over()` (the mouse is over a handle of the gizmo the last
`draw` showed; false after a frame with nothing selected, so picking works again).

`hierarchy_cache` is the hierarchy panel's flat row list: one pre-order walk per structural change
(`invalidate()`, `rebuild`), expansion state (`set_expanded(scene, entity, expanded)`, `is_expanded(scene,
entity)`, `reveal`; keyed by the entity's guid, so it survives play/stop and never passes to an entity that reuses
an index), a name filter (`set_filter`), `refresh_labels(scene, first, last)` (names and enabled state of rows
already built, O(range), no rebuild; the panel calls it for the visible rows) and
`hierarchy_row { entity, nameId, depth, flags }` rows for the clipper.

## Testing the UI

`imgui_frame_scope` gives code without the engine's `imgui_layer` a frame to draw in: when a context exists it
only brackets `begin_frame()` / `end_frame()` on it, otherwise it creates one with a built font atlas and docking
enabled and destroys it later; `set_mouse(position, leftDown)` feeds input. Nothing is rendered, so every panel
can be drawn against the null device in a unit test.

> [!WARNING]
> ImGui has one context per process and the engine's `imgui_layer` owns it. Never call `ImGui::CreateContext`
> or `SetCurrentContext` from a panel or a module; draw inside the frame the engine opened.

## API summary

| Signature (short) | Purpose |
|---|---|
| `unique_ptr<engine::layer> create_editor_layer(editor_context &)` | the UI layer for `editor_application::initialise` |
| `result<std::vector<core::image>> editor_icon_images()` / `result<void> apply_editor_icon(platform::window &)` | the editor's window icon |
| `class editor_panel { explicit editor_panel(std::string title, bool open = true); title(); is_open(); set_open(bool); draw(editor_context &) }` | a panel |
| `void panel_registry::add(panel_descriptor)` / `add_module_panel(string_view owner, const panel_desc &)` / `descriptors()` | the panel list |
| `void editor_workspace::create_default_panels(editor_context &)` / `sync_module_panels` / `update` / `draw` / `save_layout` | the dockspace |
| `void editor_workspace::request_save_scene(editor_context &)` / `request_save_scene_as` / `bool save_scene_as(editor_context &, string_view)` / `save_as_pending()` | Save Scene and Save Scene As |
| `void property_drawer_registry::set(field_kind, draw_fn)` / `install_defaults()` | widgets per field kind |
| `void gizmo_controller::apply_delta(editor_context &, const math::mat4 &)` / `end_drag` / `set_operation` / `set_space` / `set_snap` / `snap_enabled()` / `is_over()` | the gizmo |
| `void hierarchy_cache::rebuild(const scene::scene &, const selection_set &)` / `set_filter` / `set_expanded(const scene::scene &, entity, bool)` / `reveal` / `refresh_labels` / `rows()` | the hierarchy rows |
| `void hierarchy_panel::begin_rename(editor_context &, ecs::entity)` / `is_renaming()` | inline rename |
| `imgui_frame_scope::begin_frame()` / `end_frame()` / `set_mouse(const math::vec2 &, bool)` | a frame for tests |

## See also

- 📘 API: [Headless mode and the command model](headless-and-commands.md), [The AI control protocol](ai-protocol.md)
- 📘 API: [Rendering a frame](../renderer/rendering-a-frame.md) (the ImGui backend), [Writing a module](../modules/writing-a-module.md)
- 🛠 Handbook: [Editor module](../../handbook/modules/editor.md)
- 🛠 Handbook: [Logo and application icon](../../handbook/brand.md)
