---
kind: header
---

[Engine](../../engine.md) → [Assets](../assets.md)

# asset_pack.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/assets/asset_pack.h` |
| API ↗ | [Cooking, caches and packs](../../../api/assets/cooking-and-packs.md), [Assets](../../../api/assets/index.md) |

<p>The export pack, <code>content.cpak</code>: a 64-<code><a href="../core/types.html#symbol-byte">byte</a></code> header, a TOC sorted by uuid, a dependency table, a string table of asset paths and the payloads, each 16-<code><a href="../core/types.html#symbol-byte">byte</a></code> aligned with identical payloads stored once. No timestamps, no absolute paths: two packs built from the same input are <code><a href="../core/types.html#symbol-byte">byte</a></code>-identical.</p>

## Structs

| File | Description |
|---|---|
| [`pack_header`](asset-pack/pack-header.md) | Fixed header at offset 0 of the pack. |
| [`pack_stats`](asset-pack/pack-stats.md) | What <code>pack_writer::write</code> produced. |
| [`pack_toc_entry`](asset-pack/pack-toc-entry.md) | One TOC entry (72 bytes on disk). |
| [`pack_writer::entry`](asset-pack/pack-writer-entry.md) |  |

## Classes

| File | Description |
|---|---|
| [`pack_reader`](asset-pack/pack-reader.md) | Reads a pack: the header, TOC, dependency and string tables are loaded and validated at <code>open()</code>; payloads are read positionally on demand from any thread. |
| [`pack_writer`](asset-pack/pack-writer.md) | Collects blobs and writes a pack. Not thread-safe; used by the cooker on the main thread. |

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
<a id="pack_header"></a>
<a id="pack_headerblobsoffset"></a>
<a id="pack_headercontenthash"></a>
<a id="pack_headerdependencytableoffset"></a>
<a id="pack_headerentrycount"></a>
<a id="pack_headerflags"></a>
<a id="pack_headerformatversion"></a>
<a id="pack_headermagic"></a>
<a id="pack_headerpayload_alignment"></a>
<a id="pack_headersize"></a>
<a id="pack_headerstringtableoffset"></a>
<a id="pack_headertocoffset"></a>
<a id="pack_reader"></a>
<a id="pack_readercontent_hash"></a>
<a id="pack_readerdependencies_of"></a>
<a id="pack_readerfind"></a>
<a id="pack_readerfind_by_name"></a>
<a id="pack_readerheader"></a>
<a id="pack_readeris_open"></a>
<a id="pack_readermbuffer"></a>
<a id="pack_readermdependencies"></a>
<a id="pack_readermfile"></a>
<a id="pack_readermfilesize"></a>
<a id="pack_readermheader"></a>
<a id="pack_readermopen"></a>
<a id="pack_readermstrings"></a>
<a id="pack_readermtoc"></a>
<a id="pack_readername_of"></a>
<a id="pack_readeropen"></a>
<a id="pack_readeropen_bytes"></a>
<a id="pack_readeroperator"></a>
<a id="pack_readerpack_reader"></a>
<a id="pack_readerpack_reader_1"></a>
<a id="pack_readerparse_tables"></a>
<a id="pack_readerread"></a>
<a id="pack_readerread_range"></a>
<a id="pack_readertoc"></a>
<a id="pack_stats"></a>
<a id="pack_statscontenthash"></a>
<a id="pack_statsduplicatesremoved"></a>
<a id="pack_statsentrycount"></a>
<a id="pack_statspackbytes"></a>
<a id="pack_toc_entry"></a>
<a id="pack_toc_entrybloboffset"></a>
<a id="pack_toc_entryblobsize"></a>
<a id="pack_toc_entrydependencycount"></a>
<a id="pack_toc_entrydependencystart"></a>
<a id="pack_toc_entryflags"></a>
<a id="pack_toc_entryid"></a>
<a id="pack_toc_entrynamelength"></a>
<a id="pack_toc_entrynameoffset"></a>
<a id="pack_toc_entrypayloadhash"></a>
<a id="pack_toc_entrysize"></a>
<a id="pack_toc_entrytype"></a>
<a id="pack_toc_entrytypeversion"></a>
<a id="pack_writer"></a>
<a id="pack_writeradd"></a>
<a id="pack_writerbuild"></a>
<a id="pack_writerbuild_manifest"></a>
<a id="pack_writerentry"></a>
<a id="pack_writerentryassetpath"></a>
<a id="pack_writerentryblob"></a>
<a id="pack_writermentries"></a>
<a id="pack_writeroperator"></a>
<a id="pack_writerpack_writer"></a>
<a id="pack_writerpack_writer_1"></a>
<a id="pack_writersize"></a>
<a id="pack_writerwrite"></a>
<a id="pack_writerwrite_manifest"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
