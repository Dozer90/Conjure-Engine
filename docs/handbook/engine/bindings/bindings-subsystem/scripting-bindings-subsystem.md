---
kind: class
---

[Engine](../../../engine.md) → [Bindings](../../bindings.md) → [bindings_subsystem.h](../bindings-subsystem.md)

# scripting_bindings_subsystem
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::bindings` |
| Include File Path | `engine/include/conjure/bindings/bindings_subsystem.h` |
| Source File Path | `engine/src/bindings/bindings_subsystem.cpp` |
| API ↗ | [Scripting from C#: the bindings](../../../../api/bindings/index.md), [The native table](../../../../api/bindings/native-table.md) |

## Syntax

```cpp
class CONJURE_API scripting_bindings_subsystem final : public engine::engine_subsystem
```

## Inheritance Hierarchy


`engine::engine_subsystem` → `scripting_bindings_subsystem`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">scripting_bindings_subsystem();</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~scripting_bindings_subsystem() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mContext</code></td><td><code class="signature"><a href="../native-api-entries/binding-context.html">binding_context</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFilled</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mResidency</code></td><td><code class="signature"><a href="script-residency.html">script_residency</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Unload<wbr>Subscription</code></td><td><code class="signature"><a href="../../core/event/event-subscription.html">core::event_subscription</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `active_residency` {#function-active-residency}

<div class="function-declaration" id="function-active-residency-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="script-residency.html">script_residency</a> *active_residency() <span class="k">noexcept</span>;</code></pre></div>

<p>The residency the active entries use (<code>asset_load</code> / <code>asset_release</code>), or <code>nullptr</code> when no subsystem started. Tests without an application install one with <code>set_active_residency</code>.</p>

#### `begin_frame` {#function-begin-frame}

<div class="function-declaration" id="function-begin-frame-1"><pre><code><span class="k">void</span> begin_frame(
    engine::engine_context &amp;context) <span class="k">override</span>;</code></pre></div>

<p>Re-installs the counting thunks and the <code>query_end</code> hook when a re-initialised script runtime dropped them (<code>is_scope_hook_installed</code>), clears the last-error buffer and the per-frame rate-limit bits of the entry failure logs, and applies the scene fixups a force-closed scope left behind.</p>

#### `context` {#function-context}

<div class="function-declaration" id="function-context-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../native-api-entries/binding-context.html">binding_context</a> &amp;context() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The context the table was filled from (every pointer <code>null</code> before startup).</p>

#### `is_filled` {#function-is-filled}

<div class="function-declaration" id="function-is-filled-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_filled() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True once startup filled the table (<code>false</code> when scripting was disabled).</p>

#### `name` {#function-name}

<div class="function-declaration" id="function-name-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string_view name() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

<p>&quot;bindings&quot;.</p>

#### `post_startup` {#function-post-startup}

<div class="function-declaration" id="function-post-startup-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; post_startup(
    engine::engine_context &amp;context) <span class="k">override</span>;</code></pre></div>

<p>Step 17, after scripting&#x27;s <code>post_startup</code> filled the core entries: wraps them in counting thunks too (idempotent for the slots already wrapped at step 16).</p>

#### `residency` {#function-residency}

<div class="function-declaration" id="function-residency-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="script-residency.html">script_residency</a> &amp;residency() <span class="k">noexcept</span>;</code></pre></div>

<p>The handles scripts hold through <code>asset_load</code>.</p>

#### `set_active_residency` {#function-set-active-residency}

<div class="function-declaration" id="function-set-active-residency-1"><pre><code><span class="k">static</span> <span class="k">void</span> set_active_residency(
    <a href="script-residency.html">script_residency</a> *residency) <span class="k">noexcept</span>;</code></pre></div>

<p>Installs (or, with <code>nullptr</code>, removes) the residency the entries use.</p>

#### `shutdown` {#function-shutdown}

<div class="function-declaration" id="function-shutdown-1"><pre><code><span class="k">void</span> shutdown(
    engine::engine_context &amp;context) <span class="k">override</span>;</code></pre></div>

<p>Drops the residency and the subscription and forgets the active context. Safe after a failed or never attempted startup and when called twice.</p>

#### `startup` {#function-startup}

<div class="function-declaration" id="function-startup-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; startup(
    engine::engine_context &amp;context) <span class="k">override</span>;</code></pre></div>

<p>Step 16 (3.1): nothing to do without a script runtime; otherwise builds the <code><a href="../native-api-entries/binding-context.html">binding_context</a></code> from the <code><a href="../../engine/engine-context/engine-context.html">engine_context</a></code>, fills the owned slots, wraps every slot with counting thunks outside Distribution, verifies that no slot is <code>null</code> (<code>TABLE_INCOMPLETE</code> unwinds the startup) and subscribes to <code>scene_manager::on_before_unload</code> to clear the residency.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
