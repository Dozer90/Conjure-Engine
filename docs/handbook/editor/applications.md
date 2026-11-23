---
kind: component
---

[Editor](../editor.md)

# Applications
<!-- doc-template:component -->

|---|---|
| Type | Component |
| Implementation | `conjure_editor`, `conjure::editor_core` |
| Namespace | `conjure::editor` |
| Include Folder Path | `engine/include/conjure/editor` |
| Source Folder Path | `engine/src/editor` |

<!-- doc-content:purpose:start -->
Editor startup, project state, trust, play mode, and headless execution.
<!-- doc-content:purpose:end -->



## Headers

| File | Description | Provides |
|---|---|---|
| [editor_application.h](editor/editor-application.md) | The editor process object: owns the <code><a href="../engine/engine/application/application.html">engine::application</a></code>, the <code><a href="editor/editor-context/editor-context.html">editor_context</a></code> and the <code><a href="editor/protocol/protocol-server/protocol-server.html">protocol_server</a></code>, drives the frame integration headless (through a core layer) or with the UI layer the executable injects, restarts the engine when <code>project.open</code> names another project and runs --export / --exec / the interactive loop. The command file of --exec is read and parsed before the engine starts, so a missing or malformed file fails at once (decision). | [`editor_application`](editor/editor-application/editor-application.md)<br>[`startup_plan`](editor/editor-application/startup-plan.md)<br>[`startup_project`](editor/editor-application/startup-project.md) |
| [editor_context.h](editor/editor-context.md) | The editor&#x27;s service table: owns the command registry, dispatcher, undo stack, selection, play controller, events, camera, console and screenshot service, and holds non-owning references into the running <code><a href="../engine/engine/application/application.html">engine::application</a></code> (taken from <code><a href="../engine/engine/engine-context/engine-context.html">engine_context</a></code>, rebound when the editor opens another project). Handlers reach everything through it. Main thread only. | [`editor_context`](editor/editor-context/editor-context.md)<br>[`editor_context::debug_draw_flags`](editor/editor-context/editor-context-debug-draw-flags.md)<br>[`editor_host`](editor/editor-context/editor-host.md)<br>[`frame_task`](editor/editor-context/frame-task.md) |
| [editor_error.h](editor/editor-error.md) | The editor error category. The protocol&#x27;s stable error strings and the editor&#x27;s own <code><a href="../engine/core/error/error.html">core::error</a></code> codes are one enumeration: a handler returns <code>make_editor_error(code, message)</code> and the dispatcher writes <code>to_string(code)</code> into the response. Errors of other modules (ecs, scene, assets, core) that reach a response are mapped through <code>classify_error()</code>. |  |
| [editor_module_consumer.h](editor/editor-module-consumer.md) | The editor&#x27;s module consumer: turns <code>editor.command</code> records into <code>command_descriptors</code> owned by the module and keeps the module&#x27;s <code>editor.panel</code> records for the UI&#x27;s panel registry; detach removes both. It vetoes an unload with <code>UNLOAD_BLOCKED</code> while the editor plays, because scripts and systems of the module may be live. | [`editor_module_consumer`](editor/editor-module-consumer/editor-module-consumer.md)<br>[`module_panel_entry`](editor/editor-module-consumer/module-panel-entry.md) |
| [editor_options.h](editor/editor-options.md) | The <code>conjure_editor</code> command line: the editor&#x27;s own flags are parsed here, everything else (--rhi, --log-level, --frames, ...) is passed on to the engine&#x27;s parser through <code>engineArguments</code>. <code>A</code> bad editor flag is <code>INVALID_ARGS</code>; an option the engine does not know either, or a bad engine flag value, is refused by <code>editor_application::make_engine_desc</code> with <code>BAD_ARGUMENTS</code>. <code>main()</code> exits with 3 for both. | [`editor_options`](editor/editor-options/editor-options.md) |
| [headless_runner.h](editor/headless-runner.md) | <code>conjure_editor --headless --exec commands.json</code>: a JSON array of requests or NDJSON, one request per line; a missing id becomes the zero-based index. The file is read and parsed before the engine starts (<code>editor_application::initialise</code> calls <code>load_command_file</code>), so a missing or malformed file costs no engine boot. Every request is dispatched, frames are pumped until an asynchronous response exists, responses and events are written as NDJSON to the output; the run stops at the first failure unless --keep-going. When the engine could not load the project&#x27;s startup scene, the output opens with <code>warning {code: &quot;STARTUP_SCENE_FAILED&quot;, message}</code> and the run carries on over the empty world: the exit code does not change, so a command file that repairs the scene still runs. <code>editor.quit</code> ends the run after its reply, and its non-zero <code>exitCode</code> becomes the run&#x27;s exit code. Exit codes: 0 all ok, 1 a command failed, 2 startup or project failure (also a <code>project.open</code> in the file whose restart failed for both projects) or responses that could not be written, 3 bad arguments (a missing or unparseable file). | [`exec_summary`](editor/headless-runner/exec-summary.md)<br>[`headless_runner`](editor/headless-runner/headless-runner.md) |
| [log_category.h](editor/log-category.md) | The editor&#x27;s log category: every message of <code>conjure_editor_core</code>, <code>conjure_editor_ui</code> and the <code>conjure_editor</code> executable goes through <code>LOG_EDITOR</code> (&quot;editor&quot;), so the console panel and the protocol&#x27;s <code>log</code> event can filter it like any other engine category. |  |
| [play_controller.h](editor/play-controller.md) | Play mode in the editing world. <code>EDITING</code> keeps the engine <code>PAUSED</code> and the script runtime in <code>EDIT</code> mode; <code>play.start</code> captures a scene snapshot, freezes the undo stack and runs the engine; <code>play.stop</code> restores the snapshot (guids preserved), remaps the selection and thaws the undo stack. <code>A</code> scene edit made while playing is not recorded and goes with the restore; a file edit (an <code>ON_DISK</code> command) stays on disk, so the controller keeps its inverse and <code>play.stop</code> pushes all of them as one undo entry. <code>play.step</code> runs exactly n fixed steps through <code>application::step_fixed</code> and answers once they ran. The clocks play.* report belong to the play session: <code>play.start</code> records the engine&#x27;s fixedTime and fixedStepIndex, the session&#x27;s time and fixed steps count from there (they stop while paused), and the ended session&#x27;s values stay until the next <code>play.start</code>. | [`play_controller`](editor/play-controller/play-controller.md)<br>[`play_controller::pending_step`](editor/play-controller/play-controller-pending-step.md) |
| [project_trust.h](editor/project-trust.md) | Project trust. Opening a project runs code from its folder with the rights of the user who runs the editor: the engine loads its native modules and its game assembly while it starts, and <code>script.compile</code>, <code>module.build</code> and <code>play.start</code> run more of it afterwards. The editor therefore opens a project only once somebody answered for it, and <code><a href="editor/project-trust/project-trust.html">project_trust</a></code> is where the answers are kept: | [`project_trust`](editor/project-trust/project-trust.md)<br>[`trusted_project`](editor/project-trust/trusted-project.md) |

## Breakdown

<!-- doc-content:breakdown:start -->
The entry point in `editor/src/main.cpp` constructs `editor_application` over `engine::application`.
`editor_options` selects the application mode and command-line options. `headless_runner` executes requests
without the workspace UI, while using the same editor services and command model.

`editor_context` holds the services commands operate on. `project_trust` controls trusted project state;
`play_controller` owns transitions between editing and play mode. `editor_module_consumer` connects extension
records to the editor. The executable and these services are parts of the editor application.

See [Editor Architecture](editor.md) for startup, project transitions, frame integration, and headless operation.
<!-- doc-content:breakdown:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:testing:start -->

<!-- doc-content:testing:end -->
