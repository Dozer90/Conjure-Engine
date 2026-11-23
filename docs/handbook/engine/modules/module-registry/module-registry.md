---
kind: class
---

[Engine](../../../engine.md) → [Modules](../../modules.md) → [module_registry.h](../module-registry.md)

# module_registry
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::modules` |
| Include File Path | `engine/include/conjure/modules/module_registry.h` |
| Source File Path | `engine/src/modules/module_registry.cpp` |
| API ↗ | [Loading and managing modules](../../../../api/modules/loading-and-managing.md) |

## Syntax

```cpp
class CONJURE_API module_registry
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">module_registry();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~module_registry();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mState</code></td><td><code class="signature">std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../module-registry.html">registry_<wbr>state</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `all` {#function-all}

<div class="function-declaration" id="function-all-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="../module-types/module-id.html">module_id</a>&gt; all() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Every registered module, in registration order.</p>

#### `binary_path` {#function-binary-path}

<div class="function-declaration" id="function-binary-path-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::filesystem::path &amp;binary_path(
    <a href="../module-types/module-id.html">module_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The derived binary path; empty for static and manifest-only modules.</p>

#### `dependencies` {#function-dependencies}

<div class="function-declaration" id="function-dependencies-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="../module-types/module-id.html">module_id</a>&gt; dependencies(
    <a href="../module-types/module-id.html">module_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p><code>LOADED</code> modules the module depends on.</p>

#### `dependents` {#function-dependents}

<div class="function-declaration" id="function-dependents-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="../module-types/module-id.html">module_id</a>&gt; dependents(
    <a href="../module-types/module-id.html">module_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p><code>LOADED</code> modules that depend on the module.</p>

#### `find` {#function-find}

<div class="function-declaration" id="function-find-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../module-types/module-id.html">module_id</a> find(
    <a href="../../core/string-id/string-id.html">core::string_id</a> name) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The module registered under the name, or an invalid id.</p>

#### `forget_ownership` {#function-forget-ownership}

<div class="function-declaration" id="function-forget-ownership-1"><pre><code><span class="k">void</span> forget_ownership(
    <a href="../module-types/module-id.html">module_id</a> owner);</code></pre></div>

<p>Drops every ownership entry of the module.</p>

#### `is_valid` {#function-is-valid}

<div class="function-declaration" id="function-is-valid-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_valid(
    <a href="../module-types/module-id.html">module_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the id refers to a registered module and its generation is current.</p>

#### `last_error` {#function-last-error}

<div class="function-declaration" id="function-last-error-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../../core/error/error.html">core::error</a> &amp;last_error(
    <a href="../module-types/module-id.html">module_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The error of the last failed load or unload, or an empty error.</p>

#### `load_count` {#function-load-count}

<div class="function-declaration" id="function-load-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> load_count(
    <a href="../module-types/module-id.html">module_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of times the module reached <code>LOADED</code>.</p>

#### `load_order` {#function-load-order}

<div class="function-declaration" id="function-load-order-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="../module-types/module-id.html">module_id</a>&gt; load_order() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The <code>LOADED</code> modules in the order they were loaded.</p>

#### `manifest` {#function-manifest}

<div class="function-declaration" id="function-manifest-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../module-manifest/module-manifest.html">module_manifest</a> &amp;manifest(
    <a href="../module-types/module-id.html">module_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `note_ownership` {#function-note-ownership}

<div class="function-declaration" id="function-note-ownership-1"><pre><code><span class="k">void</span> note_ownership(
    <a href="../../core/string-id/string-id.html">core::string_id</a> point,
    <a href="../../core/string-id/string-id.html">core::string_id</a> key,
    <a href="../module-types/module-id.html">module_id</a> owner);</code></pre></div>

<p>Records that <code>owner</code> provided <code>key</code> under <code>point</code> (a component type name under &quot;<code>ecs.component</code>&quot;, say) so the inspector can show &quot;provided by&quot;. Called by consumers in <code>attach()</code>; the manager forgets a module&#x27;s entries when it unloads.</p>

#### `owner_of` {#function-owner-of}

<div class="function-declaration" id="function-owner-of-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../module-types/module-id.html">module_id</a> owner_of(
    <a href="../../core/string-id/string-id.html">core::string_id</a> point,
    <a href="../../core/string-id/string-id.html">core::string_id</a> key) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The module that provided the key, or an invalid id.</p>

#### `registrations` {#function-registrations}

<div class="function-declaration" id="function-registrations-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../module-registration-set/module-registration-set.html">module_registration_set</a> &amp;registrations(
    <a href="../module-types/module-id.html">module_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `root_directory` {#function-root-directory}

<div class="function-declaration" id="function-root-directory-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::filesystem::path &amp;root_directory(
    <a href="../module-types/module-id.html">module_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The directory the manifest lives in; empty for static modules.</p>

#### `size` {#function-size}

<div class="function-declaration" id="function-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-usize">usize</a> size() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of registered modules.</p>

#### `state` {#function-state}

<div class="function-declaration" id="function-state-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../module-types.html#symbol-module-state">module_state</a> state(
    <a href="../module-types/module-id.html">module_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

### Private

#### `add_record` {#function-add-record}

<div class="function-declaration" id="function-add-record-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../module-registry.html">module_record</a> &amp;add_record(
    <a href="../../core/string-id/string-id.html">core::string_id</a> name);</code></pre></div>

#### `append_load_order` {#function-append-load-order}

<div class="function-declaration" id="function-append-load-order-1"><pre><code><span class="k">void</span> append_load_order(
    <a href="../module-types/module-id.html">module_id</a> id);</code></pre></div>

#### `record` {#function-record}

<div class="function-declaration" id="function-record-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../module-registry.html">module_record</a> *record(
    <a href="../module-types/module-id.html">module_id</a> id) <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-record-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../module-registry.html">module_record</a> *record(
    <a href="../module-types/module-id.html">module_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `record_at` {#function-record-at}

<div class="function-declaration" id="function-record-at-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../module-registry.html">module_record</a> &amp;record_at(
    <a href="../../core/types.html#symbol-u16">u16</a> index) <span class="k">noexcept</span>;</code></pre></div>

#### `remove_load_order` {#function-remove-load-order}

<div class="function-declaration" id="function-remove-load-order-1"><pre><code><span class="k">void</span> remove_load_order(
    <a href="../module-types/module-id.html">module_id</a> id);</code></pre></div>

#### `retire_generation` {#function-retire-generation}

<div class="function-declaration" id="function-retire-generation-1"><pre><code><span class="k">void</span> retire_generation(
    <a href="../module-registry.html">module_record</a> &amp;record);</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
