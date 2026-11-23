---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# file_system.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/vfs/file_system.h` |
| API ↗ | [Files and the virtual file system](../../../../api/core/files.md) |

<p>The virtual file system interface.</p>

## Structs

| File | Description |
|---|---|
| [`vfs::file_stat`](file-system/vfs-file-stat.md) | What a backend knows about one entry. |

## Classes

| File | Description |
|---|---|
| [`vfs::file`](file-system/vfs-file.md) | <code>A</code> file opened for random-access reads. The size is fixed at open time; <code>read_at</code> may be called from several threads at once. An open handle never blocks other callers: the file can be rewritten, replaced, renamed over or removed meanwhile on every backend. |
| [`vfs::file_system`](file-system/vfs-file-system.md) | One storage backend. See the header comment for the path and error contract. |

## Enumerations

### `vfs::enumerate_flags` {#symbol-vfs-enumerate-flags}

```cpp
enum class enumerate_flags : u8 {
    NONE = 0,
    RECURSIVE = 1 << 0,
    FILES = 1 << 1,
    DIRECTORIES = 1 << 2,
    ALL = FILES | DIRECTORIES
};
```

Controls <code>file_system::enumerate</code>. When neither <code>FILES</code> nor <code>DIRECTORIES</code> is set, both kinds are reported, so <code>NONE</code> lists the direct children of a directory and <code>RECURSIVE</code> its whole subtree.

## Type Aliases

### `vfs::enumerate_callback` {#symbol-vfs-enumerate-callback}

```cpp
using enumerate_callback = std::function<void(std::string_view path, const file_stat &stat)>;
```

Called once per enumerated entry with the backend-relative path of the entry.

## Macros

### `vfs::CONJURE_ENUM_FLAGS` {#symbol-vfs-conjure-enum-flags}

```cpp
CONJURE_ENUM_FLAGS(enumerate_flags)
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
<a id="conjure_enum_flags"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="enumerate_callback"></a>
<a id="enumerate_flags"></a>
<a id="enumerate_flagsall"></a>
<a id="enumerate_flagsdirectories"></a>
<a id="enumerate_flagsfiles"></a>
<a id="enumerate_flagsnone"></a>
<a id="enumerate_flagsrecursive"></a>
<a id="extension-points"></a>
<a id="file"></a>
<a id="file_stat"></a>
<a id="file_statisdirectory"></a>
<a id="file_statisregular"></a>
<a id="file_statsize"></a>
<a id="file_statwritetimenanoseconds"></a>
<a id="file_system"></a>
<a id="file_systemcreate_directories"></a>
<a id="file_systemenumerate"></a>
<a id="file_systemexists"></a>
<a id="file_systemfile_system"></a>
<a id="file_systemfile_system_1"></a>
<a id="file_systemis_read_only"></a>
<a id="file_systemopen_read"></a>
<a id="file_systemoperator"></a>
<a id="file_systemread_file"></a>
<a id="file_systemread_text_file"></a>
<a id="file_systemremove"></a>
<a id="file_systemremove_all"></a>
<a id="file_systemrename"></a>
<a id="file_systemstat"></a>
<a id="file_systemwrite_file"></a>
<a id="file_systemwrite_file_atomic"></a>
<a id="file_systemwrite_text_file"></a>
<a id="filefile"></a>
<a id="filefile_1"></a>
<a id="fileoperator"></a>
<a id="fileread_at"></a>
<a id="filesize"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<a id="vfsconjure_enum_flags"></a>
<a id="vfsenumerate_callback"></a>
<a id="vfsenumerate_flags"></a>
<a id="vfsenumerate_flagsall"></a>
<a id="vfsenumerate_flagsdirectories"></a>
<a id="vfsenumerate_flagsfiles"></a>
<a id="vfsenumerate_flagsnone"></a>
<a id="vfsenumerate_flagsrecursive"></a>
<a id="vfsfile"></a>
<a id="vfsfile_stat"></a>
<a id="vfsfile_statisdirectory"></a>
<a id="vfsfile_statisregular"></a>
<a id="vfsfile_statsize"></a>
<a id="vfsfile_statwritetimenanoseconds"></a>
<a id="vfsfile_system"></a>
<a id="vfsfile_systemcreate_directories"></a>
<a id="vfsfile_systemenumerate"></a>
<a id="vfsfile_systemexists"></a>
<a id="vfsfile_systemfile_system"></a>
<a id="vfsfile_systemfile_system_1"></a>
<a id="vfsfile_systemis_read_only"></a>
<a id="vfsfile_systemopen_read"></a>
<a id="vfsfile_systemoperator"></a>
<a id="vfsfile_systemread_file"></a>
<a id="vfsfile_systemread_text_file"></a>
<a id="vfsfile_systemremove"></a>
<a id="vfsfile_systemremove_all"></a>
<a id="vfsfile_systemrename"></a>
<a id="vfsfile_systemstat"></a>
<a id="vfsfile_systemwrite_file"></a>
<a id="vfsfile_systemwrite_file_atomic"></a>
<a id="vfsfile_systemwrite_text_file"></a>
<a id="vfsfilefile"></a>
<a id="vfsfilefile_1"></a>
<a id="vfsfileoperator"></a>
<a id="vfsfileread_at"></a>
<a id="vfsfilesize"></a>
<!-- doc-legacy-anchors:end -->
