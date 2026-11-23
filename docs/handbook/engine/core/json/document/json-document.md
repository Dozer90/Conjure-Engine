---
kind: class
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [document.h](../document.md)

# json::document
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core::json` |
| Include File Path | `engine/include/conjure/core/json/document.h` |
| Source File Path | `engine/src/core/json/document.cpp` |
| API ↗ | [JSON](../../../../../api/core/json.md) |

## Syntax

```cpp
class CONJURE_API document
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">document();</code></td><td><code>A</code> document whose root is JSON <code>null</code>. The first <code>set()</code> turns the root into an object, the first <code>push_back()</code> into an array.</td></tr>
<tr><td>public</td><td><code class="signature">document(
    <span class="k">const</span> <a href="json-document.html">document</a> &amp;other);</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">document(
    <a href="json-document.html">document</a> &amp;&amp;other) <span class="k">noexcept</span>;</code></td><td>Moving transfers the tree without copying. The moved-from document is empty: its <code>root()</code> is invalid, it serialises to &quot;&quot;, every mutation is rejected (asserts or returns <code>false</code>) and it equals nothing; only assignment and destruction are meant for it. Copying it yields another empty document.</td></tr>
<tr><td>private</td><td><code class="signature"><span class="k">explicit</span> document(
    <span class="k">void</span> *handle) <span class="k">noexcept</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~document();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mHandle</code></td><td><code class="signature"><span class="k">void</span> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `array` {#function-array}

<div class="function-declaration" id="function-array-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="json-document.html">document</a> array();</code></pre></div>

<p><code>A</code> document whose root is an empty array.</p>

#### `at_pointer` {#function-at-pointer}

<div class="function-declaration" id="function-at-pointer-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../value/json-value.html">value</a> at_pointer(
    std::string_view pointer) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The value at a JSON Pointer such as &quot;/a/b/0&quot; (&quot;&quot; is the root), or an invalid value when the pointer is malformed or does not resolve.</p>

#### `clear` {#function-clear}

<div class="function-declaration" id="function-clear-1"><pre><code><span class="k">void</span> clear();</code></pre></div>

<p>Removes every member or element of a container root; any other root becomes <code>null</code>.</p>

#### `erase` {#function-erase}

<div class="function-declaration" id="function-erase-1"><pre><code><span class="k">bool</span> erase(
    std::string_view key);</code></pre></div>

<p>Removes every member named <code>key</code>. Returns <code>false</code> when there was none.</p>

<div class="function-declaration" id="function-erase-2"><pre><code><span class="k">bool</span> erase(
    <a href="../../types.html#symbol-usize">usize</a> index);</code></pre></div>

<p>Removes the element at <code>index</code>. Returns <code>false</code> when out of range.</p>

#### `from_value` {#function-from-value}

<div class="function-declaration" id="function-from-value-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="json-document.html">document</a> from_value(
    <span class="k">const</span> <a href="../value/json-value.html">value</a> &amp;source);</code></pre></div>

<p><code>A</code> document whose root is a deep copy of <code>source</code> (any node of any document). An invalid source yields a <code>null</code> root.</p>

#### `merge_patch` {#function-merge-patch}

<div class="function-declaration" id="function-merge-patch-1"><pre><code><span class="k">bool</span> merge_patch(
    <span class="k">const</span> <a href="../value/json-value.html">value</a> &amp;patch);</code></pre></div>

<p>Applies an RFC 7386 merge patch: object members are merged recursively, a <code>null</code> member removes the target member, anything else replaces the target. Returns <code>false</code> only when memory runs out; the document is then unchanged.</p>

<div class="function-declaration" id="function-merge-patch-2"><pre><code><span class="k">bool</span> merge_patch(
    <span class="k">const</span> <a href="json-document.html">document</a> &amp;patch);</code></pre></div>

#### `object` {#function-object}

<div class="function-declaration" id="function-object-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="json-document.html">document</a> object();</code></pre></div>

<p><code>A</code> document whose root is an empty object.</p>

#### `parse` {#function-parse}

<div class="function-declaration" id="function-parse-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../result/result.html">result</a>&lt;<a href="json-document.html">document</a>&gt; parse(
    std::string_view text);</code></pre></div>

<p>Parses strict RFC 8259 JSON (no comments, trailing commas, inf/nan or BOM). Fails with <code>PARSE_FAILURE</code>; the message carries the 1-based line and column, or names the limit when containers nest deeper than <code>MAX_NESTING_DEPTH</code>.</p>

#### `parse_file` {#function-parse-file}

