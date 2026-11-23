---
kind: header
---

[Editor](../../editor.md) → [Commands and Undo](../commands-and-undo.md)

# editor_command.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/editor_command.h` |
| API ↗ | [Access tokens and the audit log](../../../api/editor/access-and-audit.md), [The AI control protocol](../../../api/editor/ai-protocol.md), [Headless mode and the command model](../../../api/editor/headless-and-commands.md), [The editor](../../../api/editor/index.md) |

<p>Commands are data. Every mutation of the editor, whether it comes from a panel, a protocol client, an --exec file or the undo stack, is a <code><a href="editor-command/command-request.html">command_request</a></code> handed to the <code><a href="command-dispatcher/command-dispatcher.html">command_dispatcher</a></code>. <code>A</code> mutating handler returns the inverse request that undoes it and, when the forward request is not deterministic (assigned guids), a normalised replay request for redo.</p>

## Structs

| File | Description |
|---|---|
| [`command_descriptor`](editor-command/command-descriptor.md) |  |
| [`command_inverse`](editor-command/command-inverse.md) |  |
| [`command_invocation`](editor-command/command-invocation.md) | What a handler receives. <code>A</code> handler that fails with a detailed error (candidates, failedIndex) fills <code>errorDetails</code> before returning the error; the dispatcher copies it into the response. |
| [`command_outcome`](editor-command/command-outcome.md) |  |
| [`command_request`](editor-command/command-request.md) |  |

## Classes

| File | Description |
|---|---|
| [`async_reply`](editor-command/async-reply.md) | The handle an <code>ASYNC</code> handler keeps to answer later. Copyable (a job completion captures it by value); the completion itself runs on the main thread. <code>A</code> second completion is a programmer error (asserted, ignored). |

## Type Aliases

### `command_handler` {#symbol-command-handler}

```cpp
using command_handler = std::function<core::result<command_outcome>(command_invocation &invocation)>;
```

## Forward Declarations

### `command_dispatcher` {#symbol-command-dispatcher}

```cpp
class command_dispatcher;
```

### `editor_context` {#symbol-editor-context}

```cpp
class editor_context;
```

## Functions

### Public

#### `command_flag_name` {#function-command-flag-name}

<div class="function-declaration" id="function-command-flag-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view command_flag_name(
    <a href="command-desc.html#symbol-command-flags">command_flags</a> flag) <span class="k">noexcept</span>;</code></pre></div>

<p>The wire spelling of one flag (&quot;mutates&quot;); &quot;&quot; for a value that is not a single flag.</p>

#### `command_flag_names` {#function-command-flag-names}

<div class="function-declaration" id="function-command-flag-names-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../../engine/core/json/document/json-document.html">core::json::document</a> command_flag_names(
    <a href="command-desc.html#symbol-command-flags">command_flags</a> flags);</code></pre></div>

<p>The lower-case names of the set flags (&quot;mutates&quot;, &quot;<code>requires_scene</code>&quot;, ...), in enumeration order.</p>

#### `has_command_flag` {#function-has-command-flag}

<div class="function-declaration" id="function-has-command-flag-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">inline</span> <span class="k">bool</span> has_command_flag(
    <span class="k">const</span> <a href="editor-command/command-descriptor.html">command_descriptor</a> &amp;descriptor,
    <a href="command-desc.html#symbol-command-flags">command_flags</a> flag) <span class="k">noexcept</span>;</code></pre></div>

<p>True when the descriptor carries the flag.</p>

#### `required_access` {#function-required-access}

<div class="function-declaration" id="function-required-access-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="protocol/access-token.html#symbol-access-level">access_level</a> required_access(
    <a href="command-desc.html#symbol-command-flags">command_flags</a> flags) <span class="k">noexcept</span>;</code></pre></div>

<p>What a client must have been granted to run a command with these flags (decision of): <code>INSPECT</code> for a <code>READ_ONLY</code> command, <code>FULL_CONTROL</code> for every other one. The flags are the whole rule: no command is decided by its name, so a command added later needs full control until it is flagged.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="async_reply"></a>
<a id="async_replyasync_reply"></a>
<a id="async_replyasync_reply_1"></a>
<a id="async_replyfail"></a>
<a id="async_replymdispatcher"></a>
<a id="async_replymticket"></a>
<a id="async_replyoperator"></a>
<a id="async_replysucceed"></a>
<a id="async_replyticket"></a>
<a id="async_replyvalid"></a>
<a id="command_descriptor"></a>
<a id="command_descriptorargsschema"></a>
<a id="command_descriptorflags"></a>
<a id="command_descriptorhandler"></a>
<a id="command_descriptorname"></a>
<a id="command_descriptorowner"></a>
<a id="command_descriptorresultschema"></a>
<a id="command_descriptorsummary"></a>
<a id="command_dispatcher"></a>
<a id="command_flag_name"></a>
<a id="command_flag_names"></a>
<a id="command_handler"></a>
<a id="command_inverse"></a>
<a id="command_inverseargs"></a>
<a id="command_inversename"></a>
<a id="command_invocation"></a>
<a id="command_invocationargs"></a>
<a id="command_invocationclientid"></a>
<a id="command_invocationcontext"></a>
<a id="command_invocationerrordetails"></a>
<a id="command_invocationreply"></a>
<a id="command_outcome"></a>
<a id="command_outcomeinverse"></a>
<a id="command_outcomelabel"></a>
<a id="command_outcomereplay"></a>
<a id="command_outcomeresult"></a>
<a id="command_outcomeskiprecording"></a>
<a id="command_request"></a>
<a id="command_requestargs"></a>
<a id="command_requestclientid"></a>
<a id="command_requestcoalescekey"></a>
<a id="command_requestname"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="editor_context"></a>
<a id="extension-points"></a>
<a id="has_command_flag"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="required_access"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
