---
kind: header
---

[Engine](../../engine.md) → [Platform](../platform.md)

# key.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/platform/key.h` |
| API ↗ | [Input](../../../api/platform/input.md) |

<p>Keyboard keys and modifier flags. Key codes use <code>GLFW</code>&#x27;s numbering so the glfw backend maps with a range check, and the <code>null</code> backend, the <code>C</code># Key enum and the editor protocol share one stable numbering. <code>key::UNKNOWN</code> (0) replaces <code>GLFW_KEY_UNKNOWN</code> (-1); <code>key::DEL</code> is spelled that way because DELETE is a Windows macro.</p>

## Enumerations

### `key` {#symbol-key}

```cpp
enum class key : u16 {
    UNKNOWN = 0,
    SPACE = 32,
    APOSTROPHE = 39,
    COMMA = 44,
    MINUS,
    PERIOD,
    SLASH,
    NUM_0 = 48,
    NUM_1,
    NUM_2,
    NUM_3,
    NUM_4,
    NUM_5,
    NUM_6,
    NUM_7,
    NUM_8,
    NUM_9,
    SEMICOLON = 59,
    EQUAL = 61,
    A = 65,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,
    LEFT_BRACKET = 91,
    BACKSLASH,
    RIGHT_BRACKET,
    GRAVE_ACCENT = 96,
    WORLD_1 = 161,
    WORLD_2 = 162,
    ESCAPE = 256,
    ENTER,
    TAB,
    BACKSPACE,
    INSERT,
    DEL,
    RIGHT,
    LEFT,
    DOWN,
    UP,
    PAGE_UP,
    PAGE_DOWN,
    HOME,
    END,
    CAPS_LOCK = 280,
    SCROLL_LOCK,
    NUM_LOCK,
    PRINT_SCREEN,
    PAUSE,
    F1 = 290,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
    F13,
    F14,
    F15,
    F16,
    F17,
    F18,
    F19,
    F20,
    F21,
    F22,
    F23,
    F24,
    F25,
    KP_0 = 320,
    KP_1,
    KP_2,
    KP_3,
    KP_4,
    KP_5,
    KP_6,
    KP_7,
    KP_8,
    KP_9,
    KP_DECIMAL,
    KP_DIVIDE,
    KP_MULTIPLY,
    KP_SUBTRACT,
    KP_ADD,
    KP_ENTER,
    KP_EQUAL,
    LEFT_SHIFT = 340,
    LEFT_CONTROL,
    LEFT_ALT,
    LEFT_SUPER,
    RIGHT_SHIFT,
    RIGHT_CONTROL,
    RIGHT_ALT,
    RIGHT_SUPER,
    MENU = 348,
    LAST = MENU
};
```

### `key_modifier_flags` {#symbol-key-modifier-flags}

```cpp
enum class key_modifier_flags : u8 {   // GLFW_MOD_* values
    NONE = 0,
    SHIFT = 1 << 0,
    CONTROL = 1 << 1,
    ALT = 1 << 2,
    SUPER = 1 << 3,
    CAPS_LOCK = 1 << 4,
    NUM_LOCK = 1 << 5
};
```

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-KEY_COUNT"></a><code>KEY_COUNT</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">512</span></code> |  |

## Macros

### `CONJURE_ENUM_FLAGS` {#symbol-conjure-enum-flags}

```cpp
CONJURE_ENUM_FLAGS(key_modifier_flags)
```

## Functions

### Public

#### `is_valid_key` {#function-is-valid-key}

<div class="function-declaration" id="function-is-valid-key-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_valid_key(
    <a href="../core/types.html#symbol-u32">u32</a> code) <span class="k">noexcept</span>;</code></pre></div>

<p>True when code names a real key: 0 (<code>UNKNOWN</code>) is not valid, nor is anything above <code>key::LAST</code>.</p>

<div class="function-declaration" id="function-is-valid-key-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_valid_key(
    <a href="key.html#symbol-key">key</a> code) <span class="k">noexcept</span>;</code></pre></div>

<p>True when the key value names a real key (<code>is_valid_key</code> on its code).</p>

#### `parse_key` {#function-parse-key}

<div class="function-declaration" id="function-parse-key-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::optional&lt;<a href="key.html#symbol-key">key</a>&gt; parse_key(
    std::string_view name) <span class="k">noexcept</span>;</code></pre></div>

<p>Parses an enumerator name case-insensitively (&quot;space&quot;, &quot;<code>F12</code>&quot;, &quot;Del&quot;); nullopt for anything else, including &quot;<code>LAST</code>&quot; (an alias, not a key) and &quot;<code>UNKNOWN</code>&quot;.</p>

#### `parse_key_modifier_flags` {#function-parse-key-modifier-flags}

<div class="function-declaration" id="function-parse-key-modifier-flags-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::optional&lt;<a href="key.html#symbol-key-modifier-flags">key_modifier_flags</a>&gt; parse_key_modifier_flags(
    std::string_view text) <span class="k">noexcept</span>;</code></pre></div>

<p>Parses a &#x27;|&#x27; separated list of flag names case-insensitively (&quot;shift|alt&quot;, &quot;<code>NONE</code>&quot;, &quot;&quot;); nullopt when any name is unknown.</p>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="key.html#symbol-key">key</a> code) <span class="k">noexcept</span>;</code></pre></div>

