---
kind: header
---

[Editor](../../../editor.md) → [Protocol](../../protocol.md)

# audit_log.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/protocol/audit_log.h` |
| API ↗ | [Access tokens and the audit log](../../../../api/editor/access-and-audit.md), [The AI control protocol](../../../../api/editor/ai-protocol.md) |

<p>The audit log of the AI control protocol: one line of JSON per command a transport client had dispatched, appended to &lt;project&gt;/.conjure/<code>ai_audit.log</code>. The protocol server owns the log and decides what is recorded; this class formats a record, keeps tokens out of it and keeps the files bounded. Main thread only.</p>

## Structs

| File | Description |
|---|---|
| [`audit_entry`](audit-log/audit-entry.md) | One dispatched command as the audit log records it. The views must outlive the <code>record()</code> call only. |

## Classes

| File | Description |
|---|---|
| [`audit_log`](audit-log/audit-log.md) |  |

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="audit_entry"></a>
<a id="audit_entryaccess"></a>
<a id="audit_entryargs"></a>
<a id="audit_entryasync"></a>
<a id="audit_entrybatchchild"></a>
<a id="audit_entryclientid"></a>
<a id="audit_entrycommand"></a>
<a id="audit_entryerror"></a>
<a id="audit_entryok"></a>
<a id="audit_entrytransport"></a>
<a id="audit_log"></a>
<a id="audit_logaudit_log"></a>
<a id="audit_logaudit_log_1"></a>
<a id="audit_logclose"></a>
<a id="audit_logdefault_max_file_bytes"></a>
<a id="audit_logdirectory"></a>
<a id="audit_logfail"></a>
<a id="audit_logfile"></a>
<a id="audit_logfile_name"></a>
<a id="audit_logflush"></a>
<a id="audit_logformat_line"></a>
<a id="audit_logformat_time"></a>
<a id="audit_logis_open"></a>
<a id="audit_logmax_args_bytes"></a>
<a id="audit_logmax_array_items"></a>
<a id="audit_logmax_command_bytes"></a>
<a id="audit_logmax_depth"></a>
<a id="audit_logmax_file_bytes"></a>
<a id="audit_logmax_line_bytes"></a>
<a id="audit_logmax_object_members"></a>
<a id="audit_logmax_text_bytes"></a>
<a id="audit_logmdirectory"></a>
<a id="audit_logmfailed"></a>
<a id="audit_logmmaxfilebytes"></a>
<a id="audit_logmrecordedcount"></a>
<a id="audit_logmsize"></a>
<a id="audit_logmstream"></a>
<a id="audit_logopen"></a>
<a id="audit_logoperator"></a>
<a id="audit_logprevious_file"></a>
<a id="audit_logprevious_file_name"></a>
<a id="audit_logrecord"></a>
<a id="audit_logrecorded_count"></a>
<a id="audit_logrotate"></a>
<a id="audit_logset_directory"></a>
<a id="audit_logset_max_file_bytes"></a>
<a id="audit_logstart_file"></a>
<a id="audit_logtoken_mark"></a>
<a id="audit_logwithheld"></a>
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
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
