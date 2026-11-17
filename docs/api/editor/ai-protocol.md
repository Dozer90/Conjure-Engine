# The AI control protocol

📘 **Public API wiki** · 🛠 Handbook: [Editor module](../../handbook/modules/editor.md)

How an agent, a script or a test drives the editor: newline-delimited JSON over TCP, stdio or the in-process
loopback, and a command file for `--exec`. The wire types are `conjure/editor/protocol/protocol_message.h`, the
server `conjure/editor/protocol/protocol_server.h`, the transports `conjure/editor/protocol/protocol_transport.h`
and `conjure/editor/protocol/loopback_transport.h`, the framing `conjure/editor/protocol/line_splitter.h`, the
tokens `conjure/editor/protocol/access_token.h`, the audit log `conjure/editor/protocol/audit_log.h`, the error
codes `conjure/editor/editor_error.h`, the runner `conjure/editor/headless_runner.h`. The agent-facing
walkthrough with the whole catalogue is the skill `.claude/skills/editor-ai-control/SKILL.md`; the committed
`editor.schema` dump is `docs/generated/editor_protocol.json`.

## At a glance

| You want to... | Do |
|---|---|
| Start a server on the default port (47820, nine fallbacks) | `conjure_editor --project <dir>` (the UI) or `--headless --ai-port 47820` (headless, add `--trust-project` unless the project was trusted before); the bound port is written to `<project>/.conjure/ai_port` and the two tokens to `ai_token` and `ai_token_inspect` beside it; all three move to the new project's folder when `project.open` switches projects |
| Connect over TCP | read `<project>/.conjure/ai_port` and `ai_token`, connect to `127.0.0.1:<port>`, send `session.hello {"token": "<token>"}` first (see [Access tokens](access-and-audit.md#who-may-connect-and-what-a-client-may-do)) |
| Let a program look without letting it change anything | give it `ai_token_inspect` instead: it runs the commands flagged `read_only` and is refused the rest (see [Two tokens](access-and-audit.md#two-tokens-full-control-and-inspect-only)) |
| See what clients did | `<project>/.conjure/ai_audit.log`, one JSON line per command (see [The audit log](access-and-audit.md#the-audit-log)) |
| Open a project the editor did not create | `project.trust {"path": "<dir>"}` once (the answer is remembered per user on this machine, outside the project), then `project.open`; `--trust-project` answers for one run. Until then `project.open` answers `PROJECT_NOT_TRUSTED` and a headless `--project` exits with 2: opening runs the project's scripts and native modules, and that is never implied (see [Project trust](headless-and-commands.md#project-trust)) |
| Talk over standard input and output | `conjure_editor --project <dir> --trust-project --headless --ai-stdio` (requests on stdin, responses and events on stdout, logs on stderr); without `--ai-port` the editor exits with code 0 once standard input ends |
| Run a prepared command file and exit | `conjure_editor --project <dir> --trust-project --headless --exec commands.json [--out replies.ndjson] [--keep-going]` (without `--out` standard output carries only the NDJSON and the log goes to stderr) |
| Choose a token yourself | `--ai-token <t>` replaces the generated full-control token, `--ai-inspect-token <t>` the inspect-only one; either makes stdio and loopback clients present a token too. A command line is visible to other users of the machine, so prefer the generated tokens there |
| Discover commands, components, assets | `editor.schema`, `reflection.components`, `reflection.type`, `reflection.asset_types`, `editor.state` |
| Address an entity | by guid string, by hierarchy path `"Board/Cell_3"`, or by unique name |
| Address a field | `transform.position.x`, `script.scripts[0].fields.mine`, `camera.cameraData.projection` |
| Connect from a C++ test without sockets | `loopback_transport` added through `protocol_server::add_transport`, `loopback_client` on the other end |

## The wire

One request per line, one response per line, events between them, UTF-8, at most `MAX_MESSAGE_BYTES` (16 MiB)
per line; a longer line answers `MESSAGE_TOO_LARGE` and the client is disconnected. `id` is an integer, a string
or null and is echoed verbatim; `args` is an object (`{}` when absent). Text that is not valid UTF-8 (a script
saved in a legacy code page, a path in one) reaches the client with U+FFFD in place of each bad byte, so every
line still parses.

```
-> {"id":12,"cmd":"entity.create","args":{"name":"Player","components":{"transform":{"position":[1,0,1]}}}}
<- {"id":12,"ok":true,"result":{"guid":"a9e2c0d4-...","name":"Player","revision":42}}
<- {"id":13,"ok":false,"error":{"code":"ENTITY_NOT_FOUND","message":"No entity 'Maze/Wall_99'","details":{}}}
<- {"event":"selection.changed","seq":913,"frame":8812,"data":{"guids":["a9e2c0d4-..."],"primary":"a9e2c0d4-..."}}
```

- The first request of a TCP or stdio client is `session.hello {clientName?, token?}`; it answers
  `protocolVersion`, `engineVersion`, `schemaHash`, `clientId`, `headless` and `access` (`"full_control"` or
  `"inspect"`: what the client may run). A TCP client must send a token with it.
- The first line of a connection must be a request. A client that opens with anything else (text that is not
  JSON, JSON without `cmd`, an empty line) gets `PARSE_ERROR` or `INVALID_REQUEST` and is disconnected. After
  a first request a malformed line is answered and the connection stays.
- The server takes up to 8 TCP clients at once; a further client receives one `BUSY` line with id null and is
  closed.
- A new client receives every event except `log` and `command.executed`; `session.subscribe {"events":[...],
  "minLogLevel":"warn"}` changes that. Under backlog pressure log events are shed and `events.dropped` says so.
- Asynchronous commands (`asset.import`, `script.compile`, `module.build`, `view.screenshot`, `play.step`,
  `editor.wait_frames`, `editor.exec_file`, `project.export`) answer when they finish, frames later; at most
  eight pending per client (`BUSY`), each timing out after `--timeout-seconds` (`TIMEOUT`, default 120 s). A
  `project.open` restarts the engine and answers every pending request with `CANCELLED`.
- Every mutating result carries `revision`; `scene.changed {revision, label}` follows it.
- `selection.changed {guids, primary}` follows every selection change (also a change of the primary alone, and
  `play.stop`'s restore); `primary` is null when nothing is selected, as in `selection.get`. In the same way
  `entity.set_parent` to the root answers `parent` null, as `entity.get` does for a root.

| Error code | When |
|---|---|
| `PARSE_ERROR`, `INVALID_REQUEST`, `MESSAGE_TOO_LARGE` | the line is not JSON, not a request, or too long; `id` is null, except that `INVALID_REQUEST` echoes an integer or string `id` the object carried. A line that is too long disconnects the client, and so do the first two when the line is the client's first |
| `UNKNOWN_COMMAND` (`details.didYouMean`, empty for a name over 128 bytes; the message quotes at most 128 bytes of it), `INVALID_ARGS` (names `args.<path>`) | the request |
| `ENTITY_NOT_FOUND`, `COMPONENT_NOT_FOUND`, `TYPE_NOT_FOUND`, `ASSET_NOT_FOUND` | a reference did not resolve; `ASSET_NOT_FOUND` is an asset reference (uuid or asset path) that names no asset in the database |
| `PATH_NOT_FOUND` | a reflection path does not resolve, or a file or folder a command needs does not exist: the `asset.import` source, the `scene.open` file, the `project.open` directory or its `project.json`, the `editor.exec_file` file, a script, the `module.discover` directory, the `distributionDirectory` of a `project.export` that copies the runtime, the source file of a registered asset (`asset.set`, `asset.delete`, `asset.reimport`, `prefab.apply`) |
| `PROJECT_NOT_OPEN`, `SCENE_NOT_OPEN`, `WRONG_PLAY_STATE` (`details.state`), `PROJECT_NOT_TRUSTED` (`details {path, state, prompt}`) | a precondition; the last one is `project.open` of a project nobody answered for on this machine: send `project.trust` first (`prompt` says whether the editor window asks its user as well) |
| `NOT_AVAILABLE`, `IO_ERROR`, `COMPILE_FAILED` (`details.diagnostics`) | the environment: a null backend, a file, a build. `IO_ERROR` is a file or folder that exists but cannot be read (a directory where a file is expected or a file where a folder is, a denied access, a scene file that does not load) or a write that failed, never a missing one (`script.reload` answers `PATH_NOT_FOUND` when no assembly was built yet and `IO_ERROR` when the runtime cannot load the one that exists) |
| `BUSY`, `TIMEOUT`, `CANCELLED` | the asynchronous queue (also `BUSY`: a second `script.compile` while one runs, and `play.start` while a `project.open` waits for its restart) |
| `UNAUTHORISED`, `INTERNAL` | the handshake (a first request without a token where one is required: answered once, then the client is disconnected; a later `session.hello` with a wrong token: answered, the client stays), a command the client's token does not open (`details {required, granted}`; the client stays), an invariant |

## Who may connect, and what a client may do

The port is a loopback port that every program on the machine, and a web page through the browser, can reach,
so a TCP client must open with `session.hello` and one of the two tokens the editor writes next to `ai_port`:
`ai_token` opens every command, `ai_token_inspect` only the commands flagged `read_only`. Every command a
transport client sent is recorded in `<project>/.conjure/ai_audit.log`. The rules, the limits and the C++ side
(`access_token.h`, `audit_log.h`) are on their own page: 📘 [Access tokens and the audit log](access-and-audit.md).

**Paths in arguments.** A relative on-disk path is relative to the project root, never to the directory the
editor was started in: `editor.exec_file {path}`, `project.export {directory, distributionDirectory}`,
`module.discover {directory}`, `script.reload {assembly}`, `view.screenshot {path}` and `asset.import {source}`
(a source that is no asset path). `..` is allowed, and the answers of `project.export`, `script.reload` and
`view.screenshot` name the absolute path that was used. While no project is open a relative path is left to
the editor's working directory. An empty `directory` of `module.discover` is the project root. `project.create`
and `project.open` name a project and take their `path` as the operating system resolves it. On the command
line `--export`, `--distribution` and `--exec` are relative to the working directory, as in any shell.

## The catalogue

108 commands in 19 groups (`editor.schema` lists each with its summary, flags and the JSON schemas of its
arguments and result): `session.*`, `editor.*` (`schema`, `state`, `batch`, `begin_transaction`,
`end_transaction`, `wait_frames`, `exec_file`, `quit`), `history.*`, `project.*` (incl. `export`, `trust`,
`revoke_trust`, `trust_state`), `scene.*`, `entity.*` (`create`, `create_many`, `destroy`, `destroy_many`, `restore`, `duplicate`, `rename`, `set_parent`,
`set_transform`, `set_enabled`, `get`, `find`, `bounds`), `component.*` (`add`, `remove`, `set`, `set_many`, `get`,
`list`), `selection.*`, `asset.*`, `prefab.*`, `script.*` (`create`, `read`, `write`, `delete`, `list`, `compile`,
`reload`, `gc`, `attach`), `play.*` (`start`, `stop`, `pause`, `resume`, `step`, `state`), `input.*`, `view.*`
(`screenshot`, `set_camera`, `get_camera`, `focus`, `pick`, `mode`, `set_mode`, `debug_draw`), `physics.*`,
`reflection.*`, `module.*`, `profiler.frame_stats`, `console.*`. Flags: `mutates`, `no_undo`, `async`,
`requires_project`, `requires_scene`, `editing_only`, `playing_only`, `not_in_batch`, `on_disk` (an undoable
change to the project's files, see the warning below), `read_only` (open to inspect-only clients, see above).

Events: `log`, `selection.changed`, `play.state`, `scene.changed`, `scene.loaded`, `progress`, `script.compiled`,
`script.reloaded`, `asset.imported`, `module.state`, `command.executed`, `client.connected`,
`client.disconnected`, `events.dropped`, `warning`.

`view.screenshot {path, width?, height?, camera?, format?, waitFrames?}` answers `{path, width, height, frame,
format}`. It writes a PNG unless the path ends in `.jxl` or `format` is `"jxl"`: then a lossless JPEG XL with the
same pixels, usually a third to a half of the PNG's size. A `format` that contradicts a `.png` or `.jxl` path is
`INVALID_ARGS` (details in
[Headless mode and the command model](headless-and-commands.md#picking-screenshots-camera-console)).

Every `play.*` command answers, and every `play.state` event carries, `{state, frame, simulationFrame,
simulationTime, fixedStepIndex}`. `frame` is the engine frame; the other three are the clocks of the play session.
`play.start` sets them to 0. `simulationTime` (seconds) and `fixedStepIndex` advance with fixed steps only, so they
stand still while paused except for a `play.step`, and `simulationFrame` counts the frames simulated. `play.stop`
answers with the ended session's values, and `play.state` keeps reporting them until the next `play.start`.
`play.stop` alone adds `fileChanges`: how many file edits the ended session made (0 when none).
`profiler.frame_stats` has two fields of the same names that are instead the engine's clocks (`engine_time.time` and
`fixedStepIndex`, the clocks game code reads) since the engine started: they run on across play sessions, and
`project.open` restarts them. In a session with a renderer its `renderer` object holds the last frame's counters;
`viewsSkipped`, `lightsDropped` and `spritesDropped` count what that frame left out, every frame, while the log
warns only once when it starts.

```
-> {"id":40,"cmd":"play.step","args":{"fixedSteps":3}}
<- {"id":40,"ok":true,"result":{"state":"paused","frame":912,"simulationFrame":121,"simulationTime":2.0,"fixedStepIndex":120}}
```

> [!TIP]
> Pass `guid` / `guids` / `uuid` to `entity.create`, `entity.create_many`, `prefab.create` and
> `prefab.instantiate` in a command file: the replay then produces the same guids, and the saved scene is
> byte-identical run after run.

> [!WARNING]
> Undo is frozen while playing (`history.undo` and `history.redo` answer `WRONG_PLAY_STATE`), and `play.stop`
> restores the snapshot `play.start` took. That discards every **scene** edit made during play. It does not
> touch the project's **files**: what an `on_disk` command wrote or deleted during play is still there after
> `play.stop`.

### Edits while playing

| Edit made while playing or paused | After `play.stop` |
|---|---|
| Scene edits: `entity.*`, `component.*`, `prefab.instantiate`, `prefab.unpack`, `script.attach`, `scene.set_settings` | gone with the snapshot restore, and never recorded for undo |
| File edits, the commands flagged `on_disk`: `asset.import`, `asset.create_folder`, `asset.delete`, `asset.create_material`, `asset.set`, `asset.restore`, `prefab.apply`, `project.set_settings`, `script.create`, `script.write`, `script.delete` | still on disk; all of them together are one undo entry, `File changes during play` |
| `prefab.create`, `scene.save`, `scene.save_as` | refused with `WRONG_PLAY_STATE`: they would tie a file to scene state that the restore removes |

`play.stop` answers `fileChanges`, the number of file edits in that entry, and pushes no entry when it is 0.
One `history.undo` afterwards takes all of them back, newest first, and `history.redo` applies them again. A
batch run while playing keeps its file edits the same way and drops its scene edits; a batch that rolls back
keeps nothing. `editor.quit {saveChanges: true}` stops a play session before it saves; if the save then fails,
the answer is the save's error and the editor stays running in the editing state. Writing a script against the
running game and undoing it after the session is the intended use:

```
-> {"id":50,"cmd":"play.start"}
-> {"id":51,"cmd":"script.write","args":{"path":"Player.cs","source":"..."}}
-> {"id":52,"cmd":"play.stop"}
<- {"id":52,"ok":true,"result":{"state":"editing","frame":1310,"simulationFrame":240,"simulationTime":4.0,"fixedStepIndex":240,"fileChanges":1}}
-> {"id":53,"cmd":"history.undo"}
<- {"id":53,"ok":true,"result":{"applied":1,"labels":["File changes during play"],"canUndo":false,"canRedo":true,"revision":19}}
```

`warning` events carry `{code, message}`. Two belong to play mode and are sent once per play session each:
`UNRECORDED_PLAY_EDIT` after the first scene edit (it will be discarded; the message names the `on_disk`
commands, whose changes are kept) and `PLAY_FILE_EDIT` (with `cmd`) after the first file edit (it will stay, and becomes undoable at
`play.stop`). `STARTUP_SCENE_DELETED` (with `path`) follows an `asset.delete` of the project's startup scene.
`STARTUP_SCENE_FAILED` says the engine started without the project's startup scene: it is the first line of an
`--exec` output (see [the headless runner](#the-headless-runner)) and follows a `project.open` into such a project.

### Deleting scenes and exporting

`asset.delete {asset}` answers `{uuid, path, closedScene, startupScene}` and is undone by `history.undo`, which
restores the file with the same uuid.

- The file behind the open scene is refused with `INVALID_ARGS` until the request says `closeOpenScene: true`.
  The command then deletes the file and closes the scene (unsaved changes are dropped). Closing clears the undo
  history, so the delete is its first entry, and undoing it restores the file without reopening the scene. A
  delete the file system refuses answers its error and leaves the scene open, unsaved changes and history
  included.
- The open scene is recognised by its file, not by how its path was spelled: a scene opened as `Scenes/Main` on
  a volume that ignores case is still the asset `assets://scenes/main.scene.json`.
- `closeOpenScene` is refused inside `editor.batch`, an open transaction and an undo or redo step
  (`INVALID_ARGS`) and while playing (`WRONG_PLAY_STATE`). For an asset that is not the open scene it changes
  nothing.
- Deleting the project's startup scene succeeds; `project.json` keeps naming it, and the `warning` event above is
  the reminder to name another with `project.set_settings` or to restore the file.

```
-> {"id":7,"cmd":"asset.delete","args":{"asset":"scenes/main.scene.json"}}
<- {"id":7,"ok":false,"error":{"code":"INVALID_ARGS","message":"'assets://scenes/main.scene.json' is the open scene: pass closeOpenScene: true ...","details":{}}}
-> {"id":8,"cmd":"asset.delete","args":{"asset":"scenes/main.scene.json","closeOpenScene":true}}
<- {"id":8,"ok":true,"result":{"uuid":"5d0c...","path":"assets://scenes/main.scene.json","closedScene":true,"startupScene":false,"revision":19}}
```

`project.export {directory, distributionDirectory?, copyRuntime?, buildNativeModule?, publishManaged?}` always
produces a Distribution export; `distributionDirectory` names the Distribution build whose runtime is copied.
When the runtime is copied from it (`copyRuntime` is not `false`), a `distributionDirectory` that does not exist
answers `PATH_NOT_FOUND` (`IO_ERROR` when it is not a folder) before anything is cooked.

> [!NOTE]
> `project.export` no longer takes `config` and `project.create` no longer takes `template`: neither ever selected
> anything. A request that still sends one is refused with `INVALID_ARGS`, like any unknown argument.

## The headless runner

`headless_runner` executes `--exec`. `load_command_file(path)` reads and parses the file, and
`run(application, options, requests)` runs it: the file is a JSON array of request objects or
NDJSON (a leading UTF-8 byte order mark, as Windows PowerShell 5.1 writes, is skipped), a missing `id` becomes the
zero-based index, `${project}` in string arguments expands to the root of the project that is open when that
request runs (so it follows a `project.open` earlier in the file), frames are pumped until each asynchronous reply
exists (the dispatcher answers `TIMEOUT` after `--timeout-seconds`; should it never answer, the runner gives up
after twice that plus five seconds and ends the run), responses and events are written as NDJSON to `--out` or
standard output, and the run stops at the first failure unless `--keep-going`. `editor.quit` ends the run after
its reply, and a non-zero `exitCode` becomes the process exit code (its default 0 still reports 1 when an earlier
request failed under `--keep-going`). The warning logged for a failed request names its 1-based position and its
`id`. Exit codes of `conjure_editor`: 0 every request ok, 1 a request failed (the NDJSON names the index), 2 the
project or engine failed to start (project.json missing or invalid, a project nobody answered for and no
`--trust-project`, a device or subsystem that did not start, a busy explicit `--ai-port`, or a `project.open` in the file whose restart failed for the new and the previous
project) or the responses could not be written (the run stops after the request whose answer was lost), 3 bad
arguments (an option neither the editor nor the engine knows, a bad flag value, a flag that makes
the configuration invalid) or a command file that is missing or does not parse, 4 the graphics device was lost
(windowed only).
`exec_summary { executed, succeeded, failed, firstFailedIndex, exitCode }` summarises (`record(ok, index)` counts
one answer, the runner and `editor.exec_file` both use it);
`parse_command_file(text)` and `run_requests(application, requests, out, keepGoing)` are the in-process halves
tests use.

The command file is checked before the engine starts. `editor_application::initialise` looks at the arguments
and the project first, then reads and parses the file, and only then starts the engine, so a mistyped file name
or a syntax error costs no engine start: the process prints `--exec: cannot read '<file>'` or `--exec: cannot
parse '<file>': command file: line 3: ...` on standard error and exits with 3. When a flag or the project is
wrong as well, that is what is reported (exit 3 or 2).

A startup scene that does not load does not fail the run. The editor starts over an empty world, as it does with
a window, and the output opens with one `warning` event, before the first response:

```
<- {"event":"warning","seq":1,"frame":0,"data":{"code":"STARTUP_SCENE_FAILED","message":"startup scene 'assets://scenes/main.scene.json' failed to load: ..."}}
<- {"id":0,"ok":true,"result":{...}}
```

The exit code is the requests' alone, so a command file written to repair the scene (`scene.new` with the same
path, then `scene.save`) runs and exits 0. A script that needs the scene checks whether the first line is this
event. After a `project.open` in the file the same event follows that response when the new project's startup
scene fails.

> [!NOTE]
> Whenever standard output carries protocol data (`--ai-stdio`, `--exec` without `--out`, `--export`), every log
> line goes to standard error, so each line on standard output parses as a response or an event. On Windows the
> executable reads its arguments as UTF-8 from the wide command line and runs with the UTF-8 code page, so a
> project path in any writing system (outside the legacy code page) works.

```cpp
#include "conjure/editor/editor_application.h"
#include "conjure/editor/headless_runner.h"

editor::editor_options options = {};
options.headless = true;
options.projectPath = "C:/games/grid_sweeper";
options.execFile = "C:/games/grid_sweeper/commands/build_grid_sweeper.json";
editor::editor_application app = {};
CONJURE_TRY(app.initialise(options, {}));     // no UI layer headless; BAD_ARGUMENTS for an unusable execFile
const int exitCode = app.run();               // the runner's exit code
app.shutdown();
```

## Server and transports from C++

`protocol_server::start(options, projectRoot)` opens the transports the options ask for; `add_transport()`
plugs another one in (tests add a `loopback_transport` and talk through its `loopback_client`: `send(line)`,
`receive()`, `receive_all()`, `disconnect()`). The editor polls the server once per frame (`poll(outRequests,
maxRequests)`), dispatches, `send(response)`s and `flush()`es; `take_closed(out)` reports disconnected clients
so their pending requests are cancelled. `set_project_root(root)` moves the port file, the token files and the
audit log when the editor switches projects (`port_file()`, `token_file()`, `inspect_token_file()`);
`stdio_closed()` turns true once the stdio client is gone (standard input ended, or the client was
disconnected), and a headless `--ai-stdio` editor without a TCP server then quits with 0. `set_token(token)` and
`set_inspect_token(token)` set a token explicitly, as `--ai-token` and `--ai-inspect-token` do (`INVALID_ARGS`
for the text the other level already has); a TCP transport never runs without tokens, so `start()` and
`add_transport()` generate them when none is set. `requires_token(kind)` says whether a transport's clients must
present one, `access_of(presented)` what a presented token grants (`access_level::NONE`, `INSPECT` or
`FULL_CONTROL`), `access_of_client(id)` what a client may do now (`FULL_CONTROL` for the editor's own,
in-process clients), and `required_access(flags)` in `conjure/editor/editor_command.h` what a command asks for.
`record_command(entry)` is what the dispatcher calls for the audit log, and `audit()` is the `audit_log` itself;
that class and `access_token.h` are described in full on 📘 [Access tokens and the audit log](access-and-audit.md).
`protocol_message.h` has
the three structs (`protocol_request`, `protocol_response`, `protocol_event`), `parse_request` / `parse_response`
/ `parse_event`, `serialise` for each, `make_ok_response`, `make_error_response`, `make_id`. `line_splitter`
frames bytes into lines with the size limit and reports an oversized line as a marker at its position.

> [!NOTE]
> `transport_kind::LOCAL` (client 0) is the UI itself, which dispatches without a transport. Clients from TCP
> and stdio get ids from 1 upwards; the loopback transport hands out its own.

## API summary

| Signature (short) | Purpose |
|---|---|
| `struct protocol_request { id, command, args, coalesceKey, clientId }` / `protocol_response { id, result, errorMessage, errorCode, ok }` / `protocol_event { name, seq, frame, data }` | the three messages |
| `result<protocol_request> parse_request(string_view)` / `std::string serialise(const protocol_response &)` | wire conversion |
| `result<void> protocol_server::start(const editor_options &, const path &)` / `add_transport(unique_ptr<protocol_transport>)` | starting |
| `void poll(vector<protocol_request> &, usize)` / `send(const protocol_response &)` / `flush()` / `take_closed(vector<u32> &)` | one frame of the server |
| `void protocol_server::set_project_root(const path &)` / `bool stdio_closed()` | the port file, the token files and the audit log on a project switch, the end of a stdio session |
| `result<void> protocol_server::set_token(std::string)` / `set_inspect_token(std::string)` / `bool requires_token(transport_kind)` | an explicit token of either level, who must present one |
| `access_level access_of(string_view)` / `access_of_client(u32)` / `constexpr access_level required_access(command_flags)` | what a presented token grants, what a client may do, what a command asks for |
| `void protocol_server::record_command(audit_entry)` / `audit_log &audit()` | the audit log (the class and the token helpers: [Access tokens and the audit log](access-and-audit.md#api-summary)) |
| `loopback_client loopback_transport::connect()` | an in-process client |
| `static result<vector<protocol_request>> headless_runner::load_command_file(const path &)` / `parse_command_file(string_view)` | reading and parsing a command file, without an engine |
| `result<exec_summary> headless_runner::run(editor_application &, const editor_options &, span<const protocol_request>)` / `exec_summary run_requests(...)` | `--exec` |
| `optional<json::document> editor_application::startup_scene_warning()` | the `STARTUP_SCENE_FAILED` warning data, when the startup scene did not load |
| `std::string_view to_string(protocol_error_code)` | the wire spelling of a code |

## See also

- 📘 API: [Access tokens and the audit log](access-and-audit.md), [Headless mode and the command model](headless-and-commands.md), [The workspace UI](workspace-ui.md)
- 🛠 Handbook: [Editor module](../../handbook/modules/editor.md), [Testing](../../handbook/testing.md) (the `protocol_client` fixture)
