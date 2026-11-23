---
kind: header
---

[Editor](../../editor.md) → [Commands and Undo](../commands-and-undo.md)

# builtin_commands.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/builtin_commands.h` |
| API ↗ | [Headless mode and the command model](../../../api/editor/headless-and-commands.md), [The editor](../../../api/editor/index.md) |

<p>The built-in command catalogue: every command of the session, editor, history, console, project, scene, entity, component, asset, prefab, script, play, input, view, physics, selection, reflection, module and profiler groups. Implementations live in engine/src/editor/core/commands/<code>commands_&lt;group&gt;</code>.cpp, one free function per command plus its schema.</p>

## Functions

### Public

#### `builtin_command_count` {#function-builtin-command-count}

<div class="function-declaration" id="function-builtin-command-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../../engine/core/types.html#symbol-usize">usize</a> builtin_command_count() <span class="k">noexcept</span>;</code></pre></div>

<p>Number of commands <code>register_builtin_commands</code> adds.</p>

#### `register_builtin_commands` {#function-register-builtin-commands}

<div class="function-declaration" id="function-register-builtin-commands-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> register_builtin_commands(
    <a href="command-registry/command-registry.html">command_registry</a> &amp;registry);</code></pre></div>

<p>Registers every built-in command. Registering twice is a programmer error (asserted; the duplicates are refused by the registry).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="builtin_command_count"></a>
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="register_builtin_commands"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
