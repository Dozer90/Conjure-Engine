---
kind: class
---

[Engine](../../../engine.md) → [Core](../../core.md) → [log.h](../log.md)

# console_log_sink
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/log.h` |
| Source File Path | `engine/src/core/foundation/log.cpp` |
| API ↗ | [Errors and results](../../../../api/core/errors-and-results.md), [Logging and assertions](../../../../api/core/logging-and-assertions.md) |

## Syntax

```cpp
class CONJURE_API console_log_sink final : public log_sink
```

## Inheritance Hierarchy


[`log_sink`](log-sink.md) → `console_log_sink`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> console_log_sink(
    <span class="k">bool</span> useColour = true);</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~console_log_sink() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mUseColour</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">true</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `flush` {#function-flush}

<div class="function-declaration" id="function-flush-1"><pre><code><span class="k">void</span> flush() <span class="k">override</span>;</code></pre></div>

#### `write` {#function-write}

<div class="function-declaration" id="function-write-1"><pre><code><span class="k">void</span> write(
    <span class="k">const</span> <a href="log-message.html">log_message</a> &amp;message) <span class="k">override</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
