---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# native_file_system.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/vfs/native_file_system.h` |
| API ↗ | [Files and the virtual file system](../../../../api/core/files.md) |

<p>Declarations for <code><a href="native-file-system/vfs-native-file-system.html">native_file_system</a>.h</code>.</p>



## Classes

| File | Description |
|---|---|
| [`vfs::native_file_system`](native-file-system/vfs-native-file-system.md) | <code>A</code> directory on disk exposed as a <code><a href="file-system/vfs-file-system.html">file_system</a></code>. Every path is resolved under the root; anything that would escape it (&quot;../x&quot;, &quot;/../x&quot;, &quot;<code>C</code>:/x&quot;, &quot;//server/x&quot;) is <code>PERMISSION_DENIED</code>. Stateless apart from the root, so one instance may be used from any number of threads. Files are read through handles that share deletion, so an open reader never makes a <code>write_file_atomic</code>, rename or remove of the same file fail. <code>write_file_atomic</code> is durable: the new contents are flushed to the storage device (FlushFileBuffers, fsync) before they replace the old ones, at the cost of one device flush per call. Two limits: a volume whose driver has no flush at all still takes the write, without this guarantee, and logs one warning per process; on macOS the flush is fsync, which hands the data to the drive without waiting for the drive&#x27;s own write cache, so the guarantee there is only as strong as that cache. |

## Functions

### Public

#### `vfs::is_private_file` {#function-vfs-is-private-file}

<div class="function-declaration" id="function-vfs-is-private-file-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../result/result.html">result</a>&lt;<span class="k">bool</span>&gt; is_private_file(<span class="k">const</span> std::filesystem::path &amp;path);</code></pre></div>

<p>Whether the entry at the on-disk path <code>path</code> is a regular file that only the user the process runs as may open, by the rule <code>write_private_file</code> establishes. POSIX: a regular file (a symbolic link is not followed and is never private) owned by the effective user with no permission bit for its group or for others. Windows: a file that is not a reparse point, on a volume that keeps access control lists, owned by the process user, whose list is protected from inheritance and holds nothing but entries allowing that user. The limits of <code>write_private_file</code> apply (privileged accounts, access control lists on POSIX). <code>NOT_FOUND</code> when nothing is at <code>path</code>, <code>IO_FAILURE</code> when the entry cannot be inspected. <code>A</code> file another user made private is <code>IO_FAILURE</code> on Windows, where it cannot be opened to look, and <code>false</code> on POSIX.</p>

#### `vfs::write_private_file` {#function-vfs-write-private-file}

<div class="function-declaration" id="function-vfs-write-private-file-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../result/result.html">result</a>&lt;<span class="k">void</span>&gt; write_private_file( <span class="k">const</span> std::filesystem::path &amp;path, std::span&lt;<span class="k">const</span> std::byte&gt; data );</code></pre></div>

<p>Writes <code>data</code> to the file at the on-disk path <code>path</code> so that only the user the process runs as may open it: the place for a secret that other accounts of the machine must not read. The data goes into a new file beside <code>path</code> that is created with the restriction already in place (POSIX: mode 0600; Windows: an access control list that names that user alone, inherits nothing from the folder and makes the user the owner), and that file then takes the place of <code>path</code> in one step. <code>A</code> reader therefore sees the old contents or the new ones, never a part, and what was at <code>path</code> before (a file with wider permissions, a symbolic link) is replaced, not written through. The folder must exist. <code>path</code> must name a file: an empty path, one that ends in a separator and one that is a directory are <code>INVALID_ARGUMENT</code>, and nothing is created. Limits. <code>UNSUPPORTED</code>, with nothing written, when the volume cannot restrict a file to its owner: FAT and exFAT, and POSIX mounts whose owner and mode are fixed by mount options. Accounts the operating system lets pass any permission still read the file (root; on Windows administrators and backup operators). Access control lists beyond the mode bits are not examined on POSIX (macOS). The folder is the caller&#x27;s business: an account that may write it can replace or delete the file, though not read it. The data is not flushed to the device, so a power cut may lose it. Every other failure is <code>IO_FAILURE</code> with the operating system&#x27;s text. The data is written only once the restriction was checked on the new file, and a failure removes that file again; should the removal fail as well, it stays beside <code>path</code> as &quot;&lt;name&gt;.tmp-&lt;uuid&gt;&quot;, either holding none of the data or restricted to its owner.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="is_private_file"></a>
<a id="native_file_system"></a>
<a id="native_file_systemcreate_directories"></a>
<a id="native_file_systemenumerate"></a>
<a id="native_file_systemis_read_only"></a>
<a id="native_file_systemmreadonly"></a>
<a id="native_file_systemmroot"></a>
<a id="native_file_systemnative_file_system"></a>
<a id="native_file_systemnative_file_system_1"></a>
<a id="native_file_systemnative_path_of"></a>
<a id="native_file_systemopen_read"></a>
<a id="native_file_systemread_file"></a>
<a id="native_file_systemremove"></a>
<a id="native_file_systemremove_all"></a>
<a id="native_file_systemrename"></a>
<a id="native_file_systemresolve"></a>
<a id="native_file_systemresolve_writable"></a>
<a id="native_file_systemroot"></a>
<a id="native_file_systemstat"></a>
<a id="native_file_systemto_native"></a>
<a id="native_file_systemwrite_bytes"></a>
<a id="native_file_systemwrite_file"></a>
<a id="native_file_systemwrite_file_atomic"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<a id="vfsis_private_file"></a>
<a id="vfsnative_file_system"></a>
<a id="vfsnative_file_systemcreate_directories"></a>
<a id="vfsnative_file_systemenumerate"></a>
<a id="vfsnative_file_systemis_read_only"></a>
<a id="vfsnative_file_systemmreadonly"></a>
<a id="vfsnative_file_systemmroot"></a>
<a id="vfsnative_file_systemnative_file_system"></a>
<a id="vfsnative_file_systemnative_file_system_1"></a>
<a id="vfsnative_file_systemnative_path_of"></a>
<a id="vfsnative_file_systemopen_read"></a>
<a id="vfsnative_file_systemread_file"></a>
<a id="vfsnative_file_systemremove"></a>
<a id="vfsnative_file_systemremove_all"></a>
<a id="vfsnative_file_systemrename"></a>
<a id="vfsnative_file_systemresolve"></a>
<a id="vfsnative_file_systemresolve_writable"></a>
<a id="vfsnative_file_systemroot"></a>
<a id="vfsnative_file_systemstat"></a>
<a id="vfsnative_file_systemto_native"></a>
<a id="vfsnative_file_systemwrite_bytes"></a>
<a id="vfsnative_file_systemwrite_file"></a>
<a id="vfsnative_file_systemwrite_file_atomic"></a>
<a id="vfswrite_private_file"></a>
<a id="write_private_file"></a>
<!-- doc-legacy-anchors:end -->
