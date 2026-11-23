---
kind: class
---

[Engine](../../../engine.md) → [Scripting](../../scripting.md) → [script_class_registry.h](../script-class-registry.md)

# script_class_registry
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::scripting` |
| Include File Path | `engine/include/conjure/scripting/script_class_registry.h` |
| Source File Path | `engine/src/scripting/script_class_registry.cpp` |
| API ↗ | [Scripting](../../../../api/scripting/index.md), [Scripts, components and the runtime](../../../../api/scripting/scripts-and-components.md) |

## Syntax

```cpp
class CONJURE_API script_class_registry
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">script_class_registry();</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">script_class_registry(
    <a href="script-class-registry.html">script_class_registry</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~script_class_registry();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mActive</code></td><td><code class="signature"><a href="script-class-registry-table.html">table</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mStaging</code></td><td><code class="signature"><a href="script-class-registry-table.html">table</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mStagingOpen</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `begin_staging` {#function-begin-staging}

<div class="function-declaration" id="function-begin-staging-1"><pre><code><span class="k">void</span> begin_staging();</code></pre></div>

<p>Empties the staging half and opens it for <code>stage_class</code>/<code>stage_field</code>.</p>

#### `classes` {#function-classes}

<div class="function-declaration" id="function-classes-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="script-class-info.html">script_class_info</a>&gt; classes() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Active classes in registration order.</p>

#### `clear` {#function-clear}

<div class="function-declaration" id="function-clear-1"><pre><code><span class="k">void</span> clear();</code></pre></div>

<p>Empties both halves.</p>

#### `commit_staging` {#function-commit-staging}

<div class="function-declaration" id="function-commit-staging-1"><pre><code><span class="k">void</span> commit_staging();</code></pre></div>

<p>Sorts every staged class&#x27; fields, makes the staging half active and closes staging. Requires an open staging (<code>begin_staging</code>); otherwise asserts and leaves the active table untouched.</p>

#### `discard_staging` {#function-discard-staging}

<div class="function-declaration" id="function-discard-staging-1"><pre><code><span class="k">void</span> discard_staging();</code></pre></div>

<p>Drops the staging half; the active schema is untouched.</p>

#### `find` {#function-find}

<div class="function-declaration" id="function-find-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="script-class-info.html">script_class_info</a> *find(
    <a href="../../core/types.html#symbol-u64">u64</a> classHash) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The active class with this hash, or <code>nullptr</code>.</p>

<div class="function-declaration" id="function-find-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="script-class-info.html">script_class_info</a> *find(
    std::string_view fullName) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The active class with this full name, or <code>nullptr</code>.</p>

#### `find_staged` {#function-find-staged}

<div class="function-declaration" id="function-find-staged-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="script-class-info.html">script_class_info</a> *find_staged(
    <a href="../../core/types.html#symbol-u64">u64</a> classHash) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The staged class with this hash, or <code>nullptr</code>.</p>

#### `index_of` {#function-index-of}

<div class="function-declaration" id="function-index-of-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> index_of(
    <a href="../../core/types.html#symbol-u64">u64</a> classHash) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Dense index of an active class (the order of <code><a href="#function-classes-1">classes</a>()</code>), or <code>UINT32_MAX</code>.</p>

#### `is_staging_open` {#function-is-staging-open}

<div class="function-declaration" id="function-is-staging-open-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_staging_open() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `size` {#function-size}

<div class="function-declaration" id="function-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-usize">usize</a> size() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `stage_class` {#function-stage-class}

<div class="function-declaration" id="function-stage-class-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; stage_class(
    <a href="script-class-info.html">script_class_info</a> info);</code></pre></div>

<p>Adds a class to the staging half. <code>ALREADY_EXISTS</code> for a hash already staged, <code>INVALID_STATE</code> when no staging is open.</p>

#### `stage_field` {#function-stage-field}

<div class="function-declaration" id="function-stage-field-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; stage_field(
    <a href="../../core/types.html#symbol-u64">u64</a> classHash,
    <a href="script-field-info.html">script_field_info</a> field);</code></pre></div>

<p>Adds a field to a staged class. <code>CLASS_NOT_FOUND</code>, <code>ALREADY_EXISTS</code> for a duplicate field hash.</p>

#### `staged_classes` {#function-staged-classes}

<div class="function-declaration" id="function-staged-classes-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="script-class-info.html">script_class_info</a>&gt; staged_classes() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

### Private

#### `find_in` {#function-find-in}

<div class="function-declaration" id="function-find-in-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <span class="k">const</span> <a href="script-class-info.html">script_class_info</a> *find_in(
    <span class="k">const</span> <a href="script-class-registry-table.html">table</a> &amp;source,
    <a href="../../core/types.html#symbol-u64">u64</a> classHash) <span class="k">noexcept</span>;</code></pre></div>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="script-class-registry.html">script_class_registry</a> &amp;<span class="k">operator</span>=(
    <a href="script-class-registry.html">script_class_registry</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
