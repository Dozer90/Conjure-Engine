---
kind: header
---

[Engine](../../engine.md) → [Assets](../assets.md)

# file_watcher.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/assets/file_watcher.h` |
| API ↗ | [Database, importers and hot reload](../../../api/assets/database-and-import.md) |

<p>Produces debounced file changes. Main-thread API.</p>

## Structs

| File | Description |
|---|---|
| [`file_change`](file-watcher/file-change.md) | One debounced change. The path is the file-system path of the file. |
| [`polling_file_watcher::file_state`](file-watcher/polling-file-watcher-file-state.md) |  |

## Classes

| File | Description |
|---|---|
| [`file_watcher`](file-watcher/file-watcher.md) | Produces debounced file changes. Main-thread API. |
| [`polling_file_watcher`](file-watcher/polling-file-watcher.md) | Stat-based watcher. <code>poll()</code> schedules one stat pass per interval on the job system and collects the pass&#x27;s results; <code>scan_now()</code> runs a pass synchronously (tests, tools). Atomic-write temporaries (&quot;*.tmp&quot;, &quot;*.tmp-&lt;uuid&gt;&quot;) are never tracked or reported. |

## Enumerations

### `file_change_kind` {#symbol-file-change-kind}

```cpp
enum class file_change_kind : u8 {
    ADDED = 0,
    MODIFIED,
    REMOVED
};
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
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="file_change"></a>
<a id="file_change_kind"></a>
<a id="file_change_kindadded"></a>
<a id="file_change_kindmodified"></a>
<a id="file_change_kindremoved"></a>
<a id="file_changekind"></a>
<a id="file_changeoperator"></a>
<a id="file_changepath"></a>
<a id="file_watcher"></a>
<a id="file_watcheradd_root"></a>
<a id="file_watcherfile_watcher"></a>
<a id="file_watcherfile_watcher_1"></a>
<a id="file_watcheroperator"></a>
<a id="file_watcherpoll"></a>
<a id="file_watchersnapshot"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="polling_file_watcher"></a>
<a id="polling_file_watcheradd_root"></a>
<a id="polling_file_watcherapply_pass"></a>
<a id="polling_file_watcherfile_state"></a>
<a id="polling_file_watcherfile_stateoperator"></a>
<a id="polling_file_watcherfile_statesize"></a>
<a id="polling_file_watcherfile_statewritetime"></a>
<a id="polling_file_watcherinterval"></a>
<a id="polling_file_watchermfilesystem"></a>
<a id="polling_file_watchermhasbaseline"></a>
<a id="polling_file_watchermhaspassed"></a>
<a id="polling_file_watcherminterval"></a>
<a id="polling_file_watchermjobs"></a>
<a id="polling_file_watchermlastpassfailure"></a>
<a id="polling_file_watchermlastpassstart"></a>
<a id="polling_file_watchermmutex"></a>
<a id="polling_file_watchermpass"></a>
<a id="polling_file_watchermpassresult"></a>
<a id="polling_file_watchermpassresultready"></a>
<a id="polling_file_watchermpending"></a>
<a id="polling_file_watchermpendingremoval"></a>
<a id="polling_file_watchermready"></a>
<a id="polling_file_watchermroots"></a>
<a id="polling_file_watchermsnapshot"></a>
<a id="polling_file_watcherpoll"></a>
<a id="polling_file_watcherpolling_file_watcher"></a>
<a id="polling_file_watcherpolling_file_watcher_1"></a>
<a id="polling_file_watcherscan_now"></a>
<a id="polling_file_watchersnapshot"></a>
<a id="polling_file_watcherstat_roots"></a>
<a id="polling_file_watchertracked_file_count"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
