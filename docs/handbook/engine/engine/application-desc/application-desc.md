---
kind: struct
---

[Engine](../../../engine.md) → [Engine](../../engine.md) → [application_desc.h](../application-desc.md)

# application_desc
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::engine` |
| Include File Path | `engine/include/conjure/engine/application_desc.h` |
| Source File Path | `engine/src/engine/application_desc.cpp` |
| API ↗ | [Engine: the application and the frame](../../../../api/engine/index.md), [Running the engine](../../../../api/engine/running-the-engine.md) |

## Syntax

```cpp
struct CONJURE_API application_desc
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">clock<wbr>Override</code></td><td><code class="signature"><a href="../../core/clock/clock.html">core::clock</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">config</code></td><td><code class="signature"><a href="../engine-config/engine-config.html">engine_config</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">project</code></td><td><code class="signature">std::optional&lt;<a href="../project/project.html">project</a>&gt;</code></td><td><code class="signature">std::nullopt</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">showHelp</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">unrecognised<wbr>Arguments</code></td><td><code class="signature">std::vector&lt;std::string&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `from_command_line` {#function-from-command-line}

<div class="function-declaration" id="function-from-command-line-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="application-desc.html">application_desc</a>&gt; from_command_line(
    std::span&lt;<span class="k">const</span> std::string_view&gt; arguments,
    <span class="k">const</span> <a href="../engine-config/engine-config.html">engine_config</a> &amp;baseConfig);</code></pre></div>

<p>Performs the whole configuration merge: <code>baseConfig</code>, <code>headless_defaults()</code> when --headless is given (before the project and the remaining flags), then <code>project.json</code> (when --project names a directory), then the command-line overrides, then <code>validate()</code>. Fails with <code>BAD_ARGUMENTS</code>, the project errors or <code>INVALID_CONFIG</code>; a desc with <code>showHelp</code> set is returned without loading anything. <code>A</code> real --rhi backend together with --headless logs a <code>WARN</code>: a headless run renders on the <code>null</code> device only, so the flag has no effect. --headless leaves <code>scripting.enabled</code> <code>false</code> whatever the project says; --scripting, applied with the remaining flags, sets it to <code>true</code>.</p>

#### `from_options` {#function-from-options}

<div class="function-declaration" id="function-from-options-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="application-desc.html">application_desc</a>&gt; from_options(
    <span class="k">const</span> <a href="../command-line/command-line-options.html">command_line_options</a> &amp;options,
    <span class="k">const</span> <a href="../engine-config/engine-config.html">engine_config</a> &amp;baseConfig);</code></pre></div>

<p>The same merge from already parsed options.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
