---
kind: header
---

[Engine](../../engine.md) → [Engine](../engine.md)

# exit_code.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/engine/exit_code.h` |
| API ↗ | [Engine: the application and the frame](../../../api/engine/index.md), [Running the engine](../../../api/engine/running-the-engine.md) |

<p>Process exit codes of the executables. <code>application::run()</code> returns one of these as an int; runtime/<code>main.cpp</code> and <code>conjure_editor</code> return them from <code>main()</code>.</p>

## Enumerations

### `exit_code` {#symbol-exit-code}

```cpp
enum class exit_code : int {
    OK = 0,
    INIT_FAILURE = 1,       // initialise() failed
    RUNTIME_FAILURE = 2,    // step_frame() reported an unrecoverable error or a resize failed twice
    BAD_ARGUMENTS = 3,      // application_desc::from_command_line failed
    DEVICE_LOST = 4         // the graphics device was lost (no recovery in v1)
};
```

## Functions

### Public

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="exit-code.html#symbol-exit-code">exit_code</a> code) <span class="k">noexcept</span>;</code></pre></div>

<p>The enumerator name of a code (&quot;<code>RUNTIME_FAILURE</code>&quot;); &quot;<code>UNKNOWN</code>&quot; for values outside the enumeration.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="exit_code"></a>
<a id="exit_codebad_arguments"></a>
<a id="exit_codedevice_lost"></a>
<a id="exit_codeinit_failure"></a>
<a id="exit_codeok"></a>
<a id="exit_coderuntime_failure"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
