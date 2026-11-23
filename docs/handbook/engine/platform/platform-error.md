---
kind: header
---

[Engine](../../engine.md) → [Platform](../platform.md)

# platform_error.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/platform/platform_error.h` |
| API ↗ | [Platform: windows, input and events](../../../api/platform/index.md), [Windows and events](../../../api/platform/windows-and-events.md) |

<p>Error codes of the platform module. Errors are built with <code>core::make_error(platform_error::X, &quot;...&quot;)</code> and tested with <code>error::is(platform_error::X)</code>. Generic conditions (a bad gamepad index, an unknown key name) use <code><a href="../core/error.html#symbol-error-code">core::error_code</a></code> instead.</p>

## Enumerations

### `platform_error` {#symbol-platform-error}

```cpp
enum class platform_error : u16 {
    BACKEND_NOT_COMPILED = 1,   // backend_kind requested but CONJURE_PLATFORM_GLFW is OFF
    UNKNOWN_BACKEND,            // parse_backend_kind: name is neither "null" nor "glfw"
    INIT_FAILED,                // glfwInit failed (no display server, missing libraries); message has GLFW text
    INVALID_DESC,               // zero size when not fullscreen, title longer than 255 bytes, second live window
    WINDOW_CREATE_FAILED,       // glfwCreateWindow returned null
    MONITOR_NOT_FOUND,          // monitorIndex >= monitors().size()
    VULKAN_UNSUPPORTED,         // glfwVulkanSupported() false, or any Vulkan call on the null backend
    SURFACE_CREATE_FAILED,      // glfwCreateWindowSurface returned a VkResult != VK_SUCCESS (code in message)
    CLIPBOARD_UNAVAILABLE,      // glfwGetClipboardString returned null (no text / platform error)
    FEATURE_UNAVAILABLE,        // raw mouse motion, cursor_mode::CAPTURED or window positioning unsupported here
    COUNT
};
```

## Macros

### `CONJURE_DECLARE_ERROR_CATEGORY` {#symbol-conjure-declare-error-category}

```cpp
CONJURE_DECLARE_ERROR_CATEGORY(conjure::platform::platform_error, "platform", conjure::platform::platform_error_name)
```

## Functions

### Public

#### `platform_error_name` {#function-platform-error-name}

<div class="function-declaration" id="function-platform-error-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view platform_error_name(
    <a href="platform-error.html#symbol-platform-error">platform_error</a> code) <span class="k">noexcept</span>;</code></pre></div>

<p>The enumerator name of a platform error code (&quot;<code>INIT_FAILED</code>&quot;), or &quot;&quot; for unknown values.</p>

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
<a id="conjure_declare_error_category"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="platform_error"></a>
<a id="platform_error_name"></a>
<a id="platform_errorbackend_not_compiled"></a>
<a id="platform_errorclipboard_unavailable"></a>
<a id="platform_errorcount"></a>
<a id="platform_errorfeature_unavailable"></a>
<a id="platform_errorinit_failed"></a>
<a id="platform_errorinvalid_desc"></a>
<a id="platform_errormonitor_not_found"></a>
<a id="platform_errorsurface_create_failed"></a>
<a id="platform_errorunknown_backend"></a>
<a id="platform_errorvulkan_unsupported"></a>
<a id="platform_errorwindow_create_failed"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
