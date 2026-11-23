---
kind: class
---

[Editor](../../../editor.md) → [Applications](../../applications.md) → [editor_application.h](../editor-application.md)

# editor_application
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/editor_application.h` |
| Source File Path | `engine/src/editor/core/editor_application.cpp` |
| API ↗ | [The AI control protocol](../../../../api/editor/ai-protocol.md), [Headless mode and the command model](../../../../api/editor/headless-and-commands.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
class CONJURE_API editor_application final : public editor_host
```

## Inheritance Hierarchy


[`editor_host`](../editor-context/editor-host.md) → `editor_application`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">editor_application();</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~editor_application() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mContext</code></td><td><code class="signature">std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../editor-context/editor-context.html">editor_<wbr>context</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mEngine</code></td><td><code class="signature">std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../../../engine/engine/application/application.html">engine::<wbr>application</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Exec<wbr>Requests</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="../protocol/protocol-message/protocol-request.html">protocol_<wbr>request</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mExitCode</code></td><td><code class="signature"><span class="k">int</span></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mInitialised</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mOptions</code></td><td><code class="signature"><a href="../editor-options/editor-options.html">editor_options</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Pending<wbr>Project</code></td><td><code class="signature">std::<wbr>optional&lt;<wbr>std::<wbr>filesystem::<wbr>path&gt;</code></td><td><code class="signature">std::nullopt</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Quit<wbr>Requested</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mServer</code></td><td><code class="signature">std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../protocol/protocol-server/protocol-server.html">protocol_<wbr>server</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mTicks</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mTrust</code></td><td><code class="signature"><a href="../project-trust/project-trust.html">project_trust</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mUiFactory</code></td><td><code class="signature"><a href="../editor-application.html#symbol-editor-application-layer-factory">layer_factory</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `can_ask_about_trust` {#function-can-ask-about-trust}

<div class="function-declaration" id="function-can-ask-about-trust-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <span class="k">bool</span> can_ask_about_trust(
    <span class="k">const</span> <a href="../editor-options/editor-options.html">editor_options</a> &amp;options) <span class="k">noexcept</span>;</code></pre></div>

<p>Whether a session with these options has somebody to ask about a project nobody answered for: a window, and neither --exec nor --export, which run to their end unattended. <code><a href="#function-initialise-1">initialise</a>()</code> starts such a session without its untrusted project and asks; every other session is refused.</p>

#### `context` {#function-context}

<div class="function-declaration" id="function-context-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../editor-context/editor-context.html">editor_context</a> &amp;context() <span class="k">noexcept</span>;</code></pre></div>

#### `engine` {#function-engine}

<div class="function-declaration" id="function-engine-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/engine/application/application.html">engine::application</a> &amp;engine() <span class="k">noexcept</span>;</code></pre></div>

#### `exit_code` {#function-exit-code}

<div class="function-declaration" id="function-exit-code-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">int</span> exit_code() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `has_pending_project_open` {#function-has-pending-project-open}

<div class="function-declaration" id="function-has-pending-project-open-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> has_pending_project_open() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `headless_tick` {#function-headless-tick}

<div class="function-declaration" id="function-headless-tick-1"><pre><code><span class="k">void</span> headless_tick();</code></pre></div>

<p>The frame body outside a frame (tests): what the frame layer calls once per frame.</p>

#### `initialise` {#function-initialise}

<div class="function-declaration" id="function-initialise-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; initialise(
    <span class="k">const</span> <a href="../editor-options/editor-options.html">editor_options</a> &amp;options,
    <a href="../editor-application.html#symbol-editor-application-layer-factory">layer_factory</a> uiFactory);</code></pre></div>

<p>Builds the engine over the options (headless: <code>null</code> platform, rhi and audio; the scripting runtime on the <code>null</code> host so script.* commands work without .NET), initialises it, creates the context, pushes the frame layer (and the UI layer from the factory when not headless) and starts the protocol server. Fails with the engine&#x27;s error or, headless, <code>IO_ERROR</code> when an explicit port is busy (a windowed editor logs that and runs without the server). Nothing heavy starts before the cheap checks are through, in this order: the arguments, <code>project.json</code> and the merged configuration (<code>make_engine_desc</code>), then the command file of --exec when <code><a href="#function-run-1">run</a>()</code> will execute it (<code>headless_runner::load_command_file</code>; a missing or malformed file is the engine&#x27;s <code>BAD_ARGUMENTS</code> with the runner&#x27;s message), then whether the project may be opened, and only then the engine. <code>conjure_editor</code>&#x27;s main exits 3 for <code>BAD_ARGUMENTS</code> or an <code>INVALID_CONFIG</code> that a flag caused, otherwise 2. Project trust (decision). <code>options.trustStoreFile</code> names the store of remembered answers (empty: nothing is remembered) and <code>options.trustProject</code> answers for <code>options.projectPath</code> for this session. <code>A</code> project nobody answered for is not opened. Where nobody can be asked (headless, --exec, --export) initialise fails with <code>PROJECT_NOT_TRUSTED</code> and no engine starts. <code>A</code> windowed session that runs neither --exec nor --export starts without the project instead (<code><a href="#function-options-1">options</a>()</code> then names none) and asks: the context&#x27;s <code>trust_prompt()</code> names the project, a <code>WARN</code> says so, and <code>project.trust</code> followed by <code>project.open</code> opens it. <code>resolve_startup_project</code> is that decision, and the engine is started from the description <code>plan_startup</code> makes from its answer alone.</p>