<p>The enumerator name (&quot;<code>SPACE</code>&quot;, &quot;<code>DEL</code>&quot;, &quot;<code>KP_ENTER</code>&quot;); &quot;<code>UNKNOWN</code>&quot; for <code>key::UNKNOWN</code> and &quot;&quot; for values that are not enumerators.</p>

<div class="function-declaration" id="function-to-string-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string to_string(
    <a href="key.html#symbol-key-modifier-flags">key_modifier_flags</a> modifiers);</code></pre></div>

<p>The set flags joined with &#x27;|&#x27; (&quot;<code>SHIFT</code>|<code>CONTROL</code>&quot;), &quot;<code>NONE</code>&quot; for an empty set. Bits outside the known flags are ignored.</p>

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
<a id="conjure_enum_flags"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="is_valid_key"></a>
<a id="key"></a>
<a id="key_count"></a>
<a id="key_modifier_flags"></a>
<a id="key_modifier_flagsalt"></a>
<a id="key_modifier_flagscaps_lock"></a>
<a id="key_modifier_flagscontrol"></a>
<a id="key_modifier_flagsnone"></a>
<a id="key_modifier_flagsnum_lock"></a>
<a id="key_modifier_flagsshift"></a>
<a id="key_modifier_flagssuper"></a>
<a id="keya"></a>
<a id="keyapostrophe"></a>
<a id="keyb"></a>
<a id="keybackslash"></a>
<a id="keybackspace"></a>
<a id="keyc"></a>
<a id="keycaps_lock"></a>
<a id="keycomma"></a>
<a id="keyd"></a>
<a id="keydel"></a>
<a id="keydown"></a>
<a id="keye"></a>
<a id="keyend"></a>
<a id="keyenter"></a>
<a id="keyequal"></a>
<a id="keyescape"></a>
<a id="keyf"></a>
<a id="keyf1"></a>
<a id="keyf10"></a>
<a id="keyf11"></a>
<a id="keyf12"></a>
<a id="keyf13"></a>
<a id="keyf14"></a>
<a id="keyf15"></a>
<a id="keyf16"></a>
<a id="keyf17"></a>
<a id="keyf18"></a>
<a id="keyf19"></a>
<a id="keyf2"></a>
<a id="keyf20"></a>
<a id="keyf21"></a>
<a id="keyf22"></a>
<a id="keyf23"></a>
<a id="keyf24"></a>
<a id="keyf25"></a>
<a id="keyf3"></a>
<a id="keyf4"></a>
<a id="keyf5"></a>
<a id="keyf6"></a>
<a id="keyf7"></a>
<a id="keyf8"></a>
<a id="keyf9"></a>
<a id="keyg"></a>
<a id="keygrave_accent"></a>
<a id="keyh_1"></a>
<a id="keyhome"></a>
<a id="keyi"></a>
<a id="keyinsert"></a>
<a id="keyj"></a>
<a id="keyk"></a>
<a id="keykp_0"></a>
<a id="keykp_1"></a>
<a id="keykp_2"></a>
<a id="keykp_3"></a>
<a id="keykp_4"></a>
<a id="keykp_5"></a>
<a id="keykp_6"></a>
<a id="keykp_7"></a>
<a id="keykp_8"></a>
<a id="keykp_9"></a>
<a id="keykp_add"></a>
<a id="keykp_decimal"></a>
<a id="keykp_divide"></a>
<a id="keykp_enter"></a>
<a id="keykp_equal"></a>
<a id="keykp_multiply"></a>
<a id="keykp_subtract"></a>
<a id="keyl"></a>
<a id="keylast"></a>
<a id="keyleft"></a>
<a id="keyleft_alt"></a>
<a id="keyleft_bracket"></a>
<a id="keyleft_control"></a>
<a id="keyleft_shift"></a>
<a id="keyleft_super"></a>
<a id="keym"></a>
<a id="keymenu"></a>
<a id="keyminus"></a>
<a id="keyn"></a>
<a id="keynum_0"></a>
<a id="keynum_1"></a>
<a id="keynum_2"></a>
<a id="keynum_3"></a>
<a id="keynum_4"></a>
<a id="keynum_5"></a>
<a id="keynum_6"></a>
<a id="keynum_7"></a>
<a id="keynum_8"></a>
<a id="keynum_9"></a>
<a id="keynum_lock"></a>
<a id="keyo"></a>
<a id="keyp"></a>
<a id="keypage_down"></a>
<a id="keypage_up"></a>
<a id="keypause"></a>
<a id="keyperiod"></a>
<a id="keyprint_screen"></a>
<a id="keyq"></a>
<a id="keyr"></a>
<a id="keyright"></a>
<a id="keyright_alt"></a>
<a id="keyright_bracket"></a>
<a id="keyright_control"></a>
<a id="keyright_shift"></a>
<a id="keyright_super"></a>
<a id="keys"></a>
<a id="keyscroll_lock"></a>
<a id="keysemicolon"></a>
<a id="keyslash"></a>
<a id="keyspace"></a>
<a id="keyt"></a>
<a id="keytab"></a>
<a id="keyu"></a>
<a id="keyunknown"></a>
<a id="keyup"></a>
<a id="keyv"></a>
<a id="keyw"></a>
<a id="keyworld_1"></a>
<a id="keyworld_2"></a>
<a id="keyx"></a>
<a id="keyy"></a>
<a id="keyz"></a>
<a id="other-declarations"></a>
<a id="parse_key"></a>
<a id="parse_key_modifier_flags"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
