---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# path_utils.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/vfs/path_utils.h` |
| API ↗ | [Files and the virtual file system](../../../../api/core/files.md) |

<p>Lexical helpers for the &#x27;/&#x27;-separated UTF-8 paths used by the virtual file system. None of these touch the disk. Backslashes are accepted as separators on input and always written as forward slashes on output.</p>

## Functions

### Public

#### `vfs::path_utils::extension` {#function-vfs-path-utils-extension}

<div class="function-declaration" id="function-vfs-path-utils-extension-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string extension(std::string_view path);</code></pre></div>

<p>The lower-cased extension including the dot (&quot;.<code>PNG</code>&quot; -&gt; &quot;.png&quot;), or &quot;&quot; when there is none.</p>

#### `vfs::path_utils::filename` {#function-vfs-path-utils-filename}

<div class="function-declaration" id="function-vfs-path-utils-filename-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view filename(std::string_view path) <span class="k">noexcept</span>;</code></pre></div>

<p>The last segment (&quot;a/b/<code>c.txt</code>&quot; -&gt; &quot;<code>c.txt</code>&quot;). Empty when the path ends with a separator.</p>

#### `vfs::path_utils::has_extension` {#function-vfs-path-utils-has-extension}

<div class="function-declaration" id="function-vfs-path-utils-has-extension-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">bool</span> has_extension(std::string_view path, std::string_view extension) <span class="k">noexcept</span>;</code></pre></div>

<p>True when the path&#x27;s extension equals <code>extension</code> ignoring case; the dot is optional (&quot;png&quot; and &quot;.png&quot; both match &quot;<code>a.PNG</code>&quot;).</p>

#### `vfs::path_utils::is_absolute` {#function-vfs-path-utils-is-absolute}

<div class="function-declaration" id="function-vfs-path-utils-is-absolute-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">bool</span> is_absolute(std::string_view path) <span class="k">noexcept</span>;</code></pre></div>

<p>True for paths starting with a separator, a drive letter (&quot;<code>C</code>:&quot;) or a UNC prefix.</p>

#### `vfs::path_utils::is_portable_name` {#function-vfs-path-utils-is-portable-name}

<div class="function-declaration" id="function-vfs-path-utils-is-portable-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">bool</span> is_portable_name(std::string_view name) <span class="k">noexcept</span>;</code></pre></div>

<p>True when <code>name</code> (one path segment, not a path) may be given to a new file or directory: the rule every <code><a href="file-system/vfs-file-system.html">file_system</a></code> applies wherever it creates an entry, on every platform, so that content authored on one opens on all of them. <code>A</code> name is not portable when it is empty, holds a path separator, a control character (<code>U</code>+0000 to <code>U</code>+001F, <code>U</code>+007F) or one of: * ? &quot; &lt; &gt; |, ends with a dot or a space, or is a Windows device name in any case and with any extension (CON, PRN, AUX, NUL, COM1 to COM9, LPT1 to LPT9: &quot;nul&quot;, &quot;<code>Aux.txt</code>&quot;). Check a name the user typed with this before creating the entry; <code><a href="file-system/vfs-file-system.html">file_system</a></code> reports the same verdict as <code>INVALID_ARGUMENT</code>.</p>

#### `vfs::path_utils::join` {#function-vfs-path-utils-join}

<div class="function-declaration" id="function-vfs-path-utils-join-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string join(std::string_view left, std::string_view right);</code></pre></div>

<p>Joins two paths with exactly one separator. An empty side yields the other side unchanged; an absolute right side replaces the left side. The result is not normalised.</p>

#### `vfs::path_utils::normalize` {#function-vfs-path-utils-normalize}

<div class="function-declaration" id="function-vfs-path-utils-normalize-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string normalize(std::string_view path);</code></pre></div>

<p>Produces the canonical spelling of a path: forward slashes only, no empty or &quot;.&quot; segments, &quot;..&quot; resolved against the preceding segment (a leading &quot;..&quot; of a relative path is kept, a &quot;..&quot; at the root of an absolute path is dropped) and no trailing slash. Drive letters (&quot;<code>C</code>:/x&quot;) and UNC prefixes (&quot;//server/share&quot;) are preserved. &quot;.&quot; and &quot;&quot; both become &quot;&quot;.</p>

#### `vfs::path_utils::parent` {#function-vfs-path-utils-parent}

<div class="function-declaration" id="function-vfs-path-utils-parent-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view <a href="../../scene/components/parent.html">parent</a>(std::string_view path) <span class="k">noexcept</span>;</code></pre></div>

<p>Everything before the last separator (&quot;a/b/<code>c.txt</code>&quot; -&gt; &quot;a/b&quot;). <code>A</code> single segment has the empty parent; the parent of &quot;/a&quot; is &quot;/&quot; and of &quot;<code>C</code>:/a&quot; is &quot;<code>C</code>:/&quot;: a root keeps its separator (in the spelling the input used), so a root is its own parent. <code>A</code> trailing separator is ignored.</p>

#### `vfs::path_utils::relative_to` {#function-vfs-path-utils-relative-to}

<div class="function-declaration" id="function-vfs-path-utils-relative-to-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string relative_to(std::string_view path, std::string_view base);</code></pre></div>

<p>Expresses <code>path</code> relative to <code>base</code> lexically (&quot;a/b/c&quot; relative to &quot;a/x&quot; -&gt; &quot;../b/c&quot;). Both inputs are normalised first. Equal paths yield &quot;&quot;. When one side is absolute and the other is not, or the roots differ, the normalised <code>path</code> is returned unchanged. Only the root prefix (a drive letter) is compared ignoring case; segments are compared exactly, so &quot;<code>C</code>:/Users/x&quot; relative to &quot;c:/users&quot; is &quot;../Users/x&quot;. <code>A</code> lexical helper cannot know the case rules of the volume a path will be used on.</p>

#### `vfs::path_utils::stem` {#function-vfs-path-utils-stem}

<div class="function-declaration" id="function-vfs-path-utils-stem-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view stem(std::string_view path) <span class="k">noexcept</span>;</code></pre></div>

<p>The file name without its extension (&quot;<code>c.tar.gz</code>&quot; -&gt; &quot;<code>c.tar</code>&quot;). Names that start with a dot and have no other dot (&quot;.gitignore&quot;) are returned whole.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension"></a>
<a id="extension-points"></a>
<a id="filename"></a>
<a id="has_extension"></a>
<a id="internals-and-algorithms"></a>
<a id="is_absolute"></a>
<a id="is_portable_name"></a>
<a id="join"></a>
<a id="normalize"></a>
<a id="parent"></a>
<a id="relative_to"></a>
<a id="stem"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<a id="vfspath_utilsextension"></a>
<a id="vfspath_utilsfilename"></a>
<a id="vfspath_utilshas_extension"></a>
<a id="vfspath_utilsis_absolute"></a>
<a id="vfspath_utilsis_portable_name"></a>
<a id="vfspath_utilsjoin"></a>
<a id="vfspath_utilsnormalize"></a>
<a id="vfspath_utilsparent"></a>
<a id="vfspath_utilsrelative_to"></a>
<a id="vfspath_utilsstem"></a>
<!-- doc-legacy-anchors:end -->
