---
kind: header
---

[Editor](../../editor.md) → [Commands and Undo](../commands-and-undo.md)

# command_dispatcher.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/command_dispatcher.h` |
| API ↗ | [Headless mode and the command model](../../../api/editor/headless-and-commands.md), [The editor](../../../api/editor/index.md) |

<p>The one execution path: the scope of the client&#x27;s token, schema validation, preconditions, the handler, undo recording, revision bump, events and the audit record, for every source of commands. While playing the undo stack is frozen: a scene edit is executed and not recorded, an <code>ON_DISK</code> command&#x27;s inverse goes to the play controller, which makes one undo entry of them at <code>play.stop</code>. Asynchronous handlers park their request and answer through <code><a href="editor-command/async-reply.html">async_reply</a></code>; <code>resolve_completed()</code> hands the finished responses back once per frame and times the stale ones out. Main thread only.</p>

## Structs

| File | Description |
|---|---|
| [`command_dispatcher::batch_state`](command-dispatcher/command-dispatcher-batch-state.md) | What a running <code>editor.batch</code> collects instead of the undo stack. <code>fileCommands</code> runs parallel to <code>entry.undo</code>: the name of the <code>ON_DISK</code> child that recorded the pair, &quot;&quot; for a scene edit, so a batch that ran while playing can hand its file edits to the play controller. |
| [`command_dispatcher::pending_request`](command-dispatcher/command-dispatcher-pending-request.md) |  |

## Classes

| File | Description |
|---|---|
| [`command_dispatcher`](command-dispatcher/command-dispatcher.md) |  |

## Forward Declarations

### `editor_context` {#symbol-editor-context}

```cpp
class editor_context;
```

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="command_dispatcher"></a>
<a id="command_dispatcheraccess_of_client"></a>
<a id="command_dispatcherasync_timeout_seconds"></a>
<a id="command_dispatcheraudit"></a>
<a id="command_dispatcherbatch_state"></a>
<a id="command_dispatcherbatch_statedepth"></a>
<a id="command_dispatcherbatch_stateentry"></a>
<a id="command_dispatcherbatch_statefilecommands"></a>
<a id="command_dispatchercancel_all"></a>
<a id="command_dispatchercancel_client"></a>
<a id="command_dispatchercheck_preconditions"></a>
<a id="command_dispatchercommand_dispatcher"></a>
<a id="command_dispatchercommand_dispatcher_1"></a>
<a id="command_dispatchercomplete"></a>
<a id="command_dispatcherdefault_async_timeout_seconds"></a>
<a id="command_dispatcherdefault_max_pending_per_client"></a>
<a id="command_dispatcherdispatch"></a>
<a id="command_dispatcherdispatch_internal"></a>
<a id="command_dispatcherdispatch_inverses"></a>
<a id="command_dispatcherdispatch_replays"></a>
<a id="command_dispatcherexecute"></a>
<a id="command_dispatcherin_batch"></a>
<a id="command_dispatcheris_pending"></a>
<a id="command_dispatcherkeep_play_edits"></a>
<a id="command_dispatchermasynctimeoutseconds"></a>
<a id="command_dispatchermbatch"></a>
<a id="command_dispatchermclock"></a>
<a id="command_dispatchermcompleted"></a>
<a id="command_dispatchermcontext"></a>
<a id="command_dispatchermdispatchingticket"></a>
<a id="command_dispatchermmaxpendingperclient"></a>
<a id="command_dispatchermnextticket"></a>
<a id="command_dispatchermonlocalresponse"></a>
<a id="command_dispatchermpending"></a>
<a id="command_dispatchermregistry"></a>
<a id="command_dispatchermsuppressrecording"></a>
<a id="command_dispatchermsuppressrevision"></a>
<a id="command_dispatchermsynchronousresponse"></a>
<a id="command_dispatchermundo"></a>
<a id="command_dispatchernow_seconds"></a>
<a id="command_dispatcheron_local_response"></a>
<a id="command_dispatcheroperator"></a>
<a id="command_dispatcherpending_count"></a>
<a id="command_dispatcherpending_request"></a>
<a id="command_dispatcherpending_requestclientid"></a>
<a id="command_dispatcherpending_requestcommand"></a>
<a id="command_dispatcherpending_requestdeadlineseconds"></a>
<a id="command_dispatcherpending_requestid"></a>
<a id="command_dispatcherpending_requestticket"></a>
<a id="command_dispatcherrecord_outcome"></a>
<a id="command_dispatcherreplaying"></a>
<a id="command_dispatcherresolve_completed"></a>
<a id="command_dispatcherrun_batch"></a>
<a id="command_dispatcherset_async_timeout_seconds"></a>
<a id="command_dispatcherset_clock"></a>
<a id="command_dispatcherset_max_pending_per_client"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="editor_context"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
