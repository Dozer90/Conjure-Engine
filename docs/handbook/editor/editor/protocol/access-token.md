---
kind: header
---

[Editor](../../../editor.md) → [Protocol](../../protocol.md)

# access_token.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/protocol/access_token.h` |
| API ↗ | [Access tokens and the audit log](../../../../api/editor/access-and-audit.md), [The AI control protocol](../../../../api/editor/ai-protocol.md) |

<p>The secrets of the AI control protocol: what a token grants, how one is generated, where it is written for clients, and the comparisons that do not leak it: every place that holds a token against text a client sent uses one of the <code>constant_time_</code> functions below. The protocol server owns the tokens; nothing here logs or stores one.</p>

## Enumerations

### `access_level` {#symbol-access-level}

```cpp
enum class access_level : u8 {
    NONE = 0,           // nothing: no token the server knows was presented
    INSPECT = 1,        // the commands flagged READ_ONLY: looking at the editor, changing nothing
    FULL_CONTROL = 2    // every command
};
```

What a connected client may do (decision of). The levels are ordered: a higher one includes everything below it, so callers compare with &lt; and &gt;=. Which level a command asks for is <code>required_access()</code> in conjure/editor/<code>editor_command.h</code>.

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-ACCESS_TOKEN_BYTES"></a><code>ACCESS_TOKEN_BYTES</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../../../engine/core/types.html#symbol-usize">usize</a></code> | <code class="signature"><span class="mi">32</span></code> | Random bytes in a generated token. |
| <a id="variable-ACCESS_TOKEN_LENGTH"></a><code>ACCESS_TOKEN_LENGTH</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../../../engine/core/types.html#symbol-usize">usize</a></code> | <code class="signature">ACCESS_TOKEN_BYTES * <span class="mi">2</span></code> | Characters of a generated token: two lower-case hexadecimal digits per <code><a href="../../../engine/core/types.html#symbol-byte">byte</a></code>. |

## Functions

### Public

#### `constant_time_contains` {#function-constant-time-contains}

<div class="function-declaration" id="function-constant-time-contains-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">bool</span> constant_time_contains(
    std::string_view text,
    std::string_view token) <span class="k">noexcept</span>;</code></pre></div>

<p>True when <code>text</code> contains <code>token</code>. Unlike a substring search, which stops at the first <code><a href="../../../engine/core/types.html#symbol-byte">byte</a></code> that differs and so takes longer the more of the token a text holds, every position of the text is compared over the token&#x27;s whole length (<code>constant_time_starts_at</code>) and none ends the loop early: the time depends on the two lengths alone. That costs <code>text.size()</code> * <code>token.size()</code> steps, so a caller bounds the text it hands over. An empty token is contained in nothing.</p>

#### `constant_time_equals` {#function-constant-time-equals}

<div class="function-declaration" id="function-constant-time-equals-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">bool</span> constant_time_equals(
    std::string_view presented,
    std::string_view expected) <span class="k">noexcept</span>;</code></pre></div>

<p>True when the two texts are equal. Every <code><a href="../../../engine/core/types.html#symbol-byte">byte</a></code> of <code>presented</code> is compared whatever the outcome so far, so the time taken does not tell a client how long a prefix of its guess was right. The length of <code>expected</code> is not hidden (a generated token always has <code>ACCESS_TOKEN_LENGTH</code> characters). Two empty texts are equal.</p>

#### `constant_time_starts_at` {#function-constant-time-starts-at}

<div class="function-declaration" id="function-constant-time-starts-at-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">bool</span> constant_time_starts_at(
    std::string_view text,
    <a href="../../../engine/core/types.html#symbol-usize">usize</a> offset,
    std::string_view token) <span class="k">noexcept</span>;</code></pre></div>

<p>True when <code>token</code> starts at <code><a href="../../../engine/core/types.html#symbol-byte">byte</a></code> <code>offset</code> of <code>text</code>. The comparison is <code>constant_time_equals</code> over the token&#x27;s length, so the time taken tells nothing about how much of the token the text has there. False for an empty token and where the text is too short to hold it.</p>

#### `generate_access_token` {#function-generate-access-token}

<div class="function-declaration" id="function-generate-access-token-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string generate_access_token();</code></pre></div>

<p><code>A</code> new token: <code>ACCESS_TOKEN_BYTES</code> bytes from <code>std::random_device(&quot;/dev/urandom&quot;)</code>, written as <code>ACCESS_TOKEN_LENGTH</code> lower-case hexadecimal digits. Where the bytes come from depends on the standard library, and the device is named so that each of the three supported ones reads the operating system&#x27;s generator: libstdc++ reads the device /dev/urandom (asked for nothing, it would take the processor&#x27;s RDSEED or RDRAND instruction on x86 before the kernel); libc++ reads that device, or calls getentropy or arc4random where it was built for those, and accepts this name in all three builds; the Microsoft library ignores the name and calls <code>rand_s</code>, which is the system generator. The <code>C</code>++ standard promises none of this, so a port to another library must check it. <code>A</code> host whose generator cannot be opened or read makes <code>std::random_device</code> throw, which engine code never catches: the process ends. Thread-safe.</p>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="access-token.html#symbol-access-level">access_level</a> level) <span class="k">noexcept</span>;</code></pre></div>

<p>The wire spelling of a level, as <code>session.hello</code> and the audit log write it: &quot;none&quot;, &quot;inspect&quot;, &quot;<code>full_control</code>&quot;.</p>

#### `token_file_name` {#function-token-file-name}

<div class="function-declaration" id="function-token-file-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view token_file_name(
    <a href="access-token.html#symbol-access-level">access_level</a> level) <span class="k">noexcept</span>;</code></pre></div>

<p>The name of the file under &lt;project&gt;/.conjure that holds the token of a level, next to <code>ai_port</code>: &quot;<code>ai_token</code>&quot; for <code>FULL_CONTROL</code>, &quot;<code>ai_token_inspect</code>&quot; for <code>INSPECT</code>. <code>NONE</code> has no token and no file (&quot;&quot;).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="access_level"></a>
<a id="access_levelfull_control"></a>
<a id="access_levelinspect"></a>
<a id="access_levelnone"></a>
<a id="access_token_bytes"></a>
<a id="access_token_length"></a>
<a id="classes"></a>
<a id="constant_time_contains"></a>
<a id="constant_time_equals"></a>
<a id="constant_time_starts_at"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="generate_access_token"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="token_file_name"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
