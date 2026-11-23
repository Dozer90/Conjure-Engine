---
kind: class
---

[Engine](../../../engine.md) → [Modules](../../modules.md) → [module_context.h](../module-context.md)

# module_context
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::modules` |
| Include File Path | `engine/include/conjure/modules/module_context.h` |
| Source File Path | `engine/src/modules/module_context.cpp` |
| API ↗ | [Writing a module](../../../../api/modules/writing-a-module.md) |

## Syntax

```cpp
class CONJURE_API module_context
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">module_context(
    <a href="../module-types/module-id.html">module_id</a> id,
    <a href="../module-types.html#symbol-module-host-kind">module_host_kind</a> host,
    <a href="../module-version/semantic-version.html">semantic_version</a> engineVersion,
    <span class="k">const</span> <a href="../module-manifest/module-manifest.html">module_manifest</a> &amp;manifest,
    std::filesystem::path rootDirectory,
    <span class="k">const</span> <a href="../module-service/service-table.html">service_table</a> &amp;services);</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~module_context();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">m<wbr>Engine<wbr>Version</code></td><td><code class="signature"><a href="../module-version/semantic-version.html">semantic_version</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mHost</code></td><td><code class="signature"><a href="../module-types.html#symbol-module-host-kind">module_host_kind</a></code></td><td><code class="signature">module_<wbr>host_<wbr>kind::<wbr>RUNTIME</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mId</code></td><td><code class="signature"><a href="../module-types/module-id.html">module_id</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mManifest</code></td><td><code class="signature"><span class="k">const</span> <a href="../module-manifest/module-manifest.html">module_manifest</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRegistrar</code></td><td><code class="signature"><a href="../module-registrar/module-registrar.html">module_registrar</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Root<wbr>Directory</code></td><td><code class="signature">std::filesystem::path</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mServices</code></td><td><code class="signature"><span class="k">const</span> <a href="../module-service/service-table.html">service_table</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Subscriptions</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="../../core/event/event-subscription.html">core::<wbr>event_<wbr>subscription</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `begin_entry` {#function-begin-entry}

<div class="function-declaration" id="function-begin-entry-1"><pre><code><span class="k">void</span> begin_entry(
    <a href="../module-registrar/module-registrar.html">module_registrar</a> &amp;registrar) <span class="k">noexcept</span>;</code></pre></div>

<p>Binds the registrar for the duration of the entry. Called by the module manager.</p>

#### `end_entry` {#function-end-entry}

<div class="function-declaration" id="function-end-entry-1"><pre><code><span class="k">void</span> end_entry() <span class="k">noexcept</span>;</code></pre></div>

<p>Unbinds the registrar once the entry returned. Called by the module manager.</p>

#### `engine_version` {#function-engine-version}

<div class="function-declaration" id="function-engine-version-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../module-version/semantic-version.html">semantic_version</a> engine_version() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `host` {#function-host}

<div class="function-declaration" id="function-host-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../module-types.html#symbol-module-host-kind">module_host_kind</a> host() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `id` {#function-id}

<div class="function-declaration" id="function-id-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../module-types/module-id.html">module_id</a> id() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `is_entry_running` {#function-is-entry-running}

<div class="function-declaration" id="function-is-entry-running-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_entry_running() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True while the entry runs and <code><a href="#function-registrar-1">registrar</a>()</code> may be called.</p>

#### `manifest` {#function-manifest}

<div class="function-declaration" id="function-manifest-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../module-manifest/module-manifest.html">module_manifest</a> &amp;manifest() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `registrar` {#function-registrar}

<div class="function-declaration" id="function-registrar-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../module-registrar/module-registrar.html">module_registrar</a> &amp;registrar() <span class="k">noexcept</span>;</code></pre></div>

<p>The registrar, available only while the entry runs; asserts (and returns a registrar over a scratch set) outside of it.</p>

#### `require_service` {#function-require-service}

<div class="function-declaration" id="function-require-service-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ServiceType&gt; [[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;ServiceType *&gt; require_service() <span class="k">const</span>;</code></pre></div>

<p>The host service, or <code>SERVICE_MISSING</code> naming the interface.</p>

#### `retain` {#function-retain}

<div class="function-declaration" id="function-retain-1"><pre><code><span class="k">void</span> retain(
    <a href="../../core/event/event-subscription.html">core::event_subscription</a> &amp;&amp;subscription);</code></pre></div>

<p>Owns an event subscription so its callback (module code) is released before the binary is unmapped. Module code never hands the host a callback any other way except through descriptors.</p>

#### `retained_subscription_count` {#function-retained-subscription-count}

<div class="function-declaration" id="function-retained-subscription-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-usize">usize</a> retained_subscription_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of retained subscriptions.</p>

#### `root_directory` {#function-root-directory}

<div class="function-declaration" id="function-root-directory-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::filesystem::path &amp;root_directory() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `try_service` {#function-try-service}

<div class="function-declaration" id="function-try-service-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ServiceType&gt; [[<span class="na">nodiscard</span>]] ServiceType *try_service() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The host service, or <code>nullptr</code> when the host did not provide it.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
