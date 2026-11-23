---
kind: class
---

[Engine](../../../engine.md) → [Modules](../../modules.md) → [module_consumer.h](../module-consumer.md)

# module_consumer
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::modules` |
| Include File Path | `engine/include/conjure/modules/module_consumer.h` |
| Source File Path | `engine/src/modules/module_consumer.cpp` |
| API ↗ | [Loading and managing modules](../../../../api/modules/loading-and-managing.md) |

## Syntax

```cpp
class CONJURE_API module_consumer
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">module_consumer();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">virtual</span> ~module_consumer();</code></td><td></td></tr>
</tbody>
</table>



## Functions

### Public

#### `attach` {#function-attach}

<div class="function-declaration" id="function-attach-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> attach(
    <a href="../module-types/module-id.html">module_id</a> id,
    <a href="../module-context/module-context.html">module_context</a> &amp;context,
    <span class="k">const</span> <a href="../module-registration-set/module-registration-set.html">module_registration_set</a> &amp;set) = <span class="mi">0</span>;</code></pre></div>

<p>Applies the registrations. Cannot fail: <code><a href="#function-validate-1">validate</a>()</code> already accepted them.</p>

#### `can_detach` {#function-can-detach}

<div class="function-declaration" id="function-can-detach-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; can_detach(
    <a href="../module-types/module-id.html">module_id</a> id,
    <a href="../module-types.html#symbol-unload-policy">unload_policy</a> policy) = <span class="mi">0</span>;</code></pre></div>

<p>Side-effect free veto: <code>IN_USE</code> under <code>REFUSE_IF_IN_USE</code>, <code>UNLOAD_BLOCKED</code> in play mode.</p>

#### `detach` {#function-detach}

<div class="function-declaration" id="function-detach-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> detach(
    <a href="../module-types/module-id.html">module_id</a> id,
    <a href="reload-context.html">reload_context</a> *reload) = <span class="mi">0</span>;</code></pre></div>

<p>Removes everything the module registered. <code>reload</code> is <code>null</code> for a plain unload and points at the snapshot to fill during a reload.</p>

#### `live_use_count` {#function-live-use-count}

<div class="function-declaration" id="function-live-use-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../core/types.html#symbol-u32">u32</a> live_use_count(
    <a href="../module-types/module-id.html">module_id</a> id) <span class="k">const</span> = <span class="mi">0</span>;</code></pre></div>

<p>How many live objects still use the module&#x27;s registrations (entities with its components, open assets, ...).</p>

#### `name` {#function-name}

<div class="function-declaration" id="function-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../core/string-id/string-id.html">core::string_id</a> name() <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

<p>The consumer&#x27;s name (&quot;ecs&quot;), used in <code>REGISTRATION_CONFLICT</code> messages.</p>

#### `participates_in_managed_reload` {#function-participates-in-managed-reload}

<div class="function-declaration" id="function-participates-in-managed-reload-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <span class="k">bool</span> participates_in_managed_reload() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True for consumers that take part in <code>reload_scope::MANAGED_ONLY</code> (scripting).</p>

#### `restore` {#function-restore}

<div class="function-declaration" id="function-restore-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> restore(
    <a href="../module-types/module-id.html">module_id</a> id,
    <span class="k">const</span> <a href="reload-context.html">reload_context</a> &amp;reload) = <span class="mi">0</span>;</code></pre></div>

<p>Restores what <code><a href="#function-detach-1">detach</a>()</code> stored, after the module was attached again.</p>

#### `validate` {#function-validate}

<div class="function-declaration" id="function-validate-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; validate(
    <a href="../module-types/module-id.html">module_id</a> id,
    <span class="k">const</span> <a href="../module-registration-set/module-registration-set.html">module_registration_set</a> &amp;set,
    <span class="k">const</span> <a href="../module-registry/module-registry.html">module_registry</a> &amp;registry) = <span class="mi">0</span>;</code></pre></div>

<p>Rejects conflicting registrations before anything is applied. Runs for every consumer before the first attach, so a veto leaves the engine untouched.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->
