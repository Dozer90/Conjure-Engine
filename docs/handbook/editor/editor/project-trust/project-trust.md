---
kind: class
---

[Editor](../../../editor.md) → [Applications](../../applications.md) → [project_trust.h](../project-trust.md)

# project_trust
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/project_trust.h` |
| Source File Path | `engine/src/editor/core/project_trust.cpp` |
| API ↗ | [Headless mode and the command model](../../../../api/editor/headless-and-commands.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
class CONJURE_API project_trust
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">project_trust();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~project_trust();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">MAX_<wbr>ORIGIN_<wbr>BYTES</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../../engine/core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">32</span></code></td><td>An origin text is cut to this many bytes before it is stored.</td></tr>
<tr><td>public</td><td><code class="signature">MAX_<wbr>REMEMBERED_<wbr>PROJECTS</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../../engine/core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">1024</span></code></td><td>The store keeps the most recent answers: one more than this drops the oldest, and that project is asked about again.</td></tr>
<tr><td>public</td><td><code class="signature">MAX_<wbr>STORE_<wbr>BYTES</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../../engine/core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">4</span> * MIB</code></td><td><code>A</code> store file larger than this is not read, and none is written: an answer that would take the store past it holds for the session instead. Paths of ordinary length stay far below it (<code>MAX_REMEMBERED_PROJECTS</code> entries of 4 KiB each would not).</td></tr>
<tr><td>private</td><td><code class="signature">mSession</code></td><td><code class="signature">std::vector&lt;std::string&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mStoreFile</code></td><td><code class="signature">std::filesystem::path</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">STORE_<wbr>FILE_<wbr>NAME</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code></td><td><code class="signature"><span class="s">"trusted_<wbr>projects.json"</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">STORE_<wbr>SCHEMA_<wbr>VERSION</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">1</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `is_trusted` {#function-is-trusted}

<div class="function-declaration" id="function-is-trusted-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_trusted(
    <span class="k">const</span> std::filesystem::path &amp;projectRoot) <span class="k">const</span>;</code></pre></div>

<p>True unless <code><a href="#function-state-of-1">state_of</a>()</code> is <code>UNTRUSTED</code>.</p>

#### `project_key` {#function-project-key}

<div class="function-declaration" id="function-project-key-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../../engine/core/result/result.html">core::result</a>&lt;std::string&gt; project_key(
    <span class="k">const</span> std::filesystem::path &amp;projectRoot);</code></pre></div>

<p>The text a project is remembered under: its root folder made absolute against the working directory, with symbolic links, &quot;.&quot; and &quot;..&quot; resolved by the file system as far as the path exists, in UTF-8 with forward slashes and no trailing separator. Two spellings the operating system resolves to the same text are one project; two it does not (another letter case on a volume that ignores case but does not report the stored one) are two, and the second is asked about again. <code>INVALID_ARGUMENT</code> for an empty path and for one that is not valid UTF-8, decided from the path as given before the file system is asked, so the answer is the same on every volume, and again from the resolved text (the working directory, the target of a link). <code>IO_FAILURE</code> when the path cannot be resolved.</p>

#### `remembered` {#function-remembered}

<div class="function-declaration" id="function-remembered-1"><pre><code>[[<span class="na">nodiscard</span>]] std::vector&lt;<a href="trusted-project.html">trusted_project</a>&gt; remembered() <span class="k">const</span>;</code></pre></div>

<p>The answers the store holds, oldest first; empty without a store file and for a store that cannot be used (see the file comment).</p>

#### `revoke` {#function-revoke}

<div class="function-declaration" id="function-revoke-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/result/result.html">core::result</a>&lt;<span class="k">bool</span>&gt; revoke(
    <span class="k">const</span> std::filesystem::path &amp;projectRoot);</code></pre></div>

<p>Forgets the project: removes it from the session answers and from the store, and returns whether either held it. The folder need not exist any more. <code>IO_FAILURE</code>, with the session answer already removed, when the store names the project and cannot be rewritten: the project is then still trusted on disk. <code>A</code> project that is open stays open; the answer counts the next time it is opened.</p>

#### `set_store_file` {#function-set-store-file}

<div class="function-declaration" id="function-set-store-file-1"><pre><code><span class="k">void</span> set_store_file(
    std::filesystem::path file);</code></pre></div>

<p>Names the trust store file; an empty path (the default) means that nothing is remembered and every answer is for the session. Nothing is read or created by this call. The session answers are kept.</p>

#### `state_of` {#function-state-of}

<div class="function-declaration" id="function-state-of-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../project-trust.html#symbol-trust-state">trust_state</a> state_of(
    <span class="k">const</span> std::filesystem::path &amp;projectRoot) <span class="k">const</span>;</code></pre></div>

<p>What is known about the project: <code>REMEMBERED</code> when the store names it, <code>SESSION</code> when it was trusted for this session, <code>UNTRUSTED</code> otherwise and for a path <code><a href="#function-project-key-1">project_key</a>()</code> refuses. The store file is read on every call, so an answer another editor of this user recorded or revoked is seen at once.</p>

#### `store_file` {#function-store-file}

<div class="function-declaration" id="function-store-file-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::filesystem::path &amp;store_file() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `trust` {#function-trust}

<div class="function-declaration" id="function-trust-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/result/result.html">core::result</a>&lt;<a href="../project-trust.html#symbol-trust-state">trust_state</a>&gt; trust(
    <span class="k">const</span> std::filesystem::path &amp;projectRoot,
    <span class="k">bool</span> remember,
    std::string_view origin);</code></pre></div>

<p>Records that the project is trusted and returns the state it now has. With <code>remember</code> and a store file the answer is written to the store (the entry of the same project is replaced, the oldest entry dropped beyond <code>MAX_REMEMBERED_PROJECTS</code>; the folder of the store is created). When that is not asked for, there is no store file, the store cannot be written or would grow past <code>MAX_STORE_BYTES</code>, or the store does not show the answer right after it was written (another editor of this user wrote the file in between), the answer holds for the session, with one <code>WARN</code> that names why in the last three cases: a &quot;yes&quot; this call returned is never <code>UNTRUSTED</code>, only not remembered. What this call cannot do is keep a remembered answer in the store afterwards: the other editor&#x27;s write can still drop it, and the project is then asked about again. <code>origin</code> says who answered and is stored with the entry, cut to <code>MAX_ORIGIN_BYTES</code>. Whether the folder holds a project is the caller&#x27;s question (the <code>project.trust</code> command asks it). Fails as <code><a href="#function-project-key-1">project_key</a>()</code> does, and then records nothing.</p>

### Private

#### `read_store` {#function-read-store}

<div class="function-declaration" id="function-read-store-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/result/result.html">core::result</a>&lt;std::vector&lt;<a href="trusted-project.html">trusted_project</a>&gt;&gt; read_store() <span class="k">const</span>;</code></pre></div>

#### `state_of_key` {#function-state-of-key}

<div class="function-declaration" id="function-state-of-key-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../project-trust.html#symbol-trust-state">trust_state</a> state_of_key(
    <span class="k">const</span> std::string &amp;key) <span class="k">const</span>;</code></pre></div>

#### `trust_for_session` {#function-trust-for-session}

<div class="function-declaration" id="function-trust-for-session-1"><pre><code><span class="k">void</span> trust_for_session(
    <span class="k">const</span> std::string &amp;key);</code></pre></div>

#### `write_store` {#function-write-store}

<div class="function-declaration" id="function-write-store-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; write_store(
    <span class="k">const</span> std::vector&lt;<a href="trusted-project.html">trusted_project</a>&gt; &amp;projects) <span class="k">const</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