<div class="function-declaration" id="function-parse-file-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../result/result.html">result</a>&lt;<a href="json-document.html">document</a>&gt; parse_file(
    <span class="k">const</span> std::filesystem::path &amp;path);</code></pre></div>

<p>Reads a file and parses it. Fails with <code>IO_FAILURE</code> when the path is not a readable regular file and <code>PARSE_FAILURE</code> when its content is not JSON. <code>A</code> leading UTF-8 <code><a href="../../types.html#symbol-byte">byte</a></code> order mark is skipped (as <code>file_system::read_text_file</code> does); <code><a href="#function-parse-1">parse</a>()</code> itself rejects one.</p>

#### `push_back` {#function-push-back}

<div class="function-declaration" id="function-push-back-1"><pre><code><span class="k">void</span> push_back(
    <span class="k">bool</span> item);</code></pre></div>

<p>Appends a scalar element. Non-negative integers are stored unsigned, negative ones signed; non-finite reals assert and store <code>null</code>; a <code>null</code> <code>C</code> string stores <code>null</code>.</p>

<div class="function-declaration" id="function-push-back-2"><pre><code><span class="k">void</span> push_back(
    <a href="../../types.html#symbol-i32">i32</a> item);</code></pre></div>

<div class="function-declaration" id="function-push-back-3"><pre><code><span class="k">void</span> push_back(
    <a href="../../types.html#symbol-u32">u32</a> item);</code></pre></div>

<div class="function-declaration" id="function-push-back-4"><pre><code><span class="k">void</span> push_back(
    <a href="../../types.html#symbol-i64">i64</a> item);</code></pre></div>

<div class="function-declaration" id="function-push-back-5"><pre><code><span class="k">void</span> push_back(
    <a href="../../types.html#symbol-u64">u64</a> item);</code></pre></div>

<div class="function-declaration" id="function-push-back-6"><pre><code><span class="k">void</span> push_back(
    <a href="../../types.html#symbol-f32">f32</a> item);</code></pre></div>

<div class="function-declaration" id="function-push-back-7"><pre><code><span class="k">void</span> push_back(
    <a href="../../types.html#symbol-f64">f64</a> item);</code></pre></div>

<div class="function-declaration" id="function-push-back-8"><pre><code><span class="k">void</span> push_back(
    std::string_view item);</code></pre></div>

<div class="function-declaration" id="function-push-back-9"><pre><code><span class="k">void</span> push_back(
    <span class="k">const</span> <span class="k">char</span> *item);</code></pre></div>

<div class="function-declaration" id="function-push-back-10"><pre><code><span class="k">void</span> push_back(
    std::nullptr_t);</code></pre></div>

<div class="function-declaration" id="function-push-back-11"><pre><code><span class="k">template</span> &lt;<a href="../document.html#symbol-json-detail-other-integer">detail::other_integer</a> T&gt; <span class="k">void</span> push_back(
    T item);</code></pre></div>

<div class="function-declaration" id="function-push-back-12"><pre><code><span class="k">void</span> push_back(
    <span class="k">const</span> <a href="json-document.html">document</a> &amp;item);</code></pre></div>

<p>Appends a deep copy of another document&#x27;s tree or of a value from any document.</p>

<div class="function-declaration" id="function-push-back-13"><pre><code><span class="k">void</span> push_back(
    <span class="k">const</span> <a href="../value/json-value.html">value</a> &amp;item);</code></pre></div>

#### `root` {#function-root}

<div class="function-declaration" id="function-root-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../value/json-value.html">value</a> root() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p><code>A</code> view of the root value. Valid for every document except a moved-from one.</p>

#### `set` {#function-set}

<div class="function-declaration" id="function-set-1"><pre><code><span class="k">void</span> set(
    std::string_view key,
    <span class="k">bool</span> item);</code></pre></div>

<p>Sets member <code>key</code> to a scalar. Non-negative integers are stored unsigned, negative ones signed; non-finite reals assert and store <code>null</code>; a <code>null</code> <code>C</code> string stores <code>null</code>.</p>

<div class="function-declaration" id="function-set-2"><pre><code><span class="k">void</span> set(
    std::string_view key,
    <a href="../../types.html#symbol-i32">i32</a> item);</code></pre></div>

<div class="function-declaration" id="function-set-3"><pre><code><span class="k">void</span> set(
    std::string_view key,
    <a href="../../types.html#symbol-u32">u32</a> item);</code></pre></div>

<div class="function-declaration" id="function-set-4"><pre><code><span class="k">void</span> set(
    std::string_view key,
    <a href="../../types.html#symbol-i64">i64</a> item);</code></pre></div>

