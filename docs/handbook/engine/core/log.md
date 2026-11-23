---
kind: header
---

[Engine](../../engine.md) → [Core](../core.md)

# log.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/log.h` |
| API ↗ | [Errors and results](../../../api/core/errors-and-results.md), [Logging and assertions](../../../api/core/logging-and-assertions.md) |

<p>Logging.</p>

## Structs

| File | Description |
|---|---|
| [`log_message`](log/log-message.md) | <code>A</code> formatted message delivered to sinks. Views are valid only during the write call. |

## Classes

| File | Description |
|---|---|
| [`callback_log_sink`](log/callback-log-sink.md) | Forwards every message to a callable. Used by the editor console and by tests. The callable is invoked from the logging thread and must be thread-safe. |
| [`console_log_sink`](log/console-log-sink.md) | Writes to stdout (below <code>WARN</code>) and stderr (<code>WARN</code> and above) with optional ANSI colours. |
| [`file_log_sink`](log/file-log-sink.md) | Appends to a file. The file is created (with parent directories) on construction. |
| [`log_category`](log/log-category.md) | <code>A</code> named channel with its own minimum level. Instances are global and register themselves so tools can list and adjust categories at runtime. |
| [`log_sink`](log/log-sink.md) | Destination for log messages. The logger calls <code>write()</code> from any thread, possibly concurrently, and without holding its own lock, so a sink must synchronise itself and may safely log or add/remove sinks from inside <code>write()</code>. |

## Enumerations

### `log_level` {#symbol-log-level}

```cpp
enum class log_level : u8 {
    TRACE = 0,
    DEBUG,
    INFO,
    WARN,
    ERR,        // "ERROR" is a Windows macro (wingdi.h); ERR keeps the enum usable everywhere
    FATAL,
    OFF
};
```

## Forward Declarations

### `file_log_sink::state` {#symbol-file-log-sink-state}

```cpp
struct state;
```

## Variables

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-LOG_CORE"></a><code>LOG_CORE</code> | <code class="signature"><span class="k">extern</span> <span class="na">CONJURE_<wbr>API</span> <a href="log/log-category.html">log_<wbr>category</a></code> | — | Category used by core itself and by code that has no better home. |

## Functions

### Public

#### `format_log_line` {#function-format-log-line}

<div class="function-declaration" id="function-format-log-line-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string format_log_line(
    <span class="k">const</span> <a href="log/log-message.html">log_message</a> &amp;message);</code></pre></div>

<p>Formats a message the way the console and file sinks do: &quot;HH:MM:<code>SS.mmm</code> [LEVEL] [category] text&quot;.</p>

#### `log::add_sink` {#function-log-add-sink}

<div class="function-declaration" id="function-log-add-sink-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> add_sink(std::shared_ptr&lt;<a href="log/log-sink.html">log_sink</a>&gt; sink);</code></pre></div>

<p>Adds a sink. Sinks are shared so the caller may keep a reference for later removal.</p>

#### `log::categories` {#function-log-categories}

<div class="function-declaration" id="function-log-categories-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::vector&lt;<a href="log/log-category.html">log_category</a> *&gt; categories();</code></pre></div>

<p>Every registered category, in registration order.</p>

#### `log::clear_sinks` {#function-log-clear-sinks}

<div class="function-declaration" id="function-log-clear-sinks-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> clear_sinks();</code></pre></div>

<p>Removes every sink.</p>

#### `log::console_enabled` {#function-log-console-enabled}

<div class="function-declaration" id="function-log-console-enabled-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">bool</span> console_enabled() <span class="k">noexcept</span>;</code></pre></div>

#### `log::find_category` {#function-log-find-category}

<div class="function-declaration" id="function-log-find-category-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="log/log-category.html">log_category</a> *find_category(std::string_view name);</code></pre></div>

<p>Finds a category by name, or <code>nullptr</code>.</p>

#### `log::flush` {#function-log-flush}

<div class="function-declaration" id="function-log-flush-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> flush();</code></pre></div>

<p>Flushes every sink.</p>

#### `log::global_level` {#function-log-global-level}

<div class="function-declaration" id="function-log-global-level-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="log.html#symbol-log-level">log_level</a> global_level() <span class="k">noexcept</span>;</code></pre></div>

#### `log::is_enabled` {#function-log-is-enabled}

<div class="function-declaration" id="function-log-is-enabled-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">bool</span> is_enabled(<span class="k">const</span> <a href="log/log-category.html">log_category</a> &amp;category, <a href="log.html#symbol-log-level">log_level</a> level) <span class="k">noexcept</span>;</code></pre></div>

<p>True when a message at <code>level</code> for <code>category</code> would reach the sinks.</p>

#### `log::remove_sink` {#function-log-remove-sink}

<div class="function-declaration" id="function-log-remove-sink-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> remove_sink(<span class="k">const</span> std::shared_ptr&lt;<a href="log/log-sink.html">log_sink</a>&gt; &amp;sink);</code></pre></div>

<p>Removes a previously added sink. Unknown sinks are ignored.</p>

#### `log::set_category_level` {#function-log-set-category-level}

<div class="function-declaration" id="function-log-set-category-level-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">bool</span> set_category_level(std::string_view name, <a href="log.html#symbol-log-level">log_level</a> level);</code></pre></div>

