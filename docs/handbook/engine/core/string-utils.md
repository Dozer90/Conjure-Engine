---
kind: header
---

[Engine](../../engine.md) → [Core](../core.md)

# string_utils.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/string_utils.h` |
| API ↗ | [Strings and identifiers](../../../api/core/strings-and-identifiers.md) |

<p>ASCII-oriented string helpers used throughout the engine for identifiers, paths, config values and log output. Case conversion and classification are ASCII only on purpose: engine identifiers are ASCII, and locale-aware behaviour would make builds non-deterministic. UTF-8 is treated as opaque bytes everywhere except the explicit UTF-8 &lt;-&gt; UTF-16/UTF-32 conversions.</p>

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-str::REPLACEMENT_CHARACTER"></a><code>str::REPLACEMENT_CHARACTER</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> char32_<wbr>t</code> | <code class="signature"><span class="mi">0</span>xFFFD</code> | The replacement character written for malformed input during conversions. |
| <a id="variable-str::WHITESPACE"></a><code>str::WHITESPACE</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">" \t\n\r\f\v"</span></code> | The characters <code>trim()</code> removes by default: space, tab, newline, carriage return, form feed and vertical tab. |

## Functions

### Public

#### `str::contains` {#function-str-contains}

<div class="function-declaration" id="function-str-contains-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> contains(std::string_view text, std::string_view needle) <span class="k">noexcept</span>;</code></pre></div>

<p>True when <code>needle</code> occurs in <code>text</code>. Every string contains &quot;&quot;.</p>

<div class="function-declaration" id="function-str-contains-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> contains(std::string_view text, <span class="k">char</span> character) <span class="k">noexcept</span>;</code></pre></div>

<p>True when <code>character</code> occurs in <code>text</code>.</p>

#### `str::detail::parse_unsigned_digits` {#function-str-detail-parse-unsigned-digits}

<div class="function-declaration" id="function-str-detail-parse-unsigned-digits-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">bool</span> parse_unsigned_digits( std::string_view text, <span class="k">int</span> base, <span class="k">bool</span> allowNegative, <a href="types.html#symbol-u64">u64</a> limitPositive, <a href="types.html#symbol-u64">u64</a> limitNegative, <a href="types.html#symbol-u64">u64</a> &amp;magnitude, <span class="k">bool</span> &amp;negative ) <span class="k">noexcept</span>;</code></pre></div>

<p>Shared body of <code>parse_integer</code>: strips the sign and prefix, then converts the digits. <code>negative</code> is only accepted when <code>allowNegative</code> is set. Returns <code>false</code> on any error.</p>

#### `str::ends_with` {#function-str-ends-with}

<div class="function-declaration" id="function-str-ends-with-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> ends_with(std::string_view text, std::string_view suffix) <span class="k">noexcept</span>;</code></pre></div>

<p>True when <code>text</code> ends with <code>suffix</code> (<code><a href="types.html#symbol-byte">byte</a></code>-wise). Every string ends with &quot;&quot;.</p>

#### `str::format_bytes` {#function-str-format-bytes}

<div class="function-declaration" id="function-str-format-bytes-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string format_bytes(<a href="types.html#symbol-u64">u64</a> bytes);</code></pre></div>

<p>Human readable <code><a href="types.html#symbol-byte">byte</a></code> count with binary units: &quot;512 <code>B</code>&quot;, &quot;1.0 KiB&quot;, &quot;1.5 MiB&quot;, &quot;2.0 GiB&quot;. Values below 1 KiB print as whole bytes; larger values print one decimal.</p>

#### `str::from_utf16` {#function-str-from-utf16}

<div class="function-declaration" id="function-str-from-utf16-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string from_utf16(std::u16string_view utf16);</code></pre></div>

<p>Encodes UTF-16 as UTF-8. Unpaired surrogates become <code>U</code>+FFFD; the function never fails.</p>

#### `str::from_utf32` {#function-str-from-utf32}

<div class="function-declaration" id="function-str-from-utf32-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string from_utf32(std::u32string_view utf32);</code></pre></div>

<p>Encodes UTF-32 as UTF-8. Surrogate code points and values above <code>U</code>+10FFFF become <code>U</code>+FFFD; the function never fails.</p>

#### `str::from_wide` {#function-str-from-wide}

<div class="function-declaration" id="function-str-from-wide-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string from_wide(std::wstring_view wide);</code></pre></div>

