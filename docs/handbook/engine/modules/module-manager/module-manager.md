---
kind: class
---

[Engine](../../../engine.md) → [Modules](../../modules.md) → [module_manager.h](../module-manager.md)

# module_manager
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::modules` |
| Include File Path | `engine/include/conjure/modules/module_manager.h` |
| Source File Path | `engine/src/modules/module_manager.cpp` |
| API ↗ | [Loading and managing modules](../../../../api/modules/loading-and-managing.md) |

## Syntax

```cpp
class CONJURE_API module_manager
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">module_manager(
    <span class="k">const</span> <a href="module-manager-desc.html">module_manager_desc</a> &amp;desc,
    <a href="../../core/event-bus/event-bus.html">core::event_bus</a> &amp;events);</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~module_manager();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mConsumers</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="../module-consumer/module-consumer.html">module_<wbr>consumer</a> *&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mDesc</code></td><td><code class="signature"><a href="module-manager-desc.html">module_manager_desc</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mEvents</code></td><td><code class="signature"><a href="../../core/event-bus/event-bus.html">core::event_bus</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mHasPending</code></td><td><code class="signature">std::atomic&lt;<span class="k">bool</span>&gt;</code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Next<wbr>Load<wbr>Order</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">1</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPending</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="module-manager-pending-operation.html">pending_<wbr>operation</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRegistry</code></td><td><code class="signature"><a href="../module-registry/module-registry.html">module_registry</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mServices</code></td><td><code class="signature"><a href="../module-service/service-table.html">service_table</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `add_consumer` {#function-add-consumer}

<div class="function-declaration" id="function-add-consumer-1"><pre><code><span class="k">void</span> add_consumer(
    <a href="../module-consumer/module-consumer.html">module_consumer</a> &amp;consumer);</code></pre></div>

<p>Adds a consumer; consumers attach in add order and detach in reverse. Asserts when a module is <code>LOADED</code> or the consumer was already added.</p>

#### `add_manifest` {#function-add-manifest}

<div class="function-declaration" id="function-add-manifest-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../module-types/module-id.html">module_id</a>&gt; add_manifest(
    <span class="k">const</span> std::filesystem::path &amp;manifestPath);</code></pre></div>

<p>Registers a module from its <code>module.json</code>; the module root is the file&#x27;s directory. <code>A</code> manifest that excludes the host platform is registered as <code>SKIPPED</code>.</p>

#### `add_static_module` {#function-add-static-module}

<div class="function-declaration" id="function-add-static-module-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../module-types/module-id.html">module_id</a>&gt; add_static_module(
    <span class="k">const</span> <a href="../module-entry/static-module-desc.html">static_module_desc</a> &amp;desc);</code></pre></div>

<p>Registers a module compiled into the host. The descriptor must outlive the manager. <code>MANIFEST_INVALID</code>, <code>INVALID_NAME</code> or <code>DUPLICATE_NAME</code> on a bad manifest.</p>

#### `any_loaded` {#function-any-loaded}

<div class="function-declaration" id="function-any-loaded-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> any_loaded() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True while at least one module is <code>LOADED</code>.</p>

#### `apply_pending` {#function-apply-pending}

<div class="function-declaration" id="function-apply-pending-1"><pre><code><span class="k">void</span> apply_pending();</code></pre></div>

<p>Performs every request in order. Costs one relaxed atomic load when idle. Requests for stale ids are dropped with a warning; failures are logged and kept in <code>last_error()</code>.</p>

#### `desc` {#function-desc}

<div class="function-declaration" id="function-desc-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="module-manager-desc.html">module_manager_desc</a> &amp;desc() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `discover` {#function-discover}

<div class="function-declaration" id="function-discover-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt; discover(
    <span class="k">const</span> std::filesystem::path &amp;searchDirectory);</code></pre></div>

<p>Registers every immediate subdirectory of <code>searchDirectory</code> that holds a <code>module.json</code>, in name order, and returns how many were added. Per-module errors are collected into one error (valid modules are still added); a missing directory is <code>MANIFEST_NOT_FOUND</code>.</p>

#### `export_layout` {#function-export-layout}

<div class="function-declaration" id="function-export-layout-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; export_layout(
    <span class="k">const</span> std::filesystem::path &amp;outputDirectory,
    <a href="../module-types.html#symbol-build-config">build_config</a> targetConfig) <span class="k">const</span>;</code></pre></div>

<p>Writes &lt;out&gt;/modules/&lt;name&gt;/ (manifest without editorOnly, the bin/&lt;platform&gt;-&lt;arch&gt;-&lt;config&gt;/ directory and the managed assemblies) for every <code>LOADED</code> module that is not an engine module, not editor-only and not static. The bin directory is copied entry by entry with its subfolders, following links, and leaves out every file or folder named *.lib, *.exp, *.ilk, *.a, *.pdb or *.dSYM (ASCII case ignored): dependent libraries and data files ship, link and debug by-products and native symbols do not. Files already in the output are overwritten. Other files of an earlier export stay, except that such by-products anywhere under the output bin directory are removed first (links there are not followed), unless that directory is the module&#x27;s own bin directory or holds it. <code>BINARY_NOT_FOUND</code> names the missing configuration; <code>EXPORT_FAILED</code> any create, write, list, resolve, remove or copy error, an entry that is neither a file nor a folder (a broken link), a link back to a folder being copied and a missing managed assembly. <code>A</code> failure leaves the files copied before it in place.</p>

