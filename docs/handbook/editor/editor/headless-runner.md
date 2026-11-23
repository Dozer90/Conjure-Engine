---
kind: header
---

[Editor](../../editor.md) → [Applications](../applications.md)

# headless_runner.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/headless_runner.h` |
| API ↗ | [The AI control protocol](../../../api/editor/ai-protocol.md), [The editor](../../../api/editor/index.md) |

<p><code>conjure_editor --headless --exec commands.json</code>: a JSON array of requests or NDJSON, one request per line; a missing id becomes the zero-based index. The file is read and parsed before the engine starts (<code>editor_application::initialise</code> calls <code>load_command_file</code>), so a missing or malformed file costs no engine boot. Every request is dispatched, frames are pumped until an asynchronous response exists, responses and events are written as NDJSON to the output; the run stops at the first failure unless --keep-going. When the engine could not load the project&#x27;s startup scene, the output opens with <code>warning {code: &quot;STARTUP_SCENE_FAILED&quot;, message}</code> and the run carries on over the empty world: the exit code does not change, so a command file that repairs the scene still runs. <code>editor.quit</code> ends the run after its reply, and its non-zero <code>exitCode</code> becomes the run&#x27;s exit code. Exit codes: 0 all ok, 1 a command failed, 2 startup or project failure (also a <code>project.open</code> in the file whose restart failed for both projects) or responses that could not be written, 3 bad arguments (a missing or unparseable file).</p>

## Structs

| File | Description |
|---|---|
| [`exec_summary`](headless-runner/exec-summary.md) |  |

## Classes

| File | Description |
|---|---|
| [`headless_runner`](headless-runner/headless-runner.md) |  |

## Forward Declarations

### `editor_application` {#symbol-editor-application}

```cpp
class editor_application;
```

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
<a id="editor_application"></a>
<a id="exec_summary"></a>
<a id="exec_summaryexecuted"></a>
<a id="exec_summaryexitcode"></a>
<a id="exec_summaryfailed"></a>
<a id="exec_summaryfirstfailedindex"></a>
<a id="exec_summaryrecord"></a>
<a id="exec_summarysucceeded"></a>
<a id="exec_summaryto_json"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="headless_runner"></a>
<a id="headless_runnerexpand_placeholders"></a>
<a id="headless_runnerframes_pumped"></a>
<a id="headless_runnerheadless_runner"></a>
<a id="headless_runnerheadless_runner_1"></a>
<a id="headless_runnerload_command_file"></a>
<a id="headless_runnermframespumped"></a>
<a id="headless_runneroperator"></a>
<a id="headless_runnerparse_command_file"></a>
<a id="headless_runnerproject_placeholder"></a>
<a id="headless_runnerrun"></a>
<a id="headless_runnerrun_requests"></a>
<a id="internals-and-algorithms"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
