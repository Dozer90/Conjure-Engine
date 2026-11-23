---
kind: header
---

[Engine](../../engine.md) → [Core](../core.md)

# assert.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/assert.h` |
| API ↗ | [Logging and assertions](../../../api/core/logging-and-assertions.md), [Processes, libraries and images](../../../api/core/system.md) |

<p>Assertions.</p>

## Structs

| File | Description |
|---|---|
| [`assert_info`](assert/assert-info.md) | Everything known about a failed assertion. |

## Enumerations

### `assert_action` {#symbol-assert-action}

```cpp
enum class assert_action : u8 {
    BREAK,      // break into the debugger (or trap when none is attached)
    CONTINUE,   // carry on as if the assertion passed
    ABORT       // terminate the process
};
```

What the program does after the handler returns.

## Type Aliases

### `assert_handler` {#symbol-assert-handler}

```cpp
using assert_handler = assert_action (*)(const assert_info &info);
```

## Functions

### Public

#### `current_assert_handler` {#function-current-assert-handler}

<div class="function-declaration" id="function-current-assert-handler-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="assert.html#symbol-assert-handler">assert_handler</a> current_assert_handler() <span class="k">noexcept</span>;</code></pre></div>

<p>The handler currently in effect.</p>

#### `default_assert_handler` {#function-default-assert-handler}

<div class="function-declaration" id="function-default-assert-handler-1"><pre><code><span class="na">CONJURE_API</span> <a href="assert.html#symbol-assert-action">assert_action</a> default_assert_handler(
    <span class="k">const</span> <a href="assert/assert-info.html">assert_info</a> &amp;info);</code></pre></div>

<p>The built-in handler: logs the failure and asks to break.</p>

#### `detail::format_assert_message` {#function-detail-format-assert-message}

<div class="function-declaration" id="function-detail-format-assert-message-1"><pre><code><span class="k">inline</span> std::string format_assert_message();</code></pre></div>

<div class="function-declaration" id="function-detail-format-assert-message-2"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span>... Args&gt; std::string format_assert_message(std::format_string&lt;Args...&gt; format, Args &amp;&amp;...args);</code></pre></div>

#### `detail::report_assert_failure` {#function-detail-report-assert-failure}

<div class="function-declaration" id="function-detail-report-assert-failure-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">bool</span> report_assert_failure( <span class="k">const</span> <span class="k">char</span> *expression, <span class="k">const</span> <span class="k">char</span> *file, <span class="k">int</span> line, <span class="k">const</span> <span class="k">char</span> *function, std::string message );</code></pre></div>

<p>Reports a failure to the active handler. Returns <code>true</code> when the caller should break.</p>

#### `set_assert_handler` {#function-set-assert-handler}

<div class="function-declaration" id="function-set-assert-handler-1"><pre><code><span class="na">CONJURE_API</span> <a href="assert.html#symbol-assert-handler">assert_handler</a> set_assert_handler(
    <a href="assert.html#symbol-assert-handler">assert_handler</a> handler) <span class="k">noexcept</span>;</code></pre></div>

<p>Installs a new assertion handler and returns the previous one. Pass <code>nullptr</code> to restore the default handler, which logs the failure and breaks.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="assert_action"></a>
<a id="assert_actionabort"></a>
<a id="assert_actionbreak"></a>
<a id="assert_actioncontinue"></a>
<a id="assert_handler"></a>
<a id="assert_info"></a>
<a id="assert_infoexpression"></a>
<a id="assert_infofile"></a>
<a id="assert_infofunction"></a>
<a id="assert_infoline"></a>
<a id="assert_infomessage"></a>
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="current_assert_handler"></a>
<a id="declaration-summaries"></a>
<a id="default_assert_handler"></a>
<a id="dependencies"></a>
<a id="detailformat_assert_message"></a>
<a id="detailreport_assert_failure"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="format_assert_message"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="report_assert_failure"></a>
<a id="set_assert_handler"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
