---
kind: header
---

[Editor](../../editor.md) → [Commands and Undo](../commands-and-undo.md)

# undo_stack.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/undo_stack.h` |
| API ↗ | [Headless mode and the command model](../../../api/editor/headless-and-commands.md), [The editor](../../../api/editor/index.md) |

<p>Undo as inverse commands. An entry holds the inverses (applied in reverse by <code>history.undo</code>) and the replays (applied forwards by <code>history.redo</code>); the dispatcher records one entry per mutating command, or appends to the client&#x27;s open transaction / the running batch. Depth 1024, 256 MiB of serialised arguments (oldest evicted first), coalescing of drag-style edits, frozen during play.</p>

## Structs

| File | Description |
|---|---|
| [`undo_entry`](undo-stack/undo-entry.md) |  |

## Classes

| File | Description |
|---|---|
| [`transaction_scope`](undo-stack/transaction-scope.md) | RAII transaction: begins in the constructor (inactive when a transaction is already open) and ends in the destructor. |
| [`undo_stack`](undo-stack/undo-stack.md) |  |

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="transaction_scope"></a>
<a id="transaction_scopeactive"></a>
<a id="transaction_scopeend"></a>
<a id="transaction_scopemactive"></a>
<a id="transaction_scopemclientid"></a>
<a id="transaction_scopemstack"></a>
<a id="transaction_scopeoperator"></a>
<a id="transaction_scopetransaction_scope"></a>
<a id="transaction_scopetransaction_scope_1"></a>
<a id="undo_entry"></a>
<a id="undo_entrybytesize"></a>
<a id="undo_entryclientid"></a>
<a id="undo_entrycoalescekey"></a>
<a id="undo_entrylabel"></a>
<a id="undo_entryredo"></a>
<a id="undo_entrysingle_command_name"></a>
<a id="undo_entryundo"></a>
<a id="undo_stack"></a>
<a id="undo_stackbegin_transaction"></a>
<a id="undo_stackbyte_budget"></a>
<a id="undo_stackbyte_size"></a>
<a id="undo_stackcan_redo"></a>
<a id="undo_stackcan_undo"></a>
<a id="undo_stackclear"></a>
<a id="undo_stackcursor"></a>
<a id="undo_stackdefault_byte_budget"></a>
<a id="undo_stackdefault_max_depth"></a>
<a id="undo_stackdiscard_at_cursor"></a>
<a id="undo_stackend_transaction"></a>
<a id="undo_stackentries"></a>
<a id="undo_stackevict"></a>
<a id="undo_stackfind_transaction"></a>
<a id="undo_stackfrozen"></a>
<a id="undo_stackhas_open_transaction"></a>
<a id="undo_stackhas_transaction"></a>
<a id="undo_stackmax_depth"></a>
<a id="undo_stackmbytebudget"></a>
<a id="undo_stackmbytes"></a>
<a id="undo_stackmcursor"></a>
<a id="undo_stackmeasure"></a>
<a id="undo_stackmentries"></a>
<a id="undo_stackmfrozen"></a>
<a id="undo_stackmmaxdepth"></a>
<a id="undo_stackmtransactions"></a>
<a id="undo_stackoperator"></a>
<a id="undo_stackpop_for_redo"></a>
<a id="undo_stackpop_for_undo"></a>
<a id="undo_stackpush"></a>
<a id="undo_stackpush_entry"></a>
<a id="undo_stackrecord"></a>
<a id="undo_stackset_byte_budget"></a>
<a id="undo_stackset_frozen"></a>
<a id="undo_stackset_max_depth"></a>
<a id="undo_stacksize"></a>
<a id="undo_stackundo_stack"></a>
<a id="undo_stackundo_stack_1"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
