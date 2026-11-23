---
kind: class
---

[Engine](../../../engine.md) → [Engine](../../engine.md) → [application.h](../application.md)

# application
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::engine` |
| Include File Path | `engine/include/conjure/engine/application.h` |
| Source File Path | `engine/src/engine/application.cpp` |
| API ↗ | [Engine: the application and the frame](../../../../api/engine/index.md), [Running the engine](../../../../api/engine/running-the-engine.md) |

## Syntax

```cpp
class CONJURE_API application
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> application(
    <a href="../application-desc/application-desc.html">application_desc</a> desc);</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~application();</code></td><td>Shuts down when the owner forgot (shutdown is idempotent).</td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">m<wbr>Accepting<wbr>Module<wbr>Subsystems</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mClock</code></td><td><code class="signature"><a href="../../core/clock/clock.html">core::clock</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mContext</code></td><td><code class="signature"><a href="../engine-context/engine-context.html">engine_context</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mDesc</code></td><td><code class="signature"><a href="../application-desc/application-desc.html">application_desc</a></code></td><td>—</td><td></td></tr>
<tr><td>private</td><td><code class="signature">mExitCode</code></td><td><code class="signature"><a href="../exit-code.html#symbol-exit-code">exit_code</a></code></td><td><code class="signature">exit_code::OK</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFixedClock</code></td><td><code class="signature"><a href="../fixed-step-clock/fixed-step-clock.html">fixed_step_clock</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Fixed<wbr>Steps<wbr>Pending</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFrameLoop</code></td><td><code class="signature">std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../application.html">frame_<wbr>loop</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mInFrame</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mLayers</code></td><td><code class="signature"><a href="../layer-stack/layer-stack.html">layer_stack</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Main<wbr>Thread<wbr>Queue</code></td><td><code class="signature">std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../application.html">main_<wbr>thread_<wbr>queue</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mOwnedClock</code></td><td><code class="signature">std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../../core/clock/clock.html">core::<wbr>clock</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPacer</code></td><td><code class="signature">std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../application.html">frame_<wbr>pacer</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Pending<wbr>Fixed<wbr>Steps</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Quit<wbr>Requested</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mServices</code></td><td><code class="signature">std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../application.html">engine_<wbr>services</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Simulation<wbr>Mode</code></td><td><code class="signature"><a href="../application.html#symbol-simulation-mode">simulation_mode</a></code></td><td><code class="signature">simulation_<wbr>mode::<wbr>RUNNING</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Started<wbr>Subsystem<wbr>Count</code></td><td><code class="signature"><a href="../../core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Startup<wbr>Scene<wbr>Failure</code></td><td><code class="signature">std::<wbr>optional&lt;<wbr><a href="../../core/error/error.html">core::<wbr>error</a>&gt;</code></td><td><code class="signature">std::nullopt</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mState</code></td><td><code class="signature"><a href="../application.html#symbol-application-state">application_state</a></code></td><td><code class="signature">application_<wbr>state::<wbr>CREATED</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mStats</code></td><td><code class="signature"><a href="../frame-stats/frame-stats.html">frame_stats</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSubsystems</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr>std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../engine-subsystem/engine-subsystem.html">engine_<wbr>subsystem</a>&gt;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mTime</code></td><td><code class="signature"><a href="../engine-time/engine-time.html">engine_time</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>User<wbr>Subsystem<wbr>Begin</code></td><td><code class="signature"><a href="../../core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>User<wbr>Subsystem<wbr>Count</code></td><td><code class="signature"><a href="../../core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>User<wbr>Subsystems</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr>std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../engine-subsystem/engine-subsystem.html">engine_<wbr>subsystem</a>&gt;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `add_subsystem` {#function-add-subsystem}

<div class="function-declaration" id="function-add-subsystem-1"><pre><code><span class="k">void</span> add_subsystem(
    std::unique_ptr&lt;<a href="../engine-subsystem/engine-subsystem.html">engine_subsystem</a>&gt; subsystem);</code></pre></div>

