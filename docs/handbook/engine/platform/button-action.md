---
kind: header
---

[Engine](../../engine.md) → [Platform](../platform.md)

# button_action.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/platform/button_action.h` |
| API ↗ | [Input](../../../api/platform/input.md) |

<p>What happened to a key or button. The values are <code>GLFW</code>&#x27;s <code>GLFW_RELEASE</code> / <code>GLFW_PRESS</code> / <code>GLFW_REPEAT</code> and travel unchanged through the <code>C</code> ABI (<code><a href="../scripting/native-api.html#symbol-conjure-button-action">conjure_button_action</a></code>).</p>

## Enumerations

### `button_action` {#symbol-button-action}

```cpp
enum class button_action : u8 {
    RELEASE = 0,
    PRESS = 1,
    REPEAT = 2
};
```

## Functions

### Public

#### `parse_button_action` {#function-parse-button-action}

<div class="function-declaration" id="function-parse-button-action-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::optional&lt;<a href="button-action.html#symbol-button-action">button_action</a>&gt; parse_button_action(
    std::string_view name) <span class="k">noexcept</span>;</code></pre></div>

<p>Parses an enumerator name case-insensitively; nullopt for anything else.</p>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="button-action.html#symbol-button-action">button_action</a> action) <span class="k">noexcept</span>;</code></pre></div>

<p>The enumerator name (&quot;<code>PRESS</code>&quot;), or &quot;&quot; for unknown values.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="button_action"></a>
<a id="button_actionpress"></a>
<a id="button_actionrelease"></a>
<a id="button_actionrepeat"></a>
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="parse_button_action"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
