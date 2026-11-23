---
kind: header
---

[Engine](../../engine.md) → [Assets](../assets.md)

# asset_database.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/assets/asset_database.h` |
| API ↗ | [Database, importers and hot reload](../../../api/assets/database-and-import.md), [Assets](../../../api/assets/index.md) |

<p>The project&#x27;s sources, their identities and their cooked state.</p>

## Structs

| File | Description |
|---|---|
| [`asset_database::entry`](asset-database/asset-database-entry.md) |  |
| [`asset_mount`](asset-database/asset-mount.md) | <code>A</code> root directory of sources, addressed as &quot;&lt;name&gt;://&lt;relative path&gt;&quot;. |
| [`asset_path_parts`](asset-database/asset-path-parts.md) | The pieces of &quot;mount://relative/path#sub/path&quot;. |
| [`asset_record`](asset-database/asset-record.md) | One asset or sub-asset known to the database. |
| [`import_failure`](asset-database/import-failure.md) | One source that failed to import. |
| [`import_report`](asset-database/import-report.md) | What <code>import()</code> did. |
| [`scan_report`](asset-database/scan-report.md) | What <code>scan()</code> found. |

## Classes

| File | Description |
|---|---|
| [`asset_database`](asset-database/asset-database.md) | The project&#x27;s sources, their identities and their cooked state. |

## Enumerations

### `asset_record_status` {#symbol-asset-record-status}

```cpp
enum class asset_record_status : u8 {
    OK = 0,
    MISSING_SOURCE,         // the source file disappeared; a loaded payload stays loaded
    META_INVALID,           // the sidecar is corrupt; never overwritten
    DUPLICATE_UUID,         // another source (first in sorted path order) owns this uuid
    IMPORT_FAILED,          // the last import failed; the previous blob, if any, is kept
    NO_IMPORTER,            // no importer for the extension; browsable, never loadable
    CYCLIC_DEPENDENCY       // member of a dependency cycle found at import
};
```

Why a record may not be loadable.

## Forward Declarations

### `asset_database::source_job` {#symbol-asset-database-source-job}

```cpp
struct source_job;
```

## Functions

### Public

#### `make_asset_path` {#function-make-asset-path}

<div class="function-declaration" id="function-make-asset-path-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string make_asset_path(
    std::string_view mount,
    std::string_view relativePath,
    std::string_view subPath = {});</code></pre></div>

<p>Builds &quot;mount://relative#sub&quot; (the &#x27;#&#x27; part only when <code>subPath</code> is not empty).</p>

#### `split_asset_path` {#function-split-asset-path}

<div class="function-declaration" id="function-split-asset-path-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::optional&lt;<a href="asset-database/asset-path-parts.html">asset_path_parts</a>&gt; split_asset_path(
    std::string_view assetPath) <span class="k">noexcept</span>;</code></pre></div>

<p>Splits an asset path. nullopt when there is no &quot;://&quot; or the mount or path is empty.</p>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="asset-database.html#symbol-asset-record-status">asset_record_status</a> status) <span class="k">noexcept</span>;</code></pre></div>

