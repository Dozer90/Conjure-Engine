---
kind: header
---

[Engine](../../engine.md) → [Core](../core.md)

# dynamic_library.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/dynamic_library.h` |
| API ↗ | [Processes, libraries and images](../../../api/core/system.md) |

<p>RAII handle to a loaded shared library. Move-only; the library is unloaded when the last owner is destroyed. Symbols looked up through it are invalid after that.</p>



## Classes

| File | Description |
|---|---|
| [`dynamic_library`](dynamic-library/dynamic-library.md) | RAII handle to a loaded shared library. Move-only; the library is unloaded when the last owner is destroyed. Symbols looked up through it are invalid after that. |

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
<a id="dynamic_library"></a>
<a id="dynamic_librarydecorate_name"></a>
<a id="dynamic_librarydynamic_library"></a>
<a id="dynamic_librarydynamic_library_1"></a>
<a id="dynamic_libraryextension"></a>
<a id="dynamic_libraryis_loaded"></a>
<a id="dynamic_libraryload"></a>
<a id="dynamic_librarymhandle"></a>
<a id="dynamic_librarympath"></a>
<a id="dynamic_libraryoperator"></a>
<a id="dynamic_librarypath"></a>
<a id="dynamic_libraryprefix"></a>
<a id="dynamic_librarysymbol"></a>
<a id="dynamic_librarysymbol_as"></a>
<a id="dynamic_libraryunload"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
