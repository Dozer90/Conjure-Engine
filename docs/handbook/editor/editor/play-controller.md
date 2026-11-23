---
kind: header
---

[Editor](../../editor.md) → [Applications](../applications.md)

# play_controller.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/play_controller.h` |
| API ↗ | [Headless mode and the command model](../../../api/editor/headless-and-commands.md), [The editor](../../../api/editor/index.md) |

<p>Play mode in the editing world. <code>EDITING</code> keeps the engine <code>PAUSED</code> and the script runtime in <code>EDIT</code> mode; <code>play.start</code> captures a scene snapshot, freezes the undo stack and runs the engine; <code>play.stop</code> restores the snapshot (guids preserved), remaps the selection and thaws the undo stack. <code>A</code> scene edit made while playing is not recorded and goes with the restore; a file edit (an <code>ON_DISK</code> command) stays on disk, so the controller keeps its inverse and <code>play.stop</code> pushes all of them as one undo entry. <code>play.step</code> runs exactly n fixed steps through <code>application::step_fixed</code> and answers once they ran. The clocks play.* report belong to the play session: <code>play.start</code> records the engine&#x27;s fixedTime and fixedStepIndex, the session&#x27;s time and fixed steps count from there (they stop while paused), and the ended session&#x27;s values stay until the next <code>play.start</code>.</p>

## Structs

| File | Description |
|---|---|
| [`play_controller::pending_step`](play-controller/play-controller-pending-step.md) |  |

## Classes

| File | Description |
|---|---|
| [`play_controller`](play-controller/play-controller.md) |  |

## Enumerations

### `play_state` {#symbol-play-state}

```cpp
enum class play_state : u8 {
    EDITING = 0,
    PLAYING,
    PAUSED
};
```

## Forward Declarations

### `editor_context` {#symbol-editor-context}

```cpp
class editor_context;
```

## Functions

### Public

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="play-controller.html#symbol-play-state">play_state</a> state) <span class="k">noexcept</span>;</code></pre></div>

<p>The wire name of a state (&quot;editing&quot;).</p>

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
<a id="editor_context"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="play_controller"></a>
<a id="play_controllerapply_engine_mode"></a>
<a id="play_controllerdescribe"></a>
<a id="play_controlleremit_state"></a>
<a id="play_controllerfile_edits_label"></a>
<a id="play_controllerhas_pending_step"></a>
<a id="play_controlleris_editing"></a>
<a id="play_controllerkeep_file_edit"></a>
<a id="play_controllerkept_file_edits"></a>
<a id="play_controllermcontext"></a>
<a id="play_controllermendedfixedsteps"></a>
<a id="play_controllermendedsimulationtime"></a>
<a id="play_controllermfileedits"></a>
<a id="play_controllermpendingstep"></a>
<a id="play_controllermselectionbackup"></a>
<a id="play_controllermsessionstartfixedstep"></a>
<a id="play_controllermsessionstartfixedtime"></a>
<a id="play_controllermsessionstartframe"></a>
<a id="play_controllermsimulationframes"></a>
<a id="play_controllermsnapshot"></a>
<a id="play_controllermstate"></a>
<a id="play_controllermwarnedfileedit"></a>
<a id="play_controllermwarnedunrecordededit"></a>
<a id="play_controllernote_unrecorded_edit"></a>
<a id="play_controlleroperator"></a>
<a id="play_controllerpause"></a>
<a id="play_controllerpending_step"></a>
<a id="play_controllerpending_stepreply"></a>
<a id="play_controllerpending_steprequestedframe"></a>
<a id="play_controllerpending_steptargetfixedstepindex"></a>
<a id="play_controllerplay_controller"></a>
<a id="play_controllerplay_controller_1"></a>
<a id="play_controllerresume"></a>
<a id="play_controllerset_script_mode"></a>
<a id="play_controllersimulation_fixed_steps"></a>
<a id="play_controllersimulation_frame"></a>
<a id="play_controllersimulation_time"></a>
<a id="play_controllerstart"></a>
<a id="play_controllerstate"></a>
<a id="play_controllerstep"></a>
<a id="play_controllerstop"></a>
<a id="play_controllertick"></a>
<a id="play_state"></a>
<a id="play_stateediting"></a>
<a id="play_statepaused"></a>
<a id="play_stateplaying"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
