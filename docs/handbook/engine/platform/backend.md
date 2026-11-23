---
kind: header
---

[Engine](../../engine.md) → [Platform](../platform.md)

# backend.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/platform/backend.h` |
| API ↗ | [Windows and events](../../../api/platform/windows-and-events.md) |

<p>Which implementation sits behind a <code><a href="window/window.html">platform::window</a></code>: the real <code>GLFW</code> 3.4 window or the virtual <code>null</code> window used by every headless test and server build.</p>

## Enumerations

### `backend_kind` {#symbol-backend-kind}

```cpp
enum class backend_kind : u8 {
    NULL_BACKEND = 0,   // virtual window: no OS calls, events queued by tests
    GLFW = 1,           // real OS window through GLFW 3.4 (CONJURE_PLATFORM_GLFW)
    COUNT
};
```

## Functions

### Public

#### `default_backend_kind` {#function-default-backend-kind}

<div class="function-declaration" id="function-default-backend-kind-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="backend.html#symbol-backend-kind">backend_kind</a> default_backend_kind() <span class="k">noexcept</span>;</code></pre></div>

<p><code>GLFW</code> when compiled, otherwise <code>NULL_BACKEND</code>.</p>

#### `is_backend_compiled` {#function-is-backend-compiled}

<div class="function-declaration" id="function-is-backend-compiled-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">bool</span> is_backend_compiled(
    <a href="backend.html#symbol-backend-kind">backend_kind</a> kind) <span class="k">noexcept</span>;</code></pre></div>

<p>True when the backend was compiled into this library. The <code>null</code> backend always is; the glfw backend only when <code>CONJURE_PLATFORM_GLFW</code> was ON.</p>

#### `parse_backend_kind` {#function-parse-backend-kind}

<div class="function-declaration" id="function-parse-backend-kind-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<a href="backend.html#symbol-backend-kind">backend_kind</a>&gt; parse_backend_kind(
    std::string_view name) <span class="k">noexcept</span>;</code></pre></div>

<p>Parses a backend name case-insensitively (&quot;<code>null</code>&quot;, &quot;glfw&quot;). Any other name fails with <code>platform_error::UNKNOWN_BACKEND</code>.</p>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="backend.html#symbol-backend-kind">backend_kind</a> kind) <span class="k">noexcept</span>;</code></pre></div>

<p>The configuration name of a backend: &quot;<code>null</code>&quot; or &quot;glfw&quot;; &quot;&quot; for unknown values.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="backend_kind"></a>
<a id="backend_kindcount"></a>
<a id="backend_kindglfw"></a>
<a id="backend_kindnull_backend"></a>
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="default_backend_kind"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="is_backend_compiled"></a>
<a id="other-declarations"></a>
<a id="parse_backend_kind"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
