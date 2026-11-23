# Access tokens and the audit log
<!-- doc-template:wiki -->
[API](../index.md) → [Editor](index.md)

<!-- doc-content:purpose:start -->
Who may drive the editor over the AI control protocol, how much each client may do, and the record of what
clients did. The tokens are `conjure/editor/protocol/access_token.h`, the record `conjure/editor/protocol/audit_log.h`;
the server that applies both is `conjure/editor/protocol/protocol_server.h` (📘 [The AI control
protocol](ai-protocol.md)). Which projects the editor opens at all is project trust
(📘 [Headless mode and the command model](headless-and-commands.md#project-trust)).
<!-- doc-content:purpose:end -->

|---|---|
| Name | Access tokens and the audit log |
| Module | Editor |
| `#include` | `conjure/editor/editor_command.h`, `conjure/editor/protocol/access_token.h`, `conjure/editor/protocol/audit_log.h`, `conjure/editor/protocol/protocol_server.h` |
| Engineering Handbook | [Engineering handbook](../../handbook/editor/editor.md) |

## Syntax

<!-- doc-content:syntax:start -->
Usage and complete examples are described in the reference sections below.
<!-- doc-content:syntax:end -->

## At a glance

<!-- doc-content:at_a_glance:start -->
| You want to... | Do |
|---|---|
| Connect over TCP | read `<project>/.conjure/ai_port` and `ai_token`, connect to `127.0.0.1:<port>`, send `session.hello {"token": "<token>"}` first |
| Let a program look without letting it change anything | give it `ai_token_inspect` instead: it runs the commands flagged `read_only` and is refused the rest |
| Choose a token yourself | `--ai-token <t>` (full control) or `--ai-inspect-token <t>` (inspect only); either gates every transport |
| See what clients did | `<project>/.conjure/ai_audit.log`, one JSON line per command |
| Compare client text with a token in C++ | `constant_time_equals`, `constant_time_starts_at`, `constant_time_contains`, never `==` or `find` |
<!-- doc-content:at_a_glance:end -->

## Variables and Properties

<!-- doc-content:variables:start -->
Types, fields, and state are described in the reference sections below.
<!-- doc-content:variables:end -->

<!-- doc-content:body:start -->
## Who may connect, and what a client may do

The port is a loopback port: every program on the machine can reach it, and so can a web page through the
browser. The editor therefore asks TCP clients for a token, and it does so by default.

- **The tokens.** When the TCP server starts, the editor generates two random tokens (64 hexadecimal digits
  each) and writes each, followed by a newline, to a file next to `ai_port`: `<project>/.conjure/ai_token` and
  `ai_token_inspect`. They are written before `ai_port`, so a client that finds the port finds them. Only the
  user who runs the editor can read them: mode 0600 on Linux and macOS, an access control list that names that
  user alone on Windows.
- **The handshake.** A TCP client's first request is `session.hello` with `"token"`. Any other first request,
  and a hello with no token or a wrong one, answers `UNAUTHORISED` once and the connection is closed; the log
  gets one INFO line naming the client's id, never what it sent. No event reaches a client before its valid
  hello. No check of client text against a token stops at a wrong byte.
- **stdio and the loopback.** A stdio client is the process that started the editor, and the loopback is code
  inside the editor: neither presents a generated token. With `--ai-token <t>` or `--ai-inspect-token <t>` every
  transport is gated and `<t>` is the token in its file. An empty value is refused, and so is the same text for
  both.
- **Where a token never appears.** The editor writes none into a log line, an event, an error message or an
  audit record; `UNAUTHORISED` names the files, not their content. Do not send a token as data (a name, a path).

```
-> {"id":1,"cmd":"session.hello","args":{"clientName":"my-agent","token":"<content of .conjure/ai_token>"}}
<- {"id":1,"ok":true,"result":{"protocolVersion":1,"engineVersion":"...","schemaHash":"...","clientId":3,"headless":true,"access":"full_control"}}
```

> [!IMPORTANT]
> A client that presented the full-control token has the reach of the user who runs the editor. It can read and
> write files anywhere that user can (`asset.import`, `editor.exec_file`, `project.export`, `view.screenshot`),
> and it can build and load code (`script.compile`, `script.reload`, `module.build`, `module.load`). No command
> confines its paths to the project. Give `ai_token` only to a program you would let run as you.

> [!WARNING]
> The limits of the token files. An account the operating system lets pass any permission (root, an
> administrator) can read them. A project on a FAT or exFAT volume gets no token files, because such a volume
> cannot restrict a file to its owner: the editor logs a warning, and TCP clients then need a token given with
> `--ai-token` or `--ai-inspect-token`. An editor started without a project writes none of the files, so its
> TCP port cannot be used until a project is open, unless one of those options was given. Two editors on one
> project overwrite each other's files: the files name the editor that started last, and the editor that exits
> first removes them.

## Two tokens: full control and inspect only

`ai_token` opens every command. `ai_token_inspect` opens the commands flagged `read_only` in `editor.schema`
and nothing else; the flag is the whole rule, so a command a module adds needs full control unless the module
flags it. A `read_only` command changes nothing that clients share: not the scene, not a file, not the
selection, the camera, the play state, the console or the history. It starts no program and loads no code.

| An inspect-only client may run | It is refused (`UNAUTHORISED`, `details {"required":"full_control","granted":"inspect"}`) |
|---|---|
| `session.*`, `editor.schema`, `editor.state`, `editor.wait_frames`, `project.info`, `project.list_scenes`, `project.trust_state`, `scene.info`, `scene.get_hierarchy`, `scene.query`, `entity.get`, `entity.find`, `entity.bounds`, `component.get`, `component.list`, `selection.get`, `asset.info`, `asset.list`, `script.list`, `script.read`, `reflection.*`, `physics.raycast`, `physics.overlap_sphere`, `view.get_camera`, `view.mode`, `view.pick`, `play.state`, `history.list`, `module.list`, `console.read`, `profiler.frame_stats` | everything else: every `mutates` command, and the commands that change no scene but are not harmless, such as `editor.quit`, `editor.exec_file`, `editor.batch` (also a batch of reads), `play.start`, `selection.set`, `view.set_camera`, `view.screenshot` (it writes a file), `script.compile`, `module.build`, `project.open`, `project.trust`, `project.revoke_trust`, `history.undo` |

- The refusal comes before the arguments are checked, and the client stays connected. An inspect-only client
  receives events like any other client.
- The last accepted `session.hello` decides: a later hello with the other token raises or lowers the same
  connection, and its answer says so in `access`. A hello with a wrong token is refused and changes nothing.
- Where no token is required (stdio and the loopback, by default) a client can still present the inspect token
  to restrict itself; any other text is ignored there, as before.
- `--ai-inspect-token <t>` without `--ai-token` gives a session only its user can change: the full-control
  token is generated, and nobody was told it unless a TCP server wrote it to `ai_token`.

> [!WARNING]
> Inspect-only limits what a client can change, not what it can learn: it reads the scene, the scripts under
> `scripts/`, the asset list, the console and every event. It cannot read the token files (`script.read` ends
> inside `scripts/`). The editor cannot check a module's `read_only` flag: a module that flags a command which
> changes something hands that change to every inspect-only client.

## The audit log

Every request of a TCP, stdio or loopback client that reached the dispatcher is appended, one JSON object per
line, to `<project>/.conjure/ai_audit.log`: refused and unknown commands included, each child of an
`editor.batch` as a line of its own (`"batch": true`, before the batch's line).

```
{"time":"2026-11-17T09:15:02Z","clientId":3,"transport":"tcp","access":"inspect","cmd":"entity.destroy","args":{"entity":"Player"},"ok":false,"error":"UNAUTHORISED"}
```

- `time` is UTC; `access` is what the client held when the command ran (for `session.hello`, what the hello
  granted); `error` is there when `ok` is false; `"async": true` marks a command that was accepted and had not
  finished, and its outcome is not recorded.
- No token is recorded: a member named `token` is written as `"[withheld]"` in any command, and a token inside
  any other text of the request as `"[token]"`. A token that is part of the record's own words (a one-letter
  `--ai-token`) cannot be kept out.
- The log is bounded: a line is at most 4 KiB (long strings, arrays, objects over 32 members and deep nesting
  are cut, arguments over 2 KiB are replaced by a note of their size), a file at most 4 MiB, and one earlier
  file is kept as `ai_audit.1.log`. It holds the most recent commands only; a busy client pushes older ones out.
- Not recorded: the editor's own requests (the UI, `--exec`, `--export`, and the commands inside a file run by
  `editor.exec_file`, whose own line names the file), a client that failed the handshake, and anything while no
  project is open (a session's first `project.open` included).
- The file is created for the user running the editor alone, like the token files. A file already there that
  other users may open (another account made it, or it is a hard link to such a file) is replaced by a new one
  with a warning, and its records are lost; on FAT and exFAT the file is an ordinary one and is appended to as
  it is. When it cannot be written the editor warns once and stops recording until the project changes.
- It is a record for you, not evidence: a full-control client can delete it. Two editors on one project write
  to the same files and each counts only its own lines, so a file can then reach twice its limit.

## From C++

`access_token.h` holds the levels and the token helpers. `access_level` is ordered (`NONE < INSPECT <
FULL_CONTROL`), so compare it with `<` and `>=`; `to_string(level)` is its wire name (`"none"`, `"inspect"`,
`"full_control"`). `generate_access_token()` returns `ACCESS_TOKEN_LENGTH` (64) lower-case hexadecimal digits
made of `ACCESS_TOKEN_BYTES` (32) bytes from the operating system's generator. `token_file_name(level)` is the
file under `<project>/.conjure` that holds a level's token (`ai_token`, `ai_token_inspect`, `""` for `NONE`).
Three comparisons never stop at the first byte that differs, so their time does not tell a client how much of a
token its text got right: `constant_time_equals(presented, expected)`, `constant_time_starts_at(text, offset,
token)` (the token at one position of the text) and `constant_time_contains(text, token)`. The last one costs
`text.size() * token.size()` steps, so bound the text before you hand it over.

`audit_log` is the record, owned by the server (`protocol_server::audit()`); the server's
`record_command(entry)` is what the dispatcher calls. An `audit_entry` holds `command` and `args` (views that
must outlive the `record()` call only), `clientId`, `transport`, `access`, `error`, `ok`, `async` and
`batchChild`. `set_directory(folder)` names the folder (`directory()` reads it back; empty means nowhere, and
`record()` then does nothing) and nothing is created before the first record. `file()` and `previous_file()` are
the two file paths, `set_max_file_bytes(bytes)` / `max_file_bytes()` the limit (raised to at least
`MAX_LINE_BYTES`). `record(entry, secrets)` appends one line with the tokens in `secrets` kept out, `flush()`
hands the lines to the operating system (the server calls it once per frame), `close()` closes the file and
keeps the folder, `is_open()` and `recorded_count()` report. The static `format_line(entry, secrets, time)`
writes one record as text and `format_time(time)` the UTC time stamp.

On the server, `set_token(token)` and `set_inspect_token(token)` set a token explicitly, as `--ai-token` and
`--ai-inspect-token` do (`INVALID_ARGS` for the text the other level already has); a TCP transport never runs
without tokens, so `start()` and `add_transport()` generate them when none is set. `requires_token(kind)` says
whether a transport's clients must present one, `access_of(presented)` what a presented token grants,
`access_of_client(id)` what a client may do now (`FULL_CONTROL` for the editor's own, in-process clients), and
`required_access(flags)` in `conjure/editor/editor_command.h` what a command asks for.
<!-- doc-content:body:end -->

## Functions

<!-- doc-content:functions:start -->
<a id="api-summary"></a>

| Signature (short) | Purpose |
|---|---|
| `enum class access_level : u8 { NONE, INSPECT, FULL_CONTROL }` / `string_view to_string(access_level)` | what a client may do, a level's wire name |
| `std::string generate_access_token()` / `ACCESS_TOKEN_BYTES` / `ACCESS_TOKEN_LENGTH` / `string_view token_file_name(access_level)` | a new token, its size, the file that holds a level's token |
| `bool constant_time_equals(string_view, string_view)` / `constant_time_starts_at(string_view, usize, string_view)` / `constant_time_contains(string_view, string_view)` | comparisons with a token that do not stop at the first difference; the caller bounds the text |
| `struct audit_entry { command, args, clientId, transport, access, error, ok, async, batchChild }` | one dispatched command |
| `void audit_log::set_directory(const path &)` / `directory()` / `file()` / `previous_file()` | where the log lives |
| `void audit_log::set_max_file_bytes(usize)` / `usize max_file_bytes()` | the size bound of one file |
| `void audit_log::record(const audit_entry &, span<const string_view>)` / `flush()` / `close()` / `bool is_open()` / `u64 recorded_count()` | writing records |
| `static std::string audit_log::format_line(const audit_entry &, span<const string_view>, time_point)` / `format_time(time_point)` | one record as text, a UTC time stamp |
| `void protocol_server::record_command(audit_entry)` / `audit_log &audit()` | the server's side of the log |
| `result<void> protocol_server::set_token(std::string)` / `set_inspect_token(std::string)` / `bool requires_token(transport_kind)` | an explicit token of either level, who must present one |
| `access_level access_of(string_view)` / `access_of_client(u32)` / `constexpr access_level required_access(command_flags)` | what a presented token grants, what a client may do, what a command asks for |
<!-- doc-content:functions:end -->

------------------------------------------

**See also:**

<!-- doc-content:related:start -->
- 📘 API: [The AI control protocol](ai-protocol.md), [Headless mode and the command model](headless-and-commands.md) (project trust)
- 🛠 Handbook: [Editor module](../../handbook/editor/editor.md)
<!-- doc-content:related:end -->

<!-- doc-legacy-anchors:start -->
<a id="see-also"></a>
<!-- doc-legacy-anchors:end -->