#### `load` {#function-load}

<div class="function-declaration" id="function-load-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; load(
    <a href="../module-types/module-id.html">module_id</a> id);</code></pre></div>

<p>Loads the module and the transitive closure of its dependencies in order, stopping at the first failure (earlier successes stay loaded).</p>

#### `load_all` {#function-load-all}

<div class="function-declaration" id="function-load-all-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; load_all();</code></pre></div>

<p>Resolves every registered module and loads them in order, continuing past failures. Returns the first error.</p>

#### `pending_count` {#function-pending-count}

<div class="function-declaration" id="function-pending-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-usize">usize</a> pending_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of requests waiting for <code><a href="#function-apply-pending-1">apply_pending</a>()</code>.</p>

#### `provide_service` {#function-provide-service}

<div class="function-declaration" id="function-provide-service-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ServiceType&gt; <span class="k">void</span> provide_service(
    ServiceType &amp;service);</code></pre></div>

<p>Provides a host service to module contexts. Asserts when a module is <code>LOADED</code>.</p>

#### `registry` {#function-registry}

<div class="function-declaration" id="function-registry-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../module-registry/module-registry.html">module_registry</a> &amp;registry() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-registry-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../module-registry/module-registry.html">module_registry</a> &amp;registry() <span class="k">noexcept</span>;</code></pre></div>

<p>Mutable registry for consumers recording ownership in <code>attach()</code>.</p>

#### `reload` {#function-reload}

<div class="function-declaration" id="function-reload-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; reload(
    <a href="../module-types/module-id.html">module_id</a> id,
    <a href="../module-types.html#symbol-reload-scope">reload_scope</a> scope);</code></pre></div>

<p>Reloads the module and its <code>LOADED</code> dependents: detach with snapshots in reverse load order, close and reopen the binaries (<code>FULL</code>), attach in order and restore. <code>A</code> failing load leaves that module and the rest <code>UNLOADED</code> with their snapshots kept for the next successful load. The id stays valid across a reload.</p>

#### `remove_consumer` {#function-remove-consumer}

<div class="function-declaration" id="function-remove-consumer-1"><pre><code><span class="k">void</span> remove_consumer(
    <a href="../module-consumer/module-consumer.html">module_consumer</a> &amp;consumer);</code></pre></div>

<p>Removes a consumer. Asserts when a module is <code>LOADED</code>.</p>

#### `request_load` {#function-request-load}

<div class="function-declaration" id="function-request-load-1"><pre><code><span class="k">void</span> request_load(
    <a href="../module-types/module-id.html">module_id</a> id);</code></pre></div>

#### `request_reload` {#function-request-reload}

<div class="function-declaration" id="function-request-reload-1"><pre><code><span class="k">void</span> request_reload(
    <a href="../module-types/module-id.html">module_id</a> id,
    <a href="../module-types.html#symbol-reload-scope">reload_scope</a> scope);</code></pre></div>

#### `request_unload` {#function-request-unload}

<div class="function-declaration" id="function-request-unload-1"><pre><code><span class="k">void</span> request_unload(
    <a href="../module-types/module-id.html">module_id</a> id,
    <a href="../module-types.html#symbol-unload-policy">unload_policy</a> policy);</code></pre></div>

#### `services` {#function-services}

<div class="function-declaration" id="function-services-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../module-service/service-table.html">service_table</a> &amp;services() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `unload` {#function-unload}

<div class="function-declaration" id="function-unload-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; unload(
    <a href="../module-types/module-id.html">module_id</a> id,
    <a href="../module-types.html#symbol-unload-policy">unload_policy</a> policy);</code></pre></div>

<p>Unloads a <code>LOADED</code> module without <code>LOADED</code> dependents. Pre-checks (<code>HAS_DEPENDENTS</code>, the consumers&#x27; <code>can_detach</code>) are side-effect free.</p>

<div class="function-declaration" id="function-unload-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; unload(
    <a href="../module-types/module-id.html">module_id</a> id);</code></pre></div>

#### `unload_all` {#function-unload-all}

<div class="function-declaration" id="function-unload-all-1"><pre><code><span class="k">void</span> unload_all();</code></pre></div>

<p>Unloads every <code>LOADED</code> module in reverse load order without the <code>can_detach</code> pre-check (consumers detach as under <code>REMOVE_INSTANCES</code>). Called by the destructor.</p>

### Private

#### `apply_one` {#function-apply-one}

<div class="function-declaration" id="function-apply-one-1"><pre><code><span class="k">void</span> apply_one(
    <span class="k">const</span> <a href="module-manager-pending-operation.html">pending_operation</a> &amp;operation);</code></pre></div>