<p>Upper-case name of a status.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="asset_database"></a>
<a id="asset_databaseadd_file"></a>
<a id="asset_databaseadd_mount"></a>
<a id="asset_databaseapply_import_results"></a>
<a id="asset_databaseasset_database"></a>
<a id="asset_databaseasset_database_1"></a>
<a id="asset_databaseasset_path_of"></a>
<a id="asset_databasecache"></a>
<a id="asset_databasecollect_dependents"></a>
<a id="asset_databasecontains"></a>
<a id="asset_databasecreate_asset"></a>
<a id="asset_databasedelete_asset"></a>
<a id="asset_databasedependencies_of"></a>
<a id="asset_databasedependency_closure"></a>
<a id="asset_databasedependents_of"></a>
<a id="asset_databasedetect_cycles_locked"></a>
<a id="asset_databaseensure_open"></a>
<a id="asset_databaseentry"></a>
<a id="asset_databaseentrydependencies"></a>
<a id="asset_databaseentryedgecount"></a>
<a id="asset_databaseentryfirstedge"></a>
<a id="asset_databaseentryfirstreverseedge"></a>
<a id="asset_databaseentryimporterswitched"></a>
<a id="asset_databaseentryrecord"></a>
<a id="asset_databaseentryreverseedgecount"></a>
<a id="asset_databaseentrysidefiles"></a>
<a id="asset_databasefile_path_for"></a>
<a id="asset_databasefile_path_of"></a>
<a id="asset_databasefind"></a>
<a id="asset_databasefind_by_file_path"></a>
<a id="asset_databasefind_by_path"></a>
<a id="asset_databasefind_entry_locked"></a>
<a id="asset_databasehash_source"></a>
<a id="asset_databasehash_source_bytes"></a>
<a id="asset_databaseimport"></a>
<a id="asset_databaseimport_stale"></a>
<a id="asset_databaseis_open"></a>
<a id="asset_databaseload_snapshot"></a>
<a id="asset_databasemark_source_missing"></a>
<a id="asset_databasemark_source_present"></a>
<a id="asset_databasemcache"></a>
<a id="asset_databasemcacheroot"></a>
<a id="asset_databasemchildren"></a>
<a id="asset_databasementries"></a>
<a id="asset_databasemfilesystem"></a>
<a id="asset_databasemforwardedges"></a>
<a id="asset_databasemimporters"></a>
<a id="asset_databasemindexbyid"></a>
<a id="asset_databasemindexbypath"></a>
<a id="asset_databasemjobs"></a>
<a id="asset_databasemmounts"></a>
<a id="asset_databasemmutex"></a>
<a id="asset_databasemount_index"></a>
<a id="asset_databasemounts"></a>
<a id="asset_databasemove_asset"></a>
<a id="asset_databasemplatform"></a>
<a id="asset_databasemreverseedges"></a>
<a id="asset_databasemsidefileowners"></a>
<a id="asset_databasemstale"></a>
<a id="asset_databasemtypes"></a>
<a id="asset_databaseopen"></a>
<a id="asset_databaseoperator"></a>
<a id="asset_databaseowners_of_side_file"></a>
<a id="asset_databaseplatform"></a>
<a id="asset_databaseread_meta"></a>
<a id="asset_databaserebuild_graphs_locked"></a>
<a id="asset_databaserebuild_indexes_locked"></a>
<a id="asset_databaserecompute_stale"></a>
<a id="asset_databaserecord_count"></a>
<a id="asset_databaserecords"></a>
<a id="asset_databaseregister_source"></a>
<a id="asset_databasesave_snapshot"></a>
<a id="asset_databasescan"></a>
<a id="asset_databaseset_importer"></a>
<a id="asset_databaseset_source_status"></a>
<a id="asset_databasesource_job"></a>
<a id="asset_databasestale_ids"></a>
<a id="asset_databasestatus_of"></a>
<a id="asset_databasetype_of"></a>
<a id="asset_databasetypes"></a>
<a id="asset_databasevfs"></a>
<a id="asset_databasewrite_import_log"></a>
<a id="asset_databasewrite_meta"></a>
<a id="asset_mount"></a>
<a id="asset_mountname"></a>
<a id="asset_mountrootpath"></a>
<a id="asset_path_parts"></a>
<a id="asset_path_partsmount"></a>
<a id="asset_path_partsrelativepath"></a>
<a id="asset_path_partssubpath"></a>
<a id="asset_record"></a>
<a id="asset_record_status"></a>
<a id="asset_record_statuscyclic_dependency"></a>
<a id="asset_record_statusduplicate_uuid"></a>
<a id="asset_record_statusimport_failed"></a>
<a id="asset_record_statusmeta_invalid"></a>
<a id="asset_record_statusmissing_source"></a>
<a id="asset_record_statusno_importer"></a>
<a id="asset_record_statusok"></a>
<a id="asset_recorddisplayname"></a>
<a id="asset_recordid"></a>
<a id="asset_recordimporterid"></a>
<a id="asset_recordimporterversion"></a>
<a id="asset_recordis_sub_asset"></a>
<a id="asset_recordmountindex"></a>
<a id="asset_recordparentid"></a>
<a id="asset_recordpayloadhash"></a>
<a id="asset_recordrelativepath"></a>
<a id="asset_recordsettingshash"></a>
<a id="asset_recordsourcehash"></a>
<a id="asset_recordsourcesize"></a>
<a id="asset_recordsourcewritetime"></a>
<a id="asset_recordstatus"></a>
<a id="asset_recordsubpath"></a>
<a id="asset_recordtype"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="import_failure"></a>
<a id="import_failureassetpath"></a>
<a id="import_failureid"></a>
<a id="import_failuremessage"></a>
<a id="import_report"></a>
<a id="import_reportassetsproduced"></a>
<a id="import_reportchanged"></a>
<a id="import_reportfailures"></a>
<a id="import_reportsourcesfailed"></a>
<a id="import_reportsourcesimported"></a>
<a id="import_reportsourcesskipped"></a>
<a id="import_reportwarnings"></a>
<a id="internals-and-algorithms"></a>
<a id="make_asset_path"></a>
<a id="other-declarations"></a>
<a id="scan_report"></a>
<a id="scan_reportduplicateuuids"></a>
<a id="scan_reportfilesscanned"></a>
<a id="scan_reporthashed"></a>
<a id="scan_reportinvalidmetas"></a>
<a id="scan_reportmetascreated"></a>
<a id="scan_reportmissingimporters"></a>
<a id="scan_reportrecordsadded"></a>
<a id="scan_reportrecordsremoved"></a>
<a id="scan_reportstale"></a>
<a id="split_asset_path"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
