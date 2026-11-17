# The editor

📘 **Public API wiki** · 🛠 Handbook: [Editor module](../../handbook/modules/editor.md)

`conjure_editor` is the tool you build games in, and the tool an AI agent drives. Every edit is a command: the
Dear ImGui panels, the newline-delimited JSON protocol over TCP or stdio, and the headless `--exec` command file
all send the same `entity.create`, `component.set`, `prefab.instantiate`, `play.start` requests to one
dispatcher, which validates them, records their inverses for undo and emits events. The editor runs on top of
the engine's `application` (📘 API: [Engine](../engine/index.md)); headless it runs on the null window, device
and audio backend with the script runtime on the null host, so every command works without a GPU or .NET.
Native types live in `namespace conjure::editor` (the UI in `conjure::editor::ui`).

## The mental model in five sentences

1. The editor is `editor_application`: it owns an `engine::application`, an `editor_context` (registry,
   dispatcher, undo stack, selection, play controller, events, camera, console, screenshots) and a
   `protocol_server`; `run()` either exports, executes a command file or loops frames.
2. A request is `{"id", "cmd", "args"}`; the dispatcher looks the command up, validates `args` against its
   JSON schema, checks the preconditions (`REQUIRES_PROJECT`, `REQUIRES_SCENE`, `EDITING_ONLY`), runs the
   handler and answers `{"id", "ok", "result"}` or `{"id", "ok": false, "error": {"code", "message"}}`.
3. A mutating command returns its inverse; the undo stack stores it, `history.undo` dispatches it. Batches and
   transactions group several commands into one undo entry; play mode freezes the stack and `play.stop`
   restores the scene snapshot byte for byte.
4. Entities cross the protocol as scene guids or hierarchy paths (`"Board/Cell_3"`), never as ECS ids; component
   fields as `<component>.<field>.<subfield>` paths over the reflection description.
5. Everything is discoverable at run time: `editor.schema` dumps the catalogue, `reflection.components` the
   component types, `reflection.asset_types` the importers; the committed dump is
   `docs/generated/editor_protocol.json` and the agent skill `.claude/skills/editor-ai-control/SKILL.md`.

## Where to go next

| Page | Read it when you want to... |
|---|---|
| [The AI control protocol](ai-protocol.md) | drive the editor from a script or an agent: start it, connect, the wire format, the catalogue, events, errors, the headless `--exec` runner |
| [Access tokens and the audit log](access-and-audit.md) | let a program connect, give it full control or inspect-only access, read what clients did; the C++ token helpers and `audit_log` |
| [Headless mode and the command model](headless-and-commands.md) | embed or extend the editor from C++: `editor_application`, `editor_context`, the dispatcher and registry, undo, play, picking, screenshots, module commands and panels; which projects the editor opens (project trust) |
| [The workspace UI](workspace-ui.md) | the Dear ImGui workspace: panels, the inspector's property drawers, the gizmo, writing a panel for a module |

## Headers

| Header | What it holds |
|---|---|
| `conjure/editor/editor_application.h`, `conjure/editor/editor_options.h`, `conjure/editor/headless_runner.h` | the process object, the command line, `--exec` |
| `conjure/editor/editor_context.h` | the editor's service table |
| `conjure/editor/project_trust.h` | which projects the editor may open: the answers and the trust store |
| `conjure/editor/command_dispatcher.h`, `conjure/editor/command_registry.h`, `conjure/editor/editor_command.h`, `conjure/editor/builtin_commands.h`, `conjure/editor/schema_validator.h` | the command model |
| `conjure/editor/undo_stack.h`, `conjure/editor/selection_set.h`, `conjure/editor/play_controller.h`, `conjure/editor/picker.h`, `conjure/editor/screenshot_service.h`, `conjure/editor/editor_camera.h`, `conjure/editor/console_buffer.h` | the editor services |
| `conjure/editor/entity_ref.h`, `conjure/editor/reflection_path.h`, `conjure/editor/event_broadcaster.h`, `conjure/editor/editor_error.h` | addressing, events, error codes |
| `conjure/editor/command_desc.h`, `conjure/editor/panel_desc.h`, `conjure/editor/editor_module_consumer.h` | what a module adds to the editor |
| `conjure/editor/protocol/protocol_message.h`, `protocol_server.h`, `protocol_transport.h`, `loopback_transport.h`, `line_splitter.h`, `access_token.h`, `audit_log.h` | the wire, its two tokens and the audit log |
| `conjure/editor/ui/editor_layer.h`, `editor_workspace.h`, `editor_panel.h`, `property_drawer.h`, `gizmo_controller.h`, `hierarchy_cache.h`, `imgui_frame_scope.h`, `ui/panels/*.h` | the workspace |
| `conjure/editor/log_category.h` | `LOG_EDITOR` |

