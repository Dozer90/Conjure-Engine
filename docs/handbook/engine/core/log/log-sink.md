---
kind: class
---

[Engine](../../../engine.md) → [Core](../../core.md) → [log.h](../log.md)

# log_sink
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/log.h` |
| Source File Path | `engine/src/core/foundation/log.cpp` |
| API ↗ | [Errors and results](../../../../api/core/errors-and-results.md), [Logging and assertions](../../../../api/core/logging-and-assertions.md) |

## Syntax

```cpp
class CONJURE_API log_sink
```

## Derived Classes


<details><summary>Classes that use this as a base</summary>

<ul>

<li><a href="callback-log-sink.html">callback_log_sink</a></li>

<li><a href="console-log-sink.html">console_log_sink</a></li>

<li><a href="file-log-sink.html">file_log_sink</a></li>

</ul>

</details>

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">log_sink();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">virtual</span> ~log_sink();</code></td><td></td></tr>
</tbody>
</table>



## Functions

### Public

#### `flush` {#function-flush}

<div class="function-declaration" id="function-flush-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> flush();</code></pre></div>

#### `write` {#function-write}

<div class="function-declaration" id="function-write-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> write(
    <span class="k">const</span> <a href="log-message.html">log_message</a> &amp;message) = <span class="mi">0</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->