<p>Platform wide string to UTF-8. Inverse of <code>to_wide()</code>; unpaired surrogates and invalid UTF-32 code points become <code>U</code>+FFFD, as in <code>from_utf16()</code>/<code>from_utf32()</code>.</p>

#### `str::iends_with` {#function-str-iends-with}

<div class="function-declaration" id="function-str-iends-with-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> iends_with(std::string_view text, std::string_view suffix) <span class="k">noexcept</span>;</code></pre></div>

<p><code>ends_with()</code> ignoring ASCII case.</p>

#### `str::iequals` {#function-str-iequals}

<div class="function-declaration" id="function-str-iequals-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> iequals(std::string_view left, std::string_view right) <span class="k">noexcept</span>;</code></pre></div>

<p>True when the two strings are equal ignoring ASCII case.</p>

#### `str::is_alnum` {#function-str-is-alnum}

<div class="function-declaration" id="function-str-is-alnum-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_alnum(<span class="k">char</span> character) <span class="k">noexcept</span>;</code></pre></div>

<p>True for ASCII letters and digits.</p>

#### `str::is_alpha` {#function-str-is-alpha}

<div class="function-declaration" id="function-str-is-alpha-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_alpha(<span class="k">char</span> character) <span class="k">noexcept</span>;</code></pre></div>

<p>True for <code>&#x27;a&#x27;</code>..<code>&#x27;z&#x27;</code> and <code>&#x27;A&#x27;</code>..<code>&#x27;Z&#x27;</code>.</p>

#### `str::is_digit` {#function-str-is-digit}

<div class="function-declaration" id="function-str-is-digit-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_digit(<span class="k">char</span> character) <span class="k">noexcept</span>;</code></pre></div>

<p>True for &#x27;0&#x27;..&#x27;9&#x27;.</p>

#### `str::is_space` {#function-str-is-space}

<div class="function-declaration" id="function-str-is-space-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_space(<span class="k">char</span> character) <span class="k">noexcept</span>;</code></pre></div>

<p>True for the characters in <code>WHITESPACE</code>.</p>

#### `str::is_valid_identifier` {#function-str-is-valid-identifier}

<div class="function-declaration" id="function-str-is-valid-identifier-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_valid_identifier(std::string_view text) <span class="k">noexcept</span>;</code></pre></div>

<p>True when <code>text</code> is a <code>C</code>-style identifier: non-empty, starts with an ASCII letter or <code>&#x27;_&#x27;</code>, and continues with ASCII letters, digits or <code>&#x27;_&#x27;</code>.</p>

#### `str::is_valid_utf8` {#function-str-is-valid-utf8}

<div class="function-declaration" id="function-str-is-valid-utf8-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">bool</span> is_valid_utf8(std::string_view text) <span class="k">noexcept</span>;</code></pre></div>

<p>True when <code>text</code> is a valid UTF-8 <code><a href="types.html#symbol-byte">byte</a></code> sequence (no overlong forms, surrogates or code points above <code>U</code>+10FFFF). The empty string is valid.</p>

#### `str::istarts_with` {#function-str-istarts-with}

<div class="function-declaration" id="function-str-istarts-with-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> istarts_with(std::string_view text, std::string_view prefix) <span class="k">noexcept</span>;</code></pre></div>

<p><code>starts_with()</code> ignoring ASCII case.</p>

#### `str::join` {#function-str-join}

<div class="function-declaration" id="function-str-join-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> Range&gt; <span class="k">requires</span> std::convertible_to&lt;std::ranges::range_value_t&lt;Range&gt;, std::string_view&gt; [[<span class="na">nodiscard</span>]] std::string join(<span class="k">const</span> Range &amp;pieces, std::string_view separator);</code></pre></div>

<p>Concatenates the pieces with <code>separator</code> between them. Accepts any range whose elements convert to <code>std::string_view</code> (<code>std::vector&lt;std::string&gt;</code>, <code>std::span&lt;std::string_view&gt;</code>, ...).</p>

#### `str::parse_bool` {#function-str-parse-bool}

<div class="function-declaration" id="function-str-parse-bool-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::optional&lt;<span class="k">bool</span>&gt; parse_bool(std::string_view text) <span class="k">noexcept</span>;</code></pre></div>

