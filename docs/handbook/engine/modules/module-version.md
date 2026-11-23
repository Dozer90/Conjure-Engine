---
kind: header
---

[Engine](../../engine.md) → [Modules](../modules.md)

# module_version.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/modules/module_version.h` |
| API ↗ | [Writing a module](../../../api/modules/writing-a-module.md) |

<p>Semantic versions and version ranges of the module system.</p>

## Structs

| File | Description |
|---|---|
| [`semantic_version`](module-version/semantic-version.md) | <code>major.minor.patch</code>, each at most 65535. Field names avoid <code>major</code>/<code>minor</code>, which glibc defines as macros. |
| [`std::formatter<conjure::modules::semantic_version>`](module-version/std-formatter-conjure-modules-semantic-version.md) |  |
| [`std::formatter<conjure::modules::version_range>`](module-version/std-formatter-conjure-modules-version-range.md) |  |
| [`version_range`](module-version/version-range.md) | Half-open range [<code>minInclusive</code>, <code>maxExclusive</code>). The default is &quot;*&quot;: every version. |

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-MAX_SEMANTIC_VERSION"></a><code>MAX_SEMANTIC_VERSION</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="module-version/semantic-version.html">semantic_<wbr>version</a></code> | <code class="signature">{<span class="mi">0</span>xFFFF, <span class="mi">0</span>xFFFF, <span class="mi">0</span>xFFFF}</code> | Largest representable version; the open upper bound of unbounded ranges. |

## Functions

### Public

#### `host_engine_version` {#function-host-engine-version}

<div class="function-declaration" id="function-host-engine-version-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="module-version/semantic-version.html">semantic_version</a> host_engine_version() <span class="k">noexcept</span>;</code></pre></div>

<p>The version of the engine this code was compiled from (<code>build_info.h</code>).</p>

#### `parse_version` {#function-parse-version}

<div class="function-declaration" id="function-parse-version-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<a href="module-version/semantic-version.html">semantic_version</a>&gt; parse_version(
    std::string_view text);</code></pre></div>

<p>Parses &quot;<code>major.minor.patch</code>&quot; (all three required, decimal without leading zeros, no sign, no tags, each at most 65535). <code>PARSE_FAILURE</code> otherwise.</p>

#### `parse_version_range` {#function-parse-version-range}

<div class="function-declaration" id="function-parse-version-range-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<a href="module-version/version-range.html">version_range</a>&gt; parse_version_range(
    std::string_view text);</code></pre></div>

<p>Parses the range grammar described in the header comment. <code>A</code> reversed or empty range (&quot;&gt;=2.0.0 &lt;1.0.0&quot;) is <code>PARSE_FAILURE</code>.</p>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string to_string(
    <a href="module-version/semantic-version.html">semantic_version</a> version);</code></pre></div>

<p>&quot;<code>major.minor.patch</code>&quot;.</p>

<div class="function-declaration" id="function-to-string-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string to_string(
    <span class="k">const</span> <a href="module-version/version-range.html">version_range</a> &amp;range);</code></pre></div>

<p>Canonical text that <code><a href="#function-parse-version-range-1">parse_version_range</a>()</code> turns back into the same range: &quot;*&quot; for an unbounded range, &quot;1.2.3&quot; for a single version, &quot;&gt;=<code>a.b.c</code>&quot; for an open range and &quot;&gt;=<code>a.b.c</code> &lt;<code>d.e.f</code>&quot; otherwise (caret and tilde forms are not preserved).</p>

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
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="host_engine_version"></a>
<a id="internals-and-algorithms"></a>
<a id="max_semantic_version"></a>
<a id="parse_version"></a>
<a id="parse_version_range"></a>
<a id="semantic_version"></a>
<a id="semantic_versionmajorversion"></a>
<a id="semantic_versionminorversion"></a>
<a id="semantic_versionoperator"></a>
<a id="semantic_versionpatchversion"></a>
<a id="std"></a>
<a id="stdformat"></a>
<a id="stdformatterconjuremodulessemantic_version"></a>
<a id="stdformatterconjuremodulessemantic_versionformat"></a>
<a id="stdformatterconjuremodulesversion_range"></a>
<a id="stdformatterconjuremodulesversion_rangeformat"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="variables-and-properties"></a>
<a id="version_range"></a>
<a id="version_rangecontains"></a>
<a id="version_rangeis_unbounded"></a>
<a id="version_rangemaxexclusive"></a>
<a id="version_rangemininclusive"></a>
<a id="version_rangeoperator"></a>
<!-- doc-legacy-anchors:end -->