<div class="function-declaration" id="function-set-5"><pre><code><span class="k">void</span> set(
    std::string_view key,
    <a href="../../types.html#symbol-u64">u64</a> item);</code></pre></div>

<div class="function-declaration" id="function-set-6"><pre><code><span class="k">void</span> set(
    std::string_view key,
    <a href="../../types.html#symbol-f32">f32</a> item);</code></pre></div>

<div class="function-declaration" id="function-set-7"><pre><code><span class="k">void</span> set(
    std::string_view key,
    <a href="../../types.html#symbol-f64">f64</a> item);</code></pre></div>

<div class="function-declaration" id="function-set-8"><pre><code><span class="k">void</span> set(
    std::string_view key,
    std::string_view item);</code></pre></div>

<div class="function-declaration" id="function-set-9"><pre><code><span class="k">void</span> set(
    std::string_view key,
    <span class="k">const</span> <span class="k">char</span> *item);</code></pre></div>

<div class="function-declaration" id="function-set-10"><pre><code><span class="k">void</span> set(
    std::string_view key,
    std::nullptr_t);</code></pre></div>

<div class="function-declaration" id="function-set-11"><pre><code><span class="k">template</span> &lt;<a href="../document.html#symbol-json-detail-other-integer">detail::other_integer</a> T&gt; <span class="k">void</span> set(
    std::string_view key,
    T item);</code></pre></div>

<div class="function-declaration" id="function-set-12"><pre><code><span class="k">void</span> set(
    std::string_view key,
    <span class="k">const</span> <a href="json-document.html">document</a> &amp;item);</code></pre></div>

<p>Deep-copies another document&#x27;s tree under <code>key</code>.</p>

<div class="function-declaration" id="function-set-13"><pre><code><span class="k">void</span> set(
    std::string_view key,
    <span class="k">const</span> <a href="../value/json-value.html">value</a> &amp;item);</code></pre></div>

<p>Deep-copies a value (from this or any other document) under <code>key</code>.</p>

#### `set_null` {#function-set-null}

<div class="function-declaration" id="function-set-null-1"><pre><code><span class="k">void</span> set_null(
    std::string_view key);</code></pre></div>

<p>Sets <code>key</code> to JSON <code>null</code> (the member stays; use <code><a href="#function-erase-1">erase</a>()</code> to remove it).</p>

#### `set_pointer` {#function-set-pointer}

<div class="function-declaration" id="function-set-pointer-1"><pre><code><span class="k">bool</span> set_pointer(
    std::string_view pointer,
    <span class="k">bool</span> item);</code></pre></div>

<p>Sets the value at a JSON Pointer, creating missing intermediate objects. In an array the last token replaces an existing index, while &quot;-&quot; or an index equal to the current size appends (RFC 6902 &quot;add&quot; semantics). Returns <code>false</code> for a malformed pointer or when an existing parent is a scalar or an array index is past the end.</p>

<div class="function-declaration" id="function-set-pointer-2"><pre><code><span class="k">bool</span> set_pointer(
    std::string_view pointer,
    <a href="../../types.html#symbol-i32">i32</a> item);</code></pre></div>

<div class="function-declaration" id="function-set-pointer-3"><pre><code><span class="k">bool</span> set_pointer(
    std::string_view pointer,
    <a href="../../types.html#symbol-u32">u32</a> item);</code></pre></div>

<div class="function-declaration" id="function-set-pointer-4"><pre><code><span class="k">bool</span> set_pointer(
    std::string_view pointer,
    <a href="../../types.html#symbol-i64">i64</a> item);</code></pre></div>

<div class="function-declaration" id="function-set-pointer-5"><pre><code><span class="k">bool</span> set_pointer(
    std::string_view pointer,
    <a href="../../types.html#symbol-u64">u64</a> item);</code></pre></div>

<div class="function-declaration" id="function-set-pointer-6"><pre><code><span class="k">bool</span> set_pointer(
    std::string_view pointer,
    <a href="../../types.html#symbol-f32">f32</a> item);</code></pre></div>

<div class="function-declaration" id="function-set-pointer-7"><pre><code><span class="k">bool</span> set_pointer(
    std::string_view pointer,
    <a href="../../types.html#symbol-f64">f64</a> item);</code></pre></div>

<div class="function-declaration" id="function-set-pointer-8"><pre><code><span class="k">bool</span> set_pointer(
    std::string_view pointer,
    std::string_view item);</code></pre></div>

<div class="function-declaration" id="function-set-pointer-9"><pre><code><span class="k">bool</span> set_pointer(
    std::string_view pointer,
    <span class="k">const</span> <span class="k">char</span> *item);</code></pre></div>

