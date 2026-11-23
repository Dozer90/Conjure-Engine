---
kind: header
---

[Engine](../../../engine.md) → [Engine](../../engine.md)

# check_registry.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/engine/diagnostics/check_registry.h` |
| API ↗ | [Export and diagnostics](../../../../api/engine/export-and-diagnostics.md), [Engine: the application and the frame](../../../../api/engine/index.md) |

<p>Self-verifying samples: scripts report named checks through the <code>Conjure.Diagnostics.Check</code> binding and the test harness asserts <code>all_passed()</code> after the run. Thread-safe: <code>report()</code> may be called from any thread.</p>

## Structs

| File | Description |
|---|---|
| [`diagnostics::check_record`](check-registry/diagnostics-check-record.md) |  |

## Classes

| File | Description |
|---|---|
| [`diagnostics::check_registry`](check-registry/diagnostics-check-registry.md) |  |

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="check_record"></a>
<a id="check_recorddetail"></a>
<a id="check_recordframe"></a>
<a id="check_recordname"></a>
<a id="check_recordoperator"></a>
<a id="check_recordpassed"></a>
<a id="check_registry"></a>
<a id="check_registryall_passed"></a>
<a id="check_registrycheck_registry"></a>
<a id="check_registrycheck_registry_1"></a>
<a id="check_registryclear"></a>
<a id="check_registryfailed_count"></a>
<a id="check_registrymfailedcount"></a>
<a id="check_registrymmutex"></a>
<a id="check_registrymrecords"></a>
<a id="check_registryoperator"></a>
<a id="check_registryrecord_count"></a>
<a id="check_registryrecords"></a>
<a id="check_registryreport"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="diagnosticscheck_record"></a>
<a id="diagnosticscheck_recorddetail"></a>
<a id="diagnosticscheck_recordframe"></a>
<a id="diagnosticscheck_recordname"></a>
<a id="diagnosticscheck_recordoperator"></a>
<a id="diagnosticscheck_recordpassed"></a>
<a id="diagnosticscheck_registry"></a>
<a id="diagnosticscheck_registryall_passed"></a>
<a id="diagnosticscheck_registrycheck_registry"></a>
<a id="diagnosticscheck_registrycheck_registry_1"></a>
<a id="diagnosticscheck_registryclear"></a>
<a id="diagnosticscheck_registryfailed_count"></a>
<a id="diagnosticscheck_registrymfailedcount"></a>
<a id="diagnosticscheck_registrymmutex"></a>
<a id="diagnosticscheck_registrymrecords"></a>
<a id="diagnosticscheck_registryoperator"></a>
<a id="diagnosticscheck_registryrecord_count"></a>
<a id="diagnosticscheck_registryrecords"></a>
<a id="diagnosticscheck_registryreport"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
