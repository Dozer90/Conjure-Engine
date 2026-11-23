---
kind: class
---

[Engine](../../../engine.md) → [Core](../../core.md) → [log.h](../log.md)

# callback_log_sink
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/log.h` |
| Source File Path | `engine/src/core/foundation/log.cpp` |
| API ↗ | [Errors and results](../../../../api/core/errors-and-results.md), [Logging and assertions](../../../../api/core/logging-and-assertions.md) |

## Syntax

```cpp
class CONJURE_API callback_log_sink final : public log_sink
```

## Inheritance Hierarchy


[`log_sink`](log-sink.md) → `callback_log_sink`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> callback_log_sink(
    std::function&lt;<span class="k">void</span>(<span class="k">const</span> <a href="log-message.html">log_message</a> &amp;)&gt; callback);</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~callback_log_sink() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>



## Functions

### Public

#### `write` {#function-write}

<div class="function-declaration" id="function-write-1"><pre><code><span class="k">void</span> write(
    <span class="k">const</span> <a href="log-message.html">log_message</a> &amp;message) <span class="k">override</span>;</code></pre></div>

### Private

#### `void` {#function-void}

<div class="function-declaration" id="function-void-1"><pre><code>std::function&lt;<span class="k">void</span>(
    <span class="k">const</span> <a href="log-message.html">log_message</a> &amp;)&gt; mCallback =;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->
