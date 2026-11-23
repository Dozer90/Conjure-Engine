---
kind: struct
---

[Engine](../../../engine.md) → [Core](../../core.md) → [process.h](../process.md)

# process_desc
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/process.h` |
| Source File Path | `engine/src/core/system/process.cpp` |
| API ↗ | [Processes, libraries and images](../../../../api/core/system.md) |

## Syntax

```cpp
struct process_desc
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">arguments</code></td><td><code class="signature">std::vector&lt;std::string&gt;</code></td><td><code class="signature">{}</code></td><td>argv[1..] of the child, unquoted. An argument containing a NUL character is <code>INVALID_ARGUMENT</code>.</td></tr>
<tr><td>public</td><td><code class="signature">capture<wbr>Stderr</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">true</code></td><td>Capture the child&#x27;s standard error into <code>process_result::standardError</code>. When <code>false</code> the child inherits the parent&#x27;s standard error.</td></tr>
<tr><td>public</td><td><code class="signature">capture<wbr>Stdout</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">true</code></td><td>Capture the child&#x27;s standard output into <code>process_result::standardOutput</code>. When <code>false</code> the child inherits the parent&#x27;s standard output.</td></tr>
<tr><td>public</td><td><code class="signature">environment</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr>std::<wbr>pair&lt;<wbr>std::<wbr>string,<wbr> std::<wbr>string&gt;&gt;</code></td><td><code class="signature">{}</code></td><td>Variables added to, or overriding, the inherited environment. <code>A</code> pair with an empty value removes the variable. Names are case-insensitive on Windows; an empty name, one containing &#x27;=&#x27; or a NUL character in a name or value is <code>INVALID_ARGUMENT</code>.</td></tr>
<tr><td>public</td><td><code class="signature">executable</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td>Path of the program, or a bare name searched in the inherited PATH (and, on Windows, the parent&#x27;s directory). <code>A</code> relative path is resolved against the parent&#x27;s current directory, not against <code>workingDirectory</code>. On Windows a .bat or .cmd file (any case, also spelt with trailing dots or spaces) is <code>INVALID_ARGUMENT</code> because <code>cmd.exe</code> would re-parse the arguments; a NUL character is <code>INVALID_ARGUMENT</code> everywhere.</td></tr>
<tr><td>public</td><td><code class="signature">mergeStderr</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td>Send the child&#x27;s standard error into the standard output stream so the two are interleaved in <code>process_result::standardOutput</code>. Requires <code>captureStdout</code>.</td></tr>
<tr><td>public</td><td><code class="signature">timeout<wbr>Seconds</code></td><td><code class="signature"><a href="../types.html#symbol-f64">f64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td>Wall-clock limit for the whole run, including the time the child&#x27;s descendants keep its captured output open. When exceeded the child (and every <code><a href="process.html">process</a></code> it spawned) is killed and <code>process_result::timedOut</code> is set. 0 means no limit. Must be finite and not negative; values above <code>process::MAX_TIMEOUT_SECONDS</code> behave like that maximum.</td></tr>
<tr><td>public</td><td><code class="signature">working<wbr>Directory</code></td><td><code class="signature">std::filesystem::path</code></td><td><code class="signature">{}</code></td><td>Current directory of the child. Empty inherits the parent&#x27;s. Must exist.</td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
