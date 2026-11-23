---
kind: class
---

[Editor](../../../editor.md) → [Commands and Undo](../../commands-and-undo.md) → [undo_stack.h](../undo-stack.md)

# undo_stack
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/undo_stack.h` |
| Source File Path | `engine/src/editor/core/undo_stack.cpp` |
| API ↗ | [Headless mode and the command model](../../../../api/editor/headless-and-commands.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
class CONJURE_API undo_stack
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">undo_stack();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~undo_stack();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">DEFAULT_<wbr>BYTE_<wbr>BUDGET</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../../engine/core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">256</span> * MIB</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">DEFAULT_<wbr>MAX_<wbr>DEPTH</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../../engine/core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">1024</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mByteBudget</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-usize">usize</a></code></td><td><code class="signature">DEFAULT_<wbr>BYTE_<wbr>BUDGET</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mBytes</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mCursor</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mEntries</code></td><td><code class="signature">std::deque&lt;<a href="undo-entry.html">undo_entry</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFrozen</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mMaxDepth</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-usize">usize</a></code></td><td><code class="signature">DEFAULT_<wbr>MAX_<wbr>DEPTH</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Transactions</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr>std::<wbr>pair&lt;<wbr>u32,<wbr> <a href="undo-entry.html">undo_<wbr>entry</a>&gt;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `begin_transaction` {#function-begin-transaction}

<div class="function-declaration" id="function-begin-transaction-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; begin_transaction(
    std::string_view label,
    <a href="../../../engine/core/types.html#symbol-u32">u32</a> clientId);</code></pre></div>

<p>Opens a transaction for the client. <code>INVALID_REQUEST</code> when one is already open.</p>

#### `byte_budget` {#function-byte-budget}

<div class="function-declaration" id="function-byte-budget-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-usize">usize</a> byte_budget() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `byte_size` {#function-byte-size}

<div class="function-declaration" id="function-byte-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-usize">usize</a> byte_size() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Serialised bytes currently held.</p>

#### `can_redo` {#function-can-redo}

<div class="function-declaration" id="function-can-redo-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> can_redo() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `can_undo` {#function-can-undo}

<div class="function-declaration" id="function-can-undo-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> can_undo() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `clear` {#function-clear}

<div class="function-declaration" id="function-clear-1"><pre><code><span class="k">void</span> clear();</code></pre></div>

<p>Forgets every entry and every open transaction.</p>

#### `cursor` {#function-cursor}

<div class="function-declaration" id="function-cursor-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-usize">usize</a> cursor() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `discard_at_cursor` {#function-discard-at-cursor}

<div class="function-declaration" id="function-discard-at-cursor-1"><pre><code><span class="k">bool</span> discard_at_cursor(
    <span class="k">bool</span> afterUndo);</code></pre></div>

<p>Removes the entry a failed undo or redo just popped (the one at the cursor for a failed undo, the one below it for a failed redo). False when there is no such entry.</p>

#### `end_transaction` {#function-end-transaction}

<div class="function-declaration" id="function-end-transaction-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; end_transaction(
    <a href="../../../engine/core/types.html#symbol-u32">u32</a> clientId);</code></pre></div>

<p>Closes the client&#x27;s transaction and pushes it when it recorded anything. <code>INVALID_REQUEST</code> when none is open.</p>

#### `entries` {#function-entries}

<div class="function-declaration" id="function-entries-1"><pre><code>[[<span class="na">nodiscard</span>]] std::vector&lt;<span class="k">const</span> <a href="undo-entry.html">undo_entry</a> *&gt; entries() <span class="k">const</span>;</code></pre></div>

<p>Every entry, oldest first (entries below <code><a href="#function-cursor-1">cursor</a>()</code> are undoable).</p>

#### `frozen` {#function-frozen}

<div class="function-declaration" id="function-frozen-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> frozen() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `has_open_transaction` {#function-has-open-transaction}

<div class="function-declaration" id="function-has-open-transaction-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> has_open_transaction() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True while any client has an open transaction (<code>history.undo</code>, redo and clear are refused then: ending the transaction would push past the cursor and drop the redo tail).</p>

#### `has_transaction` {#function-has-transaction}

<div class="function-declaration" id="function-has-transaction-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> has_transaction(
    <a href="../../../engine/core/types.html#symbol-u32">u32</a> clientId) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True while the client has an open transaction.</p>

#### `max_depth` {#function-max-depth}

<div class="function-declaration" id="function-max-depth-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-usize">usize</a> max_depth() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `measure` {#function-measure}

<div class="function-declaration" id="function-measure-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../../engine/core/types.html#symbol-usize">usize</a> measure(
    <span class="k">const</span> <a href="undo-entry.html">undo_entry</a> &amp;entry);</code></pre></div>

<p>The serialised <code><a href="../../../engine/core/types.html#symbol-byte">byte</a></code> size an entry accounts for.</p>

#### `pop_for_redo` {#function-pop-for-redo}

<div class="function-declaration" id="function-pop-for-redo-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="undo-entry.html">undo_entry</a>&gt; pop_for_redo();</code></pre></div>

<p>The entry to redo next (the cursor moves up); nullopt when nothing can be redone.</p>

#### `pop_for_undo` {#function-pop-for-undo}

<div class="function-declaration" id="function-pop-for-undo-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="undo-entry.html">undo_entry</a>&gt; pop_for_undo();</code></pre></div>

<p>The entry to undo next (the cursor moves down); nullopt when nothing can be undone.</p>

#### `push` {#function-push}

<div class="function-declaration" id="function-push-1"><pre><code><span class="k">void</span> push(
    <a href="undo-entry.html">undo_entry</a> entry);</code></pre></div>

<p>Pushes a complete entry (a batch). Empty entries are ignored.</p>

#### `record` {#function-record}

<div class="function-declaration" id="function-record-1"><pre><code><span class="k">void</span> record(
    <a href="../editor-command/command-request.html">command_request</a> replay,
    <a href="../editor-command/command-inverse.html">command_inverse</a> inverse,
    std::string_view label,
    <a href="../../../engine/core/types.html#symbol-u32">u32</a> clientId,
    <a href="../../../engine/core/types.html#symbol-u64">u64</a> coalesceKey);</code></pre></div>

<p>Records one mutating command: appended to the client&#x27;s open transaction when there is one (coalesced with the transaction&#x27;s last record when the key and command match), coalesced into the top entry when the keys match (same non-zero key, same single command name: the new replay replaces the old, the first inverse is kept), otherwise pushed as a new entry after discarding the redo tail. Nothing is recorded while frozen.</p>

#### `set_byte_budget` {#function-set-byte-budget}

<div class="function-declaration" id="function-set-byte-budget-1"><pre><code><span class="k">void</span> set_byte_budget(
    <a href="../../../engine/core/types.html#symbol-usize">usize</a> bytes) <span class="k">noexcept</span>;</code></pre></div>

<p>Changes the budget and evicts the oldest entries until it holds.</p>

#### `set_frozen` {#function-set-frozen}

<div class="function-declaration" id="function-set-frozen-1"><pre><code><span class="k">void</span> set_frozen(
    <span class="k">bool</span> frozen) <span class="k">noexcept</span>;</code></pre></div>

#### `set_max_depth` {#function-set-max-depth}

<div class="function-declaration" id="function-set-max-depth-1"><pre><code><span class="k">void</span> set_max_depth(
    <a href="../../../engine/core/types.html#symbol-usize">usize</a> depth) <span class="k">noexcept</span>;</code></pre></div>

<p>Changes the depth cap and evicts the oldest entries until it holds (at least 1).</p>

#### `size` {#function-size}

<div class="function-declaration" id="function-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-usize">usize</a> size() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

### Private

#### `evict` {#function-evict}

<div class="function-declaration" id="function-evict-1"><pre><code><span class="k">void</span> evict();</code></pre></div>

#### `find_transaction` {#function-find-transaction}

<div class="function-declaration" id="function-find-transaction-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="undo-entry.html">undo_entry</a> *find_transaction(
    <a href="../../../engine/core/types.html#symbol-u32">u32</a> clientId) <span class="k">noexcept</span>;</code></pre></div>

#### `push_entry` {#function-push-entry}

<div class="function-declaration" id="function-push-entry-1"><pre><code><span class="k">void</span> push_entry(
    <a href="undo-entry.html">undo_entry</a> entry);</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
