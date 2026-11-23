---
kind: class
---

[Engine](../../../engine.md) → [Modules](../../modules.md) → [module_service.h](../module-service.md)

# service_table
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::modules` |
| Include File Path | `engine/include/conjure/modules/module_service.h` |
| Source File Path | `engine/src/modules/module_service.cpp` |
| API ↗ | [Writing a module](../../../../api/modules/writing-a-module.md) |

## Syntax

```cpp
class CONJURE_API service_table
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">service_table();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~service_table();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mServices</code></td><td><code class="signature"><a href="../../core/containers/flat-map/flat-map.html">core::<wbr>flat_<wbr>map</a>&lt;<wbr>service_<wbr>id,<wbr> <span class="k">void</span> *&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `clear` {#function-clear}

<div class="function-declaration" id="function-clear-1"><pre><code><span class="k">void</span> clear() <span class="k">noexcept</span>;</code></pre></div>

<p>Forgets every service.</p>

#### `contains` {#function-contains}

<div class="function-declaration" id="function-contains-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> contains(
    <a href="service-id.html">service_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when a service is registered under the id.</p>

#### `find` {#function-find}

<div class="function-declaration" id="function-find-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ServiceType&gt; [[<span class="na">nodiscard</span>]] ServiceType *find() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The object provided for ServiceType, or <code>nullptr</code>.</p>

#### `provide` {#function-provide}

<div class="function-declaration" id="function-provide-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ServiceType&gt; <span class="k">void</span> provide(
    ServiceType &amp;service);</code></pre></div>

<p>Registers a host object under <code>ServiceType::SERVICE_ID</code>. Providing the same service twice asserts and keeps the first.</p>

#### `size` {#function-size}

<div class="function-declaration" id="function-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-usize">usize</a> size() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of provided services.</p>

### Private

#### `find_raw` {#function-find-raw}

<div class="function-declaration" id="function-find-raw-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">void</span> *find_raw(
    <a href="service-id.html">service_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `provide_raw` {#function-provide-raw}

<div class="function-declaration" id="function-provide-raw-1"><pre><code><span class="k">void</span> provide_raw(
    <a href="service-id.html">service_id</a> id,
    <span class="k">void</span> *service,
    std::string_view name);</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
