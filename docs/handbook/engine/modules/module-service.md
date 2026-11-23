---
kind: header
---

[Engine](../../engine.md) → [Modules](../modules.md)

# module_service.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/modules/module_service.h` |
| API ↗ | [Writing a module](../../../api/modules/writing-a-module.md) |

<p>Host services modules may ask for.</p>

## Structs

| File | Description |
|---|---|
| [`service_id`](module-service/service-id.md) | Identity of a service interface: the FNV-1a 64 hash of its qualified name. |

## Classes

| File | Description |
|---|---|
| [`service_table`](module-service/service-table.md) | Maps service ids to host objects. Services are provided by host code only. |

## Functions

### Public

#### `make_service_id` {#function-make-service-id}

<div class="function-declaration" id="function-make-service-id-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="module-service/service-id.html">service_id</a> make_service_id(
    std::string_view qualifiedName) <span class="k">noexcept</span>;</code></pre></div>

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
<a id="internals-and-algorithms"></a>
<a id="make_service_id"></a>
<a id="service_id"></a>
<a id="service_idhash"></a>
<a id="service_idoperator"></a>
<a id="service_table"></a>
<a id="service_tableclear"></a>
<a id="service_tablecontains"></a>
<a id="service_tablefind"></a>
<a id="service_tablefind_raw"></a>
<a id="service_tablemservices"></a>
<a id="service_tableoperator"></a>
<a id="service_tableprovide"></a>
<a id="service_tableprovide_raw"></a>
<a id="service_tableservice_table"></a>
<a id="service_tableservice_table_1"></a>
<a id="service_tablesize"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
