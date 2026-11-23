---
kind: header
---

[Engine](../../engine.md) → [Scripting](../scripting.md)

# hot_reload.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/scripting/hot_reload.h` |
| API ↗ | [Hosting, the native API and hot reload](../../../api/scripting/hosting-and-hot-reload.md), [Scripting](../../../api/scripting/index.md) |

<p>Hot reload driver. Requests (editor build success, the <code>script.reload</code> command, or the controller&#x27;s own watch on the output assembly) are debounced and executed by <code>tick()</code> at the frame boundary, before the first script phase.</p>



## Classes

| File | Description |
|---|---|
| [`hot_reload_controller`](hot-reload/hot-reload-controller.md) |  |

## Enumerations

### `reload_state` {#symbol-reload-state}

```cpp
enum class reload_state : u8 {
    IDLE,
    PENDING,        // requested, waiting for the debounce window
    RELOADING       // reload_game_assembly in progress (visible only from inside on_reloaded)
};
```

## Functions

### Public

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="hot-reload.html#symbol-reload-state">reload_state</a> state) <span class="k">noexcept</span>;</code></pre></div>

<p>The upper-case name of a state (&quot;<code>PENDING</code>&quot;).</p>

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
<a id="hot_reload_controller"></a>
<a id="hot_reload_controllerassembly_path"></a>
<a id="hot_reload_controllerdebounce_seconds"></a>
<a id="hot_reload_controllerhot_reload_controller"></a>
<a id="hot_reload_controllerhot_reload_controller_1"></a>
<a id="hot_reload_controllerlast_result"></a>
<a id="hot_reload_controllermassemblypath"></a>
<a id="hot_reload_controllermdebounceseconds"></a>
<a id="hot_reload_controllermhaswatchstamp"></a>
<a id="hot_reload_controllermimmediate"></a>
<a id="hot_reload_controllermlastresult"></a>
<a id="hot_reload_controllermlastwatchtime"></a>
<a id="hot_reload_controllermreloadcount"></a>
<a id="hot_reload_controllermrequestcount"></a>
<a id="hot_reload_controllermrequesttime"></a>
<a id="hot_reload_controllermruntime"></a>
<a id="hot_reload_controllermstate"></a>
<a id="hot_reload_controllermwatchintervalseconds"></a>
<a id="hot_reload_controllermwatchretriesleft"></a>
<a id="hot_reload_controllermwatchstamp"></a>
<a id="hot_reload_controllermwatchtriggered"></a>
<a id="hot_reload_controlleroperator"></a>
<a id="hot_reload_controllerperform_reload"></a>
<a id="hot_reload_controllerpoll_assembly_file"></a>
<a id="hot_reload_controllerreload_count"></a>
<a id="hot_reload_controllerrequest"></a>
<a id="hot_reload_controllerrequest_count"></a>
<a id="hot_reload_controllerset_assembly_path"></a>
<a id="hot_reload_controllerset_debounce_seconds"></a>
<a id="hot_reload_controllerset_watch_interval_seconds"></a>
<a id="hot_reload_controllerstate"></a>
<a id="hot_reload_controllertick"></a>
<a id="hot_reload_controllerwatch_interval_seconds"></a>
<a id="hot_reload_controllerwatch_retry_limit"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="reload_state"></a>
<a id="reload_stateidle"></a>
<a id="reload_statepending"></a>
<a id="reload_statereloading"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