#### `is_initialised` {#function-is-initialised}

<div class="function-declaration" id="function-is-initialised-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_initialised() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `is_quit_requested` {#function-is-quit-requested}

<div class="function-declaration" id="function-is-quit-requested-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_quit_requested() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True once <code><a href="#function-request-quit-1">request_quit</a>()</code> was called on the editor; it stays <code>true</code> for the application&#x27;s lifetime. <code>A</code> quit the engine asked for by itself is read from <code><a href="#function-engine-1">engine</a>()</code>.<code><a href="#function-is-quit-requested-1">is_quit_requested</a>()</code> and is not included.</p>

#### `make_engine_desc` {#function-make-engine-desc}

<div class="function-declaration" id="function-make-engine-desc-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../../engine/core/result/result.html">core::result</a>&lt;<a href="../../../engine/engine/application-desc/application-desc.html">engine::application_desc</a>&gt; make_engine_desc(
    <span class="k">const</span> <a href="../editor-options/editor-options.html">editor_options</a> &amp;options,
    <span class="k">const</span> std::filesystem::path &amp;projectPath);</code></pre></div>

<p>The engine configuration the options produce for a project directory (empty = none): the merge of <code>engine_config::defaults()</code> (editor = <code>true</code>), --headless, <code>project.json</code> and the engine arguments, plus the editor&#x27;s scripting policy. Fails with <code>BAD_ARGUMENTS</code> naming every option neither the editor nor the engine knows, before the project is loaded. Exposed for tests.</p>

#### `options` {#function-options}

<div class="function-declaration" id="function-options-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../editor-options/editor-options.html">editor_options</a> &amp;options() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `plan_startup` {#function-plan-startup}

<div class="function-declaration" id="function-plan-startup-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../../engine/core/result/result.html">core::result</a>&lt;<a href="startup-plan.html">startup_plan</a>&gt; plan_startup(
    <span class="k">const</span> <a href="../editor-options/editor-options.html">editor_options</a> &amp;options,
    <a href="../project-trust/project-trust.html">project_trust</a> &amp;trust);</code></pre></div>

<p><code>resolve_startup_project</code>, then <code>make_engine_desc</code> over the project it lets through: the description <code><a href="#function-initialise-1">initialise</a>()</code> starts the engine from. <code>A</code> project left out for the window is in neither the description nor anything made from it (its <code>project.json</code>, native modules and game assembly are not loaded); <code>plan.unanswered</code> names it. Fails as either of the two does.</p>

#### `pump_frame` {#function-pump-frame}

<div class="function-declaration" id="function-pump-frame-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; pump_frame();</code></pre></div>

<p>Runs one engine frame (the frame layer ticks the context) and applies a pending project restart afterwards. Tests and the headless runner pump with it.</p>

#### `quit_requested` {#function-quit-requested}

<div class="function-declaration" id="function-quit-requested-1"><pre><code>[[<span class="na">deprecated</span>(<span class="s">"use is_quit_requested()"</span>)]] [[<span class="na">nodiscard</span>]] <span class="k">bool</span> quit_requested() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The former name of <code><a href="#function-is-quit-requested-1">is_quit_requested</a>()</code>, kept for one wave; returns <code><a href="#function-is-quit-requested-1">is_quit_requested</a>()</code>.</p>

#### `request_open_project` {#function-request-open-project}

<div class="function-declaration" id="function-request-open-project-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; request_open_project(
    <span class="k">const</span> std::filesystem::path &amp;directory) <span class="k">override</span>;</code></pre></div>

<p>Checks <code>project.json</code> in the directory, then that the project is trusted (<code>PROJECT_NOT_TRUSTED</code> otherwise, and nothing is scheduled), then schedules the engine restart over it for the next frame boundary. <code>A</code> project whose trust is revoked before that boundary is not opened: the restart is dropped with a <code>WARN</code> and the editor stays on the project it has.</p>

#### `request_quit` {#function-request-quit}

<div class="function-declaration" id="function-request-quit-1"><pre><code><span class="k">void</span> request_quit(
    <span class="k">int</span> exitCode) <span class="k">override</span>;</code></pre></div>

