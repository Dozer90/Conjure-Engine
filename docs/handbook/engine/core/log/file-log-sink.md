---
kind: class
---

[Engine](../../../engine.md) → [Core](../../core.md) → [log.h](../log.md)

# file_log_sink
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/log.h` |
| Source File Path | `engine/src/core/foundation/log.cpp` |
| API ↗ | [Errors and results](../../../../api/core/errors-and-results.md), [Logging and assertions](../../../../api/core/logging-and-assertions.md) |

## Syntax

```cpp
class CONJURE_API file_log_sink final : public log_sink
```

## Inheritance Hierarchy


[`log_sink`](log-sink.md) → `file_log_sink`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> file_log_sink(
    <span class="k">const</span> std::filesystem::path &amp;path,
    <span class="k">bool</span> truncate = true);</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~file_log_sink() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mState</code></td><td><code class="signature">std::unique_ptr&lt;<a href="../event/event-state.html">state</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `flush` {#function-flush}

<div class="function-declaration" id="function-flush-1"><pre><code><span class="k">void</span> flush() <span class="k">override</span>;</code></pre></div>

#### `is_open` {#function-is-open}

<div class="function-declaration" id="function-is-open-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_open() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>False when the file could not be opened; messages are then dropped silently.</p>

#### `write` {#function-write}

<div class="function-declaration" id="function-write-1"><pre><code><span class="k">void</span> write(
    <span class="k">const</span> <a href="log-message.html">log_message</a> &amp;message) <span class="k">override</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
