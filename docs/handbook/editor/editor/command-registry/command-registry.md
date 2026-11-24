---
kind: class
---

[Editor](../../../editor.md) → [Commands and Undo](../../commands-and-undo.md) → [command_registry.h](../command-registry.md)

# command_registry
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/command_registry.h` |
| Source File Path | `engine/src/editor/core/command_registry.cpp` |
| API ↗ | [Headless mode and the command model](../../../../api/editor/headless-and-commands.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
class CONJURE_API command_registry
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">command_registry();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~command_registry();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mEntries</code></td><td><code class="signature">std::vector&lt;<a href="command-registry-entry.html">entry</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSchemaHash</code></td><td><code class="signature"><span class="k">mutable</span> <a href="../../../engine/core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Schema<wbr>Hash<wbr>Stale</code></td><td><code class="signature"><span class="k">mutable</span> <span class="k">bool</span></code></td><td><code class="signature">true</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `add` {#function-add}

<div class="function-declaration" id="function-add-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; add(
    <a href="../editor-command/command-descriptor.html">command_descriptor</a> descriptor);</code></pre></div>

<p>Registers a command. <code>INVALID_ARGS</code> when the name is empty or taken, the handler is empty, a schema does not compile, or the flags contradict each other: <code>ON_DISK</code> needs <code>MUTATES</code> and excludes <code>NO_UNDO</code> (the flag promises an inverse to keep while playing), <code>READ_ONLY</code> excludes <code>MUTATES</code> (the flag opens the command to inspect-only clients), and <code>SYNC_IN_BATCH</code> needs <code>ASYNC</code> and excludes <code>NOT_IN_BATCH</code> (the flag promises an answer inside a batch). An <code>ASYNC</code> command without <code>SYNC_IN_BATCH</code> is registered with <code>NOT_IN_BATCH</code> added.</p>

#### `args_schema` {#function-args-schema}

<div class="function-declaration" id="function-args-schema-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../schema-validator/compiled-schema.html">compiled_schema</a> *args_schema(
    std::string_view name) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The compiled argument schema of a command, or <code>nullptr</code>.</p>

#### `closest_names` {#function-closest-names}

<div class="function-declaration" id="function-closest-names-1"><pre><code>[[<span class="na">nodiscard</span>]] std::vector&lt;std::string&gt; closest_names(
    std::string_view name,
    <a href="../../../engine/core/types.html#symbol-usize">usize</a> count) <span class="k">const</span>;</code></pre></div>

<p>The names closest to <code>name</code> by Levenshtein distance (ties by name), at most <code>count</code>. Empty for a name longer than 128 bytes: no command is spelt like that, and the distance would cost its length times every command&#x27;s.</p>

#### `describe` {#function-describe}

<div class="function-declaration" id="function-describe-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/json/document/json-document.html">core::json::document</a> describe(
    <span class="k">const</span> <a href="../editor-command/command-descriptor.html">command_descriptor</a> &amp;descriptor) <span class="k">const</span>;</code></pre></div>

<p>One command as <code>editor.schema</code> describes it: {name, summary, flags:[...], owner, args, result}.</p>

#### `describe_all` {#function-describe-all}

<div class="function-declaration" id="function-describe-all-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/json/document/json-document.html">core::json::document</a> describe_all(
    <span class="k">bool</span> includeUiOnly) <span class="k">const</span>;</code></pre></div>

<p>The whole catalogue: {protocolVersion, schemaHash, commands:[...], events:[...], errorCodes:[...]}. <code>UI_ONLY</code> commands are included only when asked.</p>

#### `descriptors` {#function-descriptors}

<div class="function-declaration" id="function-descriptors-1"><pre><code>[[<span class="na">nodiscard</span>]] std::vector&lt;<span class="k">const</span> <a href="../editor-command/command-descriptor.html">command_descriptor</a> *&gt; descriptors() <span class="k">const</span>;</code></pre></div>

<p>Every descriptor in name order (the span is invalidated by add/remove).</p>

#### `find` {#function-find}

<div class="function-declaration" id="function-find-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../editor-command/command-descriptor.html">command_descriptor</a> *find(
    std::string_view name) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The descriptor, or <code>nullptr</code>.</p>

#### `names` {#function-names}

<div class="function-declaration" id="function-names-1"><pre><code>[[<span class="na">nodiscard</span>]] std::vector&lt;std::string&gt; names() <span class="k">const</span>;</code></pre></div>

<p>Every name in order.</p>

#### `remove` {#function-remove}

<div class="function-declaration" id="function-remove-1"><pre><code><span class="k">bool</span> remove(
    std::string_view name);</code></pre></div>

<p>Removes one command. False when unknown.</p>

#### `remove_owned_by` {#function-remove-owned-by}

<div class="function-declaration" id="function-remove-owned-by-1"><pre><code><a href="../../../engine/core/types.html#symbol-usize">usize</a> remove_owned_by(
    std::string_view owner);</code></pre></div>

<p>Removes every command whose owner matches and returns how many.</p>

#### `schema_hash` {#function-schema-hash}

<div class="function-declaration" id="function-schema-hash-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-u64">u64</a> schema_hash() <span class="k">const</span>;</code></pre></div>

<p>FNV-1a 64 of the canonical catalogue text (<code>UI_ONLY</code> excluded): identical for identical catalogues, different when any name, schema or flag changes. Computed on the first call after a change, so registering the whole catalogue command by command stays linear.</p>

#### `size` {#function-size}

<div class="function-declaration" id="function-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-usize">usize</a> size() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

### Private

#### `find_entry` {#function-find-entry}

<div class="function-declaration" id="function-find-entry-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="command-registry-entry.html">entry</a> *find_entry(
    std::string_view name) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `recompute_hash` {#function-recompute-hash}

<div class="function-declaration" id="function-recompute-hash-1"><pre><code><span class="k">void</span> recompute_hash() <span class="k">const</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