<p>Parses &quot;<code>true</code>&quot;/&quot;<code>false</code>&quot;, &quot;yes&quot;/&quot;no&quot;, &quot;on&quot;/&quot;off&quot; and &quot;1&quot;/&quot;0&quot;, ignoring ASCII case.</p>

#### `str::parse_double` {#function-str-parse-double}

<div class="function-declaration" id="function-str-parse-double-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::optional&lt;<a href="types.html#symbol-f64">f64</a>&gt; parse_double(std::string_view text) <span class="k">noexcept</span>;</code></pre></div>

<p>Parses the whole of <code>text</code> as a decimal floating-point number: optional sign, digits, optional fraction and exponent (&quot;1&quot;, &quot;-2.5&quot;, &quot;1e-3&quot;, &quot;.5&quot;, &quot;7.&quot;), plus &quot;inf&quot;/&quot;nan&quot; in any case. Hexadecimal floats, whitespace and trailing characters are rejected.</p>

#### `str::parse_float` {#function-str-parse-float}

<div class="function-declaration" id="function-str-parse-float-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::optional&lt;<a href="types.html#symbol-f32">f32</a>&gt; parse_float(std::string_view text) <span class="k">noexcept</span>;</code></pre></div>

<p><code>parse_double()</code> narrowed to <code><a href="types.html#symbol-f32">f32</a></code>. Finite values that overflow <code><a href="types.html#symbol-f32">f32</a></code> are rejected.</p>

#### `str::parse_int` {#function-str-parse-int}

<div class="function-declaration" id="function-str-parse-int-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::optional&lt;<a href="types.html#symbol-i64">i64</a>&gt; parse_int(std::string_view text, <span class="k">int</span> base = <span class="mi">10</span>) <span class="k">noexcept</span>;</code></pre></div>

<p><code>parse_integer&lt;<a href="types.html#symbol-i64">i64</a>&gt;</code>.</p>

#### `str::parse_integer` {#function-str-parse-integer}

<div class="function-declaration" id="function-str-parse-integer-1"><pre><code><span class="k">template</span> &lt;std::integral T&gt; <span class="k">requires</span>(!std::same_as&lt;T, <span class="k">bool</span>&gt;) [[<span class="na">nodiscard</span>]] std::optional&lt;T&gt; parse_integer(std::string_view text, <span class="k">int</span> base = <span class="mi">10</span>) <span class="k">noexcept</span>;</code></pre></div>

<p>Parses the whole of <code>text</code> as an integer of type <code>T</code> in <code>base</code> (2..36). Accepts an optional leading &#x27;+&#x27; or (for signed types) &#x27;-&#x27;, and for base 16 an optional &quot;0x&quot;/&quot;0X&quot; prefix. No whitespace, no trailing characters, no digits outside the base; out-of-range values fail.</p>

<div class="function-declaration" id="function-str-parse-integer-2"><pre><code><span class="k">template</span> &lt;std::integral T&gt; <span class="k">requires</span>(!std::same_as&lt;T, <span class="k">bool</span>&gt;) std::optional&lt;T&gt; parse_integer(std::string_view text, <span class="k">int</span> base) <span class="k">noexcept</span>;</code></pre></div>

#### `str::parse_uint` {#function-str-parse-uint}

<div class="function-declaration" id="function-str-parse-uint-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::optional&lt;<a href="types.html#symbol-u64">u64</a>&gt; parse_uint(std::string_view text, <span class="k">int</span> base = <span class="mi">10</span>) <span class="k">noexcept</span>;</code></pre></div>

<p><code>parse_integer&lt;<a href="types.html#symbol-u64">u64</a>&gt;</code>. <code>A</code> leading &#x27;-&#x27; is rejected.</p>

#### `str::replace_all` {#function-str-replace-all}

<div class="function-declaration" id="function-str-replace-all-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string replace_all( std::string_view text, std::string_view from, std::string_view to );</code></pre></div>

<p>Copy of <code>text</code> with every non-overlapping occurrence of <code>from</code> replaced by <code>to</code>, scanning left to right. An empty <code>from</code> returns <code>text</code> unchanged.</p>

#### `str::split` {#function-str-split}

<div class="function-declaration" id="function-str-split-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::vector&lt;std::string_view&gt; split( std::string_view text, <span class="k">char</span> delimiter, <span class="k">bool</span> keepEmpty = true );</code></pre></div>

