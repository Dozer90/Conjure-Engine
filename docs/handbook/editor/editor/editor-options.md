---
kind: header
---

[Editor](../../editor.md) → [Applications](../applications.md)

# editor_options.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/editor_options.h` |
| API ↗ | [Headless mode and the command model](../../../api/editor/headless-and-commands.md), [The editor](../../../api/editor/index.md) |

<p>The <code>conjure_editor</code> command line: the editor&#x27;s own flags are parsed here, everything else (--rhi, --log-level, --frames, ...) is passed on to the engine&#x27;s parser through <code>engineArguments</code>. <code>A</code> bad editor flag is <code>INVALID_ARGS</code>; an option the engine does not know either, or a bad engine flag value, is refused by <code>editor_application::make_engine_desc</code> with <code>BAD_ARGUMENTS</code>. <code>main()</code> exits with 3 for both.</p>

## Structs

| File | Description |
|---|---|
| [`editor_options`](editor-options/editor-options.md) |  |

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-AI_PORT_FALLBACK_COUNT"></a><code>AI_PORT_FALLBACK_COUNT</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../../engine/core/types.html#symbol-u16">u16</a></code> | <code class="signature"><span class="mi">9</span></code> | Ports tried after a busy default port. |
| <a id="variable-DEFAULT_AI_PORT"></a><code>DEFAULT_AI_PORT</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../../engine/core/types.html#symbol-u16">u16</a></code> | <code class="signature"><span class="mi">47820</span></code> | The default <code>TCP</code> port of the AI control protocol; nine more are tried when it is busy and the port was not given explicitly. |

## Functions

### Public

#### `editor_options_help` {#function-editor-options-help}

<div class="function-declaration" id="function-editor-options-help-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string editor_options_help();</code></pre></div>

<p>The usage text printed for --help and after a bad flag: the editor&#x27;s own options followed by the engine options it hands on (<code>engine::command_line_help()</code> without the lines of --project, --headless, --no-scripting and --help, which the editor&#x27;s own lines describe, and without the engine&#x27;s note on unknown options).</p>

#### `parse_editor_options` {#function-parse-editor-options}

<div class="function-declaration" id="function-parse-editor-options-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../../engine/core/result/result.html">core::result</a>&lt;<a href="editor-options/editor-options.html">editor_options</a>&gt; parse_editor_options(
    std::span&lt;<span class="k">const</span> std::string_view&gt; args);</code></pre></div>

<p>Parses argv[1..]. <code>INVALID_ARGS</code> for a flag without its value, a value that does not parse (port above 65535, a timeout that is not a positive finite number, an empty --ai-token, --ai-inspect-token or --trust-store), --ai-token and --ai-inspect-token naming the same token, --exec together with --ai-stdio, --out without --exec, --offscreen without --headless, --export or --trust-project without --project, or --distribution without --export. Every other token is kept in <code>engineArguments</code> for the engine&#x27;s parser, and <code>editor_application::make_engine_desc</code> refuses what that parser does not know either.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="ai_port_fallback_count"></a>
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="default_ai_port"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="editor_options"></a>
<a id="editor_options_help"></a>
<a id="editor_optionsaiinspecttoken"></a>
<a id="editor_optionsaiport"></a>
<a id="editor_optionsaistdio"></a>
<a id="editor_optionsaitoken"></a>
<a id="editor_optionsdotnethost"></a>
<a id="editor_optionsenginearguments"></a>
<a id="editor_optionsexecfile"></a>
<a id="editor_optionsexportdirectory"></a>
<a id="editor_optionsexportdistribution"></a>
<a id="editor_optionsfirst_port"></a>
<a id="editor_optionsheadless"></a>
<a id="editor_optionskeepgoing"></a>
<a id="editor_optionsnoscripting"></a>
<a id="editor_optionsoffscreen"></a>
<a id="editor_optionsoperator"></a>
<a id="editor_optionsoutputfile"></a>
<a id="editor_optionsport_is_explicit"></a>
<a id="editor_optionsprojectpath"></a>
<a id="editor_optionsshowhelp"></a>
<a id="editor_optionsstdout_carries_protocol"></a>
<a id="editor_optionstimeoutseconds"></a>
<a id="editor_optionstrustproject"></a>
<a id="editor_optionstruststorefile"></a>
<a id="editor_optionswants_tcp_server"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="parse_editor_options"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
