---
kind: class
---

[Engine](../../../../engine.md) → [Engine](../../../engine.md) → [check_registry.h](../check-registry.md)

# diagnostics::check_registry
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::engine::diagnostics` |
| Include File Path | `engine/include/conjure/engine/diagnostics/check_registry.h` |
| Source File Path | `engine/src/engine/check_registry.cpp` |
| API ↗ | [Export and diagnostics](../../../../../api/engine/export-and-diagnostics.md), [Engine: the application and the frame](../../../../../api/engine/index.md) |

## Syntax

```cpp
class CONJURE_API check_registry
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">check_registry();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Additional constructor overloads have been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~check_registry();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mFailedCount</code></td><td><code class="signature"><a href="../../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mMutex</code></td><td><code class="signature"><span class="k">mutable</span> std::mutex</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRecords</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="diagnostics-check-record.html">check_<wbr>record</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `all_passed` {#function-all-passed}

<div class="function-declaration" id="function-all-passed-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> all_passed() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when no record failed (also for an empty registry).</p>

#### `clear` {#function-clear}

<div class="function-declaration" id="function-clear-1"><pre><code><span class="k">void</span> clear();</code></pre></div>

<p>Forgets every record.</p>

#### `failed_count` {#function-failed-count}

<div class="function-declaration" id="function-failed-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../core/types.html#symbol-u64">u64</a> failed_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Records with passed == <code>false</code>.</p>

#### `record_count` {#function-record-count}

<div class="function-declaration" id="function-record-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../core/types.html#symbol-usize">usize</a> record_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Records reported so far.</p>

#### `records` {#function-records}

<div class="function-declaration" id="function-records-1"><pre><code>[[<span class="na">nodiscard</span>]] std::vector&lt;<a href="diagnostics-check-record.html">check_record</a>&gt; records() <span class="k">const</span>;</code></pre></div>

<p><code>A</code> copy of every record in report order.</p>

#### `report` {#function-report}

<div class="function-declaration" id="function-report-1"><pre><code><span class="k">void</span> report(
    std::string_view name,
    <span class="k">bool</span> passed,
    std::string_view detail,
    <a href="../../../core/types.html#symbol-u64">u64</a> frame);</code></pre></div>

<p>Appends a record. Thread-safe.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
