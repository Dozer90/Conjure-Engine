---
kind: class
---

[Engine](../../../engine.md) → [Modules](../../modules.md) → [module_registrar.h](../module-registrar.md)

# module_registrar
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::modules` |
| Include File Path | `engine/include/conjure/modules/module_registrar.h` |
| Source File Path | `engine/src/modules/module_registrar.cpp` |
| API ↗ | [Writing a module](../../../../api/modules/writing-a-module.md) |

## Syntax

```cpp
class CONJURE_API module_registrar
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> module_registrar(
    <a href="../module-registration-set/module-registration-set.html">module_registration_set</a> &amp;set) <span class="k">noexcept</span>;</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~module_registrar();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mSet</code></td><td><code class="signature"><a href="../module-registration-set/module-registration-set.html">module_<wbr>registration_<wbr>set</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `fail` {#function-fail}

<div class="function-declaration" id="function-fail-1"><pre><code><span class="k">void</span> fail(
    std::string_view reason);</code></pre></div>

<p>Records a reason the module cannot load. Reasons accumulate; one is enough to reject the load after the entry returns.</p>

#### `register_extension` {#function-register-extension}

<div class="function-declaration" id="function-register-extension-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> DescType&gt; <span class="k">void</span> register_extension(
    <span class="k">const</span> DescType &amp;desc);</code></pre></div>

<p>Stores a trivially copyable descriptor under <code>DescType::EXTENSION_POINT</code>. The consuming area defines the descriptor; strings inside may point at module literals, consumers copy what they keep.</p>

#### `register_managed_assembly` {#function-register-managed-assembly}

<div class="function-declaration" id="function-register-managed-assembly-1"><pre><code><span class="k">void</span> register_managed_assembly(
    std::string_view relativePath);</code></pre></div>

<p>Adds a managed assembly path relative to the module root. Duplicates are ignored. <code>A</code> path that leaves the root (a &quot;..&quot; segment, a leading separator, a drive) asserts and is recorded as a failure, like <code><a href="#function-fail-1">fail</a>()</code>, so the load is rejected.</p>

#### `registrations` {#function-registrations}

<div class="function-declaration" id="function-registrations-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../module-registration-set/module-registration-set.html">module_registration_set</a> &amp;registrations() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The set being filled.</p>

#### `set_lifecycle` {#function-set-lifecycle}

<div class="function-declaration" id="function-set-lifecycle-1"><pre><code><span class="k">void</span> set_lifecycle(
    <span class="k">const</span> <a href="../module-registration-set/module-lifecycle.html">module_lifecycle</a> &amp;lifecycle) <span class="k">noexcept</span>;</code></pre></div>

<p>Installs the lifecycle callbacks; the last call wins.</p>

### Private

#### `append_extension` {#function-append-extension}

<div class="function-declaration" id="function-append-extension-1"><pre><code><span class="k">void</span> append_extension(
    <a href="../../core/string-id/string-id.html">core::string_id</a> point,
    <span class="k">const</span> <span class="k">void</span> *data,
    <a href="../../core/types.html#symbol-usize">usize</a> size,
    <a href="../../core/types.html#symbol-usize">usize</a> alignment);</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
