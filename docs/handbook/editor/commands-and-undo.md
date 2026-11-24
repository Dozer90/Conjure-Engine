---
kind: component
---

[Editor](../editor.md)

# Commands and Undo
<!-- doc-template:component -->

|---|---|
| Type | Component |
| Implementation | `conjure::editor_core` |
| Namespace | `conjure::editor` |
| Include Folder Path | `engine/include/conjure/editor` |
| Source Folder Path | `engine/src/editor` |

<!-- doc-content:purpose:start -->
Command registration, validation, dispatch, references, and undo history.
<!-- doc-content:purpose:end -->



## Headers

| File | Description | Provides |
|---|---|---|
| [builtin_commands.h](editor/builtin-commands.md) | The built-in command catalogue: every command of the session, editor, history, console, project, scene, entity, component, asset, prefab, script, play, input, view, physics, selection, reflection, module and profiler groups. Implementations live in engine/src/editor/core/commands/<code>commands_&lt;group&gt;</code>.cpp, one free function per command plus its schema. |  |
| [command_desc.h](editor/command-desc.md) | One command a module exports. Strings may point at module literals: the consumer copies what it keeps. | [`command_desc`](editor/command-desc/command-desc.md) |
| [command_dispatcher.h](editor/command-dispatcher.md) | The one execution path: the scope of the client&#x27;s token, schema validation, preconditions, the handler, undo recording, revision bump, events and the audit record, for every source of commands. While playing the undo stack is frozen: a scene edit is executed and not recorded, an <code>ON_DISK</code> command&#x27;s inverse goes to the play controller, which makes one undo entry of them at <code>play.stop</code>. Asynchronous handlers park their request and answer through <code><a href="editor/editor-command/async-reply.html">async_reply</a></code>; <code>resolve_completed()</code> hands the finished responses back once per frame and times the stale ones out. An asynchronous command that changes the scene (<code>ASYNC</code> \| <code>MUTATES</code>) is recorded, dirties the scene, bumps the revision and emits <code>scene.changed</code> when it completes, not when it is dispatched; inside a batch, an undo or redo step or a rollback it must complete inside its handler (<code>SYNC_IN_BATCH</code>). Only a request left parked counts toward a client&#x27;s cap of pending requests (<code>BUSY</code> beyond it): one that must complete inside its handler is never refused for it. Main thread only. | [`command_dispatcher`](editor/command-dispatcher/command-dispatcher.md)<br>[`command_dispatcher::batch_state`](editor/command-dispatcher/command-dispatcher-batch-state.md)<br>[`command_dispatcher::pending_request`](editor/command-dispatcher/command-dispatcher-pending-request.md) |
| [command_registry.h](editor/command-registry.md) | The command catalogue: descriptors by name with their compiled argument schemas, the self-description <code>editor.schema</code> returns and the hash <code>session.hello</code> reports. Main thread only; <code>module.load</code> adds commands at runtime and <code>module.unload</code> removes them by owner. | [`command_registry`](editor/command-registry/command-registry.md)<br>[`command_registry::entry`](editor/command-registry/command-registry-entry.md) |
| [editor_command.h](editor/editor-command.md) | Commands are data. Every mutation of the editor, whether it comes from a panel, a protocol client, an --exec file or the undo stack, is a <code><a href="editor/editor-command/command-request.html">command_request</a></code> handed to the <code><a href="editor/command-dispatcher/command-dispatcher.html">command_dispatcher</a></code>. <code>A</code> mutating handler returns the inverse request that undoes it and, when the forward request is not deterministic (assigned guids), a normalised replay request for redo. | [`async_reply`](editor/editor-command/async-reply.md)<br>[`command_descriptor`](editor/editor-command/command-descriptor.md)<br>[`command_inverse`](editor/editor-command/command-inverse.md)<br>[`command_invocation`](editor/editor-command/command-invocation.md)<br>[`command_outcome`](editor/editor-command/command-outcome.md)<br>[`command_request`](editor/editor-command/command-request.md) |
| [entity_ref.h](editor/entity-ref.md) | Entity addressing: a protocol argument names an entity by guid (a uuid string) or by hierarchy path (&quot;Root/Child/Leaf&quot;); <code><a href="../engine/ecs/entity/entity.html">ecs::entity</a></code> ids never cross the protocol. Responses always carry guids. | [`entity_ref`](editor/entity-ref/entity-ref.md) |
| [reflection_path.h](editor/reflection-path.md) | Property paths: &quot;<code>transform.position.x</code>&quot;, &quot;materials[1]&quot;, &quot;<code>script.fields.speed</code>&quot;. The first segment names the component type (resolved by the command through the component registry), the rest walks the component&#x27;s reflection description down to one value. Values move through JSON only: the component is serialised with <code>ecs::component_to_json</code>, patched at the JSON pointer the path resolves to, and written back with <code>ecs::component_from_json</code>. | [`field_ref`](editor/reflection-path/field-ref.md)<br>[`path_segment`](editor/reflection-path/path-segment.md)<br>[`reflection_path`](editor/reflection-path/reflection-path.md) |
| [schema_validator.h](editor/schema-validator.md) | The hand-written JSON Schema subset: type (one name or an array of names), properties, required, additionalProperties (default <code>false</code> at the top level of a command&#x27;s args, <code>true</code> below), items, enum, minimum, maximum, minItems, maxItems, oneOf (shallow), plus the documentation keywords description, title and default. Any other keyword fails compilation, so a typo in a descriptor is caught at registration rather than silently accepted. | [`compiled_schema`](editor/schema-validator/compiled-schema.md) |
| [undo_stack.h](editor/undo-stack.md) | Undo as inverse commands. An entry holds the inverses (applied in reverse by <code>history.undo</code>) and the replays (applied forwards by <code>history.redo</code>); the dispatcher records one entry per mutating command, or appends to the client&#x27;s open transaction / the running batch. Depth 1024, 256 MiB of serialised arguments (oldest evicted first), coalescing of drag-style edits, frozen during play. | [`transaction_scope`](editor/undo-stack/transaction-scope.md)<br>[`undo_entry`](editor/undo-stack/undo-entry.md)<br>[`undo_stack`](editor/undo-stack/undo-stack.md) |

## Breakdown

<!-- doc-content:breakdown:start -->
Command descriptors identify operations and their requirements. `command_registry` owns registrations,
`schema_validator` validates arguments, and `command_dispatcher` checks command requirements before invoking
handlers. `builtin_commands` registers the operations supplied with the editor.

`entity_ref` and `reflection_path` address entities and reflected properties. Mutating commands return inverse
operations that `undo_stack` records; transactions and coalescing combine related edits into useful undo steps.
The same command model serves the graphical interface and remote or headless clients.

See [Editor Architecture](editor.md) for dispatch, failure handling, transactions, coalescing, and play-mode undo.
<!-- doc-content:breakdown:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:testing:start -->

<!-- doc-content:testing:end -->