<p>Splits <code>text</code> at every <code>delimiter</code>. The pieces are views into <code>text</code>. With <code>keepEmpty</code> the result has exactly one more piece than there are delimiters (so &quot;&quot; yields [&quot;&quot;] and &quot;a,,b&quot; yields [&quot;a&quot;, &quot;&quot;, &quot;b&quot;]); without it empty pieces are dropped.</p>

<div class="function-declaration" id="function-str-split-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::vector&lt;std::string_view&gt; split( std::string_view text, std::string_view delimiter, <span class="k">bool</span> keepEmpty = true );</code></pre></div>

<p>Splits <code>text</code> at every occurrence of the multi-character <code>delimiter</code>. An empty delimiter yields the whole text as a single piece. Same <code>keepEmpty</code> rules as the char overload.</p>

#### `str::split_lines` {#function-str-split-lines}

<div class="function-declaration" id="function-str-split-lines-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::vector&lt;std::string_view&gt; split_lines(std::string_view text);</code></pre></div>

<p>Splits <code>text</code> into lines at &quot;\n&quot;, &quot;\r\n&quot; or a lone &quot;\r&quot;. Line terminators are not part of the pieces; a trailing terminator does not produce an extra empty line.</p>

#### `str::starts_with` {#function-str-starts-with}

<div class="function-declaration" id="function-str-starts-with-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> starts_with(std::string_view text, std::string_view prefix) <span class="k">noexcept</span>;</code></pre></div>

<p>True when <code>text</code> begins with <code>prefix</code> (<code><a href="types.html#symbol-byte">byte</a></code>-wise). Every string starts with &quot;&quot;.</p>

#### `str::to_lower` {#function-str-to-lower}

<div class="function-declaration" id="function-str-to-lower-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">char</span> to_lower(<span class="k">char</span> character) <span class="k">noexcept</span>;</code></pre></div>

<p>Lower-cases one ASCII letter; every other <code><a href="types.html#symbol-byte">byte</a></code> is returned unchanged.</p>

<div class="function-declaration" id="function-str-to-lower-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string to_lower(std::string_view text);</code></pre></div>

<p>Copy of <code>text</code> with ASCII letters lower-cased. Other bytes (including UTF-8) are untouched.</p>

#### `str::to_lower_in_place` {#function-str-to-lower-in-place}

<div class="function-declaration" id="function-str-to-lower-in-place-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> to_lower_in_place(std::string &amp;text) <span class="k">noexcept</span>;</code></pre></div>

<p>Lower-cases the ASCII letters of <code>text</code> in place.</p>

#### `str::to_upper` {#function-str-to-upper}

<div class="function-declaration" id="function-str-to-upper-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">char</span> to_upper(<span class="k">char</span> character) <span class="k">noexcept</span>;</code></pre></div>

<p>Upper-cases one ASCII letter; every other <code><a href="types.html#symbol-byte">byte</a></code> is returned unchanged.</p>

<div class="function-declaration" id="function-str-to-upper-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string to_upper(std::string_view text);</code></pre></div>

<p>Copy of <code>text</code> with ASCII letters upper-cased. Other bytes (including UTF-8) are untouched.</p>

#### `str::to_upper_in_place` {#function-str-to-upper-in-place}

<div class="function-declaration" id="function-str-to-upper-in-place-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> to_upper_in_place(std::string &amp;text) <span class="k">noexcept</span>;</code></pre></div>

<p>Upper-cases the ASCII letters of <code>text</code> in place.</p>

#### `str::to_utf16` {#function-str-to-utf16}

<div class="function-declaration" id="function-str-to-utf16-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::u16string to_utf16(std::string_view utf8);</code></pre></div>

<p>Decodes UTF-8 to UTF-16. Malformed bytes (truncated sequences, overlong forms, encoded surrogates, code points above <code>U</code>+10FFFF) each become <code>U</code>+FFFD; the function never fails.</p>

#### `str::to_utf32` {#function-str-to-utf32}

<div class="function-declaration" id="function-str-to-utf32-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::u32string to_utf32(std::string_view utf8);</code></pre></div>

<p>Decodes UTF-8 to UTF-32 (one code point per element). Same error handling as <code>to_utf16()</code>.</p>

#### `str::to_wide` {#function-str-to-wide}

<div class="function-declaration" id="function-str-to-wide-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::wstring to_wide(std::string_view utf8);</code></pre></div>

