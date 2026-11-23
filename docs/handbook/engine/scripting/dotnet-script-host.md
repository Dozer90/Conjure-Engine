---
kind: header
---

[Engine](../../engine.md) → [Scripting](../scripting.md)

# dotnet_script_host.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/scripting/dotnet_script_host.h` |
| API ↗ | [Hosting, the native API and hot reload](../../../api/scripting/hosting-and-hot-reload.md), [Scripting](../../../api/scripting/index.md) |

<p>The real host: one CLR per process, started on first use by the private <code>hostfxr_runtime</code> singleton; every <code>dotnet_script_host::initialise</code> re-runs the handshake (managed Initialize is idempotent), so hosts may be created and destroyed freely.</p>



## Classes

| File | Description |
|---|---|
| [`dotnet_script_host`](dotnet-script-host/dotnet-script-host.md) |  |

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
<a id="dotnet_script_host"></a>
<a id="dotnet_script_hostbackend"></a>
<a id="dotnet_script_hostdotnet_script_host"></a>
<a id="dotnet_script_hostdotnet_script_host_1"></a>
<a id="dotnet_script_hostinitialize"></a>
<a id="dotnet_script_hostis_runtime_started"></a>
<a id="dotnet_script_hostmanaged_api"></a>
<a id="dotnet_script_hostmanaged_directory"></a>
<a id="dotnet_script_hostmmanagedapi"></a>
<a id="dotnet_script_hostmmanageddirectory"></a>
<a id="dotnet_script_hostmmode"></a>
<a id="dotnet_script_hostmnativeapi"></a>
<a id="dotnet_script_hostmode"></a>
<a id="dotnet_script_hostmstatus"></a>
<a id="dotnet_script_hostshutdown"></a>
<a id="dotnet_script_hoststatus"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
