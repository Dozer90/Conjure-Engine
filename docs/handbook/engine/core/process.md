---
kind: header
---

[Engine](../../engine.md) → [Core](../core.md)

# process.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/process.h` |
| API ↗ | [Processes, libraries and images](../../../api/core/system.md) |

<p><code>A</code> child <code><a href="process/process.html">process</a></code> started by <code>start()</code>, or run to completion by <code>run()</code>. Move-only. Destroying a <code><a href="process/process.html">process</a></code> that was not waited for kills the child and its <code><a href="process/process.html">process</a></code> tree first, so an engine never leaks a tool it launched and never blocks on a grandchild that kept the output open. Threading: <code>read_output()</code>, <code>read_error()</code>, <code>is_running()</code> and <code>kill()</code> may be called from any thread, including while another thread blocks in <code>wait()</code>. <code>start()</code>, <code>wait()</code> and destruction belong to one thread at a time.</p>

## Structs

| File | Description |
|---|---|
| [`process_desc`](process/process-desc.md) | Everything needed to launch a child <code><a href="process/process.html">process</a></code>. |
| [`process_result`](process/process-result.md) | What a finished child produced. |

## Classes

| File | Description |
|---|---|
| [`process`](process/process.md) | <code>A</code> child <code><a href="process/process.html">process</a></code> started by <code>start()</code>, or run to completion by <code>run()</code>. Move-only. Destroying a <code><a href="process/process.html">process</a></code> that was not waited for kills the child and its <code><a href="process/process.html">process</a></code> tree first, so an engine never leaks a tool it launched and never blocks on a grandchild that kept the output open. Threading: <code>read_output()</code>, <code>read_error()</code>, <code>is_running()</code> and <code>kill()</code> may be called from any thread, including while another thread blocks in <code>wait()</code>. <code>start()</code>, <code>wait()</code> and destruction belong to one thread at a time. |

## Forward Declarations

### `process::state` {#symbol-process-state}

```cpp
class state;
```

## Functions

### Public

#### `current_process_id` {#function-current-process-id}

<div class="function-declaration" id="function-current-process-id-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="types.html#symbol-u32">u32</a> current_process_id() <span class="k">noexcept</span>;</code></pre></div>

<p>The operating system&#x27;s identifier of the calling <code><a href="process/process.html">process</a></code> (GetCurrentProcessId / getpid).</p>

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
<a id="process"></a>
<a id="process_desc"></a>
<a id="process_descarguments"></a>
<a id="process_desccapturestderr"></a>
<a id="process_desccapturestdout"></a>
<a id="process_descenvironment"></a>
<a id="process_descexecutable"></a>
<a id="process_descmergestderr"></a>
<a id="process_desctimeoutseconds"></a>
<a id="process_descworkingdirectory"></a>
<a id="process_result"></a>
<a id="process_resultelapsedseconds"></a>
<a id="process_resultexitcode"></a>
<a id="process_resultstandarderror"></a>
<a id="process_resultstandardoutput"></a>
<a id="process_resulttimedout"></a>
<a id="processis_running"></a>
<a id="processkill"></a>
<a id="processkilled_exit_code"></a>
<a id="processmax_timeout_seconds"></a>
<a id="processmstate"></a>
<a id="processoperator"></a>
<a id="processprocess"></a>
<a id="processprocess_1"></a>
<a id="processprocess_id"></a>
<a id="processread_error"></a>
<a id="processread_output"></a>
<a id="processrun"></a>
<a id="processstart"></a>
<a id="processstate"></a>
<a id="processwait"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