<p>Executables and plugins add subsystems before <code><a href="#function-initialise-1">initialise</a>()</code> only (a module entry may add one during startup step 15, through the <code><a href="application.html">application</a></code> service); afterwards the call asserts and is ignored. <code>A</code> <code>null</code> pointer asserts and is ignored. Startup order: the built-in adapters, then the subsystems module entries added (step 15), then the ones added here in push order (step 16); shutdown reverses it.</p>

#### `cancel_fixed_steps` {#function-cancel-fixed-steps}

<div class="function-declaration" id="function-cancel-fixed-steps-1"><pre><code><span class="k">void</span> cancel_fixed_steps() <span class="k">noexcept</span>;</code></pre></div>

<p>Drops a <code>step_fixed</code> request the next frame has not consumed yet, so that frame runs as the simulation mode says. Editor <code>play.stop</code> calls it: a forced step overrides <code>PAUSED</code>, and a step left behind would run the simulation over the restored editing scene. <code>A</code> no-op without a request.</p>

#### `clock` {#function-clock}

<div class="function-declaration" id="function-clock-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/clock/clock.html">core::clock</a> &amp;clock() <span class="k">noexcept</span>;</code></pre></div>

<p>The clock frames are timed with: the injected override or the owned steady clock.</p>

#### `context` {#function-context}

<div class="function-declaration" id="function-context-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../engine-context/engine-context.html">engine_context</a> &amp;context() <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-context-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../engine-context/engine-context.html">engine_context</a> &amp;context() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `current_exit_code` {#function-current-exit-code}

<div class="function-declaration" id="function-current-exit-code-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../exit-code.html#symbol-exit-code">exit_code</a> current_exit_code() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The exit code <code><a href="#function-run-1">run</a>()</code> will return (<code>OK</code> until a quit request or a failure).</p>

#### `current_simulation_mode` {#function-current-simulation-mode}

