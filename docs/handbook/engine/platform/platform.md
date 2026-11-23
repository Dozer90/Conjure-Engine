---
kind: header
---

[Engine](../../engine.md) → [Platform](../platform.md)

# platform.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/platform/platform.h` |
| API ↗ | [Platform: windows, input and events](../../../api/platform/index.md) |

<p>Umbrella header of the platform module: window, events, input and the backend queries. Include this rather than the individual headers unless compile time matters.</p>

## Functions

### Public

#### `backend_version_string` {#function-backend-version-string}

<div class="function-declaration" id="function-backend-version-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view backend_version_string(
    <a href="backend.html#symbol-backend-kind">backend_kind</a> kind) <span class="k">noexcept</span>;</code></pre></div>

<p>The version string of a backend: <code>glfwGetVersionString()</code> for <code>GLFW</code> (when compiled, otherwise &quot;glfw (not compiled)&quot;), &quot;<code>null</code>&quot; for the <code>null</code> backend, &quot;&quot; for unknown kinds.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="backend_version_string"></a>
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
