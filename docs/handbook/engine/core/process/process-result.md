---
kind: struct
---

[Engine](../../../engine.md) → [Core](../../core.md) → [process.h](../process.md)

# process_result
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/process.h` |
| Source File Path | `engine/src/core/system/process.cpp` |
| API ↗ | [Processes, libraries and images](../../../../api/core/system.md) |

## Syntax

```cpp
struct process_result
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">elapsed<wbr>Seconds</code></td><td><code class="signature"><a href="../types.html#symbol-f64">f64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td>Seconds between the launch and the child&#x27;s exit as observed by <code>wait()</code>.</td></tr>
<tr><td>public</td><td><code class="signature">exitCode</code></td><td><code class="signature"><a href="../types.html#symbol-i32">i32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td>The child&#x27;s exit code. <code>process::KILLED_EXIT_CODE</code> when it was killed by <code>kill()</code> or by the timeout, and also when the exit status could not be obtained (POSIX: the host ignores SIGCHLD or reaped the child itself); on POSIX a death by another signal reports the negated signal number. <code>A</code> child that exited on its own keeps its code even when the timeout later kills the descendants it left behind (<code>timedOut</code> is set in that case too).</td></tr>
<tr><td>public</td><td><code class="signature">standard<wbr>Error</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td>Captured standard error not yet consumed through <code>read_error()</code>, <code><a href="../types.html#symbol-byte">byte</a></code> for <code><a href="../types.html#symbol-byte">byte</a></code>.</td></tr>
<tr><td>public</td><td><code class="signature">standard<wbr>Output</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td>Captured standard output not yet consumed through <code>read_output()</code>, <code><a href="../types.html#symbol-byte">byte</a></code> for <code><a href="../types.html#symbol-byte">byte</a></code>.</td></tr>
<tr><td>public</td><td><code class="signature">timedOut</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td>True when the child was killed because <code>process_desc::timeoutSeconds</code> elapsed.</td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
