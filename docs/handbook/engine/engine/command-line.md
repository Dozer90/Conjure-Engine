---
kind: header
---

[Engine](../../engine.md) → [Engine](../engine.md)

# command_line.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/engine/command_line.h` |
| API ↗ | [Engine: the application and the frame](../../../api/engine/index.md), [Running the engine](../../../api/engine/running-the-engine.md) |

<p>The command line: <code>parse_command_line()</code> turns the arguments into a set of optional overrides, <code>apply_command_line()</code> lays them over an <code><a href="engine-config/engine-config.html">engine_config</a></code>, and <code>application_desc::from_command_line()</code> orders the whole merge (defaults &lt; --headless defaults &lt; <code>project.json</code> &lt; the remaining flags).</p>

## Structs

| File | Description |
|---|---|
| [`command_line_options`](command-line/command-line-options.md) |  |

## Functions

### Public

#### `apply_command_line` {#function-apply-command-line}

<div class="function-declaration" id="function-apply-command-line-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> apply_command_line(
    <span class="k">const</span> <a href="command-line/command-line-options.html">command_line_options</a> &amp;options,
    <a href="engine-config/engine-config.html">engine_config</a> &amp;config);</code></pre></div>

<p>Applies every override except <code>headless</code> and <code>projectPath</code> (which <code>from_command_line</code> handles before the project is loaded): --scene, --frames, backends, --scripting, the --no-* switches, window, time, pacing, jobs, the GPU validation level and log settings. --rhi <code>null</code> leaves rendering enabled on the <code>null</code> device; --audio off disables audio. <code>scripting</code> sets <code>scripting.enabled</code>, <code>noScripting</code> clears it and is applied second, so options filled by hand with both (<code>parse_command_line</code> refuses the pair) end with scripting off.</p>

#### `command_line_help` {#function-command-line-help}

<div class="function-declaration" id="function-command-line-help-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view command_line_help() <span class="k">noexcept</span>;</code></pre></div>

<p>The usage text printed for --help.</p>

#### `parse_command_line` {#function-parse-command-line}

<div class="function-declaration" id="function-parse-command-line-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<a href="command-line/command-line-options.html">command_line_options</a>&gt; parse_command_line(
    std::span&lt;<span class="k">const</span> std::string_view&gt; arguments);</code></pre></div>

<p>Parses the arguments (argv[1..], not the program name). Fails with <code>BAD_ARGUMENTS</code> for a flag without its value (a value token that starts with -- counts as a missing value), a value that does not parse (a non-number, a zero where a positive count is needed, an unknown backend, validation level or log level), and for --scripting together with --no-scripting in either order. When --audio or --gpu-validation is given several times the last one wins. Unknown tokens, and the values that follow one until the next known flag, land in <code>unrecognisedArguments</code> in order.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="apply_command_line"></a>
<a id="classes"></a>
<a id="command_line_help"></a>
<a id="command_line_options"></a>
<a id="command_line_optionsaudio"></a>
<a id="command_line_optionsaudiooff"></a>
<a id="command_line_optionsfixedframedelta"></a>
<a id="command_line_optionsfixedhz"></a>
<a id="command_line_optionsframes"></a>
<a id="command_line_optionsfullscreen"></a>
<a id="command_line_optionsheadless"></a>
<a id="command_line_optionsheight"></a>
<a id="command_line_optionshelp"></a>
<a id="command_line_optionsjobs"></a>
<a id="command_line_optionslogfile"></a>
<a id="command_line_optionsloglevel"></a>
<a id="command_line_optionsnophysics"></a>
<a id="command_line_optionsnorender"></a>
<a id="command_line_optionsnoscripting"></a>
<a id="command_line_optionsnovsync"></a>
<a id="command_line_optionsprojectpath"></a>
<a id="command_line_optionsrhi"></a>
<a id="command_line_optionsscene"></a>
<a id="command_line_optionsscripting"></a>
<a id="command_line_optionstargethz"></a>
<a id="command_line_optionstimescale"></a>
<a id="command_line_optionsunrecognisedarguments"></a>
<a id="command_line_optionswidth"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="parse_command_line"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
