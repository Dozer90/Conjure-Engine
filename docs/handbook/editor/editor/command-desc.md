---
kind: header
---

[Editor](../../editor.md) → [Commands and Undo](../commands-and-undo.md)

# command_desc.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/command_desc.h` |
| API ↗ | [Headless mode and the command model](../../../api/editor/headless-and-commands.md), [The editor](../../../api/editor/index.md) |

<p>One command a module exports. Strings may point at module literals: the consumer copies what it keeps.</p>

## Structs

| File | Description |
|---|---|
| [`command_desc`](command-desc/command-desc.md) | One command a module exports. Strings may point at module literals: the consumer copies what it keeps. |

## Enumerations

### `command_flags` {#symbol-command-flags}

```cpp
enum class command_flags : u16 {
    NONE = 0,
    MUTATES = 1 << 0,
    REQUIRES_PROJECT = 1 << 1,
    REQUIRES_SCENE = 1 << 2,
    EDITING_ONLY = 1 << 3,
    PLAYING_ONLY = 1 << 4,
    ASYNC = 1 << 5,
    UI_ONLY = 1 << 6,
    NOT_IN_BATCH = 1 << 7,
    NO_UNDO = 1 << 8,
    ON_DISK = 1 << 9,
    READ_ONLY = 1 << 10,
    SYNC_IN_BATCH = 1 << 11
};
```

Properties of a command. <code>MUTATES</code> commands are recorded in the undo stack and bump the scene revision; <code>NO_UNDO</code> marks the mutating commands whose effect cannot be inverted (file and project operations): they bump the revision but record nothing. <code>A</code> module command (<code><a href="command-desc/command-desc.html">command_desc</a></code>) answers with a result document only and cannot supply an inverse, so the editor treats its <code>MUTATES</code> as <code>MUTATES</code> | <code>NO_UNDO</code>. <code>ON_DISK</code> marks the undoable commands whose effect lives in the project&#x27;s files and not in the scene (asset, script and project file edits): the snapshot <code>play.stop</code> restores does not take such a change back, so while playing the dispatcher keeps its inverse and <code>play.stop</code> records every kept one as a single undo entry. It needs <code>MUTATES</code>, excludes <code>NO_UNDO</code>, and promises that neither the inverse nor the replay refers to scene state; the editor drops it from a module command, which has no inverse. <code>READ_ONLY</code> marks the commands a client holding the inspect-only token may run: the handler promises to change nothing but what belongs to the calling client alone (its event subscriptions, its own pending reply), that is, not the scene, not a file, not the selection, the camera, the play state, the console or the history, and to start no program and load no code. <code>A</code> command without the flag needs the full-control token. The flag excludes <code>MUTATES</code>: the registry refuses the pair for a built-in command, and the editor drops <code>READ_ONLY</code> from a module command that also says <code>MUTATES</code>. The editor cannot check a module&#x27;s promise; a module that flags a command which changes something hands that change to every inspect-only client. <code>ASYNC</code> commands may not run inside <code>editor.batch</code> (the registry adds <code>NOT_IN_BATCH</code>) unless they also carry <code>SYNC_IN_BATCH</code>: the promise that the handler completes its reply before it returns whenever the dispatcher asks for it (<code>command_dispatcher::requires_synchronous_completion()</code>: a batch child, an undo or redo step, a batch rollback). The prefab commands that load their asset over frames are such commands. <code>SYNC_IN_BATCH</code> needs <code>ASYNC</code> and excludes <code>NOT_IN_BATCH</code> (the registry refuses anything else), and the editor drops it from a module command, whose handler cannot answer later at all.

## Type Aliases

### `command_handler_fn` {#symbol-command-handler-fn}

```cpp
using command_handler_fn = core::result<core::json::document> (*)(const core::json::value &args, void *userData);
```

<code>A</code> module command handler: receives the validated args and returns the result object. <code>userData</code> is the pointer stored in the record.

## Macros

### `CONJURE_ENUM_FLAGS` {#symbol-conjure-enum-flags}

```cpp
CONJURE_ENUM_FLAGS(command_flags)
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
<a id="classes"></a>
<a id="command_desc"></a>
<a id="command_descargsschema"></a>
<a id="command_descextension_point"></a>
<a id="command_descflags"></a>
<a id="command_deschandler"></a>
<a id="command_descname"></a>
<a id="command_descresultschema"></a>
<a id="command_descsummary"></a>
<a id="command_descuserdata"></a>
<a id="command_flags"></a>
<a id="command_flagsasync"></a>
<a id="command_flagsediting_only"></a>
<a id="command_flagsmutates"></a>
<a id="command_flagsno_undo"></a>
<a id="command_flagsnone"></a>
<a id="command_flagsnot_in_batch"></a>
<a id="command_flagson_disk"></a>
<a id="command_flagsplaying_only"></a>
<a id="command_flagsread_only"></a>
<a id="command_flagsrequires_project"></a>
<a id="command_flagsrequires_scene"></a>
<a id="command_flagsui_only"></a>
<a id="command_handler_fn"></a>
<a id="conjure_enum_flags"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
