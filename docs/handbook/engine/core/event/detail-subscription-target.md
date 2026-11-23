---
kind: class
---

[Engine](../../../engine.md) → [Core](../../core.md) → [event.h](../event.md)

# detail::subscription_target
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/event.h` |
| Source File Path | `engine/src/core/time/event.cpp` |
| API ↗ | [Time, events and random](../../../../api/core/time-and-events.md) |

## Syntax

```cpp
class CONJURE_API subscription_target
```

## Derived Classes


<details><summary>Classes that use this as a base</summary>

<ul>

<li><a href="event-state.html">event::state</a></li>

</ul>

</details>

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">subscription_target();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Additional constructor overloads have been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">virtual</span> ~subscription_target();</code></td><td></td></tr>
</tbody>
</table>



## Functions

### Public

#### `is_subscribed` {#function-is-subscribed}

<div class="function-declaration" id="function-is-subscribed-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <span class="k">bool</span> is_subscribed(
    <a href="../types.html#symbol-u64">u64</a> id) <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

<p>True while the handler registered under the id is still registered.</p>

#### `unsubscribe` {#function-unsubscribe}

<div class="function-declaration" id="function-unsubscribe-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> unsubscribe(
    <a href="../types.html#symbol-u64">u64</a> id) <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

<p>Removes the handler registered under the id. Unknown ids are ignored.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->
