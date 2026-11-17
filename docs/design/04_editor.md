# Design D - Editor and AI Control Protocol (final)

Status: **binding**. Judge's synthesis of two panel proposals (performance-first, robustness-first).
Backbone: the robustness-first threading/ownership/error model, grafted with the performance-first
data-driven command model, bulk commands and cache-driven panels. Every open question is resolved
here; implementers do not make architectural choices.

Scope: editor libraries and executable, command model and undo, AI control protocol (TCP localhost,
stdin/stdout, `--headless --exec`), command catalogue, picking, play/pause/step with snapshot restore,
play-mode persist, workspace layout, themes, screenshots, Distribution stripping, public header list.

Revised 2026-11-13 at the owner's request: play-mode **persist** (D16, 3.13), the default workspace layout
(D17, 3.14) and named, user-customisable **themes** (D18, 3.15). The visual target for all three is the
owner-approved mockup canvas "Conjure editor, finished" (Midnight default plus Autumn, Deep Ocean, Aurora,
Synthwave and High contrast); where this document and the mockup disagree, this document wins.

---

## 1. Overview and decisions

### 1.1 Shape

```
engine/include/conjure/editor/        public headers of conjure_editor_core   (no ImGui, no sockets)
engine/include/conjure/editor/ui/     public headers of conjure_editor_ui     (ImGui stays in src/)
engine/src/editor/core/               commands, undo, protocol, dispatcher, play, picking, screenshot,
                                      headless runner, editor_application
engine/src/editor/ui/                 docking workspace, panels, gizmos, property drawers
editor/main.cpp + CMakeLists.txt      the conjure_editor executable (fixed name)
tests/editor/                         conjure_editor_tests (doctest, null backends)
```

`engine/CMakeLists.txt` adds `src/editor` only when `CONJURE_BUILD_EDITOR` is ON (the root CMake already
forces it OFF for Distribution). Targets: `conjure_editor_core`, `conjure_editor_ui` (both via
`conjure_add_module()`), `conjure_editor` executable. `conjure_runtime` links neither.

### 1.2 Binding decisions