#### `resolve_startup_project` {#function-resolve-startup-project}

<div class="function-declaration" id="function-resolve-startup-project-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../../engine/core/result/result.html">core::result</a>&lt;<a href="startup-project.html">startup_project</a>&gt; resolve_startup_project(
    <span class="k">const</span> <a href="../editor-options/editor-options.html">editor_options</a> &amp;options,
    <a href="../project-trust/project-trust.html">project_trust</a> &amp;trust);</code></pre></div>

<p>What <code><a href="#function-initialise-1">initialise</a>()</code> does about <code>options.projectPath</code> (decision), decided before any engine exists. No project named: nothing to open, nothing to ask. <code>options.trustProject</code> first records a session answer in <code>trust</code> (&quot;command line&quot;). <code>A</code> project <code>trust</code> holds an answer for is the one to open. One nobody answered for is left out and named as unanswered where <code><a href="#function-can-ask-about-trust-1">can_ask_about_trust</a>(options)</code> holds; everywhere else the call fails with <code>PROJECT_NOT_TRUSTED</code>, and the message names the project and the ways to answer. Fails as <code>project_trust::trust</code> does when --trust-project names a path that has no key. Whether the folder holds a project is not looked at here (<code>make_engine_desc</code> does that first).</p>

#### `run` {#function-run}

<div class="function-declaration" id="function-run-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">int</span> run();</code></pre></div>

<p>--export: runs <code>project.export</code> and returns 0 or 1. --exec: runs the requests <code><a href="#function-initialise-1">initialise</a>()</code> parsed and returns the headless runner&#x27;s exit code (2 when --out cannot be written). --export wins when both are given. Otherwise frames until quit; returns the exit code. <code>A</code> headless --ai-stdio session without a <code>TCP</code> server quits by itself and returns 0 once standard input ends.</p>

#### `server` {#function-server}

<div class="function-declaration" id="function-server-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../protocol/protocol-server/protocol-server.html">protocol_server</a> &amp;server() <span class="k">noexcept</span>;</code></pre></div>

#### `shutdown` {#function-shutdown}

<div class="function-declaration" id="function-shutdown-1"><pre><code><span class="k">void</span> shutdown();</code></pre></div>

<p>Stops the server and shuts the engine down (idempotent).</p>

#### `startup_scene_warning` {#function-startup-scene-warning}

<div class="function-declaration" id="function-startup-scene-warning-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="../../../engine/core/json/document/json-document.html">core::json::document</a>&gt; startup_scene_warning() <span class="k">const</span>;</code></pre></div>

<p>The data of the <code>warning</code> event that reports a startup scene the running engine could not load: {code: &quot;<code>STARTUP_SCENE_FAILED</code>&quot;, message}, the message being the engine&#x27;s (<code>engine::application::startup_scene_failure</code>). nullopt when the scene loaded, when the project names none, or without an engine. The headless runner writes it as the first line of its output; after a <code>project.open</code> restart the application emits it to every client by itself.</p>

#### `ticks` {#function-ticks}

<div class="function-declaration" id="function-ticks-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-u32">u32</a> ticks() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Frames pumped since <code><a href="#function-initialise-1">initialise</a>()</code>.</p>

#### `trust` {#function-trust}

<div class="function-declaration" id="function-trust-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../project-trust/project-trust.html">project_trust</a> &amp;trust() <span class="k">noexcept</span>;</code></pre></div>

<p>The answers about which projects this application opens (decision). Valid before <code><a href="#function-initialise-1">initialise</a>()</code> and across engine restarts; the context&#x27;s <code><a href="#function-trust-1">trust</a>()</code> points at it.</p>

### Private

#### `apply_pending_project_open` {#function-apply-pending-project-open}

<div class="function-declaration" id="function-apply-pending-project-open-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; apply_pending_project_open();</code></pre></div>

#### `boot_engine` {#function-boot-engine}

<div class="function-declaration" id="function-boot-engine-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; boot_engine(
    <a href="../../../engine/engine/application-desc/application-desc.html">engine::application_desc</a> desc);</code></pre></div>

#### `require_trusted` {#function-require-trusted}

<div class="function-declaration" id="function-require-trusted-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; require_trusted(
    <span class="k">const</span> std::filesystem::path &amp;projectPath) <span class="k">const</span>;</code></pre></div>

#### `run_exec` {#function-run-exec}

<div class="function-declaration" id="function-run-exec-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">int</span> run_exec();</code></pre></div>

#### `run_export` {#function-run-export}

<div class="function-declaration" id="function-run-export-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">int</span> run_export();</code></pre></div>

#### `start_engine` {#function-start-engine}

<div class="function-declaration" id="function-start-engine-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; start_engine(
    <span class="k">const</span> std::filesystem::path &amp;projectPath);</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
