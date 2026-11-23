---
kind: header
---

[Engine](../../engine.md) → [Bindings](../bindings.md)

# bindings_subsystem.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/bindings/bindings_subsystem.h` |
| API ↗ | [Scripting from C#: the bindings](../../../api/bindings/index.md), [The native table](../../../api/bindings/native-table.md) |

<p>Startup step 16 (4.2, B9): the subsystem that fills the bindings-owned native entries before scripting&#x27;s <code>post_startup</code> runs the handshake, wraps the table in counting thunks outside Distribution, owns the asset residency scripts request through <code>asset_load</code> / <code>asset_release</code> and clears it when a scene unloads. The executables add it with <code>application.add_subsystem(std::make_unique&lt;<a href="bindings-subsystem/scripting-bindings-subsystem.html">scripting_bindings_subsystem</a>&gt;())</code> before <code>initialise()</code>.</p>



## Classes

| File | Description |
|---|---|
| [`script_residency`](bindings-subsystem/script-residency.md) | The asset handles scripts hold through <code>asset_load</code>, keyed by uuid (B9). Repeat holds of one uuid are no-ops; release drops the handle (the slot stays). Main thread only. |
| [`scripting_bindings_subsystem`](bindings-subsystem/scripting-bindings-subsystem.md) |  |

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
<a id="script_residency"></a>
<a id="script_residencyclear"></a>
<a id="script_residencyhold"></a>
<a id="script_residencyholds"></a>
<a id="script_residencymhandles"></a>
<a id="script_residencyoperator"></a>
<a id="script_residencyrelease"></a>
<a id="script_residencyscript_residency"></a>
<a id="script_residencyscript_residency_1"></a>
<a id="script_residencysize"></a>
<a id="scripting_bindings_subsystem"></a>
<a id="scripting_bindings_subsystemactive_residency"></a>
<a id="scripting_bindings_subsystembegin_frame"></a>
<a id="scripting_bindings_subsystemcontext"></a>
<a id="scripting_bindings_subsystemis_filled"></a>
<a id="scripting_bindings_subsystemmcontext"></a>
<a id="scripting_bindings_subsystemmfilled"></a>
<a id="scripting_bindings_subsystemmresidency"></a>
<a id="scripting_bindings_subsystemmunloadsubscription"></a>
<a id="scripting_bindings_subsystemname"></a>
<a id="scripting_bindings_subsystempost_startup"></a>
<a id="scripting_bindings_subsystemresidency"></a>
<a id="scripting_bindings_subsystemscripting_bindings_subsystem"></a>
<a id="scripting_bindings_subsystemscripting_bindings_subsystem_1"></a>
<a id="scripting_bindings_subsystemset_active_residency"></a>
<a id="scripting_bindings_subsystemshutdown"></a>
<a id="scripting_bindings_subsystemstartup"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
