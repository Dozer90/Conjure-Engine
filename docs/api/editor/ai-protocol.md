# The AI control protocol

📘 **Public API wiki** · 🛠 Handbook: [Editor module](../../handbook/modules/editor.md)

How an agent, a script or a test drives the editor: newline-delimited JSON over TCP, stdio or the in-process
loopback, and a command file for `--exec`. The wire types are `conjure/editor/protocol/protocol_message.h`, the
server `conjure/editor/protocol/protocol_server.h`, the transports `conjure/editor/protocol/protocol_transport.h`
and `conjure/editor/protocol/loopback_transport.h`, the framing `conjure/editor/protocol/line_splitter.h`, the
error codes `conjure/editor/editor_error.h`, the runner `conjure/editor/headless_runner.h`. The agent-facing
walkthrough with the whole catalogue is the skill `.claude/skills/editor-ai-control/SKILL.md`; the committed
`editor.schema` dump is `docs/generated/editor_protocol.json`.

## At a glance

| You want to... | Do |
|---|---|
| Start a server on the default port (47820, nine fallbacks) | `conjure_editor --project <dir>` (the UI) or `--headless --ai-port 47820`; the bound port is written to `<project>/.conjure/ai_port` |
| Talk over standard input and output | `conjure_editor --project <dir> --headless --ai-stdio` (requests on stdin, responses and events on stdout, logs on stderr) |
| Run a prepared command file and exit | `conjure_editor --project <dir> --headless --exec commands.json [--out replies.ndjson] [--keep-going]` |
| Require a token | `--ai-token <t>`; `session.hello` must carry it or every request answers `UNAUTHORISED` |
| Discover commands, components, assets | `editor.schema`, `reflection.components`, `reflection.type`, `reflection.asset_types`, `editor.state` |
| Address an entity | by guid string, by hierarchy path `"Board/Cell_3"`, or by unique name |
| Address a field | `transform.position.x`, `script.scripts[0].fields.mine`, `camera.cameraData.projection` |
| Connect from a C++ test without sockets | `loopback_transport` added through `protocol_server::add_transport`, `loopback_client` on the other end |

## The wire

One request per line, one response per line, events between them, UTF-8, at most `MAX_MESSAGE_BYTES` (16 MiB)
per line; a longer line answers `MESSAGE_TOO_LARGE` and the client is disconnected. `id` is an integer, a string
or null and is echoed verbatim; `args` is an object (`{}` when absent).

```
-> {"id":12,"cmd":"entity.create","args":{"name":"Player","components":{"transform":{"position":[1,0,1]}}}}
<- {"id":12,"ok":true,"result":{"guid":"a9e2c0d4-...","name":"Player","revision":42}}
<- {"id":13,"ok":false,"error":{"code":"ENTITY_NOT_FOUND","message":"No entity 'Maze/Wall_99'","details":{}}}
<- {"event":"selection.changed","seq":913,"frame":8812,"data":{"guids":["a9e2c0d4-..."]}}
```

- The first request of a TCP or stdio client is `session.hello {clientName?, token?}`; it answers
  `protocolVersion`, `engineVersion`, `schemaHash`, `clientId` and `headless`.
- A new client receives every event except `log` and `command.executed`; `session.subscribe {"events":[...],
  "minLogLevel":"warn"}` changes that. Under backlog pressure log events are shed and `events.dropped` says so.
- Asynchronous commands (`asset.import`, `script.compile`, `module.build`, `view.screenshot`, `play.step`,
  `editor.wait_frames`, `editor.exec_file`, `project.export`) answer when they finish, frames later; at most
  eight pending per client (`BUSY`), each timing out after `--timeout-seconds` (`TIMEOUT`, default 120 s). A
  `project.open` restarts the engine and answers every pending request with `CANCELLED`.
- Every mutating result carries `revision`; `scene.changed {revision, label}` follows it.

| Error code | When |
|---|---|
| `PARSE_ERROR`, `INVALID_REQUEST`, `MESSAGE_TOO_LARGE` | the line is not JSON, not a request, or too long (then disconnect) |
| `UNKNOWN_COMMAND` (`details.didYouMean`), `INVALID_ARGS` (names `args.<path>`) | the request |
| `ENTITY_NOT_FOUND`, `COMPONENT_NOT_FOUND`, `TYPE_NOT_FOUND`, `PATH_NOT_FOUND`, `ASSET_NOT_FOUND` | a reference did not resolve |
| `PROJECT_NOT_OPEN`, `SCENE_NOT_OPEN`, `WRONG_PLAY_STATE` (`details.state`) | a precondition |
| `NOT_AVAILABLE`, `IO_ERROR`, `COMPILE_FAILED` (`details.diagnostics`) | the environment: a null backend, a file, a build |
| `BUSY`, `TIMEOUT`, `CANCELLED` | the asynchronous queue |
| `UNAUTHORISED`, `INTERNAL` | the handshake, an invariant |

