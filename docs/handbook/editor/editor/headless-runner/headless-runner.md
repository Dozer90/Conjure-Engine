---
kind: class
---

[Editor](../../../editor.md) → [Applications](../../applications.md) → [headless_runner.h](../headless-runner.md)

# headless_runner
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/headless_runner.h` |
| Source File Path | `engine/src/editor/core/headless_runner.cpp` |
| API ↗ | [The AI control protocol](../../../../api/editor/ai-protocol.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
class CONJURE_API headless_runner
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">headless_runner();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~headless_runner();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">m<wbr>Frames<wbr>Pumped</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">PROJECT_<wbr>PLACEHOLDER</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code></td><td><code class="signature"><span class="s">"${project}"</span></code></td><td>The placeholder command files may use for the project root in string arguments.</td></tr>
</tbody>
</table>

## Functions

### Public

#### `expand_placeholders` {#function-expand-placeholders}

<div class="function-declaration" id="function-expand-placeholders-1"><pre><code><span class="k">static</span> <span class="k">void</span> expand_placeholders(
    <a href="../protocol/protocol-message/protocol-request.html">protocol_request</a> &amp;request,
    <span class="k">const</span> std::filesystem::path &amp;projectRoot);</code></pre></div>

<p>Replaces <code>PROJECT_PLACEHOLDER</code> in every string of the arguments with the project root.</p>

#### `frames_pumped` {#function-frames-pumped}

<div class="function-declaration" id="function-frames-pumped-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-u32">u32</a> frames_pumped() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Frames pumped during the last run.</p>

#### `load_command_file` {#function-load-command-file}

<div class="function-declaration" id="function-load-command-file-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../../engine/core/result/result.html">core::result</a>&lt;std::vector&lt;<a href="../protocol/protocol-message/protocol-request.html">protocol_request</a>&gt;&gt; load_command_file(
    <span class="k">const</span> std::filesystem::path &amp;file);</code></pre></div>

<p>Reads and parses the command file of --exec (<code>parse_command_file</code>). Needs no engine: the editor calls it before it starts one. <code>INVALID_ARGS</code> when the path is empty, when the file cannot be read (a missing file, a directory) or when its text is malformed; the message starts with &quot;--exec&quot; and names the file, and for malformed text the line or entry as well.</p>

#### `parse_command_file` {#function-parse-command-file}

<div class="function-declaration" id="function-parse-command-file-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../../engine/core/result/result.html">core::result</a>&lt;std::vector&lt;<a href="../protocol/protocol-message/protocol-request.html">protocol_request</a>&gt;&gt; parse_command_file(
    std::string_view text);</code></pre></div>

<p>Parses a command file: a JSON array of request objects, or NDJSON. <code>A</code> leading UTF-8 <code><a href="../../../engine/core/types.html#symbol-byte">byte</a></code> order mark is skipped, as are blank lines; a missing id becomes the index. <code>INVALID_ARGS</code> names the offending line.</p>

#### `run` {#function-run}

<div class="function-declaration" id="function-run-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/result/result.html">core::result</a>&lt;<a href="exec-summary.html">exec_summary</a>&gt; run(
    <a href="../editor-application/editor-application.html">editor_application</a> &amp;application,
    <span class="k">const</span> <a href="../editor-options/editor-options.html">editor_options</a> &amp;options,
    std::span&lt;<span class="k">const</span> <a href="../protocol/protocol-message/protocol-request.html">protocol_request</a>&gt; requests);</code></pre></div>

<p>Runs the requests of a command file (<code>load_command_file</code>) against the application, writing to <code>options.outputFile</code>, whose missing folders are created, or to <code>std::cout</code>; <code>options.keepGoing</code> is <code>run_requests</code>&#x27; <code>keepGoing</code>. <code>IO_ERROR</code> when the output file cannot be opened; nothing has run then.</p>

#### `run_requests` {#function-run-requests}

<div class="function-declaration" id="function-run-requests-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="exec-summary.html">exec_summary</a> run_requests(
    <a href="../editor-application/editor-application.html">editor_application</a> &amp;application,
    std::span&lt;<span class="k">const</span> <a href="../protocol/protocol-message/protocol-request.html">protocol_request</a>&gt; requests,
    std::ostream &amp;out,
    <span class="k">bool</span> keepGoing);</code></pre></div>

<p>Runs already parsed requests, writing NDJSON lines to <code>out</code>. When the running engine kept a startup-scene failure (<code>engine::application::startup_scene_failure</code>), the first line is the event <code>warning {code: &quot;STARTUP_SCENE_FAILED&quot;, message}</code>, written before any request runs; it changes neither the counts nor the exit code. ${project} expands to the root of the project open when each request runs. An asynchronous request is waited for until it answers; the dispatcher&#x27;s --timeout-seconds answers it with <code>TIMEOUT</code>, and should it never answer, the runner gives up after twice that plus five seconds and ends the run. <code>editor.quit</code> ends the run after its reply. <code>A</code> response that cannot be written (the stream failed) ends the run after that request, which is still counted, with <code>exitCode</code> <code>RUNTIME_FAILURE</code> (2). <code>A</code> non-zero quit code the application holds at the end (<code>editor.quit</code>&#x27;s <code>exitCode</code>, or <code>INIT_FAILURE</code> from a failed project restart) becomes <code>exitCode</code>.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
