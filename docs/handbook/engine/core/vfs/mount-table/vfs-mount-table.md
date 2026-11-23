---
kind: class
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [mount_table.h](../mount-table.md)

# vfs::mount_table
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core::vfs` |
| Include File Path | `engine/include/conjure/core/vfs/mount_table.h` |
| Source File Path | `engine/src/core/vfs/mount_table.cpp` |
| API ↗ | [Files and the virtual file system](../../../../../api/core/files.md) |

## Syntax

```cpp
class CONJURE_API mount_table final : public file_system
```

## Inheritance Hierarchy


[`vfs::file_system`](../file-system/vfs-file-system.md) → `vfs::mount_table`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">mount_table();</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~mount_table() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mMounts</code></td><td><code class="signature">std::<wbr>map&lt;<wbr>std::<wbr>string,<wbr> std::<wbr>shared_<wbr>ptr&lt;<wbr><a href="../file-system/vfs-file-system.html">file_<wbr>system</a>&gt;,<wbr> std::<wbr>less&lt;<wbr>&gt;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mMutex</code></td><td><code class="signature"><span class="k">mutable</span> std::mutex</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `create_directories` {#function-create-directories}

<div class="function-declaration" id="function-create-directories-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;<span class="k">void</span>&gt; create_directories(
    std::string_view uri) <span class="k">override</span>;</code></pre></div>

#### `enumerate` {#function-enumerate}

<div class="function-declaration" id="function-enumerate-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;<span class="k">void</span>&gt; enumerate(
    std::string_view directory,
    <a href="../file-system.html#symbol-vfs-enumerate-flags">enumerate_flags</a> flags,
    <span class="k">const</span> <a href="../file-system.html#symbol-vfs-enumerate-callback">enumerate_callback</a> &amp;callback) <span class="k">override</span>;</code></pre></div>

#### `exists` {#function-exists}

<div class="function-declaration" id="function-exists-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> exists(
    std::string_view uri) <span class="k">override</span>;</code></pre></div>

#### `find_mount` {#function-find-mount}

<div class="function-declaration" id="function-find-mount-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../file-system/vfs-file-system.html">file_system</a> *find_mount(
    std::string_view scheme) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The backend bound to a scheme (&quot;assets&quot; or &quot;assets://&quot;), or <code>nullptr</code>. The pointer stays valid until the scheme is unmounted.</p>

#### `is_read_only` {#function-is-read-only}

<div class="function-declaration" id="function-is-read-only-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_read_only() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

<p>True when at least one backend is mounted and all of them are read-only.</p>

#### `mount` {#function-mount}

<div class="function-declaration" id="function-mount-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;<span class="k">void</span>&gt; mount(
    std::string_view scheme,
    std::shared_ptr&lt;<a href="../file-system/vfs-file-system.html">file_system</a>&gt; backend);</code></pre></div>

<p>Binds a scheme (&quot;assets&quot; or &quot;assets://&quot;) to a backend. <code>INVALID_ARGUMENT</code> for a <code>null</code> backend, a malformed scheme or a mount table that already contains this one (directly or through other mount tables), <code>ALREADY_EXISTS</code> when the scheme is taken. One backend may be mounted under several schemes.</p>

#### `open_read` {#function-open-read}

<div class="function-declaration" id="function-open-read-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;std::unique_ptr&lt;<a href="../file-system/vfs-file.html">file</a>&gt;&gt; open_read(
    std::string_view uri) <span class="k">override</span>;</code></pre></div>

#### `read_file` {#function-read-file}

<div class="function-declaration" id="function-read-file-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;std::vector&lt;std::byte&gt;&gt; read_file(
    std::string_view uri) <span class="k">override</span>;</code></pre></div>

#### `read_text_file` {#function-read-text-file}

<div class="function-declaration" id="function-read-text-file-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;std::string&gt; read_text_file(
    std::string_view uri) <span class="k">override</span>;</code></pre></div>

#### `remove` {#function-remove}

<div class="function-declaration" id="function-remove-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;<span class="k">void</span>&gt; remove(
    std::string_view uri) <span class="k">override</span>;</code></pre></div>

#### `remove_all` {#function-remove-all}

<div class="function-declaration" id="function-remove-all-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;<span class="k">void</span>&gt; remove_all(
    std::string_view uri) <span class="k">override</span>;</code></pre></div>

#### `rename` {#function-rename}

<div class="function-declaration" id="function-rename-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;<span class="k">void</span>&gt; rename(
    std::string_view from,
    std::string_view to) <span class="k">override</span>;</code></pre></div>

<p>When both URIs lead to the same backend, directly or through nested mount tables, it renames (so two schemes over one backend behave like one). Across backends a file is copied and the source removed; a directory is <code>UNSUPPORTED</code>. The base contract&#x27;s codes still apply there, judged by the backends that store the two files and in the order of a backend&#x27;s own rename: <code>PERMISSION_DENIED</code> for an escape or a read-only backend at either end, <code>INVALID_ARGUMENT</code> for the root of a backend at either end and for a name at <code>to</code> that is not portable, and only then the lookups: <code>NOT_FOUND</code> for a missing <code>from</code>, <code>ALREADY_EXISTS</code> for a directory at <code>to</code>, <code>NOT_FOUND</code> when the parent of <code>to</code> is missing. All of it is checked before anything is copied. The copy is written atomically. If the source cannot be removed afterwards, a copy that created a new file is removed again, but a file it replaced is not restored.</p>

#### `schemes` {#function-schemes}

<div class="function-declaration" id="function-schemes-1"><pre><code>[[<span class="na">nodiscard</span>]] std::vector&lt;std::string&gt; schemes() <span class="k">const</span>;</code></pre></div>

<p>Every mounted scheme in ascending order.</p>

#### `split` {#function-split}

<div class="function-declaration" id="function-split-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../result/result.html">result</a>&lt;std::pair&lt;std::string_view, std::string_view&gt;&gt; split(
    std::string_view uri);</code></pre></div>

<p>Splits &quot;scheme://path&quot; into its two parts (&quot;assets://a/<code>b.png</code>&quot; -&gt; {&quot;assets&quot;, &quot;a/<code>b.png</code>&quot;}). <code>INVALID_ARGUMENT</code> when there is no &quot;://&quot; or the scheme is empty or contains characters other than letters, digits, &#x27;-&#x27;, <code>&#x27;_&#x27;</code>, &#x27;.&#x27; and &#x27;+&#x27;.</p>

#### `stat` {#function-stat}

<div class="function-declaration" id="function-stat-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;<a href="../file-system/vfs-file-stat.html">file_stat</a>&gt; stat(
    std::string_view uri) <span class="k">override</span>;</code></pre></div>

#### `unmount` {#function-unmount}

<div class="function-declaration" id="function-unmount-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;<span class="k">void</span>&gt; unmount(
    std::string_view scheme);</code></pre></div>

<p>Removes a binding. <code>NOT_FOUND</code> when the scheme is not mounted.</p>

#### `write_file` {#function-write-file}

<div class="function-declaration" id="function-write-file-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;<span class="k">void</span>&gt; write_file(
    std::string_view uri,
    std::span&lt;<span class="k">const</span> std::byte&gt; data) <span class="k">override</span>;</code></pre></div>

#### `write_file_atomic` {#function-write-file-atomic}

<div class="function-declaration" id="function-write-file-atomic-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;<span class="k">void</span>&gt; write_file_atomic(
    std::string_view uri,
    std::span&lt;<span class="k">const</span> std::byte&gt; data) <span class="k">override</span>;</code></pre></div>

### Private

#### `contains` {#function-contains}

<div class="function-declaration" id="function-contains-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> contains(
    <span class="k">const</span> <a href="../file-system/vfs-file-system.html">file_system</a> &amp;backend) <span class="k">const</span>;</code></pre></div>

#### `find_shared` {#function-find-shared}

<div class="function-declaration" id="function-find-shared-1"><pre><code>[[<span class="na">nodiscard</span>]] std::shared_ptr&lt;<a href="../file-system/vfs-file-system.html">file_system</a>&gt; find_shared(
    std::string_view scheme) <span class="k">const</span>;</code></pre></div>

#### `route` {#function-route}

<div class="function-declaration" id="function-route-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;<a href="vfs-mount-table-route-target.html">route_target</a>&gt; route(
    std::string_view uri) <span class="k">const</span>;</code></pre></div>

#### `route_to_storage` {#function-route-to-storage}

<div class="function-declaration" id="function-route-to-storage-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../result/result.html">result</a>&lt;<a href="vfs-mount-table-route-target.html">route_target</a>&gt; route_to_storage(
    <a href="vfs-mount-table-route-target.html">route_target</a> target);</code></pre></div>

#### `snapshot_backends` {#function-snapshot-backends}

<div class="function-declaration" id="function-snapshot-backends-1"><pre><code>[[<span class="na">nodiscard</span>]] std::vector&lt;std::shared_ptr&lt;<a href="../file-system/vfs-file-system.html">file_system</a>&gt;&gt; snapshot_backends() <span class="k">const</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