<div class="function-declaration" id="function-set-pointer-10"><pre><code><span class="k">bool</span> set_pointer(
    std::string_view pointer,
    std::nullptr_t);</code></pre></div>

<div class="function-declaration" id="function-set-pointer-11"><pre><code><span class="k">bool</span> set_pointer(
    std::string_view pointer,
    <span class="k">const</span> <a href="json-document.html">document</a> &amp;item);</code></pre></div>

<div class="function-declaration" id="function-set-pointer-12"><pre><code><span class="k">bool</span> set_pointer(
    std::string_view pointer,
    <span class="k">const</span> <a href="../value/json-value.html">value</a> &amp;item);</code></pre></div>

<div class="function-declaration" id="function-set-pointer-13"><pre><code><span class="k">template</span> &lt;<a href="../document.html#symbol-json-detail-other-integer">detail::other_integer</a> T&gt; <span class="k">bool</span> set_pointer(
    std::string_view pointer,
    T item);</code></pre></div>

#### `to_canonical_string` {#function-to-canonical-string}

<div class="function-declaration" id="function-to-canonical-string-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string to_canonical_string() <span class="k">const</span>;</code></pre></div>

<p>Canonical form for hashing and diffing: members sorted by key (<code><a href="../../types.html#symbol-byte">byte</a></code> order), minified, shortest round-trip reals, integers as plain digits. Identical on every platform for the same content, and identical to what <code>writer</code> produces when the same data is written with keys in sorted order. Returns an empty string (and logs a warning) only when a string holds invalid UTF-8.</p>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string to_string(
    <span class="k">bool</span> pretty = false) <span class="k">const</span>;</code></pre></div>

<p>Serialises in member order: minified, or pretty with 4-space indentation. Returns an empty string (and logs a warning) only when a string holds invalid UTF-8.</p>

#### `write_file` {#function-write-file}

<div class="function-declaration" id="function-write-file-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;<span class="k">void</span>&gt; write_file(
    <span class="k">const</span> std::filesystem::path &amp;path,
    <span class="k">bool</span> pretty = false) <span class="k">const</span>;</code></pre></div>

<p>Writes <code><a href="#function-to-string-1">to_string</a>(pretty)</code> to a file, creating parent directories. <code>IO_FAILURE</code> when the file cannot be written.</p>

### Private

#### `array` {#function-array}

<div class="function-declaration" id="function-array-2"><pre><code><span class="k">friend</span> <span class="na">CONJURE_API</span> <a href="json-document.html">document</a> array(
    std::initializer_list&lt;<a href="json-init-value.html">init_value</a>&gt; items);</code></pre></div>

#### `array_root` {#function-array-root}

<div class="function-declaration" id="function-array-root-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">void</span> *array_root();</code></pre></div>

#### `object` {#function-object}

<div class="function-declaration" id="function-object-2"><pre><code><span class="k">friend</span> <span class="na">CONJURE_API</span> <a href="json-document.html">document</a> object(
    std::initializer_list&lt;<a href="json-init-member.html">init_member</a>&gt; members);</code></pre></div>

#### `object_root` {#function-object-root}

<div class="function-declaration" id="function-object-root-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">void</span> *object_root();</code></pre></div>

#### `push_node` {#function-push-node}

<div class="function-declaration" id="function-push-node-1"><pre><code><span class="k">void</span> push_node(
    <span class="k">void</span> *node);</code></pre></div>

#### `root_node` {#function-root-node}

<div class="function-declaration" id="function-root-node-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">void</span> *root_node() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `set_node` {#function-set-node}

<div class="function-declaration" id="function-set-node-1"><pre><code><span class="k">void</span> set_node(
    std::string_view key,
    <span class="k">void</span> *node);</code></pre></div>

#### `set_pointer_node` {#function-set-pointer-node}

<div class="function-declaration" id="function-set-pointer-node-1"><pre><code><span class="k">bool</span> set_pointer_node(
    std::string_view pointer,
    <span class="k">void</span> *node);</code></pre></div>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="json-document.html">document</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="json-document.html">document</a> &amp;other);</code></pre></div>

<div class="function-declaration" id="operator-3d-2"><pre><code><a href="json-document.html">document</a> &amp;<span class="k">operator</span>=(
    <a href="json-document.html">document</a> &amp;&amp;other) <span class="k">noexcept</span>;</code></pre></div>

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="json-document.html">document</a> &amp;other) <span class="k">const</span>;</code></pre></div>

<p>Structural equality of the two roots (see <code>value::operator</code>==).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
