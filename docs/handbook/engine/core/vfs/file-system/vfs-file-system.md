---
kind: class
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [file_system.h](../file-system.md)

# vfs::file_system
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core::vfs` |
| Include File Path | `engine/include/conjure/core/vfs/file_system.h` |
| Source File Path | `engine/src/core/vfs/file_system.cpp` |
| API ↗ | [Files and the virtual file system](../../../../../api/core/files.md) |

## Syntax

```cpp
class CONJURE_API file_system
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">file_system();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Additional constructor overloads have been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">virtual</span> ~file_system();</code></td><td></td></tr>
</tbody>
</table>



## Functions

### Public

#### `create_directories` {#function-create-directories}

<div class="function-declaration" id="function-create-directories-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../result/result.html">result</a>&lt;<span class="k">void</span>&gt; create_directories(
    std::string_view path) = <span class="mi">0</span>;</code></pre></div>

<p>Creates a directory and every missing parent; an existing directory is not an error. <code>ALREADY_EXISTS</code> when a file occupies the path, <code>IO_FAILURE</code> when a file occupies one of the parents, <code>INVALID_ARGUMENT</code> when a segment of the path is not a portable name (header comment).</p>

#### `enumerate` {#function-enumerate}

<div class="function-declaration" id="function-enumerate-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../result/result.html">result</a>&lt;<span class="k">void</span>&gt; enumerate(
    std::string_view directory,
    <a href="../file-system.html#symbol-vfs-enumerate-flags">enumerate_flags</a> flags,
    <span class="k">const</span> <a href="../file-system.html#symbol-vfs-enumerate-callback">enumerate_callback</a> &amp;callback) = <span class="mi">0</span>;</code></pre></div>

<p>Lists the entries of a directory in ascending path order, each as a path relative to the backend root (&quot;dir/sub/<code>file.txt</code>&quot;). <code>NOT_FOUND</code> when the directory is missing, <code>INVALID_ARGUMENT</code> when the path names a file.</p>

#### `exists` {#function-exists}

<div class="function-declaration" id="function-exists-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <span class="k">bool</span> exists(
    std::string_view path);</code></pre></div>

<p>True when stat succeeds.</p>

#### `is_read_only` {#function-is-read-only}

<div class="function-declaration" id="function-is-read-only-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <span class="k">bool</span> is_read_only() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when every mutating operation fails with <code>PERMISSION_DENIED</code>.</p>

#### `open_read` {#function-open-read}

<div class="function-declaration" id="function-open-read-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../result/result.html">result</a>&lt;std::unique_ptr&lt;<a href="vfs-file.html">file</a>&gt;&gt; open_read(
    std::string_view path) = <span class="mi">0</span>;</code></pre></div>

<p>Opens a file for random-access reads. <code>NOT_FOUND</code> when missing, <code>INVALID_ARGUMENT</code> for a directory.</p>

#### `read_file` {#function-read-file}

<div class="function-declaration" id="function-read-file-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../result/result.html">result</a>&lt;std::vector&lt;std::byte&gt;&gt; read_file(
    std::string_view path) = <span class="mi">0</span>;</code></pre></div>

<p>Reads a whole file. <code>NOT_FOUND</code> when missing, <code>INVALID_ARGUMENT</code> for a directory.</p>

#### `read_text_file` {#function-read-text-file}

<div class="function-declaration" id="function-read-text-file-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../result/result.html">result</a>&lt;std::string&gt; read_text_file(
    std::string_view path);</code></pre></div>

<p>Reads a whole file as text. <code>A</code> leading UTF-8 <code><a href="../../types.html#symbol-byte">byte</a></code> order mark is dropped; nothing else is transcoded. Implemented on top of <code>read_file</code>.</p>

#### `remove` {#function-remove}

<div class="function-declaration" id="function-remove-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../result/result.html">result</a>&lt;<span class="k">void</span>&gt; remove(
    std::string_view path) = <span class="mi">0</span>;</code></pre></div>