| # | Decision | Rationale |
|---|---|---|
| D1 | Two static libraries: `conjure_editor_core` (everything the AI protocol and headless mode need) and `conjure_editor_ui` (ImGui/ImGuizmo). `editor/main.cpp` is the only executable code. | The entire AI surface is unit-testable with `null` backends without compiling ImGui. UI is a consumer of the same commands, never a second code path. |
| D2 | **Commands are data**: `command_request { string name; json args }`. A `command_descriptor` (name, summary, JSON-Schema subset for args/result, flags, handler) lives in a `command_registry`. UI panels, protocol clients, undo/redo and `--exec` files all submit `command_request`s to one `command_dispatcher`. | One representation; the catalogue is self-describing (`editor.schema`); parity between UI and protocol is structural, not a discipline. Each command is one free function plus one schema string, ideal for parallel implementers. |
| D3 | **Undo is an inverse command.** A mutating handler returns `command_outcome { result, inverse, replay }`; `replay` is the normalised forward request (e.g. `entity.create` with the assigned guid) so redo is deterministic. The `undo_stack` stores inverse/replay pairs and re-dispatches them. | No second mutation path to test. Transactions and batches are vectors of inverses; rollback on failure is "dispatch the inverses recorded so far". |
| D4 | **All-or-nothing handlers.** A handler validates and resolves every argument *before* its first mutation (two-phase inside one function; schema validation already happened in the dispatcher). Compound operations (`editor.batch`, transactions) roll back via inverses. | A failed protocol request never leaves a half-mutated scene. |
| D5 | Entities are addressed by **scene guid** (`core::uuid`, stable per entity, from the `scene` module) or by hierarchy path. `ecs::entity` ids never cross the protocol. | Entity ids recycle; guids survive play/stop restore, scene reload and undo. |
| D6 | **Main-thread execution at one frame point.** TCP sockets are non-blocking and polled from the main thread. The only extra thread is a blocking stdin reader (`core::jobs::dedicated_thread`). Long operations are **async commands** that park the request id and answer when a job completes (main-thread callback). | No shared editor state across threads; no lock-free queue dependency on `core`; deterministic ordering between UI actions and agent requests. |
| D7 | Protocol is **newline-delimited JSON** (`\n`, UTF-8, max 16 MiB/line). Requests carry a caller `id` (int or string, echoed; `null` when absent - a response is always sent). Events carry a monotonic `seq` and the `frame`. Error codes are stable strings. | Trivial to speak from any agent runtime; resumable logs; typed failures the agent can branch on. |
| D8 | **Bulk commands exist from day one**: `entity.create_many`, `component.set_many`, `editor.batch` (one undo entry, atomic by default). | A grid game is ~400 cell entities; one round trip, one undo entry, one archetype insertion pass. |
| D9 | **Play mode runs in the editing world** with an opaque `scene::scene_snapshot` captured on `play.start` and restored on `play.stop`. While not `EDITING` the undo stack is frozen: a scene edit executes, is not recorded and goes with the restore (one `warning` event per play session says so); `history.undo` and `history.redo` answer `WRONG_PLAY_STATE`. An edit to the project's files is not part of the snapshot and is kept for `play.stop` instead (D21). Selection is remapped by guid after restore. | Simplest consistent rule; snapshot format is the scene module's business (binary chunk copy recommended, see 8). |
| D10 | **Picking is CPU-side**: AABB broad phase over per-entity world bounds (always), triangle narrow phase when the mesh asset keeps CPU geometry (editor builds keep it), `mode:"physics"` delegates to Jolt. No GPU id buffer. | Works with the `null` RHI, unit-testable, no readback latency. |
| D11 | **Hierarchy and inspector are cache-driven**: flat `hierarchy_row` array rebuilt only when `scene::structure_version()` changes, drawn with `ImGuiListClipper`; inspector reads one JSON document per selected component per frame via `reflection_path`. | 10,000 entities cost O(visible rows) per frame. |
| D12 | **Selection is UI state** (not undoable); every change emits `selection.changed`. Editor-only state lives in sidecars: `<scene>.editor.json`, `<project>/.conjure/editor_layout.ini`, `<project>/.conjure/ai_port`, `<project>/.conjure/ai_token` and `ai_token_inspect` (D23, D25), `<project>/.conjure/ai_audit.log` (D26). Per-user preferences (active theme, user themes, recent projects) live in the user configuration folder (3.15), never in a project; the answers of project trust live in the per-user state folder of the machine (D27), outside every project for a reason of their own. | Scene/prefab/project files never learn the editor exists. |
| D13 | **Distribution stripping is linkage**: no `CONJURE_EDITOR*` macro in `engine/` outside the editor libraries themselves (whose private `CONJURE_EDITOR_WITH_BINDINGS` is allowed); a review rule, the CI grep is not written yet. Module hooks are data: trivially copyable `editor::panel_desc` / `editor::command_desc` extension records (headers `conjure/editor/panel_desc.h`, `command_desc.h`, installable with the SDK without editor internals) that an `editor_module_consumer` (`modules::module_consumer`, exists only in the editor process) applies; the runtime simply has no consumer for those extension points (R14). CI checks the Distribution runtime binary for `conjure::editor` / `ImGui` symbols. | Impossible to ship editor code by accident; zero preprocessor coupling. |
| D14 | Headless: `--headless` selects `null` platform/rhi/audio; `--offscreen` additionally creates a real RHI device without a swapchain so `view.screenshot` works without a window. | Agents get pixels in CI-like runs; unit tests stay GPU-free. |
| D16 | **Play-mode persist.** While playing or paused, a user or protocol client may pin a field, a whole component or a whole entity. On `play.stop` the snapshot is restored as before, then the pinned values (read at the moment of Stop) are re-applied as **one undoable edit**. The persist list is session state: cleared on every `play.start` and after every Stop, never saved, never carried into the next play. | Designers tune live and keep exactly what they chose; everything else still resets. One undo entry makes a bad persist one Ctrl+Z away. Clearing at Start as well as Stop means a crash mid-play cannot leave stale pins. |
| D17 | **A default docked layout is built on first run** (no saved layout for the project) with ImGui's DockBuilder, and every later arrangement is saved to `<project>/.conjure/editor_layout.ini` (D12). Window > Reset layout rebuilds the default. | An editor whose panels open as loose floating windows is unusable (code sweep of 2026-11-12, editor UI blockers). |
| D18 | **Themes are data.** A theme is a JSON document of semantic colour roles, chrome gradients, fonts and metrics. Built-in themes ship inside the editor; users duplicate, edit and save their own in a per-user folder; the active theme is a per-user setting, not a project setting. | Taste is personal and must not churn project files or version control. Data, not code, lets users and agents create themes without rebuilding the editor. |
| D15 | Discoverability: `session.hello` returns protocol/engine versions and `schemaHash`; `editor.schema` returns the whole catalogue with schemas; `reflection.components` returns every component type with fields, defaults and attributes. A test dumps `editor.schema` to `docs/generated/editor_protocol.json`. | The agent never depends on documentation that can drift. |
| D19 | **`project.export` has no `config` argument and `project.create` no `template`** (code sweep F650). Both were read and discarded; a client that still sends either gets `INVALID_ARGS`, because unknown top-level arguments are refused. An export is always a Distribution export (06_engine_loop.md 3.6), and the export dialog has no Configuration field. A validated `config` enum that selects a real export configuration is wanted later and comes back with the distribution-layout work. | A silently ignored input, with a dialog default of "development", told users they had chosen something. Refusing it is the only honest answer until it selects something. |
| D20 | **Deleting the open scene asks first, deleting the startup scene warns, and "not found" has one meaning per code** (code sweep F656). `asset.delete` refuses the file behind the open scene with `INVALID_ARGS` unless the client passes `closeOpenScene: true`; it then deletes the file and runs `scene.close {discard: true}`, in that order, so a delete the file system refuses leaves the scene open with its unsaved changes and its history (should the close fail after the delete, the file is put back). "The open scene" and "the startup scene" are decided by file, not by spelling (`editor_context::is_open_scene` / `names_asset`): on a volume that ignores case a scene opened as `Scenes/Main` is still the file registered as `scenes/main`. In the UI the asset browser raises a confirmation dialog whose "Close and Delete" sends exactly that request (while playing or paused, when the command would refuse it, the button is disabled and the dialog says to stop playing). Because closing clears the undo history, `closeOpenScene` is refused inside a batch, an open transaction and an undo or redo step (`INVALID_ARGS`) and while playing (`WRONG_PLAY_STATE`); the delete becomes the history's first entry, and undoing it restores the file without reopening the scene. Deleting the project's startup scene succeeds and emits `warning {code: "STARTUP_SCENE_DELETED", message, path}`; `project.json` keeps naming it. Error codes: a file or folder that does not exist is `PATH_NOT_FOUND` in every command (`asset.import` source, `scene.open`, `project.open`, `editor.exec_file`, `script.read` / `script.delete`, `module.discover`, the `distributionDirectory` of `project.export` when the runtime is copied from it, the source file of a registered asset in `asset.set`, `asset.delete`, `asset.reimport` and `prefab.apply`); one that exists but cannot be read (a directory where a file is expected, a denied access) is `IO_ERROR`; `ASSET_NOT_FOUND` is only an asset reference (uuid or asset path) that names no asset in the database. | An open scene whose file is gone cannot be saved back the way the user expects, so losing it must be a decision; the startup scene is often replaced on purpose, so it only needs to be said. One code per meaning lets a client branch: fix the path, fix the permissions, or import the asset. |
| D21 | **File edits made while playing become one undo entry at `play.stop`** (code sweep F089). The command flag `ON_DISK` marks the undoable commands whose effect lives in the project's files and not in the scene: `asset.import`, `asset.create_folder`, `asset.delete`, `asset.create_material`, `asset.set`, `asset.restore`, `prefab.apply`, `project.set_settings`, `script.create`, `script.write`, `script.delete`. `play.stop` restores the scene snapshot and nothing on disk, so while the undo stack is frozen the dispatcher hands the inverse and replay of each such command to the `play_controller`, also for the children of an `editor.batch` (a batch that rolls back keeps none), and `play.stop` pushes them, in the order they ran, as one undo entry labelled `File changes during play`, the way D16 turns pinned values into one entry. Its result carries `fileChanges`, the number of commands in that entry. Undo itself stays frozen during play. The flag needs `MUTATES` and excludes `NO_UNDO` (the registry refuses anything else, and a module command loses it), and it promises that neither the inverse nor the replay refers to scene state: `prefab.apply` run while playing replays as an `asset.restore` of the bytes it wrote. `prefab.create`, `scene.save` and `scene.save_as` are editing-state commands instead (`WRONG_PLAY_STATE`): the inverse of `prefab.create` unpacks an entity the restore throws away, and a save would write the play session's scene over the edited one. Warnings, once per play session each: `UNRECORDED_PLAY_EDIT` for the first scene edit, whose message names the `ON_DISK` commands as the edits that are kept, and `PLAY_FILE_EDIT {cmd}` for the first file edit. `history.clear` while playing clears the stack only: what the session kept is not history until `play.stop`. | The old rule recorded nothing while frozen and its warning said `play.stop` discards the edit, which was false for everything on disk: a script written or an asset deleted during play stayed, with no way to undo it. Refusing those commands would block editing scripts against the running game, a real workflow; keeping the inverses costs one vector and makes every file change one Ctrl+Z away. |
| D22 | **`--exec` checks its command file before the engine starts, and a startup scene that did not load is the first line of its output** (code sweep F659). `editor_application::initialise` checks what needs no engine, in this order: the arguments, `project.json` and the merged configuration (`make_engine_desc`), then the command file (`headless_runner::load_command_file` reads and parses it); only then does the engine start. A missing or malformed file is still `BAD_ARGUMENTS` and exit code 3, but no window, device or asset scan comes first, and a bad flag or a missing project is still what is reported when the file is unusable as well. A startup scene the engine cannot load remains what 06_engine_loop.md makes it for an editor, a start over an empty world, and it does not change the exit code. The engine keeps the failure (`engine::application::startup_scene_failure()`), and the runner writes `warning {code: "STARTUP_SCENE_FAILED", message}` as the first line of the NDJSON output, before any request runs. After a `project.open` restart the application emits the same event to every client of the session. | A mistyped file name cost a whole engine start before it was reported, and a run over a broken startup scene looked like a clean run over an empty world: only the log said otherwise, and a script does not read the log. Failing the run instead would lock out the command files written to repair that scene. |
| D23 | **A TCP client presents a token, and the editor generates one by default** (code sweep F740). When a TCP transport starts and no token was given, the server takes 32 bytes from `std::random_device("/dev/urandom")` (`generate_access_token`, 64 hexadecimal digits); the device is named so that each supported standard library reads the operating system's generator: libstdc++ the device `/dev/urandom` (asked for nothing it would take the processor's RDSEED or RDRAND instruction on x86), libc++ that device, `getentropy` or `arc4random` as it was built, the Microsoft library `rand_s` (it ignores the name). The server writes them, with a newline, to `<project>/.conjure/ai_token`, before it writes `ai_port` beside it. The file goes through `core::vfs::write_private_file`: created with mode 0600 on POSIX and, on Windows, with an access control list that names the user running the editor alone and inherits nothing from the folder. A client reads the two files and sends `session.hello {token}` as its first request; anything else answers `UNAUTHORISED` once and the connection is closed, with one INFO log line that names the client id and nothing the client sent. The check is `constant_time_equals`, in one place (`protocol_server::access_of`), for the handshake and for the `session.hello` handler. The guards that keep a token out of text the editor passes on or records (`protocol_server::contains_token`, the audit log of D26) search with `constant_time_contains` / `constant_time_starts_at`, which compare the whole token at every position without stopping early, over a bounded text (`contains_token` counts a text over 1 KiB as holding a token without searching it): no client text is held against a token with an ordinary search. Which transports are gated: TCP always (a TCP transport never runs without a token); stdio and the in-process loopback only under an explicit `--ai-token <t>`, which then gates every transport as before and is also the token in the file. An empty `--ai-token` is refused (`INVALID_ARGS`). The files follow `project.open` and are removed when the server stops. Limits, all stated where they apply: accounts the operating system lets pass any permission (root, administrators) can read the file; a volume that cannot restrict a file to its owner (FAT, exFAT) gets no token file, a WARN says so, and TCP clients then need `--ai-token`; a server started without a project writes neither file, so its TCP port cannot be used until a project is open (INFO with a window, WARN headless) unless `--ai-token` was given; the files of a second editor on the same project replace the first editor's, and whichever editor stops first removes them. The token is never logged, never part of an event and never part of an error message. The token code carries more than one token: `access_level` is what a token grants, `client_state::access` records it, and `token_file_name(level)` names the file of a level; D25 adds the inspect-only token on that shape, and with it `--ai-inspect-token`, which gates every transport as `--ai-token` does. | The loopback port was open to every program of the machine and, through a browser, to every web page; `--ai-token` was the only gate, it was off by default and its value sits in the process list. A generated token in an owner-only file is on by default and needs nothing typed. stdio is the process that started the editor and the loopback is code inside it: neither can read a file before a project exists, and neither is an attacker the token could stop. |
| D24 | **A protocol client is the local user; the connection is what is guarded** (code sweep F187). A client that got through the gate of D23 may do what the user who runs the editor may do: read and write files anywhere that user can, build and load code (`script.compile`, `script.reload`, `module.build`, `module.load`), start an export. No command confines its paths to the project, and none pretends to. Three rules follow from that. (1) The argument checks stay, as checks of well-formed input: `is_safe_tool_argument` on what `module.build` and `script.compile` hand to `cmake` and `dotnet`, and the manifest's `binary` being a plain file name, stop a value from being read as an option or a path by the tool; they are not a sandbox. (2) A relative on-disk path in an argument is relative to the project root, never to the editor's working directory, which a client neither chose nor knows: `editor.exec_file {path}`, `project.export {directory, distributionDirectory}`, `module.discover {directory}`, `script.reload {assembly}`, `view.screenshot {path}` and `asset.import {source}` (a source that is no asset path) go through `support::project_relative_path` (lexical, `..` allowed; an empty text and any path while no project is open stay as given, and `module.discover` alone reads an empty `directory` as the project root, as it always did). `project.create` and `project.open` name a project root and keep taking their `path` as the operating system resolves it. The command line is not a protocol client: `--export` and `--distribution` are made absolute against the working directory before `project.export` sees them, and `--exec` reads its file directly. (3) A client whose first line is not a request (`PARSE_ERROR` or `INVALID_REQUEST` before any line of it parsed) gets that answer and is disconnected, on every transport: an HTTP request a web page sends to the port ends at its request line, and the JSON in its body is never read. The AI session panel opens its loopback connection with `session.hello` for the same reason. Decided with it, and all three built: an inspect-only token beside the full-control one (D25), an audit log of protocol commands under `.conjure/` (D26), and a trust question before an untrusted project's scripts or native modules run (D27). D27 guards what the editor is pointed at; this decision and D23 guard who drives it. With D25 the sentence above reads "a client that presented the full-control token": the inspect-only one opens what D25 lists and nothing else. | `module.build` was hardened "so a localhost client cannot run arbitrary programs" while five other commands read, wrote or loaded any path: the stated rule was not the real one. The real one is simpler and can be kept: guard who connects (D23), then trust them as the user. |
| D25 | **Two tokens: full control and inspect only, and the command's flags decide** (code sweep F187, decided with D24). Beside the full-control token the server keeps a second one that grants `access_level::INSPECT` (`NONE < INSPECT < FULL_CONTROL`). It is generated, written and removed with the first: `<project>/.conjure/ai_token_inspect`, owner-only through `core::vfs::write_private_file`, before `ai_port`. `--ai-inspect-token <t>` replaces it; like `--ai-token` it gates every transport, the token that was not given is generated, and the same text for both is refused (`INVALID_ARGS` on the command line, in `start`, `set_token` and `set_inspect_token`), because one text cannot grant two levels. `protocol_server::access_of(presented)` is still the one comparison: both tokens are always compared, each with `constant_time_equals`. **The rule.** A new command flag, `READ_ONLY`, marks the commands that change nothing clients share (not the scene, not a file, not the selection, the camera, the play state, the console or the history) and neither start a program nor load code; what belongs to the calling client alone (its event subscriptions, its pending reply) is its own. `required_access(flags)` is the whole rule: `INSPECT` with the flag, `FULL_CONTROL` without. No command is decided by its name, so a command added later, by the editor or by a module, needs full control until it is flagged. The registry refuses `READ_ONLY` with `MUTATES`; a module command that says both keeps `MUTATES`. 35 built-in commands carry the flag (4.4 names them); `tests/editor/test_command_catalogue.cpp` pins the list and holds every one of them to the promise. **Where it is applied.** `command_dispatcher::dispatch_internal`, right after the name lookup and before schema validation, preconditions and the handler, for top-level requests and batch children alike: `UNAUTHORISED` with `details {required, granted}`, and the client stays connected. The granted level is `protocol_server::access_of_client(id)`: what the client's last accepted `session.hello` granted, or `FULL_CONTROL` for an id no transport knows (the UI, `--exec`, `--export`, an `editor.exec_file` task: the editor's own code). **The hello.** `session.hello` answers `access`. The last accepted hello decides: the other token raises or lowers the same connection; a wrong token is refused and changes nothing; on a transport that asks for no token a client can still present the inspect token to restrict itself, and any other text is ignored there as before. Limits, stated where they apply: inspect-only limits what a client changes, not what it learns (the scene, the scripts under `scripts/`, the asset list, the console, every event); `editor.batch` needs full control even for a batch of reads; `view.screenshot` needs full control because it writes a file at any path; the editor cannot check a module's `READ_ONLY` promise; the AI session panel always presents the full-control token, being the user at the editor. An inspect-only client cannot read the token files: the one flagged command that returns a file's content, `script.read`, ends inside `scripts/` (links resolved). Events reach inspect-only clients too, so the two places where one client's slip would be passed on are checked with `protocol_server::contains_token`: the client name `session.hello` logs and the command name in `command.executed` (a text over 1 KiB is treated as holding a token and is not passed on); what a client sends as ordinary data (an entity name) is its own to guard. | "An authorised client is the user" (D24) leaves no way to let a program watch: a reviewer agent, a dashboard, a recording tool each needed the token that can also delete the project. A second token costs one comparison. Deciding by flag, opt-in, keeps the rule in one function and makes the safe answer the default for every command nobody thought about; deciding by `MUTATES` alone would have opened `editor.quit`, `play.start`, `script.compile` and `view.screenshot`. |
| D26 | **Protocol commands are recorded in an audit log under `.conjure/`** (code sweep F187, decided with D24). Every request of a transport client (TCP, stdio, loopback, the AI session panel among them) that reached the dispatcher is appended to `<project>/.conjure/ai_audit.log`, one JSON object per line: `{time (UTC), clientId, transport, access, cmd, args, ok}`, plus `error` (the code) when it failed, `"async": true` when it was accepted and had not finished, and `"batch": true` for a child of `editor.batch` (children first, then the batch). Refused requests (`UNAUTHORISED` by scope, `INVALID_ARGS`, `UNKNOWN_COMMAND`) are recorded like any other. `command_dispatcher::dispatch` and `run_batch` hand the request to `protocol_server::record_command`, which adds transport and access and drops ids no transport knows; `audit_log` (`conjure/editor/protocol/audit_log.h`) formats, bounds and writes. **No token.** The value of a member named `token` is written as `"[withheld]"` at any depth of any command (so also for a hello under a mistyped command name); wherever else a text of the request (command name, key, string value) contains one of the two tokens that part is `"[token]"`, replaced before any cut; arguments whose serialised form still contains one (a number equal to an all-digit token) are withheld as a whole. The tokens are looked for with the `constant_time_` functions of D23, never with a substring search. One limit: the record's own words are not client text, so a token that is part of them (a one-letter `--ai-token`) cannot be kept out. **Bounded.** A line is at most 4 KiB (command name cut to 128 bytes, a string or key to 160, an array to 16 items, an object to 32 members with a `"...": "<n> more"` member for the rest, nesting to 6 levels, arguments over 2 KiB replaced by a note of their size); the copy stops once what it kept passes 2 KiB, so the work of a record is bounded too, whatever the width or length of the request (one line may be 16 MiB); a file at most 4 MiB; a line that would pass that makes the file `ai_audit.1.log`, replacing the one before, so the two never exceed 8 MiB; a file found larger when the log opens is removed. The log therefore covers the most recent commands, and a busy client pushes older lines out. **The file.** Created on the first record, owner-only through `write_private_file` (an ordinary file on FAT and exFAT, where an existing one is appended to as it is); an existing regular file that `is_private_file` does not accept (another account made it readable by others, or a hard link to one) is replaced by an owner-only one with a warning, its records lost; a symbolic link at the path is replaced, not written through (check and open are two steps). Flushed once per frame with the server. A write failure is one WARN and no more records until the project changes; it never fails a command. Not recorded: the editor's own requests (the UI's would be one line per dragged frame; `--exec`, `--export`; the commands inside a file run by `editor.exec_file`, whose own line names the file), a client that failed the handshake (it never reaches the dispatcher, so a stranger cannot fill the log), the outcome of an asynchronous command, undo inverses, redo replays and batch rollbacks (the `history.undo` or `editor.batch` line stands for them), and anything while no project is open, the session's first `project.open` included (a `project.open` from an open project is recorded in the project it left). Two editors on one project append to the same files and each counts only its own lines, so a file can then hold up to twice its limit. When the full file cannot be moved to `ai_audit.1.log` (a folder has that name, or a program holds a file open in a way that forbids the move) it is emptied in place: the bound is kept, its records are not. It is a record for the user, not evidence: a full-control client can delete it. | With D24 an authorised client acts as the user, and an agent does in a minute what a person does in a day; afterwards "what did it run, and what was refused" had no answer but a log at INFO that names no arguments. The project folder is where the session files already live and where the question is asked. Bounding it and keeping tokens out are what make it safe to leave on by default. |
| D27 | **A project is opened only after somebody answered for it: project trust** (code sweep F187, decided with D24). Opening a project runs code from its folder with the rights of the user who runs the editor: the engine's startup loads the native modules its `module.json` files name and the game assembly its `project.json` names, `script.compile` and `module.build` then run `dotnet` and `cmake` over its project files, and `play.start` runs its scripts. None of it can be taken back, so the question comes before the first of it. **One gate, at the opening.** `editor_application` starts an engine over a project only when its `project_trust` (`conjure/editor/project_trust.h`) holds an answer, and asks it in the three places a project is opened: `initialise()` for `--project`; `request_open_project()` for `project.open`, which answers the new error `PROJECT_NOT_TRUSTED` with `details {path, state, prompt}` and schedules nothing; and the restart at the frame boundary, which asks again in case the answer was revoked in between (the restart is then dropped with a WARN, before anything of the running engine is touched, and the editor stays on the project it has). An untrusted project is not opened at all. There is no restricted mode that opens it without its code: the engine has no switch that keeps a project's module manifests from loading, and a gate with a known hole would be worse than none (wanted later, it starts in 06_engine_loop.md). **Answers are explicit, never implied.** *The window.* A windowed session that runs neither `--exec` nor `--export` (`editor_application::can_ask_about_trust`) starts without its untrusted `--project` and shows a modal question, "Do you trust the authors of this project?", with the path (the resolved folder an answer records, `project_trust::project_key`, not the spelling that was typed: a path relative to the working directory or the name of a link is shown as the folder it names) and what trusting means: *Trust and Open* sends `project.trust` and then `project.open`, the two requests a protocol client sends; *Don't Open* closes the question and records nothing. A `project.open` that a protocol client sends for an untrusted project is refused like any other, and raises the same question for the person at the window. The question is context state (`editor_context::trust_prompt`), reported by `editor.state` as `trustPrompt`; it survives an engine restart and closes when anybody trusts that project or it is opened. *The command line.* `--trust-project` answers for the project of `--project`, for that run and no longer; it needs `--project` and writes nothing. Where nobody can be asked (`--headless`, `--exec`, `--export`) an untrusted `--project` ends `initialise` with `PROJECT_NOT_TRUSTED` before the engine starts, exit code 2. It is the last of the checks that need no engine (D22), so a bad flag, a missing project and an unusable `--exec` file are reported first. *The protocol.* `project.trust {path, remember?=true}` (the path must hold a project), `project.revoke_trust {path}` (the folder may be gone by then) and `project.trust_state {path?}` (`read_only`). The first two need full control and cannot run inside a batch, which could not take them back. No other command trusts, `project.open` least of all. One command records trust without having been asked to: `project.create`, for a project it wrote into a folder it made itself (one that did not exist), because everything in it then came from the editor (`result.trust`); created in a folder that was already there, even an empty one, it stays untrusted, since whoever made that folder or may write it can add a `module.json` later (the folder the editor makes gets the permissions its parent hands down). **What is remembered, and where.** A project is the resolved absolute path of its root folder (`project_trust::project_key`: links, `.` and `..` resolved by the file system, UTF-8, forward slashes). An answer for the session lives in memory. A remembered answer lives in the trust store, a JSON file `{schema_version, projects: [{path, trustedAt, origin}]}` outside every project: `trusted_projects.json` in the per-user state folder of the machine, which is `%LOCALAPPDATA%/Conjure` on Windows (the known folder that does not roam), `$XDG_STATE_HOME/conjure` or `~/.local/state/conjure` on Linux, and `~/Library/Application Support/Conjure` on macOS; `--trust-store <file>` names another file. It is not the user configuration folder of 3.15, on purpose: a list of paths of this machine must not roam to another machine, where the same path may hold other files. The folder is resolved inside the editor module (`engine/src/editor/core/user_paths_{windows,posix}.cpp`, chosen by CMake like the socket layer) until `platform` has the per-user folders 3.15 asks of it. Only `conjure_editor`'s `main` names the default file (`default_trust_store_file`); an `editor_application` given none remembers nothing, so tests and embedders never touch the user's folder by accident. The store is written through `core::vfs::write_private_file` and believed only while `core::vfs::is_private_file` holds, since an entry another account planted would open a project without the question (the check and the read are two steps on the path, not one on an open file, so a file swapped in between is believed: the store's folder must not be writable by other accounts; the default folder is not, and a folder named with `--trust-store` is the caller's choice); a store that fails that check, does not parse, has a newer schema version or exceeds 4 MiB counts as empty (one WARN per read) and is replaced by the next remembered answer. It keeps the newest 1024 answers. `origin` records who answered (`editor` for the window and the editor's own requests, the transport name for a protocol client, `created` for `project.create`), because the audit log (D26) has no project folder to write to while a session's first project is being trusted. **Limits, stated where they apply.** Trust belongs to the path, not to the files: what is later put into a trusted folder (a pull from a repository) runs without a question, and a project that was moved or copied is asked about again. Two spellings the operating system does not resolve to the same text (another letter case on a volume that ignores case and does not report the stored one) are two projects, and the second is asked about. Trusting a project trusts what its `project.json` names, wherever that lies. Revoking does not close the project that is open; it counts from the next opening, and the restart that follows a `project.open` whose engine failed to start is one: a previous project no longer trusted is not started again, and the editor goes on without a project (a WARN says so). An answer the store cannot take (no per-user folder, a volume without owner-only files, something in the way), or one the store no longer shows right after it was written (another editor rewrote it), holds for the session, with a WARN, and a revocation the store cannot take is an error. Two editors that answer at the same moment read and write the store one after the other without a lock: one answer can be lost and is then asked for again. A client that presented the full-control token is the user (D24) and may answer; an inspect-only client may only ask. A client must be connected to answer: a windowed editor that started without its untrusted project has no project folder for the token files of D23, so over TCP nobody can answer for it until the person at the window did (or `--ai-token` named a token), which is the order this decision wants. A command file is commands: one that came with a project can answer for that project, as it can do everything else, so it is read before it is run (`--exec`, `editor.exec_file`). The commands that load code from a path the caller names (`script.reload {assembly}`, `module.discover {directory}` followed by `module.load`) are D24's business, not a project being opened, and are not gated here. `conjure_runtime` runs the project it is given: running a game is the answer. | A project folder is somebody else's program. Until now `conjure_editor --project <downloaded folder>` loaded that folder's native libraries during startup, before the first frame, and nothing asked. D23 and D24 guard who drives the editor, not what it is pointed at. One gate at the opening needs no list of commands that must be kept complete and holds for every command added later; a list of gated commands is exactly what a new command slips through. Keeping the answers outside the project is what makes them answers: a store inside the folder would let the project trust itself. |

### 1.3 What was rejected and why

- Transport threads + lock-free queues (P1): a `core` dependency and a second ownership domain for no
  measurable gain; an agent issues tens of requests per second. Main-thread polling instead.
- Byte-offset inspector reads from ECS chunks (P1): couples the editor to chunk layout and cannot serve
  C# script fields. JSON through reflection is the one mechanism.
- `command_registry::freeze()` (P1): incompatible with runtime `module.load` adding commands.
- Class-per-command virtuals plus a separate handler layer (P2): two representations of every mutation;
  UI/protocol parity would become a review discipline instead of a structural fact.
- One-async-per-client `BUSY` (P2): relaxed to 8 pending async requests per client.
- Fire-and-forget requests (P1): an agent would lose error reports, so a response is always sent.

## 2. Data model

### 2.1 Commands

```cpp
namespace conjure::editor {

    enum class command_flags : std::uint16_t {       // enum class + NONE per CONVENTIONS 3 (R31)
        NONE = 0,
        MUTATES = 1 << 0,                    // Recorded in the undo stack, bumps revision.
        REQUIRES_PROJECT = 1 << 1,
        REQUIRES_SCENE = 1 << 2,
        EDITING_ONLY = 1 << 3,               // Refused while PLAYING or PAUSED.
        PLAYING_ONLY = 1 << 4,               // Refused while EDITING.
        ASYNC = 1 << 5,                      // Handler answers later through async_reply.
        UI_ONLY = 1 << 6,                    // Hidden from editor.schema (panel focus etc.).
        NOT_IN_BATCH = 1 << 7,               // Implied by ASYNC; also editor.batch, editor.begin_transaction/
                                             // end_transaction/quit, history.undo/redo, module.load/unload/reload,
                                             // play.start/stop, project.create/open, scene.new/open/close
                                             // (play.pause/resume are batchable).
        NO_UNDO = 1 << 8,                    // MUTATES without an inverse (module commands, command_desc); not recorded.
        ON_DISK = 1 << 9                     // Undoable, and the effect lives in the project's files, not in the
                                             // scene: kept while playing, one undo entry at play.stop (D21).
    };
    CONJURE_ENUM_FLAGS(command_flags);

    struct command_request {
        std::string name = "";                 // "entity.create"
        core::json::document args;             // Always an object.
        std::uint32_t clientId = 0;            // 0 = local UI.
        std::uint64_t coalesceKey = 0;         // Non-zero: merge with the previous entry of equal key.
    };

    struct command_inverse {
        std::string name = "";
        core::json::document args;
    };

    struct command_outcome {
        core::json::document result;
        std::optional<command_inverse> inverse;     // Mandatory when COMMAND_MUTATES.
        std::optional<command_request> replay;      // Normalised forward request; default = original.
        std::string label = "";                     // Undo label, e.g. "Rename 'Wall_3'".
    };

    struct command_invocation {
        editor_context &context;
        const core::json::value &args;
        std::uint32_t clientId = 0;
        async_reply reply;                          // Valid only for COMMAND_ASYNC handlers.
    };

    using command_handler = std::function<core::result<command_outcome>(command_invocation &invocation)>;

    struct command_descriptor {
        std::string name = "";
        std::string summary = "";
        std::string argsSchema = "{}";              // JSON Schema subset, parsed at registration.
        std::string resultSchema = "{}";
        command_flags flags = command_flags::NONE;
        std::string owner = "editor";               // Module name; used by module.unload (R15).
        command_handler handler;
    };
}
```

Schema subset (hand-written validator, no third-party): `type` (object, array, string, number,
integer, boolean, null, or array of types), `properties`, `required`, `additionalProperties`
(default `false` for top-level args), `items`, `enum`, `minimum`, `maximum`, `minItems`,
`maxItems`, `oneOf` (shallow). Anything else in a schema fails registration in Debug.

### 2.2 Undo stack

```cpp
struct undo_entry {
    std::string label = "";
    std::vector<command_inverse> undo;        // Applied in reverse order.
    std::vector<command_request> redo;        // Replays, applied forwards.
    std::uint64_t coalesceKey = 0;
    std::uint32_t clientId = 0;
    std::size_t byteSize = 0;                 // Serialised size, for the budget.
};
```

`undo_stack` keeps `std::deque<undo_entry> mEntries`, cursor `mCursor`, `mMaxDepth = 1024`,
`mByteBudget = 256 MiB` (oldest evicted first), `mFrozen`, and one open `transaction` per client
(`flat_hash_map<uint32_t, undo_entry>`). Coalescing: when the top entry has the same non-zero
`coalesceKey` and the same single command name, the new `redo` replaces the old one and the old
`undo` is kept (first state wins). The UI computes `coalesceKey = hash(commandName, guid, path,
dragSessionId)`; the protocol exposes it as an optional `coalesceKey` request field.

### 2.3 Protocol messages

```cpp
struct protocol_request {
    core::json::document id;              // Integer, string or null. Echoed verbatim.
    std::string command = "";
    core::json::document args;            // Object; `{}` when absent.
    std::uint64_t coalesceKey = 0;
    std::uint32_t clientId = 0;
};

struct protocol_response {
    core::json::document id;
    bool ok = false;
    core::json::document result;          // When ok.
    protocol_error_code errorCode = protocol_error_code::NONE;
    std::string errorMessage = "";
    core::json::document errorDetails;    // Optional object (diagnostics, candidates, didYouMean).
};

struct protocol_event {
    std::uint64_t seq = 0;
    std::uint64_t frame = 0;
    std::string name = "";                // "log", "selection.changed", ...
    core::json::document data;
};
```

Wire form:

```
-> {"id":12,"cmd":"entity.create","args":{"name":"Player","components":{"transform":{"position":[1,0,1]}}}}
<- {"id":12,"ok":true,"result":{"guid":"a9e2c0d4-...","revision":42}}
<- {"id":13,"ok":false,"error":{"code":"ENTITY_NOT_FOUND","message":"No entity 'Maze/Wall_99'","details":{}}}
<- {"event":"selection.changed","seq":913,"frame":8812,"data":{"guids":["a9e2c0d4-..."],"primary":"a9e2c0d4-..."}}
```

`protocol_error_code : std::uint16_t` = `NONE, PARSE_ERROR, INVALID_REQUEST, MESSAGE_TOO_LARGE, UNKNOWN_COMMAND,
INVALID_ARGS, ENTITY_NOT_FOUND, COMPONENT_NOT_FOUND, TYPE_NOT_FOUND, PATH_NOT_FOUND, ASSET_NOT_FOUND,
PROJECT_NOT_OPEN, SCENE_NOT_OPEN, WRONG_PLAY_STATE, NOT_AVAILABLE, IO_ERROR, COMPILE_FAILED, BUSY,
TIMEOUT, CANCELLED, UNAUTHORISED, PROJECT_NOT_TRUSTED, INTERNAL`. `UNKNOWN_COMMAND` details carry `didYouMean` (closest
names by edit distance). Codes travel as strings; the enum is internal. `PATH_NOT_FOUND` is a reflection path
that does not resolve or a file or folder that does not exist, `IO_ERROR` a file or folder that exists and cannot
be read (or a failed write), `ASSET_NOT_FOUND` an asset reference that names no asset (D20).

### 2.4 Client and server state

`client_state { std::uint32_t id; transport_kind kind; std::uint64_t subscriptions (event bitmask);
core::log_level minLogLevel = INFO; access_level access (D23, D25: NONE, INSPECT or FULL_CONTROL); bool spoke (D24); std::uint8_t pendingAsync; std::string
outbound (bytes not yet written); }` stored in a `core::slot_map`. Client 0 is the local UI (never
disconnects, no transport). Default subscriptions for a new client: every event except `log` and
`command.executed`. Outbound backlog above 8 MiB drops `log` events (counted, reported once via
`events.dropped` when the backlog drains); above 64 MiB the client is disconnected.

### 2.5 UI caches

```cpp
struct hierarchy_row {               // 16 bytes, flat pre-order, one per visible row.
    ecs::entity entity;              // index + generation.
    core::string_id nameId;
    std::uint16_t depth = 0;
    std::uint8_t flags = 0;          // HAS_CHILDREN | EXPANDED | SELECTED | PREFAB_INSTANCE | DISABLED
    std::uint8_t padding = 0;
};
```

`hierarchy_cache` owns `std::vector<hierarchy_row>`, an `expanded` bitset keyed by entity index and
the filter string; it rebuilds in one pre-order walk when `scene::structure_version()`, the
expansion set or the filter changes. Filtering first computes a name-match bitset (`parallel_for`
above 4096 entities), then keeps rows with a matching descendant. `selection_set` is a sorted
`std::vector<core::uuid>` plus a dense `bit_vector` by entity index for O(1) `contains` while drawing.

### 2.6 Console buffer

`console_buffer` is a fixed ring (65,536 entries, 16 MiB text) fed by a `core::log::sink` registered
at startup. Entries: `{seq, frame, level, category string_id, text offset/length}`. Both the console
panel and the `log` event / `console.read` command read from it on the main thread. The sink is
called from any thread, so the ring is guarded by a `std::mutex` (writes are a memcpy; the lock is
never held across allocation).

## 3. Algorithms and flow

### 3.1 Startup (`editor/main.cpp` -> `editor_application`)

```
parse_editor_options(argv)                      bad flag -> print usage, exit 3
editor_application::initialise(options):
    backends = headless ? (null, offscreen ? rhi:real-no-swapchain : rhi:null, audio:null)
                        : (glfw, rhi:default, audio:miniaudio)
    desc = engine::application_desc::from_command_line(args, config{editor = true, backends})   (R13)
    if options.execFile and no exportDir:                        (D22: before anything heavy starts)
        mExecRequests = headless_runner::load_command_file(options.execFile)   missing / malformed -> exit 3
    mTrust.set_store_file(options.trustStoreFile); --trust-project: mTrust.trust(project, session)      (D27)
    if options.project and not mTrust.is_trusted(project):       (the last check that needs no engine)
        headless, --exec or --export: fail PROJECT_NOT_TRUSTED                                 -> exit 2
        a window: desc = the merge without the project; the context's trust_prompt asks about it
    mEngine = make_unique<engine::application>(desc); mEngine->add_subsystem(bindings subsystem)
    mEngine->initialise()                                        failure -> exit 2
    mContext = editor_context (owns the services of section 5; binds non-owning refs from mEngine->context())
    register_builtin_commands(registry); modules->add_consumer(editor_module_consumer)   (R14; before load_all,
                                                                 so the editor consumer is registered through a
                                                                 pre-initialise hook on the modules subsystem)
    if options.project: dispatch project.open                   failure -> exit 2
    if !headless: mEngine->push_layer(ui::create_editor_layer(*mContext))   (ui lib, injected by main)
    protocol_server::start(options)  (tcp unless --ai-port 0; stdio when --ai-stdio)
editor_application::run():
    if options.exportDir  -> dispatch project.export, pump until done, exit 0/1
    if options.execFile   -> headless_runner::run(mExecRequests), exit with its code
    else                  -> mEngine->run()  (editor_layer or headless_tick drives 3.2 each frame)
```

### 3.2 Frame integration (main thread, every frame)

```
editor_layer::on_update(frameTime)      (headless: editor_application::headless_tick, same body)
    1. protocol_server.poll()                 accept, read non-blocking sockets, drain stdin queue,
                                              split lines -> protocol_requests (PARSE_ERROR replies inline)
    2. dispatcher.resolve_completed()         async jobs that finished since last frame -> responses
    3. for request in inbound (max 256/frame): response = dispatcher.dispatch(request); queue it
    4. play_controller.tick(frameTime)        decides simulation mask / single steps for the engine
    5. screenshot_service.tick()              completes readbacks that became ready
    6. hierarchy_cache.refresh_if_dirty()     (ui only)
    7. events.flush(); protocol_server.flush()  write outbound bytes, non-blocking
editor_layer::on_imgui()
    workspace.draw_dockspace(); every open panel draws; panels submit command_requests via dispatcher
```

The 256/frame cap prevents a flooding client from stalling rendering; `editor.batch` counts as one.

### 3.3 Dispatch

```
dispatch(request) -> optional<protocol_response>:
    spec = registry.find(request.command)                 none -> UNKNOWN_COMMAND (+didYouMean)
    schema_validator.check(spec.argsSchema, args)          fail -> INVALID_ARGS "args.name: expected string"
    preconditions(spec.flags, context)                    PROJECT_NOT_OPEN / SCENE_NOT_OPEN / WRONG_PLAY_STATE
    if ASYNC: client.pendingAsync >= 8 -> BUSY; park {clientId, id, deadline}; handler(invocation) ; return nullopt
    outcome = handler(invocation)                          error -> response(code, message, details)
    if MUTATES and not undo.frozen: undo.record(replay-or-request, inverse, transaction, coalesceKey)
    if MUTATES and undo.frozen:     ON_DISK -> play.keep_file_edit(cmd, replay-or-request, inverse)   (D21)
                                    otherwise play.note_unrecorded_edit()   (the restore discards the edit)
    if MUTATES: ++revision; result["revision"] = revision; emit scene.changed {revision, label}
    emit command.executed {clientId, cmd, ok}
    return response(ok, result)
```

`history.undo` / `history.redo` are commands without `COMMAND_MUTATES` that pop an entry and dispatch
its inverses (or replays) with undo recording suppressed; they bump `revision` themselves.

### 3.4 Transactions and batches

- `editor.begin_transaction {label}` opens a staging `undo_entry` for that client; subsequent mutating
  commands append to it; `editor.end_transaction` pushes it. Disconnect commits the open transaction.
- `editor.batch {commands:[{cmd,args}], label?, atomic?=true}`: dispatches children in order with
  recording redirected into one entry. On a child failure with `atomic`, the recorded inverses are
  dispatched in reverse, the entry is discarded, and the batch fails with the child's error and
  `details.failedIndex`. Children flagged `COMMAND_NOT_IN_BATCH` are rejected up front (`INVALID_ARGS`).
  Nested `editor.batch` is rejected. While playing nothing reaches the frozen stack: the batch's `ON_DISK`
  children are kept for `play.stop` one by one (D21), its scene edits are dropped, and a rolled-back batch keeps
  nothing.

### 3.5 Entity and property resolution

`entity_ref::resolve(json, scene) -> result<ecs::entity>`: a string that parses as a uuid is a guid
(`scene::guid_index` lookup); any other string is a hierarchy path `Root/Child/Grandchild` resolved
from scene roots by name (ambiguous -> `INVALID_ARGS` with `details.candidates` guids; missing ->
`ENTITY_NOT_FOUND`). Responses always return guids.

`reflection_path::parse("transform.position.x")` yields segments `{field, optional index}`; the first
segment is the component type name (as registered in the `ecs::component_registry`, which bridges `core::reflect`).
`resolve(registry)` walks `type_info` fields and returns `field_ref { const type_info *type; const field_info
*field; }` or `PATH_NOT_FOUND` naming the first unresolvable segment. Values move through JSON only:
`ecs::component_to_json(world, entity, id)` -> patch at path with `write_at` -> `ecs::component_from_json` (R40).
Enum fields accept the ALL_CAPS name or the integer. Script components expose their serialised fields
as a reflected `fields` struct (area C), so `script.fields.speed` needs no special case.

### 3.6 Bulk create

`entity.create_many {count, nameFormat?="Entity_{i}", parent?, components?, overrides?:[json patch]}`:
resolve the component template once, call `scene::create_entities(count, template)` (one archetype
insertion pass), assign names and guids in a batch, then apply per-entity override patches through
`reflection_path`. Inverse: one `entity.destroy_many {entities:[guids]}`. Budget: 10,000 entities with
4 components in < 20 ms (benchmark `bench_editor`).

### 3.7 Play / pause / step

```
(editing)    the engine runs in simulation_mode::PAUSED while EDITING: TRANSFORM/SYNC/EXTRACT still run, the
             fixed loop (physics) and UPDATE systems without RUNS_WHILE_PAUSED do not; script_runtime is in
             script_mode::EDIT (no managed instances)                                            (R13)
play.start   require EDITING, and no pending project.open (BUSY: its restart would carry the session over)
             persist_list.clear(); mSnapshot = scene_serializer::capture(scene); mSelection = selection.guids()
             mFileEdits.clear(); undo.set_frozen(true); script_runtime.set_mode(PLAY)
             engine.set_simulation_mode(RUNNING)
             state = PLAYING; emit play.state
play.pause   PLAYING -> PAUSED; engine.set_simulation_mode(PAUSED)
play.resume  PAUSED -> PLAYING; engine.set_simulation_mode(RUNNING)
play.step    {fixedSteps?=1} PAUSED only; async: engine.step_fixed(n) makes the next frame run exactly n fixed
             steps and return to PAUSED; the reply completes when engine_time.fixedStepIndex has advanced by n
play.stop    engine.set_simulation_mode(PAUSED); a pending play.step: engine.cancel_fixed_steps() (a forced step
             overrides PAUSED) and its reply fails CANCELLED
             captured = persist_list.capture(scene) (3.13, before restore)
             script_runtime.set_mode(EDIT); scene_serializer::restore(scene, mSnapshot)
             (failure -> ERROR log, empty scene, state EDITING; never a crash; captured values are dropped)
             selection.set(existing guids of mSelection); undo.set_frozen(false)
             undo.push(mFileEdits labelled "File changes during play") when not empty (D21; also after a
             failed restore: the files do not depend on the scene)
             persist_list.apply(captured) as one recorded edit (3.13); persist_list.clear(); emit play.state
```

What an edit made while not `EDITING` leaves behind depends on where its effect lives (D9, D21):

- A **scene edit** (`entity.*`, `component.*`, `prefab.instantiate`, `prefab.unpack`, `script.attach`,
  `scene.set_settings`) executes but is not recorded, and the restore at `play.stop` discards it. The first one per
  play session emits `warning {code:"UNRECORDED_PLAY_EDIT"}`; the message also names the `ON_DISK` commands
  (read from the registry), whose changes are kept.
- A **file edit** (a command flagged `ON_DISK`) executes and stays on disk: the restore does not touch files. The
  dispatcher gives its inverse and replay to `play_controller::keep_file_edit`, which collects them in the order
  they ran; the first one per play session emits `warning {code:"PLAY_FILE_EDIT", cmd}`. `play.stop` pushes the
  collection as one undo entry, `File changes during play`, and answers `fileChanges` (the count, 0 when nothing
  was kept and nothing was pushed). `history.undo` then runs the inverses newest first, `history.redo` the replays
  in order. The stop paths that do not go through the command (`module.unload`, the restart of `project.open`,
  shutdown) push the entry the same way, since `play_controller::stop()` does it.
- `prefab.create`, `scene.save` and `scene.save_as` are refused with `WRONG_PLAY_STATE`: they would tie a file to
  scene state that the restore removes. In the UI, File > Save Scene, Save Scene As and Ctrl+S do nothing while
  playing.
- `history.undo` / `history.redo` stay refused (`WRONG_PLAY_STATE`) until `play.stop`.

Budget for the scene module: 10k entities capture + restore < 50 ms combined.

Every `play.*` result and the `play.state` event are `{state, frame, simulationFrame, simulationTime,
fixedStepIndex}`. `frame` is the engine frame (`engine_time.frameIndex`); the other three are the clocks of the
play session, read from the simulation clock (code sweep of 2026-11-12, F730 and F655). `play.start` records the
engine's `fixedTime` and `fixedStepIndex`; `simulationTime` and `fixedStepIndex` are the differences from them,
and `simulationFrame` counts the frames simulated since (a `play.step` counts one). The three stop while paused
(only a `play.step` moves them) and while editing, and the ended session's values stay until the next
`play.start`, so `play.stop` answers with them. The engine's `time` is not used: it keeps running while paused.
`profiler.frame_stats` reports the engine's clocks since the engine started (`project.open` restarts them) under
the names `simulationTime` (`engine_time.time`) and `fixedStepIndex`, and its summary says so.

### 3.8 Picking

`picker::pick(ray, candidates, mode)`; candidates are `pick_candidate { core::uuid guid; math::aabb
worldBounds; ecs::entity entity; }` gathered by `gather_pick_candidates(scene)` (mesh_renderer,
sprite_renderer, light, camera; the last two use a fixed 0.5 m box). Broad phase: slab test over
all candidates (SoA, `parallel_for` above 4096), sorted by entry distance. Narrow phase (`mode:
"mesh"`, default): for candidates in order, test the mesh asset's CPU triangles in object space via
`renderer::mesh_asset::cpu_geometry()` (null -> treat the AABB hit as the hit) until a hit is closer
than the next candidate's entry distance; sprites test their quad. Ties between AABB-only hits go to
the smaller volume so a child inside its parent remains selectable. `mode:"bounds"` skips the narrow
phase; `mode:"physics"` calls `physics::physics_world::raycast`. Result: `pick_result { bool hit;
core::uuid guid; math::vec3 point; math::vec3 normal; float distance; }`.

### 3.9 Gizmos (ui)

`gizmo_controller` wraps ImGuizmo: `ImGuizmo::BeginFrame()`, `SetDrawlist()`, `SetRect()` for the
viewport, `Manipulate(view, projection, operation, mode, matrix, deltaMatrix, snap)`. On the first
`IsUsing()` frame it dispatches `editor.begin_transaction {label:"Move"}` and caches the selected
entities' world matrices; each frame with a non-identity delta it dispatches `component.set_many`
on `transform` (local values through the parent's inverse world matrix, multi-selection pivoting at
the centroid) with the drag session as `coalesceKey`; on release it dispatches `editor.end_transaction`.

### 3.10 Screenshot

`view.screenshot {path, width?, height?, camera?:"editor"|"game"|guid, format?:"png"|"jxl", waitFrames?=0}`
(async): `screenshot_service` creates (or reuses) a `renderer::render_target` of the requested size and, during
EXTRACT, appends a `render_view` targeting it with the chosen camera for `waitFrames + 1` frames; it then calls
`renderer::read_back(target)` (synchronous, waits for idle; the renderer offers no asynchronous capture, R12) on
the main thread. The service hands the `core::image` to a job that encodes it with `core::image::save_png`
(any other `format` is `INVALID_ARGS`), writes the file and completes the reply with `{path, width, height,
frame}`. `null` RHI -> `NOT_AVAILABLE` (also in headless CI; area H's tests assert that code).

As built (JPEG XL adoption): `format` is `screenshot_format` (`"png"` or `"jxl"`). Without it the path's extension
decides: `.jxl` (any case) is a lossless JPEG XL and anything else stays a PNG, so goldens and tools keep their
format; a `format` that contradicts a `.png` / `.jxl` extension, or any other value, is `INVALID_ARGS`. The job
encodes a JPEG XL with `renderer::encode_jpeg_xl` (lossless, tagged sRGB, effort 2, on the job system's workers);
`core::image` stays PNG-only and libjxl-free. A build without the renderer cannot write JPEG XL
(`screenshot_service::supports`, `NOT_AVAILABLE`). The reply adds `format`.

As built (code sweep F104): the view is not appended during EXTRACT. `renderer::submit_view(view)` queues a view
that the next `end_extract()` appends to the extracted scene, for one frame per call, and the editor submits
through `scene_view` (`engine/src/editor/core/scene_view.h`, private: the renderer is a private dependency of the
editor libraries), which owns the render target. The viewport panel keeps one `scene_view` at the panel's size in
framebuffer pixels, submits `make_editor_camera_view(camera, target, postProcess)` every frame, registers the
target's colour texture with `imgui_renderer::register_texture` and draws it under its input surface; the view's
projection equals `editor_camera::projection_matrix` for the target's aspect, so the pick ray and the gizmo agree
with the picture. The target keeps its size while a mouse button is down (a resize waits for the GPU). The
screenshot backend resolves `camera` with `resolve_camera_view` ("editor", "game" = the camera the engine shows
first, or an entity; `renderer::make_camera_view` is the one place a camera component becomes a view), submits it
on every frame the request waits, and the service counts a waited frame only when one was rendered into the
target: a request that arrives over the protocol is ticked in its own frame, before anything was drawn for it.
Still open: a scene camera keeps drawing into the back buffer behind the editor's panels (the Scene / Game split of
D18), and a real device without a window (`--offscreen`) is still refused by the engine configuration.

### 3.11 Headless runner

`conjure_editor --headless --exec commands.json --project <dir> [--trust-project] [--out responses.ndjson]
[--keep-going] [--offscreen] [--timeout-seconds 120]`. Input is a JSON array of requests or NDJSON; a missing
`id` becomes the zero-based line index. A project nobody answered for on this machine is not opened (D27): the
run ends with exit code 2 before the engine starts unless `--trust-project` answers for this run or an earlier
`project.trust` is remembered; a `project.open` inside the file needs its `project.trust` first in the same way. The file is read and parsed before the engine starts (D22): a missing or
malformed file ends the process with exit code 3 after the arguments and the project were checked and before a
window, a device or the asset scan exists. For each request: dispatch, pump `engine::application::run_frame()`
until the response exists (async), write the response as one NDJSON line to `--out` or stdout; events
are written to the same stream (they carry `event`, not `id`). Stop at the first `ok:false` unless
`--keep-going`. Exit codes: 0 all ok, 1 a command failed, 2 startup/project failure, 3 bad arguments.

When the engine could not load the project's startup scene (the file is gone, does not import or does not load),
the editor starts over an empty world as in every other mode, and the runner writes `warning {code:
"STARTUP_SCENE_FAILED", message}` as the first line of the output, before any request runs. The message is the
engine's (`engine::application::startup_scene_failure()`). The warning is counted nowhere: the exit code is the
requests' alone, so a command file that exists to repair the scene runs and exits 0. A `project.open` in the file
restarts the engine; when the new project's startup scene fails, the application emits the same event to every
client, so it appears after the `project.open` response.
`--ai-port`/`--ai-stdio` with `--headless` and no `--exec` keeps the process alive as a server.

### 3.12 Transports

- `tcp_transport`: binds `127.0.0.1:<port>` (default 47820). If busy and the port was *not* given
  explicitly, try the next 9 ports; the chosen port is logged and written to `<project>/.conjure/ai_port`.
  Explicit busy port: UI mode logs ERROR and runs without the server; headless exits 2. `listen(8)`,
  non-blocking accept/recv/send, up to 8 clients. Socket code lives in `src/editor/core/net/socket_win32.cpp`
  / `socket_posix.cpp`, selected by CMake behind `net/socket.h` (no `#ifdef` elsewhere).
- The token (D23). A TCP client's first request must be `session.hello {token}` or it receives `UNAUTHORISED`
  and is disconnected; no event reaches it before. The token is generated when the TCP transport starts
  (`protocol/access_token.h`) and written to `<project>/.conjure/ai_token` before `ai_port`, readable by the
  user running the editor only (`core::vfs::write_private_file`, the one per-OS piece, in the file system layer
  beside the other native file calls). `--ai-token <t>` replaces the generated token and gates stdio and the
  loopback as well; without it those two present nothing. `protocol_server::requires_token(kind)` is the rule,
  `access_of(presented)` the comparison (`constant_time_equals`; `contains_token` and the audit log search
  with `constant_time_contains`), `accept_hello(clientId, presented)` what the
  `session.hello` handler asks. No project, no files: the port is then unusable until `project.open`, or from
  the start with `--ai-token`. Every client should send `session.hello` first regardless.
- The first line (D24). A client whose first line does not parse as a request is answered (`PARSE_ERROR` or
  `INVALID_REQUEST`) and disconnected, on every transport; the line is not logged. After a first request a
  malformed line is answered and the client stays.
- The second token (D25). `ai_token_inspect` is written beside `ai_token` and grants `access_level::INSPECT`;
  `--ai-inspect-token <t>` replaces it and gates every transport. `access_of` tells the two apart,
  `accept_hello` records what the last accepted hello granted, `access_of_client(id)` reports it (full control
  for the editor's own, in-process ids), and the dispatcher compares it with `required_access(flags)`.
- The audit log (D26). `protocol_server::record_command(entry)` appends the requests of transport clients to
  `<project>/.conjure/ai_audit.log` through `audit_log` (`protocol/audit_log.h`), which keeps tokens out and
  the files bounded; `flush()` flushes it once per frame, `set_project_root` moves it, `stop()` closes it.
- `stdio_transport`: one `core::jobs::dedicated_thread("editor-stdin")` blocks on `std::getline` and
  pushes lines into a mutex-guarded `std::deque<std::string>`; the main thread drains it in `poll()` and
  writes with `std::fwrite` + `fflush`. With `--ai-stdio`, logging goes to stderr and file, never stdout.
- `loopback_transport`: in-process pair for tests and the UI's AI session panel.

### 3.13 Play-mode persist

Data (`conjure/editor/persist_list.h`, owned by `play_controller`, main thread only):

```cpp
enum class persist_scope : std::uint8_t {
    FIELD,          // one reflection path inside one component ("transform.position", "script.scripts[0].fields.mines")
    COMPONENT,      // one whole component
    ENTITY,         // the whole entity: every component, its name, enabled state and parent; children only when `children`
};

struct persist_entry {
    core::uuid entity;                   // scene guid (D5)
    persist_scope scope = persist_scope::FIELD;
    std::string component;               // registry name; empty for ENTITY
    std::string path;                    // reflection path inside the component; FIELD only
    bool children = false;               // ENTITY only: include the subtree
    std::uint32_t clientId = 0;          // who pinned it (0 = the UI)
    std::uint64_t pinnedAtFrame = 0;
};
```

Rules:

- Pinning is allowed only while `PLAYING` or `PAUSED` (`WRONG_PLAY_STATE` otherwise). The entity must be alive
  (`ENTITY_NOT_FOUND`); a FIELD path must resolve (`PATH_NOT_FOUND`); a COMPONENT must be present
  (`COMPONENT_NOT_FOUND`).
- An entity created during play (its guid is not in the snapshot) can only be pinned with ENTITY scope; a FIELD or
  COMPONENT pin on it is promoted to ENTITY and the reply says so (`promoted: true`). Without that the value would
  have no entity to land on after the restore.
- Entries are deduplicated: an ENTITY pin subsumes FIELD and COMPONENT pins on the same entity, a COMPONENT pin
  subsumes FIELD pins inside it. Unpinning the wider entry does not resurrect the narrower ones.
- Pins are UI state, like selection (D12): not recorded by the undo stack, announced by the `persist.changed` event.
- Values are **not** captured when pinned. They are read at Stop, so a designer can pin first and keep tuning. A
  value the game keeps rewriting (a timer, a moving body) keeps whatever it held at the moment of Stop; the Persist
  panel shows the live value so this is visible before stopping.

`persist_list::capture(scene)` runs at Stop **before** the restore. For each entry, in the order ENTITY, COMPONENT,
FIELD: resolve the guid; skip with reason `ENTITY_DESTROYED` when it is gone; read the value through the same
reflection JSON the inspector uses (`component_to_json`, `reflection_path::read_at`; for ENTITY,
`scene_serializer::save_subtree(entity)` limited to the entity itself unless `children`). Output: a vector of
ready-to-dispatch `command_request`s plus the skipped list. Nothing in capture mutates the scene.

`persist_list::apply(captured)` runs after the restore and the undo unfreeze, as one atomic `editor.batch`
labelled `Persist from play (N)`, built only from existing commands: an ENTITY entry whose guid is in the
snapshot becomes `entity.destroy` followed by `entity.restore {archive, parent, index}` with the captured archive
(same guid, same parent and sibling index); one created during play becomes `entity.restore` alone (original
parent when it still exists, else the scene root); COMPONENT entries become `component.set` of the whole component, or `component.add` when the snapshot entity lacks
it; FIELD entries become `component.set {path}`. Validation happens in capture, so the batch is all-or-nothing per
D4 over the entries that survived; skipped entries are reported, never silently dropped. On a prefab instance the
applied values become ordinary prefab overrides through the scene module's normal diff. The reply to `play.stop`
and the `persist.applied` event carry `{applied, skipped:[{entity, component, path, reason}], undoLabel}`.

Change markers (the inspector's "changed in play" dot): for the selected entities only, the inspector compares the
component JSON it already reads each frame with the same component in the snapshot. `play_controller` exposes
`snapshot_component_json(guid, component)`, decoded from the snapshot on demand and cached per selected entity for
the play session (`scene::scene_snapshot` gains a by-guid component read; owed to the scene module). The hierarchy
marks only entities touched by editor commands during play and entities with pins; it never diffs the whole scene.

Budget: capture plus apply for 100 pinned fields under 2 ms; pinning costs no per-frame work.

### 3.14 Workspace layout

On the first frame of a project with no `<project>/.conjure/editor_layout.ini`, `editor_workspace::build_default
_layout()` splits the dockspace with `ImGui::DockBuilder*`:

```
+------------------------------------------------------------------------------------------+
| title bar (OS)                                                                           |
| menu bar: File  Edit  Assets  Entity  Play  Window  Help                                 |
| toolbar (fixed, not a dock node): tools | pivot | snap | play controls | persist | undo  |
+-------------+------------------------------------------------------+---------------------+
| Hierarchy   | Scene | Game   (tabs)                                | Inspector |         |
| 262 px      | viewport, its own overlay toolbar                    | Project settings    |
|             |                                                      | 340 px              |
+-------------+------------------------------------------------------+---------------------+
| Assets | Persist | Profiler | History        | Console | AI session                     |
| 236 px, split 59 / 41                                                                    |
+------------------------------------------------------------------------------------------+
| status bar (fixed): state | scene + unsaved | persist count | AI port | build, GPU | fps    |
+------------------------------------------------------------------------------------------+
```

Sizes are at 100 % UI scale and scale with it. `io.IniFilename` points at the project's layout file (today it is
null, code sweep 2026-11-12); Window > Reset layout deletes the file's dock state and rebuilds the default; every
closed panel can be reopened from the Window menu. Started without `--project`, the editor shows a start screen
(recent projects from the user configuration folder, Open, New) instead of an empty workspace.

The window is cleared to the theme's `panel` colour before the UI pass every frame (the 2026-11-12 sweep found the
back buffer never cleared, which smeared every moved panel).

### 3.15 Themes

A theme file (`<name>.conjure-theme.json`, UTF-8, schema-validated like command arguments):

```json
{
  "schema_version": 1,
  "name": "Deep Ocean",
  "base": "dark",
  "colours": {
    "panel": "#0b1a24", "head": "#10283a", "sunken": "#071219", "tabbar": "#06141d", "menu": "#06141d",
    "line": "#163447", "line2": "#040c12", "control": "#0f2636", "controlLine": "#1e4258",
    "text": "#c4e3ef", "strong": "#e8f7fc", "muted": "#7fa9bc", "faint": "#5a7f90",
    "accent": "#2fd0e0", "accentSoft": "#0e4b5c", "accentText": "#e3fcff",
    "play": "#ff9a6b", "playSoft": "#4a2618", "playText": "#ffc2a3",
    "persist": "#a6f06a", "persistSoft": "#21401a",
    "prefab": "#5cc8ff", "script": "#b39cff", "warning": "#ffb347", "error": "#ff6b6b"
  },
  "chrome": {
    "title":   {"linear": {"angle": 90, "stops": [["#03101a", 0], ["#062638", 0.5], ["#03101a", 1]]}},
    "toolbar": {"linear": {"angle": 180, "stops": [["#0d2533", 0], ["#0a1d29", 1]]}},
    "status":  {"linear": {"angle": 90, "stops": [["#063b4f", 0], ["#0c6f86", 0.45], ["#14a3b3", 0.75], ["#0b5a6e", 1]]},
                "text": "#eafcff"},
    "viewportBackdrop": {"radial": {"centre": [0.5, 0.3], "stops": [["#10403a", 0], ["#0a2232", 0.55], ["#06131c", 1]]}},
    "grid": "#78dcff10", "gridMajor": "#78dcff21"
  },
  "fonts": {"ui": "Inter", "mono": "JetBrains Mono", "size": 13},
  "metrics": {"rounding": 5, "density": "comfortable"}
}
```

Rules:

- **Roles, not widgets.** Colours name meanings (`accent`, `play`, `persist`, `prefab`), and the editor maps them
  onto every `ImGuiCol_*` and onto its own drawing (gizmo outlines, play-mode border, persist pins, status bar).
  A theme cannot restyle one widget; that keeps every theme complete and every new widget themed for free.
- **Gradients only on chrome**: the title area, toolbar, status bar and the viewport backdrop. Panels, fields,
  lists and text backgrounds are flat so values stay readable. Linear gradients take 2 to 4 stops, radial 2 to 3;
  they are drawn with `ImDrawList` multi-colour rectangles, one strip per stop pair.
- **The scene is never themed.** `viewportBackdrop` is used only where the scene draws nothing: an editing view of a
  scene with no environment and no camera clear colour. The 2D grid and gizmo outline colours come from the theme;
  game pixels do not.
- **Validation** on load and save, reported per rule, a failing theme is refused (built-ins must pass in CI):
  `text`, `strong` and `muted` reach 4.5:1 contrast on `panel`, `head` and `sunken`; `faint` reaches 3:1;
  `accentText` on `accentSoft`, `playText` on `playSoft`, `chrome.status.text` on every status stop reach 4.5:1;
  `accent`, `play` and `persist` differ from each other by at least 20 in CIEDE2000 **and** in lightness by at
  least 8, so meaning never rests on hue alone. Missing roles fall back to the `base` theme's value.
- **Fonts**: Inter (UI) and JetBrains Mono (numbers, logs, paths) ship with the editor under the SIL Open Font
  Licence 1.1, with the licence text added to the third-party notices (when the fonts are added: a
  `conjure_third_party_notice()` with `SCOPE EDITOR` beside their pin, or with the OFL text under
  `third_party/licenses/` if nothing is fetched; 06 D15). A theme may name any font the font cache can load; unknown
  fonts fall back with one warning.

Built-in themes, compiled into `conjure_editor_ui` as embedded JSON: **Midnight** (default), **Autumn**, **Deep
Ocean**, **Aurora**, **Synthwave**, **High contrast**. A light **Daylight** theme is owed before 1.0 for bright rooms
and projectors.

Customisation (the Theme editor, `ui/panels/theme_editor_panel`, opened from Edit > Preferences > Themes):
duplicate any theme, edit roles with colour pickers and gradient-stop editors while the whole editor previews
live, see validation results inline, then Save. User themes are written to `<user config>/themes/`; Import and
Export copy a single theme file so themes can be shared. Built-ins are read-only; editing one duplicates it. The
user configuration folder is `%APPDATA%/Conjure` on Windows, `$XDG_CONFIG_HOME/conjure` (default
`~/.config/conjure`) on Linux and `~/Library/Application Support/Conjure` on macOS; `platform` owes the
`user_config_directory()` that resolves it. The active theme and the recent-projects list are stored in
`<user config>/editor_settings.json`. Theme files are watched: saving one from an external text editor re-applies it.

## 4. Public API

All headers under `engine/include/conjure/editor/`. None includes ImGui, ImGuizmo, sockets or stb.

### 4.1 Header list - `conjure_editor_core`

| Header | Main types / functions |
|---|---|
| `log_category.h` | `CONJURE_LOG_CATEGORY_DECLARE(editor)` |
| `editor_options.h` | `struct editor_options`, `parse_editor_options()` |
| `editor_error.h` | `enum class protocol_error_code`, `to_string()`, `parse_error_code()`, `make_editor_error()` |
| `editor_command.h` | `command_flags`, `command_request`, `command_inverse`, `command_outcome`, `command_invocation`, `command_handler`, `command_descriptor`, `async_reply` |
| `command_registry.h` | `class command_registry` |
| `schema_validator.h` | `validate_args(schemaJson, args) -> result<void>`, `compile_schema()` |
| `command_dispatcher.h` | `class command_dispatcher` |
| `undo_stack.h` | `struct undo_entry`, `class undo_stack`, `class transaction_scope` (RAII begin/end) |
| `entity_ref.h` | `entity_ref::resolve()`, `entity_ref::to_json(guid)` |
| `reflection_path.h` | `struct path_segment`, `class reflection_path`, `struct field_ref`, `read_at()`, `write_at()` |
| `selection_set.h` | `class selection_set` |
| `play_controller.h` | `enum class play_state`, `class play_controller` |
| `editor_camera.h` | `class editor_camera` (orbit/fly/focus, `screen_ray`, view/projection) |
| `picker.h` | `struct pick_candidate`, `struct pick_result`, `enum class pick_mode`, `pick()`, `gather_pick_candidates()` |
| `screenshot_service.h` | `struct screenshot_request`, `class screenshot_service` |
| `console_buffer.h` | `struct console_entry`, `class console_buffer` |
| `event_broadcaster.h` | `enum class editor_event`, `class event_broadcaster` |
| `editor_context.h` | `class editor_context` |
| `project_trust.h` | `enum class trust_state` (`UNTRUSTED`, `SESSION`, `REMEMBERED`), `struct trusted_project`, `class project_trust` (`project_key()`, `state_of()`, `trust()`, `revoke()`, `remembered()`), `default_trust_store_file()` (D27) |
| `persist_list.h` | `enum class persist_scope`, `struct persist_entry`, `struct persist_capture`, `class persist_list` (3.13) |
| `theme.h` | `struct theme_colours`, `struct theme_gradient`, `struct theme`, `parse_theme()`, `validate_theme()`, `builtin_themes()` (3.15; no ImGui, so the protocol and tests use it) |
| `user_settings.h` | `struct editor_user_settings` (active theme, recent projects), load/save in the user configuration folder |
| `protocol/protocol_message.h` | `protocol_request`, `protocol_response`, `protocol_event`, `parse_request()`, `serialise()` |
| `protocol/line_splitter.h` | `class line_splitter` |
| `protocol/protocol_transport.h` | `enum class transport_kind`, `struct inbound_line`, `class protocol_transport` |
| `protocol/loopback_transport.h` | `class loopback_transport` (tests, AI session panel) |
| `protocol/protocol_server.h` | `struct client_state`, `class protocol_server` |
| `protocol/access_token.h` | `enum class access_level` (`NONE`, `INSPECT`, `FULL_CONTROL`), `generate_access_token()`, `constant_time_equals()`, `constant_time_starts_at()`, `constant_time_contains()`, `token_file_name()` (D23, D25) |
| `protocol/audit_log.h` | `struct audit_entry`, `class audit_log` (D26) |
| `builtin_commands.h` | `register_builtin_commands(command_registry &registry)` |
| `panel_desc.h`, `command_desc.h` | SDK-installed module extension records (R14): `panel_desc { static constexpr core::string_id EXTENSION_POINT{"editor.panel"}; const char *title; panel_create_fn create; }`, `command_desc { EXTENSION_POINT{"editor.command"}; const char *name, *summary, *argsSchema, *resultSchema; command_flags flags; command_handler_fn handler; }`; both trivially copyable, no editor internals |
| `editor_module_consumer.h` | `class editor_module_consumer : public modules::module_consumer` (turns the records into `command_descriptor`s with `owner = module name` and `panel_registry` entries on attach, removes them on detach, vetoes `can_detach` with `UNLOAD_BLOCKED` during play; R14) |
| `headless_runner.h` | `struct exec_summary`, `class headless_runner` |
| `editor_application.h` | `class editor_application`, `using layer_factory` |

Private (`engine/src/editor/core/`): `net/socket.h` + `socket_win32.cpp` / `socket_posix.cpp`,
`user_paths.{h,cpp}` + `user_paths_windows.cpp` / `user_paths_posix.cpp` (the per-user state folder, D27),
`protocol/tcp_transport.{h,cpp}`, `protocol/stdio_transport.{h,cpp}`, `commands/commands_{session,
editor,history,project,scene,entity,component,asset,prefab,script,play,input,view,physics,selection,
console,reflection,modules,profiler}.cpp`, `snapshot_guard.{h,cpp}`, `README.md`.

### 4.2 Header list - `conjure_editor_ui` (`engine/include/conjure/editor/ui/`)

| Header | Main types |
|---|---|
| `editor_layer.h` | `class editor_layer : public engine::layer`; `create_editor_layer(editor_context &context)` |
| `editor_panel.h` | `class editor_panel` (title, draw, open flag), `struct panel_descriptor`, `class panel_registry` |
| `editor_workspace.h` | `class editor_workspace` (dockspace, `build_default_layout()`, layout load/save, menu bar, status bar, start screen) |
| `theme_applier.h` | `apply_theme(const theme &)` onto ImGui style and the editor's own draw colours; `draw_gradient(...)` |
| `property_drawer.h` | `class property_drawer_registry`, `using draw_fn` (reflection field -> ImGui widget; the float widgets read and write the `"inf"` / `"-inf"` / `null` spellings of 10_core.md C15, so an infinite or NaN value is shown as `inf`, `-inf` or `nan` and survives an edit of a neighbouring component) |
| `hierarchy_cache.h` | `struct hierarchy_row`, `class hierarchy_cache` |
| `gizmo_controller.h` | `enum class gizmo_operation`, `class gizmo_controller` |
| `panels/hierarchy_panel.h`, `inspector_panel.h`, `asset_browser_panel.h`, `viewport_panel.h`, `console_panel.h`, `profiler_panel.h`, `toolbar_panel.h`, `history_panel.h`, `ai_session_panel.h`, `persist_panel.h`, `theme_editor_panel.h`, `export_dialog.h` | one `editor_panel` subclass each |

ImGui/ImGuizmo includes appear only in `engine/src/editor/ui/**/*.cpp`.

### 4.3 Key signatures

```cpp
namespace conjure::editor {

    class command_registry {
        private:
            std::vector<command_descriptor> mDescriptors;
            core::flat_hash_map<std::string, std::uint32_t> mIndexByName;
            std::uint64_t mSchemaHash = 0;

        public:
            command_registry();
            ~command_registry();

        public:
            /// <summary>
            /// Registers a command. Fails with INVALID_ARGS when the name is taken or the schema does
            /// not parse. Main thread only.
            /// </summary>
            [[nodiscard]] core::result<void> add(command_descriptor descriptor);

            /// <summary>
            /// Removes every command whose owner matches; used by editor_module_consumer::detach (module.unload).
            /// </summary>
            std::size_t remove_owned_by(std::string_view owner);

            [[nodiscard]] const command_descriptor *find(std::string_view name) const noexcept;
            [[nodiscard]] std::span<const command_descriptor> descriptors() const noexcept;
            [[nodiscard]] core::json::document describe_all(bool includeUiOnly) const;
            [[nodiscard]] std::uint64_t schema_hash() const noexcept;
    };

    class async_reply {
        private:
            command_dispatcher *mDispatcher = nullptr;
            std::uint64_t mTicket = 0;

        public:
            async_reply();
            ~async_reply();

        public:
            /// <summary>
            /// Completes the parked request. Main thread only; a second call is a programmer error.
            /// </summary>
            void succeed(core::json::document result);
            void fail(protocol_error_code code, std::string message, core::json::document details = {});
            [[nodiscard]] bool valid() const noexcept;
    };

    class command_dispatcher {
        private:
            editor_context &mContext;
            command_registry &mRegistry;
            undo_stack &mUndo;
            core::slot_map<pending_request> mPending;
            std::uint32_t mMaxPendingPerClient = 8;
            std::chrono::seconds mAsyncTimeout{120};

        public:
            command_dispatcher(editor_context &context, command_registry &registry, undo_stack &undo);
            ~command_dispatcher();

        public:
            /// <summary>
            /// Validates and executes one request on the main thread. Returns the response, or an
            /// empty optional when the command is asynchronous and will answer through
            /// resolve_completed() later.
            /// </summary>
            [[nodiscard]] std::optional<protocol_response> dispatch(const protocol_request &request);

            /// <summary>
            /// Convenience for UI code: dispatches a command_request from client 0 and returns the
            /// outcome without protocol framing.
            /// </summary>
            [[nodiscard]] core::result<core::json::document> execute(const command_request &request);

            void resolve_completed(std::vector<protocol_response> &outResponses);
            void cancel_client(std::uint32_t clientId);
            [[nodiscard]] std::size_t pending_count(std::uint32_t clientId) const noexcept;
    };

    class undo_stack {
        public:
            [[nodiscard]] core::result<void> begin_transaction(std::string_view label, std::uint32_t clientId);
            [[nodiscard]] core::result<void> end_transaction(std::uint32_t clientId);
            void record(command_request replay, command_inverse inverse, std::string_view label);
            [[nodiscard]] std::optional<undo_entry> pop_for_undo();
            [[nodiscard]] std::optional<undo_entry> pop_for_redo();
            void set_frozen(bool frozen) noexcept;
            [[nodiscard]] bool frozen() const noexcept;
            [[nodiscard]] bool can_undo() const noexcept;
            [[nodiscard]] bool can_redo() const noexcept;
            void set_byte_budget(std::size_t bytes) noexcept;
            [[nodiscard]] std::span<const undo_entry> entries() const noexcept;
            [[nodiscard]] std::size_t cursor() const noexcept;
            void clear();
    };

    class protocol_transport {
        public:
            virtual ~protocol_transport();

        public:
            [[nodiscard]] virtual core::result<void> start() = 0;
            virtual void stop() = 0;
            virtual void poll(std::vector<inbound_line> &outLines, std::vector<std::uint32_t> &outClosed) = 0;
            virtual void send(std::uint32_t clientId, std::string_view line) = 0;
            virtual void disconnect(std::uint32_t clientId) = 0;
            virtual void flush() = 0;
            [[nodiscard]] virtual transport_kind kind() const noexcept = 0;
    };

    class protocol_server {
        public:
            [[nodiscard]] core::result<void> start(const editor_options &options);
            [[nodiscard]] core::result<void> add_transport(std::unique_ptr<protocol_transport> transport);
            void poll(std::vector<protocol_request> &outRequests);
            void send(const protocol_response &response);
            void broadcast(const protocol_event &event);
            void flush();
            void stop();
            [[nodiscard]] std::uint16_t bound_port() const noexcept;
            [[nodiscard]] std::size_t client_count() const noexcept;
    };

    class play_controller {
        public:
            [[nodiscard]] core::result<void> start();
            [[nodiscard]] core::result<void> stop();
            [[nodiscard]] core::result<void> pause();
            [[nodiscard]] core::result<void> resume();
            [[nodiscard]] core::result<void> step(std::uint32_t fixedSteps, async_reply reply);
            void tick(const engine::frame_time &frameTime);
            void note_unrecorded_edit();                 // a scene edit while playing: one warning per session
            void keep_file_edit(std::string_view command, command_request replay, command_inverse inverse);   // D21
            [[nodiscard]] std::size_t kept_file_edits() const noexcept;
            [[nodiscard]] play_state state() const noexcept;
            [[nodiscard]] std::uint64_t simulation_frame() const noexcept;
    };

    [[nodiscard]] pick_result pick(const math::ray &ray, std::span<const pick_candidate> candidates, pick_mode mode);
    [[nodiscard]] std::vector<pick_candidate> gather_pick_candidates(const scene::scene &scene);

    class headless_runner {
        public:
            [[nodiscard]] static core::result<std::vector<protocol_request>> load_command_file(
              const std::filesystem::path &file
            );
            [[nodiscard]] core::result<exec_summary> run(
              editor_application &application,
              const editor_options &options,
              std::span<const protocol_request> requests
            );
    };

    class editor_application {
        public:
            using layer_factory = std::function<std::unique_ptr<engine::layer>(editor_context &context)>;

        public:
            [[nodiscard]] core::result<void> initialise(const editor_options &options, layer_factory uiFactory);
            [[nodiscard]] int run();
            void headless_tick();
            void request_quit(int exitCode);
            [[nodiscard]] std::optional<core::json::document> startup_scene_warning() const;   // D22
            [[nodiscard]] editor_context &context() noexcept;
    };

    [[nodiscard]] core::result<editor_options> parse_editor_options(std::span<const std::string_view> args);
    void register_builtin_commands(command_registry &registry);
}
```

A command implementation is a free function in `engine/src/editor/core/commands/*.cpp`:

```cpp
static core::result<command_outcome> entity_rename(command_invocation &invocation) {
    scene::scene &scene = invocation.context.scene();
    const core::result<ecs::entity> entity = entity_ref::resolve(invocation.args["entity"], scene);
    if (!entity) {
        return entity.error();
    }

    // Everything is resolved; the single mutation below cannot fail.
    const std::string previousName = std::string(scene.name_of(*entity));
    const std::string newName = std::string(invocation.args["name"].as_string());
    scene.set_name(*entity, newName);

    const std::string guid = scene.guid_of(*entity).to_string();
    command_outcome outcome = {};
    outcome.label = std::format("Rename '{}'", previousName);
    outcome.result = core::json::object({{"guid", guid}});
    outcome.inverse = command_inverse{"entity.rename", core::json::object({{"entity", guid}, {"name", previousName}})};
    return outcome;
}
```

### 4.4 Command catalogue

Flags: M mutates, A async, P requires project, S requires scene, E editing only, D on disk (D21: kept while
playing, undoable after `play.stop`). The flag `read_only` (D25: open to inspect-only clients) has no column
here; it is carried by `session.*`, `editor.schema`, `editor.state`, `editor.wait_frames`, `project.info`,
`project.list_scenes`, `project.trust_state`, `scene.info`, `scene.get_hierarchy`, `scene.query`, `entity.get`, `entity.find`,
`entity.bounds`, `component.get`, `component.list`, `selection.get`, `asset.info`, `asset.list`, `script.list`,
`script.read`, `reflection.*`, `physics.raycast`, `physics.overlap_sphere`, `view.get_camera`, `view.mode`,
`view.pick`, `play.state`, `history.list`, `module.list`, `console.read` and `profiler.frame_stats`, and by no
other command. Required args in **bold**. Every mutating result includes `revision`.
`entity` args accept a guid or a hierarchy path.

| Command | Flags | Args -> result |
|---|---|---|
| `session.hello` | | `{token?, clientName?}` -> `{protocolVersion:1, engineVersion, schemaHash, clientId, headless, access}` (`token` is required on TCP, and on every transport under `--ai-token` or `--ai-inspect-token`, D23; `UNAUTHORISED` otherwise; `access` is `full_control` or `inspect`, what the presented token grants, D25) |
| `session.subscribe` | | `{events:[names], minLogLevel?}` -> `{subscribed}` |
| `session.ping` | | -> `{frame, revision}` |
| `editor.schema` | | `{command?}` -> whole catalogue or one descriptor |
| `editor.state` | | -> `{project, scene, playState, revision, selection, headless, offscreen, port, clients, frame, trustPrompt}` (`trustPrompt`: the path of the project the window asks about, or `null`, D27) |
| `editor.batch` | M | `{commands:[{cmd,args}], label?, atomic?}` -> `{results:[response]}` |
| `editor.begin_transaction` / `editor.end_transaction` | | `{label}` / `{}` |
| `editor.wait_frames` | A | `{frames?=1}` (at least 1) -> `{frame}` |
| `editor.exec_file` | A | `{path, keepGoing?}` -> `exec_summary` (a relative `path` is relative to the project root, D24) |
| `editor.quit` | | `{exitCode?, saveChanges?}` (`saveChanges` while playing stops the play session first: `scene.save` is editing-only, D21; when that save then fails the answer is the save's error and the editor does not quit: it stays in the editing state, the stop is not taken back) |
| `history.undo` / `history.redo` | | `{count?}` -> `{applied, labels}` |
| `history.list` / `history.clear` | | `{limit?}` -> `{entries:[{label, clientId}], cursor}` |
| `console.read` / `console.clear` | | `{sinceSeq?, minLevel?, category?, limit?}` -> `{entries:[{seq, frame, level, category, message}]}` |
| `project.create` | M | `{path, name}` -> `{path, trust}` (D19: no `template`; `trust` is `session` or `remembered` for a project made in a folder that did not exist, `untrusted` for one made in an existing folder, empty or not, D27) |
| `project.open` | | `{path}` -> `project.info` (`PATH_NOT_FOUND` for a missing directory or `project.json`, `IO_ERROR` for one that cannot be read and for a `path` that is a file, not a directory; `WRONG_PLAY_STATE` while playing; the engine restarts at the end of the frame, `play.start` answers `BUSY` until then, and the restart stops any play session first; `PROJECT_NOT_TRUSTED` with `details {path, state, prompt}` for a project nobody answered for, in which case nothing is scheduled and, in a window, the person at it is asked, D27) |
| `project.trust` | | `{path, remember?=true}` -> `{path, state, trusted, remembered}` (D27: the explicit answer; `state` is `remembered` when the trust store took it, `session` otherwise; the path must hold a project, with the errors of `project.open`; not in a batch; needs no open project) |
| `project.revoke_trust` | | `{path}` -> `{path, state, trusted, revoked}` (takes the answer back for the session and in the store; the folder need not exist; `IO_ERROR` when the store names the project and cannot be rewritten; the open project stays open; not in a batch) |
| `project.trust_state` | | `{path?}` -> `{path, state, trusted}` (`untrusted`, `session` or `remembered`; without `path` the open project, `INVALID_ARGS` when none is open; reads no folder and asks nobody) |
| `project.save` / `project.info` / `project.list_scenes` | M P / P / P | |
| `project.set_settings` | M P D | `{settings}` (startupScene, window title/size, ...) |
| `project.export` | A P | `{directory, distributionDirectory?, copyRuntime?, buildNativeModule?, publishManaged?}` -> `{directory, runtimePath, bytes, durationMs, assetCount, steps, warnings}` (always a Distribution export, D19: no `config`; relative directories are relative to the project root and the answer names the absolute one, D24; a `distributionDirectory` the runtime is to be copied from answers `PATH_NOT_FOUND` when it is not there and `IO_ERROR` when it is not a folder, before anything is cooked, D20) |
| `scene.new` | M P E | `{name, path?}` -> `{path}` |
| `scene.open` / `scene.save` / `scene.save_as` / `scene.close` | E / M E / M E / E | `{path}` / `{}` / `{path}` / `{discard?}` (`scene.open`: `PATH_NOT_FOUND` when no file is at the path, `IO_ERROR` when the file cannot be loaded; the saves are editing-only since D21: the playing scene is not the one to keep) |
| `scene.info` | S | -> `{path, entityCount, structureVersion, dirty}` |
| `scene.query` | S | `{with?:[types], without?:[types], fields?:[paths], limit?, offset?}` -> `{rows:[{guid, name, fields}], total}` |
| `scene.get_hierarchy` | S | `{root?, depth?}` -> nested `{guid, name, children}` |
| `scene.set_settings` | M S | `{settings}` (ambient, skybox HDRI, gravity, physics layers) |
| `entity.create` | M S | `{name?, parent?, guid?, components?:{type:json}}` -> `{guid}` (`guid` is a uuid string; omitted = generated; `INVALID_ARGS` (`args.guid: <uuid> is already in use`) when taken) |
| `entity.create_many` | M S | `{count, nameFormat?, parent?, components?, overrides?:[json], guids?:[uuid]}` -> `{guids}` (`guids` must hold `count` uuids when given) |
| `entity.destroy` / `entity.destroy_many` | M S | `{entity}` / `{entities}` (subtrees) |
| `entity.restore` | M S | `{archive, parent?, index?}` -> `{guid, entities}`: the inverse of `entity.destroy`, reinstates a captured subtree with its guids (undo payload, also accepted from clients) |
| `entity.duplicate` | M S | `{entity, count?, offset?}` -> `{guids}` |
| `entity.rename` / `entity.set_parent` / `entity.set_enabled` | M S | `{entity, name}` / `{entity, parent, keepWorld?, index?}` / `{entity, enabled}` |
| `entity.set_transform` | M S | `{entity, position?, rotation?, scale?, space?:"local"\|"world"}` |
| `entity.get` | S | `{entity, components?:[types]}` -> `{guid, name, parent, children, enabled, components:{type:json}}` |
| `entity.find` | S | `{name?, tag?, layer?, component?, limit?}` -> `{guids}` |
| `entity.bounds` | S | `{entity}` -> `{min, max}` |
| `component.add` / `component.remove` | M S | `{entity, type, value?}` / `{entity, type}` |
| `component.set` | M S | `{entity, path, value}` -> `{previous}` (`path` = type name replaces whole component; a qualified path `<type>.<field>.<subfield>` or `<type>.<array>[i]` sets one field; `<type>` is the registered name `conjure.scene.local_transform`, one of the aliases `editor.schema` lists (`transform`, `name`, `script`, `sprite_renderer`, `camera`, ...) or a unique registered-name suffix) |
| `component.set_many` | M S | `{sets:[{entity, path, value}]}` |
| `component.get` / `component.list` | S | `{entity, path}` -> `{value}` / `{entity}` -> `{types}` |
| `asset.import` | M P D | `{source, destination?, options?}` -> `{uuid, path, type, subAssets:[{uuid, name, type}]}` (`source`: an asset path, or a file on disk, a relative one under the project root, D24; synchronous in v1: `asset_database::add_file` + `import`; database mutations are main-thread only, R35) |
| `asset.list` / `asset.info` / `asset.create_folder` / `asset.reimport` | P (`asset.create_folder`: M P D) | uuid-or-path based (`asset.reimport`: `PATH_NOT_FOUND` when the asset's source file vanished, `IO_ERROR` when it cannot be read, checked before the importer is switched, D20) |
| `asset.delete` | M P D | `{asset, closeOpenScene?}` -> `{uuid, path, closedScene, startupScene}` (an empty folder: `{path, folder}`). The file behind the open scene needs `closeOpenScene: true`; the startup scene raises `warning {code:"STARTUP_SCENE_DELETED"}` (D20) |
| `asset.restore` | M P D | `{path, contentBase64, metaBase64?}` -> `{uuid, path}`: the inverse of `asset.delete`, writes the source (and its `.meta`) back from base64 and imports it (undo payload) |
| `asset.create_material` | M P D | `{path, baseColor?, metallic?, roughness?, emissive?, textures?:{albedo, normal, metallicRoughness, occlusion, emissive}}` -> `{uuid}` |
| `asset.set` | M P D | `{asset, path, value}` (reflection path on any asset type) |
| `prefab.create` | M P S E | `{entity, path}` -> `{uuid}` (editing-only since D21: its inverse unpacks the entity, which `play.stop` would have thrown away) |
| `prefab.instantiate` | M S | `{prefab, parent?, name?, transform?, count?}` -> `{guids}` |
| `prefab.apply` / `prefab.unpack` | M P S D / M S | `{entity}` (`prefab.apply` while playing replays as an `asset.restore` of the bytes it wrote, D21) |
| `script.create` | M P D | `{name, template?:"Behaviour"\|"Empty", namespace?}` -> `{path}` |
| `script.delete` | M P D | `{path, project?}` -> `{path, projectRemoved}`: deletes a script source under `<project>/scripts/` (undo writes it back); `project: true` also removes the project file when no source is left |
| `script.read` / `script.write` / `script.list` | P (write M P D) | `{path}` / `{path, source}` (inside `<project>/scripts/`) / -> `{scripts:[{className, path, fields}]}` |
| `script.compile` | P A | `{}` -> `{succeeded, diagnostics:[{file, line, column, severity, code, message}], durationMs}` (`scripting::managed_build_runner`; `ConjureManagedDir` is `application::managed_directory()`, the absolute SDK directory the script host uses) |
| `script.reload` / `script.gc` | P / P | `{assembly?}` (`hot_reload_controller::request(immediate = true)`, applied before the next frame's first script phase; a relative `assembly` is relative to the project root, D24) / `{}` -> `{zombies}` (`collect_garbage`); names per R15 |
| `script.attach` | M S | `{entity, className, fields?}` (sugar for `component.add script`) |
| `play.start` / `play.stop` / `play.pause` / `play.resume` | | -> `{state, frame, simulationFrame, simulationTime, fixedStepIndex}` (the play session's clocks, 3.7); `play.stop` adds `fileChanges`, the `ON_DISK` commands of the ended session, now one undo entry (D21) |
| `play.step` | A | `{fixedSteps?}` -> same as `play.state` |
| `play.state` | | -> `{state, frame, simulationFrame, simulationTime, fixedStepIndex}` (the play session's clocks, 3.7) |
| `play.persist` | S | `{**entity**, component?, path?, children?}` (no component = ENTITY scope; component without path = COMPONENT) -> `{entries, promoted}`; PLAYING or PAUSED only |
| `play.unpersist` | S | `{entity?, component?, path?, all?}` -> `{entries}` |
| `play.persisted` | | -> `{entries:[{entity, name, scope, component, path, children, before, current, clientId}]}` (`before` from the snapshot, `current` live) |
| `editor.theme.list` / `editor.theme.get` | | -> `{themes:[{name, builtIn, valid}], active}` / `{name}` -> theme document |
| `editor.theme.set` | | `{**name**}` -> `{active}` (per-user setting, not undoable; `NOT_AVAILABLE` headless) |
| `editor.theme.save` / `editor.theme.delete` | | `{**theme**}` -> `{name, validation}` / `{**name**}` (user themes only; built-ins `INVALID_ARGS`) |
| `editor.layout.reset` | | -> `{}` (rebuilds the default layout, 3.14; `NOT_AVAILABLE` headless) |
| `input.inject` / `input.clear` | | `{key?, button?, axis?, value?, action:"press"\|"release"\|"tap", holdFrames?}` |
| `view.screenshot` | A | `{path, width?, height?, camera?, format?:"png"\|"jxl", waitFrames?}` -> `{path, width, height, frame, format}` (a relative `path` is relative to the project root, and the answer names the absolute one, D24) |
| `view.set_camera` / `view.get_camera` / `view.focus` | | `{position, target, fov?}` / -> same / `{entity}` |
| `view.pick` | S | `{x?, y?}` (normalised viewport) or `{origin, direction}`, `mode?` -> `pick_result` |
| `view.set_mode` / `view.mode` / `view.debug_draw` | | `{mode:"3d"\|"2d"}` / -> `{mode}` / `{physics?, bounds?, grid?}` |
| `physics.raycast` / `physics.overlap_sphere` | S | `{origin, direction, maxDistance?, layers?}` / `{center, radius}` |
| `selection.get` / `selection.set` / `selection.clear` | | `{entities, mode?:"replace"\|"add"\|"remove"}` |
| `reflection.components` | | -> `{components:[{name, module, fields:[{name, type, default, attributes}]}]}` |
| `reflection.type` / `reflection.asset_types` | | `{name}` -> field tree / -> `{types:[{name, extensions, importOptionsSchema}]}` |
| `module.list` / `module.discover` | / | -> `{modules:[{name, version, state, lastError, providedBy}]}` / `{directory}` (relative to the project root when not absolute, D24) -> `{added}` |
| `module.load` / `module.unload` / `module.reload` | M / M / M | `{name}` / `{name, policy?:"refuse_if_in_use"\|"remove_instances"}` / `{name, scope?:"full"\|"managed_only"}`; each only enqueues `module_manager::request_*`, applied at the next frame start (async completion through the `module.state` event; the editor auto-saves the scene and stops play before a reload; R14) |
| `module.build` | P A | `{name}` -> `{succeeded, diagnostics}` (runs the project's CMake build for the editor's config through `core::process`, then `request_reload`) |
| `profiler.frame_stats` | | `{frames?}` -> `{cpuMs, gpuMs, systems:[{name, ms}], drawCalls, entityCount, simulationTime, fixedStepIndex}` (the last two are the engine's clocks since the engine started (`project.open` restarts them), not the play session's, 3.7) |

Events: `log`, `selection.changed`, `play.state`, `scene.changed {revision, label}`, `scene.loaded {path}`,
`progress {operation, fraction, message}` (export, imports, module builds), `script.compiled`, `script.reloaded`,
`asset.imported`, `module.state {name, state, error?}`, `command.executed {clientId, cmd, ok}`,
`client.connected`, `client.disconnected`, `events.dropped {count}`, `warning {code, message}` (R15; codes
`UNRECORDED_PLAY_EDIT`, `PLAY_FILE_EDIT` with `cmd` (D21), `STARTUP_SCENE_DELETED` with `path` and
`STARTUP_SCENE_FAILED`, the first line of an `--exec` output whose engine could not load the startup scene (D22)),
`persist.changed {entries}`, `persist.applied {applied, skipped, undoLabel}`, `theme.changed {active}`.

Grid-game walk-through (proves completeness; the generic minesweeper-style Grid Sweeper of 17_demo_games.md 2.1): `session.hello` -> `project.create` -> `asset.import`
(cell sprite atlas PNG, UI font) -> `asset.create_material` x3 -> `scene.new` -> `editor.batch`
[`entity.create_many` cells with `overrides` positions from the grid, `entity.create` orthographic
camera] -> `prefab.create` cell/mine -> `prefab.instantiate {count}` ->
`script.create` x3 -> `script.write` -> `script.compile` (fix loop on diagnostics) -> `script.attach`
+ `component.set script.fields.speed` -> `component.add rigid_body` / `character_controller` ->
`play.start` -> `input.inject` -> `play.step` -> `entity.get` positions -> `view.screenshot` ->
`play.stop` -> `scene.save` -> `project.set_settings {startupScene}` -> `project.export`.

## 5. Threading and ownership

- **Main thread** executes everything in `conjure::editor` except: the stdin reader thread (touches
  only its own mutex-guarded deque), the `console_buffer` sink (mutex-guarded ring, called from any
  logging thread), and jobs started by async handlers (PNG encode, script compile, import, export).
  Async jobs capture by value (guids, paths, bytes) and never touch `editor_context`; completion is
  delivered through `core::jobs::post_to_main_thread()` and consumed in `resolve_completed()`.
- **Ownership chain**: `editor_application` -> `std::unique_ptr<engine::application>`,
  `editor_context`, `protocol_server`, `project_trust` (D27; the context holds a non-owning pointer to it, and
  it outlives every engine restart), optional `headless_runner`. `editor_context` owns
  `command_registry`, `command_dispatcher`, `undo_stack`, `selection_set`, `play_controller`,
  `event_broadcaster`, `editor_camera`, `console_buffer`, `screenshot_service`,
  `editor_module_consumer`; it holds non-owning references set once in `initialise()` to
  `engine::application&`, `engine::project&`, `scene::scene_manager&`, `assets::asset_database&`,
  `core::reflect::type_registry&`, and optional pointers `renderer::renderer*`,
  `physics::physics_world*`, `scripting::script_runtime*`, `modules::module_manager*` that are null when the
  build option is off or the backend is `null` (all taken from `engine::engine_context`, R22). Handlers check
  them and return `NOT_AVAILABLE`.
- Transports own their sockets through RAII `socket_handle`. Panels own only UI state. Commands own
  nothing between calls. No raw owning pointers anywhere.
- `editor_layer` (ui) is pushed onto the engine layer stack and holds a reference to `editor_context`;
  the single ImGui context is owned by `engine::imgui_layer` (R9), drawn by the renderer's ImGui backend
  (area B) and used by the ui library and by module panels through the one context exported from the shared
  engine library (no per-module `SetCurrentContext`, `07_modules.md` D1).

## 6. Failure handling

| Situation | Behaviour |
|---|---|
| Malformed JSON line | `PARSE_ERROR` response with `id:null`; client kept, unless it is the client's first line |
| A client's first line is not a request (not JSON, or JSON without `cmd`) | `PARSE_ERROR` / `INVALID_REQUEST`, then the client is disconnected; its later lines are not read (D24) |
| A first request that is not `session.hello` with the token, on a transport that requires it | `UNAUTHORISED` (the message names `.conjure/ai_token`, never a token), then the client is disconnected (D23) |
| A later `session.hello` with a wrong token | `UNAUTHORISED`; the client keeps the access it had |
| A later `session.hello` with the other token | accepted; the client's access becomes what that token grants (D25) |
| A command without `read_only` from a client that holds inspect access | `UNAUTHORISED` with `details {required, granted}`, before the arguments are validated; nothing runs, the client stays, the request is in the audit log (D25, D26) |
| The token file cannot be written (FAT or exFAT volume, something in the way) | WARN without the token; no `ai_token`; `ai_port` is still written; TCP clients need `--ai-token` (D23). The same, with its own WARN, for `ai_token_inspect` and `--ai-inspect-token` (D25) |
| The audit log cannot be created or written (a folder at its path, a full disk) | one WARN; no records until the project changes; every command runs as usual (D26) |
| The audit log's full file cannot be moved to `ai_audit.1.log` (a folder has the name, a program holds a file open) | the full file is emptied in place: the size bound is kept, its records are not (D26) |
| Line > 16 MiB (`protocol::MAX_MESSAGE_BYTES`) or invalid UTF-8 | final `MESSAGE_TOO_LARGE` / `IO_ERROR` response with `id:null`, then the client is disconnected (R15) |
| Missing `cmd` / non-object `args` | `INVALID_REQUEST` |
| Unknown command | `UNKNOWN_COMMAND` with `details.didYouMean` |
| Schema violation | `INVALID_ARGS`; message names the offending path |
| Precondition | `PROJECT_NOT_OPEN`, `SCENE_NOT_OPEN`, `WRONG_PLAY_STATE` (details: current state) |
| Handler failure | response with its code; scene unchanged (D4) |
| `editor.quit {saveChanges: true}` while playing whose `scene.save` fails | the save's error; the editor does not quit and is left in the editing state (the play session is over, its file edits are the undo entry) |
| A file or folder a command needs is missing / exists but cannot be read | `PATH_NOT_FOUND` / `IO_ERROR` (D20); `ASSET_NOT_FOUND` only for an asset reference |
| `asset.delete` of the open scene without `closeOpenScene` | `INVALID_ARGS`; nothing closed, nothing deleted (D20) |
| `asset.delete {closeOpenScene: true}` whose file cannot be removed | the delete's own error (`IO_ERROR`); the scene stays open with its unsaved changes and its undo history, because the file is deleted before the scene is closed (D20) |
| Child of atomic batch fails | inverses rolled back, `details.failedIndex`, undo entry discarded |
| `prefab.create`, `scene.save` or `scene.save_as` while playing or paused | `WRONG_PLAY_STATE` (details: current state); nothing written (D21) |
| Async never completes | `TIMEOUT` after 120 s (`--timeout-seconds`), slot freed |
| Client disconnects | open transaction committed; pending async replies dropped (`CANCELLED` logged) |
| 9th pending async for a client | `BUSY` |
| Undo of an inverse fails | `INTERNAL`, `CONJURE_ASSERT` in Debug, entry removed, revision bumped |
| Snapshot restore fails | ERROR log, empty scene, state `EDITING`, never a crash; the session's file edits still become their undo entry (D21) |
| Port busy | fallback ports (3.12); explicit port busy: UI continues without server, headless exits 2 |
| Write backlog | `log` events dropped above 8 MiB (reported), disconnect above 64 MiB |
| `--exec` file missing / invalid | exit 3, before the engine starts (D22); a bad flag is reported first (exit 3), a missing or invalid project too (exit 2) |
| `--project` names a project nobody answered for | headless, `--exec` or `--export`: `PROJECT_NOT_TRUSTED`, exit 2, no engine started, after the checks of the row above; a window: the editor starts without the project and asks (D27) |
| `project.open` of a project nobody answered for | `PROJECT_NOT_TRUSTED` with `details {path, state, prompt}`; nothing scheduled, the open project untouched; in a window the question is raised (D27) |
| The answer for a project is revoked between `project.open` and the frame boundary | the restart is dropped with a WARN before the running engine is touched; the client's `pending: true` stays unanswered by a `scene.loaded` (D27) |
| The trust store cannot be used (another account could have written it, not JSON, a newer schema, over 4 MiB) | one WARN per read; it counts as empty, so its projects are asked about again; the next remembered answer replaces it (D27) |
| The trust store cannot be written (no per-user folder, a volume without owner-only files, something in the way) | `project.trust` succeeds with `state: "session"` and one WARN; `project.revoke_trust` of a project the store names answers `IO_ERROR` (D27) |
| The startup scene does not load | the editor starts over an empty world; an `--exec` output opens with `warning {code:"STARTUP_SCENE_FAILED"}` and the exit code is unchanged (D22) |
| `dotnet` SDK or scripting off | `script.*` -> `NOT_AVAILABLE` with an actionable message |

## 7. Headless and null behaviour

- `--headless`: `null` platform/RHI/audio; no workspace or panels are created (the layer factory is
  unused). `view.screenshot`, `view.debug_draw`, `profiler.frame_stats.gpuMs` -> `NOT_AVAILABLE`;
  `view.pick`, `view.set_camera`, `view.focus` work (editor camera lives in core). All else is identical.
- `--offscreen`: `null` platform + real RHI device without a swapchain; `view.screenshot` works. CI
  without a GPU uses `--headless` only.
- A headless session has nobody to ask, so `--project` on a project nobody answered for fails before the engine
  starts (`PROJECT_NOT_TRUSTED`, exit 2) unless `--trust-project` is given or the trust store names the project;
  a session started without a project opens one with `project.trust` followed by `project.open` (D27).
- `platform::input::inject(event)` works on the `null` backend exactly as on `glfw`, so `input.inject` drives
  headless play (R30).
- Unit tests build an `editor_context` over a null-backend `engine::application` and talk to the
  dispatcher directly or through `loopback_transport`.

## 8. Integration points

**Needs**

| Module | Need |
|---|---|
| `core` | `result/error` with error codes; `json::document/value` (yyjson wrapper) with object/array builders, deep copy, pointer-style patching; `reflect::type_registry` enumerating fields (name, type, default, attributes `range/tooltip/hidden`) with `to_json/from_json` per type; `log::add_sink(std::shared_ptr<log_sink>)` / `log::remove_sink`, `log_level`; `jobs::dedicated_thread(name, fn)` for blocking I/O, `jobs::post_to_main_thread`, `parallel_for`; `uuid`; `slot_map`, `flat_hash_map`, `bit_vector` (names per R30); `engine::frame_stats` for the profiler |
| `ecs` | `component_registry` bridging reflection: stable type names, `types()`, `ecs::component_to_json/component_from_json` (R40), `has_raw/add_raw/remove_raw`; query iteration for `scene.query`; `entity_archive` for snapshots and duplicate |
| `scene` | per-entity guid component + `guid_index::find(uuid)`; `structure_version()`; hierarchy with sibling index and `keepWorld` reparent; `create_entities(count, template)`; `scene_serializer::{save,load}_scene`, `save_subtree(entity) -> json`, `instantiate_subtree(json, parent, keepGuids)`, `capture(scene) -> scene_snapshot`, `restore(scene, snapshot)` (guids preserved, 10k entities < 50 ms); world AABB per entity; name/tag/layer queries; prefab create/instantiate/apply/unpack |
| `assets` | `asset_database::add_file` + `import(ids, jobs)` (synchronous, R35), `create_asset`, `move_asset`, `delete_asset`, `write_meta`, `records()`, `find`, `find_by_path`; `asset_manager::reload`, `last_error`, `load_blocking` (permitted inside a frame, R23); material and prefab asset types registered with reflection; `asset_manager::subscribe` events; thumbnails via jobs |
| `renderer` | `create_render_target` + synchronous `read_back` for screenshots (R12); `mesh_asset::cpu_geometry()` (kept in Debug/Development, R11); ImGui NVRHI backend (`imgui_renderer`, `register_texture` for the viewport image); debug draw; per-entity render bounds; viewport render target for the editor camera |
| `physics` | `raycast`, `overlap_sphere`; the fixed loop does not run while the engine is PAUSED (EDITING), so no separate "simulation disabled" flag is needed (R13) |
| `scripting` | `managed_build_runner::build(csproj, config, onComplete)` with `build_diagnostic` structs (R15); `hot_reload_controller::request`, `script_runtime::on_reloaded`, `set_mode`; templates copied by the editor from `managed/Conjure.Templates`; script fields reflected as `script.fields` |
| `engine` | `application(application_desc)` + `initialise()`, `run()`, `step_frame()`, layer stack, `set_simulation_mode`, `step_fixed(n)`, `engine_time`, `frame_stats` (R13); `project` load/save; `export_pipeline` (R21); `engine_config::headless_defaults()`; `engine_context` service pointers (R22) |
| `platform` | `platform::input::inject(event)` on `null` and `glfw` backends (R30); clipboard |
| `modules` | `modules::module_consumer` interface, `module_manager::add_consumer` (before `load_all`), `request_load/unload/reload`, `registry()`, module events (R14); `editor_module_consumer` removes a module's commands/panels on detach |

**Offers**: `command_registry` (module-prefixed commands), `panel_registry` / `property_drawer_registry`
for module tooling; `headless_runner` as a CI tool; `.claude/skills/editor-ai-control` generated from
`editor.schema`; `conjure_editor` CLI (`--project`, `--headless`, `--exec`, `--out`, `--keep-going`,
`--offscreen`, `--ai-port`, `--ai-stdio`, `--ai-token`, `--ai-inspect-token`, `--timeout-seconds`, `--export`,
`--distribution`, `--trust-project`, `--trust-store`, `--no-scripting`,
`--dotnet`: a headless session keeps the real .NET script host instead of the null host, for play tests that
run C#).

**Distribution stripping**: the editor is never part of a Distribution build. Root CMake forces
`CONJURE_BUILD_EDITOR OFF` in a single-config Distribution tree; a multi-config generator (the `windows-vs`
preset) builds every configuration from one set of targets, so `cmake/ConjureConfigs.cmake` offers its
Distribution configuration only when `CONJURE_BUILD_EDITOR` is OFF (with the editor on, the tree has `Debug` and
`Development`). `engine/` and `editor/` CMake guard on the option; only `conjure_editor` links the editor
libraries; `tools/check_distribution_symbols.py` fails CI if the Distribution `conjure_runtime` references
`conjure::editor` or `ImGui`; `grep -r CONJURE_EDITOR engine/` must be empty.

## 9. Implementation plan

1. CMake: `engine/src/editor/CMakeLists.txt` with both modules, `editor/CMakeLists.txt`, `tests/editor`
   via `conjure_add_test_module(editor)`; `log_category.h`; `README.md` skeleton.
2. `editor_error.h`, `protocol/protocol_message.{h,cpp}`, `protocol/line_splitter`, golden tests.
3. `editor_command.h`, `schema_validator`, `command_registry` (+ `describe_all`, `schema_hash`).
4. `undo_stack` (transactions, coalescing, budget, frozen) with tests.
5. `editor_context` over null-backend `engine::application`; `event_broadcaster`; `console_buffer`.
6. `command_dispatcher` (sync, async, pending table, timeout, cancel) + `history.*`, `session.*`,
   `editor.*` commands (batch, transactions, wait_frames).
7. `entity_ref`, `reflection_path` (+ `read_at/write_at`), `reflection.*` commands.
8. `entity.*`, `component.*` commands with inverses; test "inverse restores serialised scene exactly".
9. `scene.*`, `project.*`, `selection.*`, `console.*` commands; `selection_set`.
10. `asset.*`, `prefab.*` commands (async import through jobs).
11. `play_controller` + `play.*`, `input.*`; snapshot guard and `warning` event.
12. `editor_camera`, `picker`, `view.pick`, `view.*camera*`, `physics.*`.
13. `screenshot_service` + `view.screenshot` (fake renderer in tests).
14. `script.*` commands against a fake `script_host`; real compile in `tests/integration`.
15. Transports: `loopback`, `stdio`, `tcp` (`net/socket_*`), `protocol_server`, token, port fallback,
    backlog policy.
16. `headless_runner`, `editor_options`, `editor_application`, `editor/main.cpp`; `--export` path.
17. `editor_module_consumer`, `panel_desc.h` / `command_desc.h`, `module.*` commands (R14).
18. UI: `editor_layer`, `editor_workspace` (dockspace, ini layout), `panel_registry`, toolbar, console,
    history, hierarchy (`hierarchy_cache` + clipper), inspector (`property_drawer_registry`), asset
    browser, viewport (`gizmo_controller`, picking), profiler, AI session panel, export dialog.
19. `tests/benchmarks/bench_editor.cpp`; `docs/generated/editor_protocol.json` dump test;
    `.claude/skills/editor-ai-control` generated from it; `tools/check_distribution_symbols.py`.
20. `persist_list` + `play.persist` / `play.unpersist` / `play.persisted`; capture and apply wired into
    `play.stop`; scene module by-guid snapshot component read; Persist panel, inspector pins and markers, toolbar
    and status bar counts (3.13).
21. Default layout, back-buffer clear, Window menu, start screen, layout file per project (3.14).
22. `theme` parse/validate, built-in theme JSON, `theme_applier`, bundled fonts and their licence notices,
    `platform::user_config_directory()`, `editor_user_settings`, theme commands, Theme editor panel, file watch
    (3.15).

## 10. Tests (`tests/editor/`, target `conjure_editor_tests`, all null backends)

- `test_protocol_message.cpp`: `parse_request_int_string_missing_id`, `parse_rejects_missing_cmd`,
  `parse_rejects_non_object_args`, `serialise_response_golden`, `serialise_event_golden`,
  `error_code_round_trip`.
- `test_line_splitter.cpp`: `partial_line_across_feeds`, `many_lines_one_feed`, `crlf_tolerated`,
  `overflow_flag_at_16mib`.
- `test_schema_validator.cpp`: `required_keys`, `type_mismatch_names_path`, `enum`, `ranges`,
  `nested_objects`, `array_items`, `additional_properties_rejected`, `unsupported_keyword_fails_compile`.
- `test_command_registry.cpp`: `duplicate_name_rejected`, `bad_schema_rejected`, `remove_owned_by`,
  `describe_all_hides_ui_only`, `schema_hash_stable_and_sensitive`.
- `test_undo_stack.cpp`: `record_undo_redo_order`, `new_record_clears_redo`, `transaction_groups`,
  `per_client_transaction_isolation`, `coalesce_same_key_keeps_first_undo`, `no_coalesce_across_names`,
  `byte_budget_evicts_oldest`, `depth_cap`, `frozen_does_not_record`.
- `test_command_dispatcher.cpp`: `unknown_command_did_you_mean`, `invalid_args_before_handler`,
  `requires_scene_without_scene`, `editing_only_while_playing`, `mutates_records_and_bumps_revision`,
  `async_defers_then_resolves`, `ninth_async_is_busy`, `async_timeout`, `cancel_client_drops_pending`,
  `command_executed_event_emitted`.
- `test_batch_command.cpp`: `single_undo_entry`, `atomic_rollback_on_third_child`, `non_atomic_continues`,
  `async_child_rejected`, `nested_batch_rejected`.
- `test_entity_ref.cpp`: `guid`, `path`, `ambiguous_path_lists_candidates`, `stale_guid_not_found`.
- `test_reflection_path.cpp`: `parse_valid`, `parse_rejects_empty_double_dot_bad_index_trailing_dot`,
  `resolve_nested_struct`, `resolve_array_index`, `missing_field_names_segment`, `read_write_round_trip`,
  `enum_by_name_and_int`, `to_string_round_trip`.
- `test_commands_entity.cpp`: `create_assigns_guid_undo_destroys`, `create_with_explicit_guid_redo_stable`,
  `destroy_subtree_undo_restores_children_components_sibling_index`, `rename`, `set_parent_keep_world`,
  `duplicate_undo`, `create_many_10k_with_overrides`, `inverse_restores_serialised_scene_exactly`.
- `test_commands_component.cpp`: `add_remove_set_undo`, `set_many_single_entry`, `set_whole_component`,
  `path_not_found`, `type_mismatch_invalid_args`, `duplicate_add_invalid_args`, `script_fields_path`.
- `test_commands_scene_project.cpp`: `new_save_open_round_trip`, `query_with_without_fields`,
  `hierarchy_depth_limit`, `project_create_open_info`, `set_settings_undo`.
- `test_commands_asset_prefab.cpp`: `create_material_set_undo`, `import_async_reports_sub_assets`,
  `prefab_create_instantiate_apply_unpack_undo`.
- `test_commands_script.cpp`: `create_from_template_writes_file`, `write_outside_scripts_rejected`,
  `compile_failed_payload_shape` (fake host), `attach_is_component_add`.
- `test_play_controller.cpp`: `file_edits_during_play_one_undo_entry_at_stop` (D21),
  `start_snapshots_and_freezes`, `edits_during_play_discarded_with_warning`,
  `stop_restores_10k_entities_exactly`, `selection_remapped_by_guid`, `step_runs_exactly_n_fixed_updates`,
  `illegal_transitions_wrong_play_state`, `restore_failure_leaves_editing_state`.
- `test_persist_list.cpp`: `pin_requires_play`, `field_component_entity_scopes`, `narrow_pins_subsumed_by_wider`,
  `created_in_play_promoted_to_entity`, `value_read_at_stop_not_at_pin`, `stop_applies_one_undo_entry`,
  `undo_after_stop_restores_snapshot_values`, `destroyed_entity_skipped_and_reported`,
  `list_cleared_on_start_and_after_stop`, `persist_does_not_survive_into_next_play`,
  `prefab_instance_value_becomes_override`, `script_field_path_persists`, `entity_children_flag`,
  `pins_from_protocol_client_recorded_with_client_id`, `restore_failure_drops_captured_values`.
- `test_theme.cpp`: `builtins_parse_and_validate`, `contrast_rule_refuses_low_contrast_text`,
  `accent_play_persist_must_differ_in_lightness`, `gradient_stop_counts`, `missing_roles_fall_back_to_base`,
  `user_theme_save_load_round_trip`, `builtin_is_read_only`, `unknown_font_falls_back_with_warning`.
- `test_selection_set.cpp`: `set_add_remove_clear`, `duplicates_ignored`, `changed_event_once`.
- `test_picker.cpp`: `nearest_aabb_wins`, `miss`, `tie_smaller_volume`, `ray_inside_box`,
  `triangle_narrow_phase_beats_bounds`, `sprite_quad`, `physics_mode_delegates`.
- `test_editor_camera.cpp`: `screen_ray_centre_matches_forward`, `focus_frames_bounds`.
- `test_screenshot_service.cpp`: `null_rhi_not_available`, `fake_capture_writes_png_and_replies`,
  `jxl_decodes_to_the_png_pixels`, `format_follows_extension_unless_named`, `unknown_format_refused`.
- `test_console_buffer.cpp`: `ring_wraps`, `read_since_seq`, `min_level_filter`, `thread_safe_sink`.
- `test_event_broadcaster.cpp`: `subscription_filter`, `log_min_level`, `seq_monotonic`,
  `default_subscriptions_exclude_log`.
- `test_loopback_transport.cpp`, `test_stdio_transport.cpp` (injected streams, clean shutdown),
  `test_tcp_transport.cpp`: `two_requests_one_packet`, `request_across_two_packets`,
  `oversized_line_disconnects`, `ephemeral_port_reported`, `token_required`, `backlog_disconnect`.
- `test_protocol_server.cpp`: `events_dropped_counting`, `per_frame_request_cap`, `port_fallback_file`; D23 over
  a real socket: `generated_token_written_private_beside_the_port`, `refused_without_the_token` (no hello, no
  token, an empty, a wrong and an over-long one), `let_in_with_the_token_from_the_file`,
  `generated_token_gates_tcp_alone`, `set_token_rewrites_the_file`, `token_file_that_cannot_be_written`; D24:
  `first_line_not_a_request_disconnects` (an HTTP request with a JSON body among them) on the loopback, TCP and
  stdio, `malformed_line_after_a_first_request_keeps_the_client`. `test_access_token.cpp`:
  `generated_tokens_are_64_hex_digits_and_differ`, `constant_time_equals_every_kind_of_difference`.
  `test_ai_protocol_e2e.cpp`: `tcp_client_reads_port_and_token_and_drives_the_editor` (no log line or event
  carries the token).
- D25: `test_protocol_server.cpp`: `inspect_token_in_its_own_private_file`, `tcp_client_let_in_with_inspect_access`,
  `access_of_tells_the_tokens_apart`, `one_text_cannot_be_both_tokens`, `last_accepted_hello_decides`,
  `ungated_client_can_restrict_itself`, `explicit_token_of_one_level_gates_every_transport`.
  `test_command_dispatcher.cpp`: `inspect_client_refused_before_arguments_are_looked_at`,
  `inspect_client_runs_read_only_commands`, `in_process_clients_have_full_control`.
  `test_command_catalogue.cpp`: `read_only_list_is_pinned`, `inspect_client_refused_every_other_command`, and the
  fixture runner checks that a `read_only` command leaves the snapshot and the shared state as they were.
  `test_command_registry.cpp`: `read_only_refused_with_mutates`. `test_ai_protocol_e2e.cpp`:
  `inspect_only_tcp_client_looks_and_is_refused_every_change`.
- D26: `test_audit_log.cpp`: `format_line_fields`, `token_member_withheld`, `token_kept_out_of_every_text`,
  `line_is_bounded`, `files_stay_within_twice_the_limit`, `oversized_files_removed_on_open`, `file_is_private`,
  `symbolic_link_replaced`, `folder_in_the_way_reported_once`. `test_protocol_server.cpp`:
  `transport_clients_recorded_with_transport_and_access`. `test_command_dispatcher.cpp`:
  `every_request_of_a_transport_client_reaches_the_audit_log` (refused, unknown, asynchronous and batch children
  included; the editor's own requests not).
- D27: `test_project_trust.cpp`: `project_key_one_key_per_folder`, `symbolic_link_is_its_folder`,
  `session_answer_writes_nothing`, `remembered_answer_in_owner_only_store`, `revoke_session_and_store`,
  `store_another_account_may_have_written_not_believed`, `unusable_store_counts_as_empty`,
  `store_keeps_the_newest_answers`, `answer_the_store_cannot_take_holds_for_the_session`,
  `posix_user_state_directory_rule` (run on every host). `test_editor_application.cpp`:
  `no_engine_over_an_unanswered_project` (headless, `--exec`, `--export`), `trust_project_flag_remembers_nothing`,
  `remembered_answer_opens_a_later_run_until_revoked`, `request_open_project_refuses`,
  `revoked_before_the_frame_boundary_is_not_opened`, `can_ask_about_trust`, `windowed_session_starts_without_an_unanswered_project` (`plan_startup`, no window needed). `test_commands_project_trust.cpp`:
  `open_refused_then_trust_then_open`, `trust_session_or_remembered_with_origin`, `trust_needs_a_project`,
  `revoke_leaves_the_open_project_open`, `trust_state`, `create_trusts_only_a_new_or_empty_folder`,
  `not_in_a_batch`, `inspect_client_may_ask_and_not_answer`, `window_is_asked_and_trust_settles_it`.
  `test_editor_context.cpp`: `trust_prompt_raise_close_dismiss`. `ui/test_editor_workspace.cpp`:
  `trust_and_open_sends_trust_then_open`. `test_editor_process.cpp`: `headless_run_exits_2_for_an_unanswered
  _project`, `project_trust_is_remembered_across_runs`. The start of a windowed session without its untrusted
  project needs a real window and belongs to the real-window tier below.
- `test_headless_runner.cpp`: `array_and_ndjson_inputs`, `stop_on_first_error_exit_1`, `keep_going`,
  `output_file_contains_events`, `missing_project_exit_2`, `bad_args_exit_3`, `async_pumped_to_completion`,
  `load_command_file_needs_no_engine`, `startup_scene_failure_is_first_line` (D22; `test_editor_application.cpp`
  holds `unusable_exec_file_before_engine_start` and the repair run, `test_editor_process.cpp` the exit codes).
- `test_editor_options.cpp`, `test_hierarchy_cache.cpp` (`rebuild_only_on_version_change`,
  `filter_keeps_ancestors`, `10k_rows_flat`), `test_reflection_commands.cpp`
  (`every_component_listed_with_fields`, `every_command_has_schema`, `schema_dump_matches_generated_file`).
- `tests/integration/test_ai_builds_grid_game.cpp`: feeds `samples/games/grid_sweeper/commands/build_grid_sweeper.json`
  through `headless_runner`; asserts entity count, components, prefab instances, saved scene, and
  compiled script when `dotnet` is present (otherwise `NOT_AVAILABLE` is the asserted outcome).
- Real-window tier (label `window`, never null backends; the gap the 2026-11-12 sweep named): the windowed editor
  starts, draws 120 frames, resizes, minimises, restores and exits 0 with no debug-layer message; the back buffer
  is cleared; the default layout puts each panel inside its dock rectangle; each built-in theme renders its
  reference screenshot within tolerance; a scripted input run pins a gizmo drag during play, stops, and finds the
  value in the scene and one entry on the undo stack; the windowed editor started with `--project` on a project
  nobody answered for comes up without a project and with the trust question (D27).
- `tests/benchmarks/bench_editor.cpp`: `persist_capture_apply_100`, `create_many_10k`, `snapshot_capture_restore_10k`,
  `hierarchy_rebuild_10k`, `pick_broad_phase_10k`, `dispatch_10k_component_set`.

## 11. Out of scope / future

- Remote (non-localhost) protocol exposure and TLS; the token handshake is the only hook today.
- Multi-scene additive editing in one window; the editor edits the active scene only.
- Per-field JSON diffs for very large components (whole-component before/after is used).
- GPU id-buffer picking for skinned/instanced meshes; collaborative multi-user undo.
- Visual scripting, animation timeline, terrain/particle editors, module marketplace.
- Persist lists that survive between plays or are saved: excluded on purpose (D16); each play starts clean.
- Per-project themes, a theme marketplace, and themes that restyle individual widgets.
- Named, switchable layouts beyond the default and the saved one per project.