## Quick tour

```
conjure_editor --project C:/games/grid_sweeper                     # the window, AI server on 127.0.0.1
conjure_editor --project C:/games/grid_sweeper --trust-project --headless --exec build.json --out replies.ndjson
conjure_editor --project C:/games/grid_sweeper --trust-project --headless --export C:/builds/grid_sweeper \
  --distribution C:/conjure/bin                                     # also copy the runtime, see below
```

Opening a project runs its scripts and native modules, so the editor opens one only after you answered for it.
The window asks the first time ("Do you trust the authors of this project?") and remembers the answer for you on
this machine, outside the project. A headless run cannot ask: `--trust-project` answers for that run, and
without an answer it exits with 2. A project the editor created itself is trusted from the start
([Project trust](headless-and-commands.md#project-trust)).

`--distribution` names the folder `conjure_runtime`, `conjure_shared`, `shaders/` and `THIRD_PARTY_NOTICES.txt`
are copied from, with `nethost.dll` and the Visual C++ runtime DLLs when it holds them. A folder without the
notices exports with a warning that the game carries no licence notices, one on Windows without the runtime DLLs
with a warning that the game needs the Microsoft Visual C++ Redistributable, and debug runtime DLLs in it are
left behind with a warning.
Without `--distribution` the export holds the cooked pack and the project files only, and its reply carries a
warning that the runtime executable, library and shaders were not copied. A client that sends `project.export`
with `copyRuntime` set made that choice itself and gets no such warning.

```
-> {"id":1,"cmd":"session.hello","args":{"clientName":"my-agent"}}
<- {"id":1,"ok":true,"result":{"protocolVersion":1,"engineVersion":"0.1.0","schemaHash":"...","clientId":2}}
-> {"id":2,"cmd":"entity.create","args":{"name":"Player","components":{"transform":{"position":[1,0,1]}}}}
<- {"id":2,"ok":true,"result":{"guid":"a9e2c0d4-...","name":"Player","revision":42}}
<- {"event":"scene.changed","seq":913,"frame":8812,"data":{"revision":42,"label":"Create 'Player'"}}
```

## Local terms

| Term | Meaning here |
|---|---|
| command | a named request with a JSON schema for its arguments and its result, flags and a handler; 108 built in |
| inverse | the command that undoes a mutating command; stored by the undo stack |
| revision | a counter bumped by every mutation; every mutating result carries it |
| transport | where request lines come from: TCP (`--ai-port`), stdio (`--ai-stdio`), the in-process loopback (tests), or the local UI |
| headless | `--headless`: null platform, device and audio, the null script host (`--dotnet` keeps the real .NET host, for play tests that run C#); `view.screenshot` answers `NOT_AVAILABLE` |
| play state | `editing`, `playing` or `paused`; mutating commands are refused while playing unless flagged otherwise |

## See also

- 📘 API: [Engine: the application and the frame](../engine/index.md), [Scenes, hierarchy and prefabs](../scene/index.md), [Reflection](../core/reflection.md)
- 📘 API: [Modules and plugins](../modules/index.md) (modules add commands and panels)
- 🛠 Handbook: [Editor module](../../handbook/modules/editor.md)