<p>Sets a category&#x27;s level by name. Returns <code>false</code> when the category does not exist.</p>

#### `log::set_console_enabled` {#function-log-set-console-enabled}

<div class="function-declaration" id="function-log-set-console-enabled-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> set_console_enabled(<span class="k">bool</span> enabled);</code></pre></div>

<p>The console sink that exists until disabled. On by default so early messages are never lost; tests turn it off.</p>

#### `log::set_global_level` {#function-log-set-global-level}

<div class="function-declaration" id="function-log-set-global-level-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> set_global_level(<a href="log.html#symbol-log-level">log_level</a> level) <span class="k">noexcept</span>;</code></pre></div>

<p>Messages below this level are dropped regardless of category.</p>

#### `log::write` {#function-log-write}

<div class="function-declaration" id="function-log-write-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> write( <span class="k">const</span> <a href="log/log-category.html">log_category</a> &amp;category, <a href="log.html#symbol-log-level">log_level</a> level, <span class="k">const</span> <span class="k">char</span> *file, <span class="k">int</span> line, std::string_view text );</code></pre></div>

<p>Delivers an already formatted message to every sink.</p>

#### `parse_log_level` {#function-parse-log-level}

<div class="function-declaration" id="function-parse-log-level-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::optional&lt;<a href="log.html#symbol-log-level">log_level</a>&gt; parse_log_level(
    std::string_view name) <span class="k">noexcept</span>;</code></pre></div>

<p>Parses a level name case-insensitively (&quot;warn&quot;, &quot;ERROR&quot;, &quot;Off&quot;). The names are the ones <code>to_string</code> returns, so <code>OFF</code> parses to <code>log_level::OFF</code>. An unknown or empty name returns <code>std::nullopt</code>.</p>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="log.html#symbol-log-level">log_level</a> level) <span class="k">noexcept</span>;</code></pre></div>

<p>Returns the upper-case name of a level (&quot;<code>INFO</code>&quot;).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="add_sink"></a>
<a id="callback_log_sink"></a>
<a id="callback_log_sinkcallback_log_sink"></a>
<a id="callback_log_sinkcallback_log_sink_1"></a>
<a id="callback_log_sinkvoid"></a>
<a id="callback_log_sinkwrite"></a>
<a id="categories"></a>
<a id="clear_sinks"></a>
<a id="console_enabled"></a>
<a id="console_log_sink"></a>
<a id="console_log_sinkconsole_log_sink"></a>
<a id="console_log_sinkconsole_log_sink_1"></a>
<a id="console_log_sinkflush"></a>
<a id="console_log_sinkmusecolour"></a>
<a id="console_log_sinkwrite"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="file_log_sink"></a>
<a id="file_log_sinkfile_log_sink"></a>
<a id="file_log_sinkfile_log_sink_1"></a>
<a id="file_log_sinkflush"></a>
<a id="file_log_sinkis_open"></a>
<a id="file_log_sinkmstate"></a>
<a id="file_log_sinkstate"></a>
<a id="file_log_sinkwrite"></a>
<a id="find_category"></a>
<a id="flush"></a>
<a id="format_log_line"></a>
<a id="global_level"></a>
<a id="internals-and-algorithms"></a>
<a id="is_enabled"></a>
<a id="log_category"></a>
<a id="log_categoryis_enabled"></a>
<a id="log_categorylevel"></a>
<a id="log_categorylog_category"></a>
<a id="log_categorylog_category_1"></a>
<a id="log_categorymlevel"></a>
<a id="log_categorymname"></a>
<a id="log_categoryname"></a>
<a id="log_categoryoperator"></a>
<a id="log_categoryset_level"></a>
<a id="log_core"></a>
<a id="log_level"></a>
<a id="log_leveldebug"></a>
<a id="log_levelerr"></a>
<a id="log_levelfatal"></a>
<a id="log_levelinfo"></a>
<a id="log_leveloff"></a>
<a id="log_leveltrace"></a>
<a id="log_levelwarn"></a>
<a id="log_message"></a>
<a id="log_messagecategory"></a>
<a id="log_messagefile"></a>
<a id="log_messagelevel"></a>
<a id="log_messageline"></a>
<a id="log_messagetext"></a>
<a id="log_messagethreadid"></a>
<a id="log_messagetimestamp"></a>
<a id="log_sink"></a>
<a id="log_sinkflush"></a>
<a id="log_sinklog_sink"></a>
<a id="log_sinklog_sink_1"></a>
<a id="log_sinkoperator"></a>
<a id="log_sinkwrite"></a>
<a id="logadd_sink"></a>
<a id="logcategories"></a>
<a id="logclear_sinks"></a>
<a id="logconsole_enabled"></a>
<a id="logfind_category"></a>
<a id="logflush"></a>
<a id="logglobal_level"></a>
<a id="logis_enabled"></a>
<a id="logremove_sink"></a>
<a id="logset_category_level"></a>
<a id="logset_console_enabled"></a>
<a id="logset_global_level"></a>
<a id="logwrite"></a>
<a id="other-declarations"></a>
<a id="parse_log_level"></a>
<a id="remove_sink"></a>
<a id="set_category_level"></a>
<a id="set_console_enabled"></a>
<a id="set_global_level"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="variables-and-properties"></a>
<a id="write"></a>
<!-- doc-legacy-anchors:end -->
