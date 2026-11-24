---
kind: class
---

[Engine](../../../engine.md) → [Core](../../core.md) → [file_lock.h](../file-lock.md)

# file_lock
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/file_lock.h` |
| Source File Path | `engine/src/core/system/file_lock.cpp` |
| API ↗ | [Processes, libraries and images](../../../../api/core/system.md) |

## Syntax

```cpp
class CONJURE_API file_lock
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">file_lock();</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">file_lock(
    <a href="file-lock.html">file_lock</a> &amp;&amp;other) <span class="k">noexcept</span>;</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~file_lock();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">INVALID_<wbr>NATIVE</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> std::<wbr>intptr_<wbr>t</code></td><td><code class="signature">-<span class="mi">1</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mNative</code></td><td><code class="signature">std::intptr_t</code></td><td><code class="signature">INVALID_NATIVE</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPath</code></td><td><code class="signature">std::filesystem::path</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `held` {#function-held}

<div class="function-declaration" id="function-held-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> held() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True while this object holds the lock.</p>

#### `path` {#function-path}

<div class="function-declaration" id="function-path-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::filesystem::path &amp;path() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The path given to <code>try_acquire</code>; empty when nothing is held.</p>

#### `release` {#function-release}

<div class="function-declaration" id="function-release-1"><pre><code><span class="k">void</span> release() <span class="k">noexcept</span>;</code></pre></div>

<p>Releases the lock and closes the file. Safe when nothing is held.</p>

#### `release_and_remove` {#function-release-and-remove}

<div class="function-declaration" id="function-release-and-remove-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../result/result.html">result</a>&lt;<span class="k">void</span>&gt; release_and_remove();</code></pre></div>

<p>Deletes the file while still holding it, then releases. Where Windows cannot delete with POSIX semantics (before Windows 10 1809, or on a file system without them such as FAT) the name stays, pending deletion, until every other handle to the file has closed. <code>INVALID_STATE</code> when nothing is held. <code>NOT_FOUND</code> (POSIX) when the path no longer names the held file, which is left alone. On an OS failure the lock is released, the file stays and the error names it. Nothing is held afterwards, whatever the outcome.</p>

#### `try_acquire` {#function-try-acquire}

<div class="function-declaration" id="function-try-acquire-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../result/result.html">result</a>&lt;<a href="file-lock.html">file_lock</a>&gt; try_acquire(
    <span class="k">const</span> std::filesystem::path &amp;path,
    <a href="../file-lock.html#symbol-file-lock-open">file_lock_open</a> open);</code></pre></div>

<p>Opens (or, with <code>CREATE</code>, creates) <code>path</code> and tries to lock it without waiting. Success with <code><a href="#function-held-1">held</a>()</code> == <code>false</code> when another holder has it (Windows: also a foreign handle opened with delete access or whose share mode refuses reading, writing or deletion, or one holding a <code><a href="../types.html#symbol-byte">byte</a></code>-range lock in the file). Fails with <code>INVALID_ARGUMENT</code> for an empty path, one containing NUL or an <code>open</code> value outside <code><a href="../file-lock.html#symbol-file-lock-open">file_lock_open</a></code>; <code>NOT_FOUND</code> for a missing file under <code>EXISTING</code>, a missing parent folder, or a file removed while it was being locked; <code>PERMISSION_DENIED</code> when access is refused (Windows: mostly also for a file whose deletion is pending), the path is a directory, or (POSIX) a symbolic link; <code>UNSUPPORTED</code> when the file system has no locks (POSIX only; on Windows the share mode alone holds the file and the result is held); <code>IO_FAILURE</code> when the file kept being replaced under <code>CREATE</code>; <code>BACKEND_FAILURE</code> otherwise. Messages name the path (when there is one); those from a failed system call also carry the operating system&#x27;s text. Never removes a file, on any outcome: a file it created stays when it then fails.</p>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="file-lock.html">file_lock</a> &amp;<span class="k">operator</span>=(
    <a href="file-lock.html">file_lock</a> &amp;&amp;other) <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