<p>UTF-8 to the platform wide string: UTF-16 where <code>wchar_t</code> is 16 bits (Windows paths and Win32 calls), UTF-32 where it is 32 bits. Same error handling as <code>to_utf16()</code>/<code>to_utf32()</code>.</p>

#### `str::trim` {#function-str-trim}

<div class="function-declaration" id="function-str-trim-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> std::string_view trim( std::string_view text, std::string_view characters = WHITESPACE ) <span class="k">noexcept</span>;</code></pre></div>

<p><code>text</code> without leading and trailing characters from <code>characters</code>. Returns a view into <code>text</code>; when nothing remains, an empty view at the end of <code>text</code>.</p>

#### `str::trim_left` {#function-str-trim-left}

<div class="function-declaration" id="function-str-trim-left-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> std::string_view trim_left( std::string_view text, std::string_view characters = WHITESPACE ) <span class="k">noexcept</span>;</code></pre></div>

<p><code>text</code> without leading characters from <code>characters</code>. Returns a view into <code>text</code>; when nothing remains, an empty view at the end of <code>text</code>.</p>

#### `str::trim_right` {#function-str-trim-right}

<div class="function-declaration" id="function-str-trim-right-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> std::string_view trim_right( std::string_view text, std::string_view characters = WHITESPACE ) <span class="k">noexcept</span>;</code></pre></div>

<p><code>text</code> without trailing characters from <code>characters</code>. Returns a view into <code>text</code>; when nothing remains, an empty view at the start of <code>text</code>.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="contains"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="ends_with"></a>
<a id="extension-points"></a>
<a id="format_bytes"></a>
<a id="from_utf16"></a>
<a id="from_utf32"></a>
<a id="from_wide"></a>
<a id="iends_with"></a>
<a id="iequals"></a>
<a id="internals-and-algorithms"></a>
<a id="is_alnum"></a>
<a id="is_alpha"></a>
<a id="is_digit"></a>
<a id="is_space"></a>
<a id="is_valid_identifier"></a>
<a id="is_valid_utf8"></a>
<a id="istarts_with"></a>
<a id="join"></a>
<a id="parse_bool"></a>
<a id="parse_double"></a>
<a id="parse_float"></a>
<a id="parse_int"></a>
<a id="parse_integer"></a>
<a id="parse_uint"></a>
<a id="parse_unsigned_digits"></a>
<a id="replace_all"></a>
<a id="replacement_character"></a>
<a id="split"></a>
<a id="split_lines"></a>
<a id="starts_with"></a>
<a id="strcontains"></a>
<a id="strdetailparse_unsigned_digits"></a>
<a id="strends_with"></a>
<a id="strformat_bytes"></a>
<a id="strfrom_utf16"></a>
<a id="strfrom_utf32"></a>
<a id="strfrom_wide"></a>
<a id="striends_with"></a>
<a id="striequals"></a>
<a id="stris_alnum"></a>
<a id="stris_alpha"></a>
<a id="stris_digit"></a>
<a id="stris_space"></a>
<a id="stris_valid_identifier"></a>
<a id="stris_valid_utf8"></a>
<a id="stristarts_with"></a>
<a id="strjoin"></a>
<a id="strparse_bool"></a>
<a id="strparse_double"></a>
<a id="strparse_float"></a>
<a id="strparse_int"></a>
<a id="strparse_integer"></a>
<a id="strparse_uint"></a>
<a id="strreplace_all"></a>
<a id="strreplacement_character"></a>
<a id="strsplit"></a>
<a id="strsplit_lines"></a>
<a id="strstarts_with"></a>
<a id="strto_lower"></a>
<a id="strto_lower_in_place"></a>
<a id="strto_upper"></a>
<a id="strto_upper_in_place"></a>
<a id="strto_utf16"></a>
<a id="strto_utf32"></a>
<a id="strto_wide"></a>
<a id="strtrim"></a>
<a id="strtrim_left"></a>
<a id="strtrim_right"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="strwhitespace"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_lower"></a>
<a id="to_lower_in_place"></a>
<a id="to_upper"></a>
<a id="to_upper_in_place"></a>
<a id="to_utf16"></a>
<a id="to_utf32"></a>
<a id="to_wide"></a>
<a id="trim"></a>
<a id="trim_left"></a>
<a id="trim_right"></a>
<a id="variables-and-properties"></a>
<a id="whitespace"></a>
<!-- doc-legacy-anchors:end -->
