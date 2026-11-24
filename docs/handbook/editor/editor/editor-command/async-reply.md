---
kind: class
---

[Editor](../../../editor.md) → [Commands and Undo](../../commands-and-undo.md) → [editor_command.h](../editor-command.md)

# async_reply
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/editor_command.h` |
| Source File Path | `engine/src/editor/core/editor_command.cpp` |
| API ↗ | [Access tokens and the audit log](../../../../api/editor/access-and-audit.md), [The AI control protocol](../../../../api/editor/ai-protocol.md), [Headless mode and the command model](../../../../api/editor/headless-and-commands.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
class CONJURE_API async_reply
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">async_reply() <span class="k">noexcept</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">async_reply(
    <a href="../command-dispatcher/command-dispatcher.html">command_dispatcher</a> &amp;dispatcher,
    <a href="../../../engine/core/types.html#symbol-u64">u64</a> ticket) <span class="k">noexcept</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">async_reply(
    <span class="k">const</span> <a href="async-reply.html">async_reply</a> &amp;) = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~async_reply();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mDispatcher</code></td><td><code class="signature"><a href="../command-dispatcher/command-dispatcher.html">command_dispatcher</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mTicket</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `fail` {#function-fail}

<div class="function-declaration" id="function-fail-1"><pre><code><span class="k">void</span> fail(
    <a href="../editor-error.html#symbol-protocol-error-code">protocol_error_code</a> code,
    std::string message,
    <a href="../../../engine/core/json/document/json-document.html">core::json::document</a> details = core::json::document::object()) <span class="k">const</span>;</code></pre></div>

<p>Completes the parked request with an error.</p>

#### `is_valid` {#function-is-valid}

<div class="function-declaration" id="function-is-valid-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_valid() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True while the reply refers to a parked request that was not completed yet.</p>

#### `succeed` {#function-succeed}

<div class="function-declaration" id="function-succeed-1"><pre><code><span class="k">void</span> succeed(
    <a href="../../../engine/core/json/document/json-document.html">core::json::document</a> result) <span class="k">const</span>;</code></pre></div>

<p>Completes the parked request with a result object. For a command that is not <code>MUTATES</code>; a <code>MUTATES</code> command completes with <code><a href="#function-succeed-mutation-1">succeed_mutation</a>()</code> (a plain <code><a href="#function-succeed-1">succeed</a>()</code> of one records nothing, bumps no revision and leaves the dirty flag alone).</p>

#### `succeed_mutation` {#function-succeed-mutation}

<div class="function-declaration" id="function-succeed-mutation-1"><pre><code><span class="k">void</span> succeed_mutation(
    <a href="command-outcome.html">command_outcome</a> outcome) <span class="k">const</span>;</code></pre></div>

<p>Completes the parked request of an <code>ASYNC</code> | <code>MUTATES</code> command with what its change produced. The dispatcher does now what it does for a synchronous mutation at dispatch: records the inverse and the replay (the undo stack, the client&#x27;s open transaction, coalescing, a running batch, or the play controller while playing), marks the scene dirty unless <code>NO_UNDO</code>, bumps the revision, adds it to the result and emits <code>scene.changed</code>; an undo or redo step does neither of the last three (the history command does). The caller has already applied the change; on a reply that is no longer valid (timed out, cancelled) nothing is recorded, so a handler checks <code><a href="#function-is-valid-1">is_valid</a>()</code> before it changes anything.</p>

#### `ticket` {#function-ticket}

<div class="function-declaration" id="function-ticket-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-u64">u64</a> ticket() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `valid` {#function-valid}

<div class="function-declaration" id="function-valid-1"><pre><code>[[<span class="na">deprecated</span>(<span class="s">"use is_valid()"</span>)]] [[<span class="na">nodiscard</span>]] <span class="k">bool</span> valid() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The former name of <code><a href="#function-is-valid-1">is_valid</a>()</code>, kept for one wave; returns <code><a href="#function-is-valid-1">is_valid</a>()</code>.</p>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="async-reply.html">async_reply</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="async-reply.html">async_reply</a> &amp;) = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
