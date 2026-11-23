---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# mount_table.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/vfs/mount_table.h` |
| API ↗ | [Files and the virtual file system](../../../../api/core/files.md) |

<p>Declarations for <code><a href="mount-table/vfs-mount-table.html">mount_table</a>.h</code>.</p>

## Structs

| File | Description |
|---|---|
| [`vfs::mount_table::route_target`](mount-table/vfs-mount-table-route-target.md) |  |

## Classes

| File | Description |
|---|---|
| [`vfs::mount_table`](mount-table/vfs-mount-table.md) | Routes &quot;scheme://path&quot; URIs to mounted backends. The engine mounts project://, assets://, engine://, cooked:// and modules:// (module content lives under modules://&lt;name&gt;/). Every <code><a href="file-system/vfs-file-system.html">file_system</a></code> method accepts URIs only; a bare path is <code>INVALID_ARGUMENT</code> and an unmounted scheme is <code>NOT_FOUND</code>. Paths handed to enumerate callbacks carry their scheme so they can be fed straight back in. Mount, unmount and lookup are guarded by a mutex; backends are held by <code>shared_ptr</code> so an operation in flight keeps its backend alive even when the scheme is unmounted concurrently. |

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
<a id="mount_table"></a>
<a id="mount_tablecontains"></a>
<a id="mount_tablecreate_directories"></a>
<a id="mount_tableenumerate"></a>
<a id="mount_tableexists"></a>
<a id="mount_tablefind_mount"></a>
<a id="mount_tablefind_shared"></a>
<a id="mount_tableis_read_only"></a>
<a id="mount_tablemmounts"></a>
<a id="mount_tablemmutex"></a>
<a id="mount_tablemount"></a>
<a id="mount_tablemount_table"></a>
<a id="mount_tablemount_table_1"></a>
<a id="mount_tableopen_read"></a>
<a id="mount_tableread_file"></a>
<a id="mount_tableread_text_file"></a>
<a id="mount_tableremove"></a>
<a id="mount_tableremove_all"></a>
<a id="mount_tablerename"></a>
<a id="mount_tableroute"></a>
<a id="mount_tableroute_target"></a>
<a id="mount_tableroute_targetbackend"></a>
<a id="mount_tableroute_targetpath"></a>
<a id="mount_tableroute_targetscheme"></a>
<a id="mount_tableroute_to_storage"></a>
<a id="mount_tableschemes"></a>
<a id="mount_tablesnapshot_backends"></a>
<a id="mount_tablesplit"></a>
<a id="mount_tablestat"></a>
<a id="mount_tableunmount"></a>
<a id="mount_tablewrite_file"></a>
<a id="mount_tablewrite_file_atomic"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<a id="vfsmount_table"></a>
<a id="vfsmount_tablecontains"></a>
<a id="vfsmount_tablecreate_directories"></a>
<a id="vfsmount_tableenumerate"></a>
<a id="vfsmount_tableexists"></a>
<a id="vfsmount_tablefind_mount"></a>
<a id="vfsmount_tablefind_shared"></a>
<a id="vfsmount_tableis_read_only"></a>
<a id="vfsmount_tablemmounts"></a>
<a id="vfsmount_tablemmutex"></a>
<a id="vfsmount_tablemount"></a>
<a id="vfsmount_tablemount_table"></a>
<a id="vfsmount_tablemount_table_1"></a>
<a id="vfsmount_tableopen_read"></a>
<a id="vfsmount_tableread_file"></a>
<a id="vfsmount_tableread_text_file"></a>
<a id="vfsmount_tableremove"></a>
<a id="vfsmount_tableremove_all"></a>
<a id="vfsmount_tablerename"></a>
<a id="vfsmount_tableroute"></a>
<a id="vfsmount_tableroute_target"></a>
<a id="vfsmount_tableroute_targetbackend"></a>
<a id="vfsmount_tableroute_targetpath"></a>
<a id="vfsmount_tableroute_targetscheme"></a>
<a id="vfsmount_tableroute_to_storage"></a>
<a id="vfsmount_tableschemes"></a>
<a id="vfsmount_tablesnapshot_backends"></a>
<a id="vfsmount_tablesplit"></a>
<a id="vfsmount_tablestat"></a>
<a id="vfsmount_tableunmount"></a>
<a id="vfsmount_tablewrite_file"></a>
<a id="vfsmount_tablewrite_file_atomic"></a>
<!-- doc-legacy-anchors:end -->
