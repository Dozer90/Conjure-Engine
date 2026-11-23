---
kind: header
---

[Editor](../../../editor.md) → [Protocol](../../protocol.md)

# protocol_server.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/protocol/protocol_server.h` |
| API ↗ | [Access tokens and the audit log](../../../../api/editor/access-and-audit.md), [The AI control protocol](../../../../api/editor/ai-protocol.md) |

<p>The protocol server: owns the transports, the per-client state (access, open transaction bookkeeping is the undo stack&#x27;s), parses inbound lines into requests, serialises responses and events, applies the token handshake, the first-line rule, the <code>MESSAGE_TOO_LARGE</code> disconnect and the backlog policy (drop <code>log</code> events above 8 MiB, disconnect above 64 MiB), and keeps the audit log of the commands its clients had dispatched. Main thread only.</p>

## Structs

| File | Description |
|---|---|
| [`client_state`](protocol-server/client-state.md) |  |

## Classes

| File | Description |
|---|---|
| [`protocol_server`](protocol-server/protocol-server.md) |  |

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="client_state"></a>
<a id="client_stateaccess"></a>
<a id="client_stateauthenticated"></a>
<a id="client_stateclosing"></a>
<a id="client_stateconnected"></a>
<a id="client_statedroppedlogevents"></a>
<a id="client_stateid"></a>
<a id="client_statekind"></a>
<a id="client_statespoke"></a>
<a id="client_statetransportindex"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="protocol_server"></a>
<a id="protocol_serveraccept_hello"></a>
<a id="protocol_serveraccess_of"></a>
<a id="protocol_serveraccess_of_client"></a>
<a id="protocol_serveradd_transport"></a>
<a id="protocol_serveraudit"></a>
<a id="protocol_serverbacklog_count"></a>
<a id="protocol_serverbound_port"></a>
<a id="protocol_servercheck_handshake"></a>
<a id="protocol_serverclient"></a>
<a id="protocol_serverclient_count"></a>
<a id="protocol_servercontains_token"></a>
<a id="protocol_serverdefault_requests_per_frame"></a>
<a id="protocol_serverdeliver_event"></a>
<a id="protocol_serverdisconnect"></a>
<a id="protocol_serverdisconnect_backlog_bytes"></a>
<a id="protocol_serverensure_tokens"></a>
<a id="protocol_serverfind"></a>
<a id="protocol_serverflush"></a>
<a id="protocol_servergates_every_transport"></a>
<a id="protocol_serverhas_tcp_transport"></a>
<a id="protocol_serverinspect_token"></a>
<a id="protocol_serverinspect_token_file"></a>
<a id="protocol_serveris_running"></a>
<a id="protocol_serverlocal_loopback"></a>
<a id="protocol_serverlog_drop_backlog_bytes"></a>
<a id="protocol_servermaudit"></a>
<a id="protocol_servermbacklog"></a>
<a id="protocol_servermboundport"></a>
<a id="protocol_servermclients"></a>
<a id="protocol_servermclosedqueue"></a>
<a id="protocol_servermevents"></a>
<a id="protocol_serverminspecttoken"></a>
<a id="protocol_serverminspecttokenexplicit"></a>
<a id="protocol_serverminspecttokenfile"></a>
<a id="protocol_servermlocalloopback"></a>
<a id="protocol_servermportfile"></a>
<a id="protocol_servermrunning"></a>
<a id="protocol_servermsessiondirectory"></a>
<a id="protocol_servermstdioclosed"></a>
<a id="protocol_servermtoken"></a>
<a id="protocol_servermtokenexplicit"></a>
<a id="protocol_servermtokenfile"></a>
<a id="protocol_servermtransports"></a>
<a id="protocol_serveron_client_closed"></a>
<a id="protocol_serveron_client_opened"></a>
<a id="protocol_serveroperator"></a>
<a id="protocol_serverpoll"></a>
<a id="protocol_serverport_file"></a>
<a id="protocol_serverprotocol_server"></a>
<a id="protocol_serverprotocol_server_1"></a>
<a id="protocol_serverrecord_command"></a>
<a id="protocol_serverremove_session_files"></a>
<a id="protocol_serverrequires_token"></a>
<a id="protocol_serverrewrite_token_files"></a>
<a id="protocol_serversend"></a>
<a id="protocol_serversend_line"></a>
<a id="protocol_serverset_inspect_token"></a>
<a id="protocol_serverset_project_root"></a>
<a id="protocol_serverset_token"></a>
<a id="protocol_serverstart"></a>
<a id="protocol_serverstdio_closed"></a>
<a id="protocol_serverstop"></a>
<a id="protocol_servertake_closed"></a>
<a id="protocol_servertoken"></a>
<a id="protocol_servertoken_file"></a>
<a id="protocol_servertoken_search_bytes"></a>
<a id="protocol_servertransport_count"></a>
<a id="protocol_servertransport_of"></a>
<a id="protocol_serverwrite_session_files"></a>
<a id="protocol_serverwrite_token_file"></a>
<a id="protocol_serverwrite_token_files"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
