---
kind: scope
---

# Editor
<!-- doc-template:scope -->

<!-- doc-content:purpose:start -->
Implementation handbook for the editor libraries and application. Use this scope when changing editor
commands, the AI control protocol, panels, property drawers, or the docking workspace. Engine services are
covered by the [Engine scope](engine.md).

[Editor Architecture](editor/editor.md) explains the application and its supporting libraries.
<!-- doc-content:purpose:end -->

## Components

| Component | Description |
|---|---|
| [Applications](editor/applications.md) | Editor startup, application state, project trust, play mode, and headless execution. |
| [Commands and Undo](editor/commands-and-undo.md) | Command registration, validation, dispatch, entity and property references, and undo history. |
| [Protocol](editor/protocol.md) | Editor requests, responses, events, transports, access tokens, and audit records. |
| [User Interface](editor/user-interface.md) | The docking workspace, panels, selection, camera, picking, screenshots, and property editing. |

<!-- doc-legacy-anchors:start -->
<a id="modules"></a>
<!-- doc-legacy-anchors:end -->
