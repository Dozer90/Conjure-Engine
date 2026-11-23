---
kind: struct
---

[Editor](../../../editor.md) → [Applications](../../applications.md) → [editor_options.h](../editor-options.md)

# editor_options
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/editor_options.h` |
| Source File Path | `engine/src/editor/core/editor_options.cpp` |
| API ↗ | [Headless mode and the command model](../../../../api/editor/headless-and-commands.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
struct CONJURE_API editor_options
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">ai<wbr>Inspect<wbr>Token</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">aiPort</code></td><td><code class="signature">std::optional&lt;<a href="../../../engine/core/types.html#symbol-u16">u16</a>&gt;</code></td><td><code class="signature">std::nullopt</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">aiStdio</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">aiToken</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">dotnetHost</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">engine<wbr>Arguments</code></td><td><code class="signature">std::vector&lt;std::string&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">execFile</code></td><td><code class="signature">std::filesystem::path</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">export<wbr>Directory</code></td><td><code class="signature">std::filesystem::path</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">export<wbr>Distribution</code></td><td><code class="signature">std::filesystem::path</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">headless</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">keepGoing</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">noScripting</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">offscreen</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">outputFile</code></td><td><code class="signature">std::filesystem::path</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">projectPath</code></td><td><code class="signature">std::filesystem::path</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">showHelp</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">timeout<wbr>Seconds</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-f64">f64</a></code></td><td><code class="signature"><span class="mi">120.0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">trustProject</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">trust<wbr>Store<wbr>File</code></td><td><code class="signature">std::filesystem::path</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `first_port` {#function-first-port}

<div class="function-declaration" id="function-first-port-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-u16">u16</a> first_port() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The port to bind first: the explicit one or <code>DEFAULT_AI_PORT</code>.</p>

#### `port_is_explicit` {#function-port-is-explicit}

<div class="function-declaration" id="function-port-is-explicit-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> port_is_explicit() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the port was given on the command line (no fallback ports are tried).</p>

#### `stdout_carries_protocol` {#function-stdout-carries-protocol}

<div class="function-declaration" id="function-stdout-carries-protocol-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> stdout_carries_protocol() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when standard output carries protocol data: the responses and events of --ai-stdio, the responses of --exec without --out, or the <code>project.export</code> response of --export. Logging must then go to standard error, never standard output; <code>conjure_editor</code>&#x27;s main does that.</p>

#### `wants_tcp_server` {#function-wants-tcp-server}

<div class="function-declaration" id="function-wants-tcp-server-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> wants_tcp_server() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when a <code>TCP</code> server should start: explicit non-zero --ai-port, or no --ai-port in a windowed session. <code>A</code> headless --exec run starts no server unless a port was given.</p>

## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="editor-options.html">editor_options</a> &amp;) <span class="k">const</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
