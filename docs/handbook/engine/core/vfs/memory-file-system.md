---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# memory_file_system.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/vfs/memory_file_system.h` |
| API ↗ | [Files and the virtual file system](../../../../api/core/files.md) |

<p>Declarations for <code><a href="memory-file-system/vfs-memory-file-system.html">memory_file_system</a>.h</code>.</p>



## Classes

| File | Description |
|---|---|
| [`vfs::memory_file_system`](memory-file-system/vfs-memory-file-system.md) | <code>A</code> <code><a href="file-system/vfs-file-system.html">file_system</a></code> kept entirely in memory. Used by tests, by the cooker for staging output and wherever a scratch tree is needed. Every operation takes one mutex, so the instance is safe to share between threads; write times come from a counter that increases with every mutation, which keeps them deterministic. Names of new entries follow the same portable rule as on disk (<code><a href="file-system/vfs-file-system.html">file_system</a>.h</code>), so a test against this backend refuses what a project folder would refuse. |

## Forward Declarations

### `vfs::memory_file_system::node` {#symbol-vfs-memory-file-system-node}

```cpp
struct node;
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
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="memory_file_system"></a>
<a id="memory_file_systemcheck_writable"></a>
<a id="memory_file_systemcreate_directories"></a>
<a id="memory_file_systemensure_directory_locked"></a>
<a id="memory_file_systemenumerate"></a>
<a id="memory_file_systemis_read_only"></a>
<a id="memory_file_systemmemory_file_system"></a>
<a id="memory_file_systemmemory_file_system_1"></a>
<a id="memory_file_systemmmutex"></a>
<a id="memory_file_systemmreadonly"></a>
<a id="memory_file_systemmroot"></a>
<a id="memory_file_systemmwritecounter"></a>
<a id="memory_file_systemnode"></a>
<a id="memory_file_systemopen_read"></a>
<a id="memory_file_systemread_file"></a>
<a id="memory_file_systemremove"></a>
<a id="memory_file_systemremove_all"></a>
<a id="memory_file_systemremove_locked"></a>
<a id="memory_file_systemrename"></a>
<a id="memory_file_systemset_read_only"></a>
<a id="memory_file_systemstat"></a>
<a id="memory_file_systemwrite_counter"></a>
<a id="memory_file_systemwrite_file"></a>
<a id="memory_file_systemwrite_file_atomic"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<a id="vfsmemory_file_system"></a>
<a id="vfsmemory_file_systemcheck_writable"></a>
<a id="vfsmemory_file_systemcreate_directories"></a>
<a id="vfsmemory_file_systemensure_directory_locked"></a>
<a id="vfsmemory_file_systemenumerate"></a>
<a id="vfsmemory_file_systemis_read_only"></a>
<a id="vfsmemory_file_systemmemory_file_system"></a>
<a id="vfsmemory_file_systemmemory_file_system_1"></a>
<a id="vfsmemory_file_systemmmutex"></a>
<a id="vfsmemory_file_systemmreadonly"></a>
<a id="vfsmemory_file_systemmroot"></a>
<a id="vfsmemory_file_systemmwritecounter"></a>
<a id="vfsmemory_file_systemnode"></a>
<a id="vfsmemory_file_systemopen_read"></a>
<a id="vfsmemory_file_systemread_file"></a>
<a id="vfsmemory_file_systemremove"></a>
<a id="vfsmemory_file_systemremove_all"></a>
<a id="vfsmemory_file_systemremove_locked"></a>
<a id="vfsmemory_file_systemrename"></a>
<a id="vfsmemory_file_systemset_read_only"></a>
<a id="vfsmemory_file_systemstat"></a>
<a id="vfsmemory_file_systemwrite_counter"></a>
<a id="vfsmemory_file_systemwrite_file"></a>
<a id="vfsmemory_file_systemwrite_file_atomic"></a>
<!-- doc-legacy-anchors:end -->
