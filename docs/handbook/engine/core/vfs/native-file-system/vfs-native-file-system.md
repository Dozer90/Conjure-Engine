---
kind: class
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [native_file_system.h](../native-file-system.md)

# vfs::native_file_system
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core::vfs` |
| Include File Path | `engine/include/conjure/core/vfs/native_file_system.h` |
| Source File Path | `engine/src/core/vfs/native_file_system.cpp` |
| API ↗ | [Files and the virtual file system](../../../../../api/core/files.md) |

## Syntax

```cpp
class CONJURE_API native_file_system final : public file_system
```

## Inheritance Hierarchy


[`vfs::file_system`](../file-system/vfs-file-system.md) → `vfs::native_file_system`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> native_file_system(
    std::filesystem::path rootPath,
    <span class="k">bool</span> readOnly = false);</code></td><td>Wraps <code>rootPath</code>. The directory does not have to exist yet: the first write creates it. <code>A</code> read-only instance rejects every mutating call.</td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~native_file_system() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mReadOnly</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRoot</code></td><td><code class="signature">std::filesystem::path</code></td><td><code class="signature">{}</code></td><td></td></tr>
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

#### `root` {#function-root}

<div class="function-declaration" id="function-root-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::filesystem::path &amp;root() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The root every path is resolved against, as given to the constructor (lexically normalised, without a trailing separator).</p>

#### `stat` {#function-stat}

<div class="function-declaration" id="function-stat-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;<a href="../file-system/vfs-file-stat.html">file_stat</a>&gt; stat(
    std::string_view path) <span class="k">override</span>;</code></pre></div>

#### `to_native` {#function-to-native}

<div class="function-declaration" id="function-to-native-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;std::filesystem::path&gt; to_native(
    std::string_view path) <span class="k">const</span>;</code></pre></div>

<p>The on-disk path a backend-relative path maps to; &quot;&quot; and &quot;/&quot; map to the root itself. Checked exactly like every file method: a path that would leave the root (&quot;../x&quot;, &quot;/../x&quot;, &quot;<code>C</code>:/x&quot;, &quot;//server/x&quot;) is <code>PERMISSION_DENIED</code>, so the result is always the root or below it and is safe to hand to code outside the file system (a process, a dynamic library). Purely lexical: the path does not have to exist, and a read-only instance maps paths like any other. On Windows a path beyond the classic length limit, or one with a name that is not portable (<code>path_utils::is_portable_name</code>), comes back in the extended-length form, the one that addresses it.</p>

#### `write_file` {#function-write-file}

<div class="function-declaration" id="function-write-file-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;<span class="k">void</span>&gt; write_file(
    std::string_view path,
    std::span&lt;<span class="k">const</span> std::byte&gt; data) <span class="k">override</span>;</code></pre></div>

#### `write_file_atomic` {#function-write-file-atomic}

<div class="function-declaration" id="function-write-file-atomic-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;<span class="k">void</span>&gt; write_file_atomic(
    std::string_view path,
    std::span&lt;<span class="k">const</span> std::byte&gt; data) <span class="k">override</span>;</code></pre></div>

### Private

#### `native_path_of` {#function-native-path-of}

<div class="function-declaration" id="function-native-path-of-1"><pre><code>[[<span class="na">nodiscard</span>]] std::filesystem::path native_path_of(
    std::string_view relative) <span class="k">const</span>;</code></pre></div>

<p>Maps a path that <code>resolve_backend_path</code> already accepted (normalised, relative, inside the root) to its on-disk path. Unchecked: only the checked entry points call it.</p>

#### `resolve` {#function-resolve}

<div class="function-declaration" id="function-resolve-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;std::filesystem::path&gt; resolve(
    std::string_view path) <span class="k">const</span>;</code></pre></div>

#### `resolve_writable` {#function-resolve-writable}

<div class="function-declaration" id="function-resolve-writable-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;std::filesystem::path&gt; resolve_writable(
    std::string_view path) <span class="k">const</span>;</code></pre></div>

#### `write_bytes` {#function-write-bytes}

<div class="function-declaration" id="function-write-bytes-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;<span class="k">void</span>&gt; write_bytes(
    <span class="k">const</span> std::filesystem::path &amp;nativePath,
    std::string_view displayPath,
    std::span&lt;<span class="k">const</span> std::byte&gt; data) <span class="k">const</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