<p>Deletes a file or an empty directory. <code>INVALID_STATE</code> for a non-empty directory, <code>INVALID_ARGUMENT</code> for the root, <code>NOT_FOUND</code> when missing.</p>

#### `remove_all` {#function-remove-all}

<div class="function-declaration" id="function-remove-all-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../result/result.html">result</a>&lt;<span class="k">void</span>&gt; remove_all(
    std::string_view path);</code></pre></div>

<p>Deletes a file or a directory with everything inside it. <code>INVALID_ARGUMENT</code> for the root, <code>NOT_FOUND</code> when missing. The default walks the tree with enumerate and remove.</p>

#### `rename` {#function-rename}

<div class="function-declaration" id="function-rename-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../result/result.html">result</a>&lt;<span class="k">void</span>&gt; rename(
    std::string_view from,
    std::string_view to) = <span class="mi">0</span>;</code></pre></div>

<p>Moves a file or directory. An existing file at <code>to</code> is replaced (even while a reader holds it open); an existing directory there is <code>ALREADY_EXISTS</code>. The parent of <code>to</code> must exist (<code>NOT_FOUND</code>). Renaming an entry onto itself succeeds and changes nothing; renaming it onto another spelling of itself on a case-insensitive volume changes the case. <code>to</code> is a name this call creates, so it must be portable (header comment, <code>INVALID_ARGUMENT</code>, checked before <code>from</code> is looked up); <code>from</code> may be any name that exists.</p>

#### `stat` {#function-stat}

<div class="function-declaration" id="function-stat-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../result/result.html">result</a>&lt;<a href="vfs-file-stat.html">file_stat</a>&gt; stat(
    std::string_view path) = <span class="mi">0</span>;</code></pre></div>

<p>Describes an entry. &quot;&quot; describes the root. <code>NOT_FOUND</code> when missing.</p>

#### `write_file` {#function-write-file}

<div class="function-declaration" id="function-write-file-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../result/result.html">result</a>&lt;<span class="k">void</span>&gt; write_file(
    std::string_view path,
    std::span&lt;<span class="k">const</span> std::byte&gt; data) = <span class="mi">0</span>;</code></pre></div>

<p>Creates or truncates a file and writes <code>data</code>. Missing parent directories are created. <code>INVALID_ARGUMENT</code> when a directory occupies the path or a segment of it is not a portable name (header comment), <code>IO_FAILURE</code> when a file occupies one of the parents.</p>

#### `write_file_atomic` {#function-write-file-atomic}

<div class="function-declaration" id="function-write-file-atomic-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../result/result.html">result</a>&lt;<span class="k">void</span>&gt; write_file_atomic(
    std::string_view path,
    std::span&lt;<span class="k">const</span> std::byte&gt; data) = <span class="mi">0</span>;</code></pre></div>

<p>Like <code>write_file</code>, but the path never holds a partial file, for a concurrent reader or after a crash: the data goes to a temporary sibling (&quot;&lt;path&gt;.tmp-&lt;uuid&gt;&quot;) that then replaces the target in one step. <code>A</code> backend that stores on a device flushes the temporary to it before the swap, so after a power cut the path holds the old contents or the new ones, never a torn file (the swap itself may be lost, which leaves the old contents and possibly the temporary). Two limits: a volume whose driver has no flush at all still takes the write, without this guarantee, and logs one warning per process; on macOS the flush is fsync, which hands the data to the drive without waiting for the drive&#x27;s own write cache, so the guarantee there is only as strong as that cache. That flush costs one device round trip per call, typically milliseconds: use <code>write_file</code> for output that can be regenerated.</p>

#### `write_text_file` {#function-write-text-file}

<div class="function-declaration" id="function-write-text-file-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;<span class="k">void</span>&gt; write_text_file(
    std::string_view path,
    std::string_view text);</code></pre></div>

<p>Writes UTF-8 text with <code>write_file</code>.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->
