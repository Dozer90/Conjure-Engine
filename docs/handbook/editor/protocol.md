---
kind: component
---

[Editor](../editor.md)

# Protocol
<!-- doc-template:component -->

|---|---|
| Type | Component |
| Implementation | `conjure::editor_core` |
| Namespace | `conjure::editor` |
| Include Folder Path | `engine/include/conjure/editor` |
| Source Folder Path | `engine/src/editor` |

<!-- doc-content:purpose:start -->
Editor requests, responses, events, transports, access tokens, and audit records.
<!-- doc-content:purpose:end -->



## Headers

| File | Description | Provides |
|---|---|---|
| [access_token.h](editor/protocol/access-token.md) | The secrets of the AI control protocol: what a token grants, how one is generated, where it is written for clients, and the comparisons that do not leak it: every place that holds a token against text a client sent uses one of the <code>constant_time_</code> functions below. The protocol server owns the tokens; nothing here logs or stores one. |  |
| [audit_log.h](editor/protocol/audit-log.md) | The audit log of the AI control protocol: one line of JSON per command a transport client had dispatched, appended to &lt;project&gt;/.conjure/<code>ai_audit.log</code>. The protocol server owns the log and decides what is recorded; this class formats a record, keeps tokens out of it and keeps the files bounded. Main thread only. | [`audit_entry`](editor/protocol/audit-log/audit-entry.md)<br>[`audit_log`](editor/protocol/audit-log/audit-log.md) |
| [event_broadcaster.h](editor/event-broadcaster.md) | Events: one monotonic sequence shared by every client, a subscription bitmask per client (default everything except <code>log</code> and <code>command.executed</code>) and a sink per client that receives the <code><a href="editor/protocol/protocol-message/protocol-event.html">protocol_event</a></code>. The protocol server registers its remote clients here; the local UI and the headless runner register with in-process sinks. | [`event_broadcaster`](editor/event-broadcaster/event-broadcaster.md)<br>[`event_broadcaster::client`](editor/event-broadcaster/event-broadcaster-client.md) |
| [line_splitter.h](editor/protocol/line-splitter.md) | Newline framing: bytes arrive in arbitrary chunks; complete lines come out one at a time with a trailing &#x27;\r&#x27; removed. <code>A</code> single line that grows past the limit without a newline is replaced by an <code>OVERSIZED</code> marker at its position in the stream and sets the overflow flag: the transport answers <code>MESSAGE_TOO_LARGE</code> and disconnects the client; the rest of the oversized line is discarded until the next newline. The limit applies to one line only, never to the complete lines that were framed but not yet taken. | [`line_splitter`](editor/protocol/line-splitter/line-splitter.md)<br>[`line_splitter::framed_line`](editor/protocol/line-splitter/line-splitter-framed-line.md) |
| [loopback_transport.h](editor/protocol/loopback-transport.md) | The in-process transport: tests and the AI session panel connect a <code><a href="editor/protocol/loopback-transport/loopback-client.html">loopback_client</a></code>, write lines into it and read the responses and events back, all on the main thread (a mutex still guards the queues so a test may drive a client from a job). | [`loopback_client`](editor/protocol/loopback-transport/loopback-client.md)<br>[`loopback_transport`](editor/protocol/loopback-transport/loopback-transport.md) |
| [protocol_message.h](editor/protocol/protocol-message.md) | The wire messages: newline-delimited JSON, UTF-8, at most <code>MAX_MESSAGE_BYTES</code> per line. Requests carry a caller id (integer, string or <code>null</code>, echoed verbatim), a command name and an object of arguments; responses echo the id and carry either a result or an error; events carry the monotonic sequence number and the frame they were emitted in. | [`protocol_event`](editor/protocol/protocol-message/protocol-event.md)<br>[`protocol_request`](editor/protocol/protocol-message/protocol-request.md)<br>[`protocol_response`](editor/protocol/protocol-message/protocol-response.md) |
| [protocol_server.h](editor/protocol/protocol-server.md) | The protocol server: owns the transports, the per-client state (access, open transaction bookkeeping is the undo stack&#x27;s), parses inbound lines into requests, serialises responses and events, applies the token handshake, the first-line rule, the <code>MESSAGE_TOO_LARGE</code> disconnect and the backlog policy (drop <code>log</code> events above 8 MiB, disconnect above 64 MiB), and keeps the audit log of the commands its clients had dispatched. Main thread only. | [`client_state`](editor/protocol/protocol-server/client-state.md)<br>[`protocol_server`](editor/protocol/protocol-server/protocol-server.md) |
| [protocol_transport.h](editor/protocol/protocol-transport.md) | <code>A</code> transport moves complete lines between clients and the protocol server on the main thread: <code>poll()</code> reports connects, lines and disconnects since the last call; <code>send()</code> queues a line; <code>flush()</code> writes what the peer accepts without blocking. Client ids come from the server through the allocator so every transport&#x27;s clients share one id space. | [`inbound_line`](editor/protocol/protocol-transport/inbound-line.md)<br>[`protocol_transport`](editor/protocol/protocol-transport/protocol-transport.md) |

## Breakdown

<!-- doc-content:breakdown:start -->
The protocol exposes editor commands through newline-delimited JSON requests, responses, and events.
`protocol_message` defines the messages, `line_splitter` handles framing, and `protocol_transport` separates
message delivery from command processing. `loopback_transport` supports in-process clients and tests.

`protocol_server` coordinates clients, dispatch, replies, and transport lifecycle. Access tokens establish
per-client access levels, `audit_log` records command activity, and `event_broadcaster` delivers subscriptions.
These are editor services implemented by `conjure::editor_core`.

> [!IMPORTANT]
> Keep access-token values out of logs, command replies, events, and audit records. The access checks and
> redaction helpers belong to the protocol implementation; callers should use them consistently.

See [Editor Architecture](editor.md) for the handshake, transport behaviour, access checks, and audit lifecycle.
<!-- doc-content:breakdown:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:testing:start -->

<!-- doc-content:testing:end -->
