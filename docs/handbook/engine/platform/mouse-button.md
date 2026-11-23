---
kind: header
---

[Engine](../../engine.md) → [Platform](../platform.md)

# mouse_button.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/platform/mouse_button.h` |
| API ↗ | [Input](../../../api/platform/input.md) |

<p>Mouse buttons: <code>GLFW_MOUSE_BUTTON_</code>* numbering, eight buttons.</p>

## Enumerations

### `mouse_button` {#symbol-mouse-button}

```cpp
enum class mouse_button : u8 {
    LEFT = 0,
    RIGHT = 1,
    MIDDLE = 2,
    BUTTON_4,
    BUTTON_5,
    BUTTON_6,
    BUTTON_7,
    BUTTON_8,
    COUNT
};
```

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-MOUSE_BUTTON_COUNT"></a><code>MOUSE_BUTTON_COUNT</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">8</span></code> |  |

## Functions

### Public

#### `is_valid_mouse_button` {#function-is-valid-mouse-button}

<div class="function-declaration" id="function-is-valid-mouse-button-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_valid_mouse_button(
    <a href="mouse-button.html#symbol-mouse-button">mouse_button</a> button) <span class="k">noexcept</span>;</code></pre></div>

<p>True when the value names one of the eight buttons.</p>

#### `parse_mouse_button` {#function-parse-mouse-button}

<div class="function-declaration" id="function-parse-mouse-button-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::optional&lt;<a href="mouse-button.html#symbol-mouse-button">mouse_button</a>&gt; parse_mouse_button(
    std::string_view name) <span class="k">noexcept</span>;</code></pre></div>

<p>Parses an enumerator name case-insensitively; nullopt for anything else.</p>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="mouse-button.html#symbol-mouse-button">mouse_button</a> button) <span class="k">noexcept</span>;</code></pre></div>

<p>The enumerator name (&quot;<code>MIDDLE</code>&quot;), or &quot;&quot; for unknown values.</p>

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
<a id="internals-and-algorithms"></a>
<a id="is_valid_mouse_button"></a>
<a id="mouse_button"></a>
<a id="mouse_button_count"></a>
<a id="mouse_buttonbutton_4"></a>
<a id="mouse_buttonbutton_5"></a>
<a id="mouse_buttonbutton_6"></a>
<a id="mouse_buttonbutton_7"></a>
<a id="mouse_buttonbutton_8"></a>
<a id="mouse_buttoncount"></a>
<a id="mouse_buttonleft"></a>
<a id="mouse_buttonmiddle"></a>
<a id="mouse_buttonright"></a>
<a id="other-declarations"></a>
<a id="parse_mouse_button"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