## The catalogue

105 commands in 19 groups (`editor.schema` lists each with its summary, flags and the JSON schemas of its
arguments and result): `session.*`, `editor.*` (`schema`, `state`, `batch`, `begin_transaction`,
`end_transaction`, `wait_frames`, `exec_file`, `quit`), `history.*`, `project.*` (incl. `export`), `scene.*`,
`entity.*` (`create`, `create_many`, `destroy`, `destroy_many`, `restore`, `duplicate`, `rename`, `set_parent`,
`set_transform`, `set_enabled`, `get`, `find`, `bounds`), `component.*` (`add`, `remove`, `set`, `set_many`, `get`,
`list`), `selection.*`, `asset.*`, `prefab.*`, `script.*` (`create`, `read`, `write`, `delete`, `list`, `compile`,
`reload`, `gc`, `attach`), `play.*` (`start`, `stop`, `pause`, `resume`, `step`, `state`), `input.*`, `view.*`
(`screenshot`, `set_camera`, `get_camera`, `focus`, `pick`, `mode`, `set_mode`, `debug_draw`), `physics.*`,
`reflection.*`, `module.*`, `profiler.frame_stats`, `console.*`. Flags: `mutates`, `no_undo`, `async`,
`requires_project`, `requires_scene`, `editing_only`, `playing_only`, `not_in_batch`.

Events: `log`, `selection.changed`, `play.state`, `scene.changed`, `scene.loaded`, `progress`, `script.compiled`,
`script.reloaded`, `asset.imported`, `module.state`, `command.executed`, `client.connected`,
`client.disconnected`, `events.dropped`, `warning`.

> [!TIP]
> Pass `guid` / `guids` / `uuid` to `entity.create`, `entity.create_many`, `prefab.create` and
> `prefab.instantiate` in a command file: the replay then produces the same guids, and the saved scene is
> byte-identical run after run.

> [!WARNING]
> Undo is frozen while playing (`WRONG_PLAY_STATE`) and `play.stop` discards every edit made during play by
> restoring the snapshot `play.start` took; a `warning` event says so.

## The headless runner

`headless_runner::run(application, options)` executes `--exec`: the file is a JSON array of request objects or
NDJSON, a missing `id` becomes the zero-based index, `${project}` in string arguments expands to the project
root, frames are pumped until each asynchronous reply exists, responses and events are written as NDJSON to
`--out` or standard output, and the run stops at the first failure unless `--keep-going`. Exit codes: 0 every
request ok, 1 a request failed (the NDJSON names the index), 2 the project or engine failed to start, 3 bad
arguments or an unreadable file. `exec_summary { executed, succeeded, failed, firstFailedIndex, exitCode }`
summarises; `parse_command_file(text)` and `run_requests(application, requests, out, keepGoing)` are the
in-process halves tests use.

```cpp
#include "conjure/editor/editor_application.h"
#include "conjure/editor/headless_runner.h"

editor::editor_options options = {};
options.headless = true;
options.projectPath = "C:/games/grid_sweeper";
options.execFile = "C:/games/grid_sweeper/commands/build_grid_sweeper.json";
editor::editor_application app = {};
CONJURE_TRY(app.initialise(options, {}));     // no UI layer headless
const int exitCode = app.run();               // the runner's exit code
app.shutdown();
```

## Server and transports from C++

`protocol_server::start(options, projectRoot)` opens the transports the options ask for; `add_transport()`
plugs another one in (tests add a `loopback_transport` and talk through its `loopback_client`: `send(line)`,
`receive()`, `receive_all()`, `disconnect()`). The editor polls the server once per frame (`poll(outRequests,
maxRequests)`), dispatches, `send(response)`s and `flush()`es; `take_closed(out)` reports disconnected clients
so their pending requests are cancelled. `protocol_message.h` has the three structs (`protocol_request`,
`protocol_response`, `protocol_event`), `parse_request` / `parse_response` / `parse_event`, `serialise` for each,
`make_ok_response`, `make_error_response`, `make_id`. `line_splitter` frames bytes into lines with the size limit
and reports an oversized line as a marker at its position.

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
| `loopback_client loopback_transport::connect()` | an in-process client |
| `result<exec_summary> headless_runner::run(editor_application &, const editor_options &)` / `exec_summary run_requests(...)` / `static result<vector<protocol_request>> parse_command_file(string_view)` | `--exec` |
| `std::string_view to_string(protocol_error_code)` | the wire spelling of a code |

## See also

- 📘 API: [Headless mode and the command model](headless-and-commands.md), [The workspace UI](workspace-ui.md)
- 🛠 Handbook: [Editor module](../../handbook/modules/editor.md), [Testing](../../handbook/testing.md) (the `protocol_client` fixture)