#### `check_sync_point` {#function-check-sync-point}

<div class="function-declaration" id="function-check-sync-point-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> check_sync_point(
    <span class="k">const</span> <span class="k">char</span> *operation) <span class="k">const</span>;</code></pre></div>

#### `checked_record` {#function-checked-record}

<div class="function-declaration" id="function-checked-record-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../module-manager.html">module_record</a> *&gt; checked_record(
    <a href="../module-types/module-id.html">module_id</a> id);</code></pre></div>

#### `close_binary` {#function-close-binary}

<div class="function-declaration" id="function-close-binary-1"><pre><code><span class="k">void</span> close_binary(
    <a href="../module-manager.html">module_record</a> &amp;record);</code></pre></div>

#### `collect_reload_cascade` {#function-collect-reload-cascade}

<div class="function-declaration" id="function-collect-reload-cascade-1"><pre><code><span class="k">void</span> collect_reload_cascade(
    <a href="../module-manager.html">module_record</a> &amp;record,
    std::vector&lt;<a href="../module-types/module-id.html">module_id</a>&gt; &amp;cascade) <span class="k">const</span>;</code></pre></div>

#### `detach_record` {#function-detach-record}

<div class="function-declaration" id="function-detach-record-1"><pre><code><span class="k">void</span> detach_record(
    <a href="../module-manager.html">module_record</a> &amp;record,
    <span class="k">bool</span> reloading,
    <a href="../module-consumer/reload-context.html">reload_context</a> *snapshot);</code></pre></div>

#### `enqueue` {#function-enqueue}

<div class="function-declaration" id="function-enqueue-1"><pre><code><span class="k">void</span> enqueue(
    <a href="module-manager-pending-operation.html">pending_operation</a> operation);</code></pre></div>

#### `fail_record` {#function-fail-record}

<div class="function-declaration" id="function-fail-record-1"><pre><code><span class="k">void</span> fail_record(
    <a href="../module-manager.html">module_record</a> &amp;record,
    <a href="../../core/error/error.html">core::error</a> failure);</code></pre></div>

#### `failed_dependency_of` {#function-failed-dependency-of}

<div class="function-declaration" id="function-failed-dependency-of-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../module-manager.html">module_record</a> *failed_dependency_of(
    <span class="k">const</span> <a href="../module-manager.html">module_record</a> &amp;record) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `link_dependencies` {#function-link-dependencies}

<div class="function-declaration" id="function-link-dependencies-1"><pre><code><span class="k">void</span> link_dependencies(
    <a href="../module-manager.html">module_record</a> &amp;record);</code></pre></div>

#### `load_one` {#function-load-one}

<div class="function-declaration" id="function-load-one-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; load_one(
    <a href="../module-manager.html">module_record</a> &amp;record);</code></pre></div>

#### `load_resolved` {#function-load-resolved}

<div class="function-declaration" id="function-load-resolved-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; load_resolved(
    <span class="k">const</span> std::vector&lt;<a href="../module-types/module-id.html">module_id</a>&gt; &amp;order,
    <span class="k">bool</span> stopAtFailure);</code></pre></div>

#### `precheck_unload` {#function-precheck-unload}

<div class="function-declaration" id="function-precheck-unload-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; precheck_unload(
    <a href="../module-manager.html">module_record</a> &amp;record,
    <a href="../module-types.html#symbol-unload-policy">unload_policy</a> policy);</code></pre></div>

#### `register_manifest` {#function-register-manifest}

<div class="function-declaration" id="function-register-manifest-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../module-types/module-id.html">module_id</a>&gt; register_manifest(
    <a href="../module-manifest/module-manifest.html">module_manifest</a> manifest,
    <span class="k">const</span> std::filesystem::path &amp;rootDirectory,
    <span class="k">const</span> <a href="../module-entry/static-module-desc.html">static_module_desc</a> *staticDesc);</code></pre></div>

#### `reload_full` {#function-reload-full}

<div class="function-declaration" id="function-reload-full-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; reload_full(
    <span class="k">const</span> std::vector&lt;<a href="../module-types/module-id.html">module_id</a>&gt; &amp;cascade);</code></pre></div>

#### `reload_managed` {#function-reload-managed}

<div class="function-declaration" id="function-reload-managed-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; reload_managed(
    <span class="k">const</span> std::vector&lt;<a href="../module-types/module-id.html">module_id</a>&gt; &amp;cascade);</code></pre></div>

#### `unlink_dependencies` {#function-unlink-dependencies}

<div class="function-declaration" id="function-unlink-dependencies-1"><pre><code><span class="k">void</span> unlink_dependencies(
    <a href="../module-manager.html">module_record</a> &amp;record);</code></pre></div>

#### `unload_checked` {#function-unload-checked}

<div class="function-declaration" id="function-unload-checked-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; unload_checked(
    <a href="../module-manager.html">module_record</a> &amp;record);</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
