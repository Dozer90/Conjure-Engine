---
kind: class
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [memory_file_system.h](../memory-file-system.md)

# vfs::memory_file_system
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core::vfs` |
| Include File Path | `engine/include/conjure/core/vfs/memory_file_system.h` |
| Source File Path | `engine/src/core/vfs/memory_file_system.cpp` |
| API ↗ | [Files and the virtual file system](../../../../../api/core/files.md) |

## Syntax

```cpp
class CONJURE_API memory_file_system final : public file_system
```

## Inheritance Hierarchy


[`vfs::file_system`](../file-system/vfs-file-system.md) → `vfs::memory_file_system`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">memory_file_system();</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~memory_file_system() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mMutex</code></td><td><code class="signature"><span class="k">mutable</span> std::mutex</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mReadOnly</code></td><td><code class="signature">std::atomic&lt;<span class="k">bool</span>&gt;</code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRoot</code></td><td><code class="signature">std::unique_ptr&lt;<a href="../../../scene/prefab/prefab-builder-node.html">node</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Write<wbr>Counter</code></td><td><code class="signature"><a href="../../types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `create_directories` {#function-create-directories}

<div class="function-declaration" id="function-create-directories-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;<span class="k">void</span>&gt; create_directories(
    std::string_view path) <span class="k">override</span>;</code></pre></div>

#### `enumerate` {#function-enumerate}

<div class="function-declaration" id="function-enumerate-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;<span class="k">void</span>&gt; enumerate(
    std::string_view directory,
    <a href="../file-system.html#symbol-vfs-enumerate-flags">enumerate_flags</a> flags,
    <span class="k">const</span> <a href="../file-system.html#symbol-vfs-enumerate-callback">enumerate_callback</a> &amp;callback) <span class="k">override</span>;</code></pre></div>

#### `is_read_only` {#function-is-read-only}

<div class="function-declaration" id="function-is-read-only-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_read_only() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `open_read` {#function-open-read}

<div class="function-declaration" id="function-open-read-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;std::unique_ptr&lt;<a href="../file-system/vfs-file.html">file</a>&gt;&gt; open_read(
    std::string_view path) <span class="k">override</span>;</code></pre></div>

#### `read_file` {#function-read-file}

<div class="function-declaration" id="function-read-file-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;std::vector&lt;std::byte&gt;&gt; read_file(
    std::string_view path) <span class="k">override</span>;</code></pre></div>

#### `remove` {#function-remove}

<div class="function-declaration" id="function-remove-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;<span class="k">void</span>&gt; remove(
    std::string_view path) <span class="k">override</span>;</code></pre></div>

#### `remove_all` {#function-remove-all}

<div class="function-declaration" id="function-remove-all-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;<span class="k">void</span>&gt; remove_all(
    std::string_view path) <span class="k">override</span>;</code></pre></div>

#### `rename` {#function-rename}

<div class="function-declaration" id="function-rename-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;<span class="k">void</span>&gt; rename(
    std::string_view from,
    std::string_view to) <span class="k">override</span>;</code></pre></div>

#### `set_read_only` {#function-set-read-only}

<div class="function-declaration" id="function-set-read-only-1"><pre><code><span class="k">void</span> set_read_only(
    <span class="k">bool</span> readOnly) <span class="k">noexcept</span>;</code></pre></div>

<p>Freezes (or thaws) the tree: while read-only, every mutating call fails with <code>PERMISSION_DENIED</code>. Typically used after populating a fixture.</p>

#### `stat` {#function-stat}

<div class="function-declaration" id="function-stat-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;<a href="../file-system/vfs-file-stat.html">file_stat</a>&gt; stat(
    std::string_view path) <span class="k">override</span>;</code></pre></div>

#### `write_counter` {#function-write-counter}

<div class="function-declaration" id="function-write-counter-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-u64">u64</a> write_counter() <span class="k">const</span>;</code></pre></div>

<p>Number of mutations performed so far; equals the write time of the newest entry.</p>

#### `write_file` {#function-write-file}

<div class="function-declaration" id="function-write-file-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;<span class="k">void</span>&gt; write_file(
    std::string_view path,
    std::span&lt;<span class="k">const</span> std::byte&gt; data) <span class="k">override</span>;</code></pre></div>

#### `write_file_atomic` {#function-write-file-atomic}

<div class="function-declaration" id="function-write-file-atomic-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;<span class="k">void</span>&gt; write_file_atomic(
    std::string_view path,
    std::span&lt;<span class="k">const</span> std::byte&gt; data) <span class="k">override</span>;</code></pre></div>

### Private

#### `check_writable` {#function-check-writable}

<div class="function-declaration" id="function-check-writable-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;<span class="k">void</span>&gt; check_writable() <span class="k">const</span>;</code></pre></div>

#### `ensure_directory_locked` {#function-ensure-directory-locked}

<div class="function-declaration" id="function-ensure-directory-locked-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;<a href="../../../scene/prefab/prefab-builder-node.html">node</a> *&gt; ensure_directory_locked(
    std::string_view normalizedPath);</code></pre></div>

#### `remove_locked` {#function-remove-locked}

<div class="function-declaration" id="function-remove-locked-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;<span class="k">void</span>&gt; remove_locked(
    std::string_view path,
    <span class="k">bool</span> recursive);</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
