---
kind: class
---

[Editor](../../../editor.md) → [Applications](../../applications.md) → [play_controller.h](../play-controller.md)

# play_controller
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/play_controller.h` |
| Source File Path | `engine/src/editor/core/play_controller.cpp` |
| API ↗ | [Headless mode and the command model](../../../../api/editor/headless-and-commands.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
class CONJURE_API play_controller
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> play_controller(
    <a href="../editor-context/editor-context.html">editor_context</a> &amp;context);</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~play_controller();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">FILE_<wbr>EDITS_<wbr>LABEL</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code></td><td><code class="signature"><span class="s">"File changes during play"</span></code></td><td>The label of the undo entry <code>stop()</code> pushes for the file edits of a play session.</td></tr>
<tr><td>private</td><td><code class="signature">mContext</code></td><td><code class="signature"><a href="../editor-context/editor-context.html">editor_context</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Ended<wbr>Fixed<wbr>Steps</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Ended<wbr>Simulation<wbr>Time</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-f64">f64</a></code></td><td><code class="signature"><span class="mi">0.0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFileEdits</code></td><td><code class="signature"><a href="../undo-stack/undo-entry.html">undo_entry</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPendingStep</code></td><td><code class="signature">std::<wbr>optional&lt;<wbr><a href="play-controller-pending-step.html">pending_<wbr>step</a>&gt;</code></td><td><code class="signature">std::nullopt</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Selection<wbr>Backup</code></td><td><code class="signature">std::vector&lt;<a href="../../../engine/core/uuid/uuid.html">core::uuid</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Session<wbr>Start<wbr>Fixed<wbr>Step</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Session<wbr>Start<wbr>Fixed<wbr>Time</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-f64">f64</a></code></td><td><code class="signature"><span class="mi">0.0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Session<wbr>Start<wbr>Frame</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Simulation<wbr>Frames</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSnapshot</code></td><td><code class="signature">std::<wbr>optional&lt;<wbr><a href="../../../engine/scene/scene-snapshot/scene-snapshot.html">scene::<wbr>scene_<wbr>snapshot</a>&gt;</code></td><td><code class="signature">std::nullopt</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mState</code></td><td><code class="signature"><a href="../play-controller.html#symbol-play-state">play_state</a></code></td><td><code class="signature">play_<wbr>state::<wbr>EDITING</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Warned<wbr>File<wbr>Edit</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Warned<wbr>Unrecorded<wbr>Edit</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `apply_engine_mode` {#function-apply-engine-mode}

<div class="function-declaration" id="function-apply-engine-mode-1"><pre><code><span class="k">void</span> apply_engine_mode();</code></pre></div>

<p>Puts the engine into the mode the state requires (<code>PAUSED</code> while <code>EDITING</code>): called after the engine is (re)initialised.</p>

#### `describe` {#function-describe}

<div class="function-declaration" id="function-describe-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/json/document/json-document.html">core::json::document</a> describe() <span class="k">const</span>;</code></pre></div>

<p>The <code>play.state</code> event and the play.* results: {state, frame (the engine frame), simulationFrame, simulationTime, fixedStepIndex}, the last three from this play session (<code><a href="#function-simulation-frame-1">simulation_frame</a>()</code>, <code><a href="#function-simulation-time-1">simulation_time</a>()</code>, <code><a href="#function-simulation-fixed-steps-1">simulation_fixed_steps</a>()</code>).</p>

#### `has_pending_step` {#function-has-pending-step}

<div class="function-declaration" id="function-has-pending-step-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> has_pending_step() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `is_editing` {#function-is-editing}

<div class="function-declaration" id="function-is-editing-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_editing() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `keep_file_edit` {#function-keep-file-edit}

<div class="function-declaration" id="function-keep-file-edit-1"><pre><code><span class="k">void</span> keep_file_edit(
    std::string_view command,
    <a href="../editor-command/command-request.html">command_request</a> replay,
    <a href="../editor-command/command-inverse.html">command_inverse</a> inverse);</code></pre></div>

<p>Keeps the replay and the inverse of an <code>ON_DISK</code> command that ran while not <code>EDITING</code>: its effect lives in the project&#x27;s files, which the snapshot restore does not touch, so <code><a href="#function-stop-1">stop</a>()</code> pushes everything kept, in the order it ran, as one undo entry. Emits warning <code>PLAY_FILE_EDIT</code> once per session with the command&#x27;s name as cmd. Ignored while <code>EDITING</code>, when the undo stack records the command itself.</p>

#### `kept_file_edits` {#function-kept-file-edits}

<div class="function-declaration" id="function-kept-file-edits-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-usize">usize</a> kept_file_edits() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The file edits kept in this play session so far (what the entry <code><a href="#function-stop-1">stop</a>()</code> pushes will hold); 0 while <code>EDITING</code>.</p>

#### `note_unrecorded_edit` {#function-note-unrecorded-edit}

<div class="function-declaration" id="function-note-unrecorded-edit-1"><pre><code><span class="k">void</span> note_unrecorded_edit();</code></pre></div>

<p><code>A</code> scene edit ran while not <code>EDITING</code>: it is not recorded for undo and <code><a href="#function-stop-1">stop</a>()</code> discards it with the snapshot restore. Emits warning <code>UNRECORDED_PLAY_EDIT</code> once per session; its message names the <code>ON_DISK</code> commands, whose file changes are kept instead.</p>

#### `pause` {#function-pause}

<div class="function-declaration" id="function-pause-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; pause();</code></pre></div>

<p><code>PLAYING</code> -&gt; <code>PAUSED</code>.</p>

#### `resume` {#function-resume}

<div class="function-declaration" id="function-resume-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; resume();</code></pre></div>

<p><code>PAUSED</code> -&gt; <code>PLAYING</code>.</p>

#### `simulation_fixed_steps` {#function-simulation-fixed-steps}

<div class="function-declaration" id="function-simulation-fixed-steps-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-u64">u64</a> simulation_fixed_steps() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Fixed steps run in this play session, from the engine&#x27;s fixedStepIndex at <code>play.start</code>; while <code>EDITING</code> the count of the session that ended last (0 before the first <code>play.start</code>).</p>

#### `simulation_frame` {#function-simulation-frame}

<div class="function-declaration" id="function-simulation-frame-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-u64">u64</a> simulation_frame() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Frames simulated in this play session (<code>play.step</code> counts one); while <code>EDITING</code> the count of the session that ended last (0 before the first <code>play.start</code>).</p>

#### `simulation_time` {#function-simulation-time}

<div class="function-declaration" id="function-simulation-time-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-f64">f64</a> simulation_time() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Simulated seconds of this play session, from the engine&#x27;s fixedTime at <code>play.start</code>: it advances only with fixed steps, so it stops while paused; while <code>EDITING</code> the time of the session that ended last.</p>

#### `start` {#function-start}

<div class="function-declaration" id="function-start-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; start();</code></pre></div>

<p><code>EDITING</code> -&gt; <code>PLAYING</code>: snapshot, freeze undo, <code>PLAY</code> scripts, <code>RUNNING</code> engine, the session&#x27;s clocks back to 0. <code>WRONG_PLAY_STATE</code> otherwise; a failed snapshot is <code>INTERNAL</code> and nothing changes.</p>

#### `state` {#function-state}

<div class="function-declaration" id="function-state-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../play-controller.html#symbol-play-state">play_state</a> state() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `step` {#function-step}

<div class="function-declaration" id="function-step-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; step(
    <a href="../../../engine/core/types.html#symbol-u32">u32</a> fixedSteps,
    <a href="../editor-command/async-reply.html">async_reply</a> reply);</code></pre></div>

<p><code>PAUSED</code> only: the next frame runs exactly <code>fixedSteps</code> fixed steps; the reply completes in <code><a href="#function-tick-1">tick</a>()</code> once <code><a href="../../../engine/engine/engine-time/engine-time.html">engine_time</a>.fixedStepIndex</code> advanced by that many. <code>A</code> second step while one is pending is <code>BUSY</code>; zero steps are <code>INVALID_ARGS</code>.</p>

#### `stop` {#function-stop}

<div class="function-declaration" id="function-stop-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; stop();</code></pre></div>

<p><code>PLAYING</code> or <code>PAUSED</code> -&gt; <code>EDITING</code>: <code>PAUSED</code> engine, <code>EDIT</code> scripts, snapshot restored (a failed restore logs an ERROR and leaves the scene empty), selection remapped, undo thawed, the file edits kept by <code><a href="#function-keep-file-edit-1">keep_file_edit</a>()</code> pushed as one undo entry labelled <code>FILE_EDITS_LABEL</code> (also after a failed restore: the files do not depend on the scene), <code>play.state</code> emitted with the ended session&#x27;s clocks, which stay until the next <code><a href="#function-start-1">start</a>()</code>.</p>

#### `tick` {#function-tick}

<div class="function-declaration" id="function-tick-1"><pre><code><span class="k">void</span> tick();</code></pre></div>

<p>Once per frame: completes a pending step and counts simulation frames.</p>

### Private

#### `emit_state` {#function-emit-state}

<div class="function-declaration" id="function-emit-state-1"><pre><code><span class="k">void</span> emit_state();</code></pre></div>

#### `set_script_mode` {#function-set-script-mode}

<div class="function-declaration" id="function-set-script-mode-1"><pre><code><span class="k">void</span> set_script_mode(
    <span class="k">bool</span> playing);</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
