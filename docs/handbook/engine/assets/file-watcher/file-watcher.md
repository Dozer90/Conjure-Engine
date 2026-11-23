---
kind: class
---

[Engine](../../../engine.md) → [Assets](../../assets.md) → [file_watcher.h](../file-watcher.md)

# file_watcher
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::assets` |
| Include File Path | `engine/include/conjure/assets/file_watcher.h` |
| Source File Path | `engine/src/assets/file_watcher.cpp` |
| API ↗ | [Database, importers and hot reload](../../../../api/assets/database-and-import.md) |

## Syntax

```cpp
class CONJURE_API file_watcher
```

## Derived Classes


<details><summary>Classes that use this as a base</summary>

<ul>

<li><a href="polling-file-watcher.html">polling_file_watcher</a></li>

</ul>

</details>

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">file_watcher();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">virtual</span> ~file_watcher();</code></td><td></td></tr>
</tbody>
</table>



## Functions

### Public

#### `add_root` {#function-add-root}

<div class="function-declaration" id="function-add-root-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> add_root(
    std::string_view rootPath) = <span class="mi">0</span>;</code></pre></div>

<p>Main thread. Watches another directory tree; may be called while watching.</p>

#### `poll` {#function-poll}

<div class="function-declaration" id="function-poll-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> poll(
    std::vector&lt;<a href="file-change.html">file_change</a>&gt; &amp;outChanges) = <span class="mi">0</span>;</code></pre></div>

<p>Main thread. Moves the debounced changes since the last call into <code>outChanges</code>.</p>

#### `snapshot` {#function-snapshot}

<div class="function-declaration" id="function-snapshot-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; snapshot() = <span class="mi">0</span>;</code></pre></div>

<p>Records the current state as the baseline; emits nothing.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->
