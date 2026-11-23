---
kind: class
---

[Engine](../../../engine.md) → [Core](../../core.md) → [log.h](../log.md)

# log_category
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/log.h` |
| Source File Path | `engine/src/core/foundation/log.cpp` |
| API ↗ | [Errors and results](../../../../api/core/errors-and-results.md), [Logging and assertions](../../../../api/core/logging-and-assertions.md) |

## Syntax

```cpp
class CONJURE_API log_category
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> log_category(
    <span class="k">const</span> <span class="k">char</span> *name,
    <a href="../log.html#symbol-log-level">log_level</a> level = log_level::TRACE);</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~log_category();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mLevel</code></td><td><code class="signature">std::atomic&lt;<a href="../log.html#symbol-log-level">log_level</a>&gt;</code></td><td><code class="signature">log_level::TRACE</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mName</code></td><td><code class="signature"><span class="k">const</span> <span class="k">char</span> *</code></td><td><code class="signature"><span class="s">""</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `is_enabled` {#function-is-enabled}

<div class="function-declaration" id="function-is-enabled-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_enabled(
    <a href="../log.html#symbol-log-level">log_level</a> level) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when a message at <code>level</code> passes this category&#x27;s filter.</p>

#### `level` {#function-level}

<div class="function-declaration" id="function-level-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../log.html#symbol-log-level">log_level</a> level() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Minimum level that passes this category&#x27;s filter.</p>

#### `name` {#function-name}

<div class="function-declaration" id="function-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <span class="k">char</span> *name() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The display name used in log lines and for lookup by name.</p>

#### `set_level` {#function-set-level}

<div class="function-declaration" id="function-set-level-1"><pre><code><span class="k">void</span> set_level(
    <a href="../log.html#symbol-log-level">log_level</a> level) <span class="k">noexcept</span>;</code></pre></div>

<p>Changes the minimum level. Safe to call from any thread.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
