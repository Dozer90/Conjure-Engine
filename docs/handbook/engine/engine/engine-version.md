---
kind: header
---

[Engine](../../engine.md) → [Engine](../engine.md)

# engine_version.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/engine/engine_version.h` |
| API ↗ | [Engine: the application and the frame](../../../api/engine/index.md), [Running the engine](../../../api/engine/running-the-engine.md) |

<p>The engine version, taken from the generated conjure/core/<code>build_info.h</code> so <code>project.json</code>&#x27;s &quot;<code>engine_version</code>&quot; and the module manifests agree with the build.</p>

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-ENGINE_VERSION_MAJOR"></a><code>ENGINE_VERSION_MAJOR</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="na">CONJURE_VERSION_MAJOR</span></code> |  |
| <a id="variable-ENGINE_VERSION_MINOR"></a><code>ENGINE_VERSION_MINOR</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="na">CONJURE_VERSION_MINOR</span></code> |  |
| <a id="variable-ENGINE_VERSION_PATCH"></a><code>ENGINE_VERSION_PATCH</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="na">CONJURE_VERSION_PATCH</span></code> |  |

## Functions

### Public

#### `engine_name` {#function-engine-name}

<div class="function-declaration" id="function-engine-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view engine_name() <span class="k">noexcept</span>;</code></pre></div>

<p>The engine&#x27;s display name (&quot;Conjure&quot;).</p>

#### `engine_version_string` {#function-engine-version-string}

<div class="function-declaration" id="function-engine-version-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view engine_version_string() <span class="k">noexcept</span>;</code></pre></div>

<p>&quot;<code>major.minor.patch</code>&quot; of the running engine (&quot;0.1.0&quot;).</p>

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
<a id="engine_name"></a>
<a id="engine_version_major"></a>
<a id="engine_version_minor"></a>
<a id="engine_version_patch"></a>
<a id="engine_version_string"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
