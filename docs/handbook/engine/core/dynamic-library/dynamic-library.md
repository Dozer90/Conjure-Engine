---
kind: class
---

[Engine](../../../engine.md) → [Core](../../core.md) → [dynamic_library.h](../dynamic-library.md)

# dynamic_library
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/dynamic_library.h` |
| Source File Path | `engine/src/core/system/dynamic_library.cpp` |
| API ↗ | [Processes, libraries and images](../../../../api/core/system.md) |

## Syntax

```cpp
class CONJURE_API dynamic_library
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">dynamic_library();</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">dynamic_library(
    <a href="dynamic-library.html">dynamic_library</a> &amp;&amp;other) <span class="k">noexcept</span>;</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~dynamic_library();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mHandle</code></td><td><code class="signature"><span class="k">void</span> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPath</code></td><td><code class="signature">std::filesystem::path</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `decorate_name` {#function-decorate-name}

<div class="function-declaration" id="function-decorate-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> std::string decorate_name(
    std::string_view baseName);</code></pre></div>

<p>The platform file name of a library: &quot;<code>conjure_foo</code>&quot; becomes &quot;<code>conjure_foo.dll</code>&quot;, &quot;<code>libconjure_foo.so</code>&quot; or &quot;<code>libconjure_foo.dylib</code>&quot;. <code>A</code> name that already ends in the platform extension is returned unchanged.</p>

#### `extension` {#function-extension}

<div class="function-declaration" id="function-extension-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> std::string_view extension() <span class="k">noexcept</span>;</code></pre></div>

<p>The platform&#x27;s library extension including the dot: &quot;.dll&quot;, &quot;.so&quot; or &quot;.dylib&quot;.</p>

#### `is_loaded` {#function-is-loaded}

<div class="function-declaration" id="function-is-loaded-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_loaded() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True while a library is loaded.</p>

#### `load` {#function-load}

<div class="function-declaration" id="function-load-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../result/result.html">result</a>&lt;<a href="dynamic-library.html">dynamic_library</a>&gt; load(
    <span class="k">const</span> std::filesystem::path &amp;path);</code></pre></div>

<p>Loads a library (LoadLibraryW / dlopen with <code>RTLD_NOW</code> | <code>RTLD_LOCAL</code>). <code>A</code> bare file name is resolved through the operating system&#x27;s library search path. Fails with <code>INVALID_ARGUMENT</code> for an empty path, <code>NOT_FOUND</code> when the file does not exist and <code>BACKEND_FAILURE</code> otherwise; the message carries the operating system&#x27;s error text.</p>

#### `path` {#function-path}

<div class="function-declaration" id="function-path-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::filesystem::path &amp;path() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The path given to <code><a href="#function-load-1">load</a>()</code>, empty when nothing is loaded.</p>

#### `prefix` {#function-prefix}

<div class="function-declaration" id="function-prefix-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> std::string_view prefix() <span class="k">noexcept</span>;</code></pre></div>

<p>The platform&#x27;s library prefix: &quot;&quot; on Windows, &quot;lib&quot; elsewhere.</p>

#### `symbol` {#function-symbol}

<div class="function-declaration" id="function-symbol-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">void</span> *symbol(
    <span class="k">const</span> <span class="k">char</span> *name) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Address of an exported symbol, <code>nullptr</code> when it does not exist or nothing is loaded.</p>

#### `symbol_as` {#function-symbol-as}

<div class="function-declaration" id="function-symbol-as-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> Fn&gt; [[<span class="na">nodiscard</span>]] Fn symbol_as(
    <span class="k">const</span> <span class="k">char</span> *name) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p><code><a href="#function-symbol-1">symbol</a>()</code> converted to a pointer type, typically a function pointer. Returns <code>nullptr</code> when the symbol does not exist.</p>

#### `unload` {#function-unload}

<div class="function-declaration" id="function-unload-1"><pre><code><span class="k">void</span> unload() <span class="k">noexcept</span>;</code></pre></div>

<p>Unloads the library now. Safe to call when nothing is loaded.</p>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="dynamic-library.html">dynamic_library</a> &amp;<span class="k">operator</span>=(
    <a href="dynamic-library.html">dynamic_library</a> &amp;&amp;other) <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
