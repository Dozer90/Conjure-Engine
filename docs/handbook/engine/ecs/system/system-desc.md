---
kind: struct
---

[Engine](../../../engine.md) → [ECS](../../ecs.md) → [system.h](../system.md)

# system_desc
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::ecs` |
| Include File Path | `engine/include/conjure/ecs/system.h` |
| Source File Path | `engine/src/ecs/system.cpp` |
| API ↗ | [Command buffers and observers](../../../../api/ecs/command-buffers-and-observers.md), [Queries, views and systems](../../../../api/ecs/queries-views-and-systems.md) |

## Syntax

```cpp
struct system_desc
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">after</code></td><td><code class="signature"><a href="../../core/containers/small-vector/small-vector.html">core::<wbr>small_<wbr>vector</a>&lt;<wbr>core::<wbr>string_<wbr>id,<wbr> <span class="mi">4</span>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">before</code></td><td><code class="signature"><a href="../../core/containers/small-vector/small-vector.html">core::<wbr>small_<wbr>vector</a>&lt;<wbr>core::<wbr>string_<wbr>id,<wbr> <span class="mi">4</span>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">flags</code></td><td><code class="signature"><a href="../system.html#symbol-system-flags">system_flags</a></code></td><td><code class="signature">system_<wbr>flags::<wbr>NONE</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">name</code></td><td><code class="signature"><a href="../../core/string-id/string-id.html">core::string_id</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">phase</code></td><td><code class="signature"><a href="../system-phase.html#symbol-system-phase">system_phase</a></code></td><td><code class="signature">system_<wbr>phase::<wbr>UPDATE</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">reads</code></td><td><code class="signature"><a href="../component/component-mask.html">component_mask</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">writes</code></td><td><code class="signature"><a href="../component/component-mask.html">component_mask</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `void` {#function-void}

<div class="function-declaration" id="function-void-1"><pre><code>std::function&lt;<span class="k">void</span>(
    <a href="system-context.html">system_context</a> &amp;context)&gt; run =;</code></pre></div>

<div class="function-declaration" id="function-void-2"><pre><code>std::function&lt;<span class="k">void</span>(
    <span class="k">const</span> <a href="../component-registry/component-registry.html">component_registry</a> &amp;registry,
    <a href="../component/component-mask.html">component_mask</a> &amp;reads,
    <a href="../component/component-mask.html">component_mask</a> &amp;writes)&gt; resolveAccess =;</code></pre></div>

<p>Optional: resolves component types into reads/writes once the registry is known (<code>make_system</code> fills this so a <code><a href="system-desc.html">system_desc</a></code> needs no registry at construction). Called by <code>scheduler::add_system</code> and merged into reads/writes, and called again, on the owner thread, whenever the registry has gained component types since: a type or name the registry did not know yet is left out and joins the access set once it is registered. The hook therefore only ever sets bits, gives the same answer for the same registry, and owns whatever it captures for as long as the system is registered.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