<div class="function-declaration" id="function-current-simulation-mode-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../application.html#symbol-simulation-mode">simulation_mode</a> current_simulation_mode() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `desc` {#function-desc}

<div class="function-declaration" id="function-desc-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../application-desc/application-desc.html">application_desc</a> &amp;desc() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The descriptor the <code><a href="application.html">application</a></code> was built from (the project lives in it).</p>

#### `engine_directory` {#function-engine-directory}

<div class="function-declaration" id="function-engine-directory-1"><pre><code>[[<span class="na">nodiscard</span>]] std::filesystem::path engine_directory() <span class="k">const</span>;</code></pre></div>

<p>The directory of the running executable (absolute). The engine finds what ships with it next to the executable: shaders/, managed/, engine/ and its plugins.</p>

#### `initialise` {#function-initialise}

<div class="function-declaration" id="function-initialise-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; initialise();</code></pre></div>

<p>Starts every subsystem in the documented order (3.4). On the first failure the already-started subsystems are shut down in reverse and the error is returned; the object is then reusable. <code>ALREADY_INITIALISED</code> when the state is not <code>CREATED</code> or <code>STOPPED</code>.</p>

#### `inject_render_failure` {#function-inject-render-failure}

<div class="function-declaration" id="function-inject-render-failure-1"><pre><code><span class="k">void</span> inject_render_failure(
    <a href="../../core/error/error.html">core::error</a> error,
    <a href="../../core/types.html#symbol-u32">u32</a> frameCount);</code></pre></div>

<p>Makes the next <code>frameCount</code> <code>render_frame</code> results fail with <code>error</code> (the renderer still runs; its result is replaced). Drives the failure rules of 06 section 6 in headless tests the way <code>rhi::device::simulate_device_loss()</code> drives device loss. Ignored (asserted) when not <code>RUNNING</code>; no effect without a renderer.</p>

#### `is_inside_frame` {#function-is-inside-frame}

<div class="function-declaration" id="function-is-inside-frame-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_inside_frame() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True while a <code><a href="#function-step-frame-1">step_frame</a>()</code> is executing (module loading is refused inside a frame).</p>

#### `is_quit_requested` {#function-is-quit-requested}

<div class="function-declaration" id="function-is-quit-requested-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_quit_requested() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `layers` {#function-layers}

<div class="function-declaration" id="function-layers-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../layer-stack/layer-stack.html">layer_stack</a> &amp;layers() <span class="k">noexcept</span>;</code></pre></div>

#### `managed_directory` {#function-managed-directory}

<div class="function-declaration" id="function-managed-directory-1"><pre><code>[[<span class="na">nodiscard</span>]] std::filesystem::path managed_directory() <span class="k">const</span>;</code></pre></div>

<p>The directory holding <code>Conjure.Managed.dll</code> and the bootstrap, always absolute: the configuration&#x27;s <code>scripting.managedDirectory</code> when set (a relative one is taken against the current directory), otherwise <code><a href="#function-engine-directory-1">engine_directory</a>()</code> / &quot;managed&quot;. The script host starts from it, and a tool that builds game scripts hands it to MSBuild as ConjureManagedDir (the editor&#x27;s <code>script.compile</code>), so both always mean the same SDK.</p>

#### `post_to_main_thread` {#function-post-to-main-thread}

<div class="function-declaration" id="function-post-to-main-thread-1"><pre><code><span class="k">void</span> post_to_main_thread(
    std::function&lt;<span class="k">void</span>()&gt; task);</code></pre></div>

<p>Thread-safe. The task runs on the main thread before the next <code>FRAME_BEGIN</code> (frame step 4); a task posted during the drain runs in the next frame. Pending tasks are dropped at shutdown.</p>

#### `request_quit` {#function-request-quit}

<div class="function-declaration" id="function-request-quit-1"><pre><code><span class="k">void</span> request_quit(
    <a href="../exit-code.html#symbol-exit-code">exit_code</a> code = exit_code::OK) <span class="k">noexcept</span>;</code></pre></div>

<p>Asks <code><a href="#function-run-1">run</a>()</code> to stop after the current frame; the first code wins unless a later one is worse (<code>OK</code> &lt; <code>INIT_FAILURE</code> &lt; <code>RUNTIME_FAILURE</code> &lt; ... in enumerator order).</p>

#### `run` {#function-run}

<div class="function-declaration" id="function-run-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">int</span> run();</code></pre></div>

<p>Frames until quit or maxFrames, then <code><a href="#function-shutdown-1">shutdown</a>()</code>. Returns the process exit code; a <code>step_frame</code> failure logs, sets <code>RUNTIME_FAILURE</code> and stops. <code>INIT_FAILURE</code> when the <code><a href="application.html">application</a></code> was never initialised (the caller forgot <code><a href="#function-initialise-1">initialise</a>()</code>).</p>

#### `set_simulation_mode` {#function-set-simulation-mode}

<div class="function-declaration" id="function-set-simulation-mode-1"><pre><code><span class="k">void</span> set_simulation_mode(
    <a href="../application.html#symbol-simulation-mode">simulation_mode</a> mode) <span class="k">noexcept</span>;</code></pre></div>

#### `set_time_scale` {#function-set-time-scale}

<div class="function-declaration" id="function-set-time-scale-1"><pre><code><span class="k">void</span> set_time_scale(
    <a href="../../core/types.html#symbol-f32">f32</a> timeScale) <span class="k">noexcept</span>;</code></pre></div>

<p>Sets <code>engine_time::timeScale</code> from the next frame on; negative and non-finite values (NaN, infinity) become zero.</p>

#### `shutdown` {#function-shutdown}

<div class="function-declaration" id="function-shutdown-1"><pre><code><span class="k">void</span> shutdown();</code></pre></div>

<p>Shutdown in the documented order (3.5). Idempotent; a <code>CREATED</code> <code><a href="application.html">application</a></code> has nothing to do.</p>

#### `startup_scene_failure` {#function-startup-scene-failure}

<div class="function-declaration" id="function-startup-scene-failure-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::optional&lt;<a href="../../core/error/error.html">core::error</a>&gt; &amp;startup_scene_failure() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Why the startup scene of the last <code><a href="#function-initialise-1">initialise</a>()</code> did not load: <code>STARTUP_SCENE_FAILED</code> with the reason (not a known asset, a source that no longer imports, a load failure). nullopt when it loaded, when none was named, or when startup failed before step 20. <code>A</code> runtime fails <code><a href="#function-initialise-1">initialise</a>()</code> with this error. An editor (<code>engine_config::editor</code>) starts with an empty world instead, so the failure is kept here for whoever reports it: the editor&#x27;s --exec output opens with it. It survives <code><a href="#function-shutdown-1">shutdown</a>()</code> and is cleared by the next <code><a href="#function-initialise-1">initialise</a>()</code>.</p>

#### `state` {#function-state}

<div class="function-declaration" id="function-state-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../application.html#symbol-application-state">application_state</a> state() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `stats` {#function-stats}

<div class="function-declaration" id="function-stats-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../frame-stats/frame-stats.html">frame_stats</a> &amp;stats() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `step_fixed` {#function-step-fixed}

<div class="function-declaration" id="function-step-fixed-1"><pre><code><span class="k">void</span> step_fixed(
    <a href="../../core/types.html#symbol-u32">u32</a> fixedSteps) <span class="k">noexcept</span>;</code></pre></div>

<p>Editor <code>play.step</code>: the next frame runs exactly <code>fixedSteps</code> fixed steps regardless of the clock (its frame delta is <code>fixedSteps</code> * fixedDeltaTime) and leaves the mode <code>PAUSED</code> afterwards.</p>

#### `step_frame` {#function-step-frame}

<div class="function-declaration" id="function-step-frame-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; step_frame();</code></pre></div>

<p>Runs exactly one frame (3.1). Public so tests, the editor step button and the AI command server drive the same code as <code><a href="#function-run-1">run</a>()</code>. Returns an error only for unrecoverable runtime failures; <code>NOT_INITIALISED</code> (asserted) when the <code><a href="application.html">application</a></code> is not <code>RUNNING</code>.</p>

<div class="function-declaration" id="function-step-frame-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; step_frame(
    <a href="../../core/types.html#symbol-f64">f64</a> deltaSeconds);</code></pre></div>

<p>As <code><a href="#function-step-frame-1">step_frame</a>()</code>, but <code>deltaSeconds</code> replaces the clock / forced delta for this frame (tests), except on the first frame, which always runs exactly fixedDeltaSeconds so a run is deterministic from frame 0. The value is clamped like any frame delta: a negative or non-finite delta is a zero-length frame.</p>

#### `subsystems` {#function-subsystems}

<div class="function-declaration" id="function-subsystems-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> std::unique_ptr&lt;<a href="../engine-subsystem/engine-subsystem.html">engine_subsystem</a>&gt;&gt; subsystems() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Every subsystem in startup order: the built-in adapters, then the user-added ones. Empty before <code><a href="#function-initialise-1">initialise</a>()</code> and after <code><a href="#function-shutdown-1">shutdown</a>()</code>.</p>

#### `time` {#function-time}

<div class="function-declaration" id="function-time-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../engine-time/engine-time.html">engine_time</a> &amp;time() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

### Private

#### `load_startup_scene` {#function-load-startup-scene}

<div class="function-declaration" id="function-load-startup-scene-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; load_startup_scene();</code></pre></div>

#### `raise_exit_code` {#function-raise-exit-code}

<div class="function-declaration" id="function-raise-exit-code-1"><pre><code><span class="k">void</span> raise_exit_code(
    <a href="../exit-code.html#symbol-exit-code">exit_code</a> code) <span class="k">noexcept</span>;</code></pre></div>

#### `run_frame` {#function-run-frame}

<div class="function-declaration" id="function-run-frame-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; run_frame(
    std::optional&lt;<a href="../../core/types.html#symbol-f64">f64</a>&gt; deltaOverride);</code></pre></div>

#### `start_core` {#function-start-core}

<div class="function-declaration" id="function-start-core-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; start_core();</code></pre></div>

#### `start_subsystems` {#function-start-subsystems}

<div class="function-declaration" id="function-start-subsystems-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; start_subsystems();</code></pre></div>

#### `stop_core` {#function-stop-core}

<div class="function-declaration" id="function-stop-core-1"><pre><code><span class="k">void</span> stop_core();</code></pre></div>

#### `stop_subsystems` {#function-stop-subsystems}

<div class="function-declaration" id="function-stop-subsystems-1"><pre><code><span class="k">void</span> stop_subsystems();</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
