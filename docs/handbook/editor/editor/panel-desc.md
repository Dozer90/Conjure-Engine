---
kind: header
---

[Editor](../../editor.md) → [User Interface](../user-interface.md)

# panel_desc.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/panel_desc.h` |
| API ↗ | [Headless mode and the command model](../../../api/editor/headless-and-commands.md), [The editor](../../../api/editor/index.md) |

<p>Module-provided panels: a trivially copyable extension record. The editor&#x27;s panel registry wraps the draw function into an <code><a href="ui/editor-panel/ui-editor-panel.html">editor_panel</a></code> drawn inside the workspace with the one ImGui context the engine exports; the runtime ignores the record.</p>

## Structs

| File | Description |
|---|---|
| [`panel_desc`](panel-desc/panel-desc.md) |  |

## Type Aliases

### `panel_draw_fn` {#symbol-panel-draw-fn}

```cpp
using panel_draw_fn = void (*)(void *imguiContext, void *userData);
```

Draws the panel body. Called every frame the panel is open, between <code>ImGui::Begin</code> and <code>ImGui::End</code> of the panel window, with the current ImGui context as argument (<code>ImGui::GetCurrentContext()</code>).

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
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="panel_desc"></a>
<a id="panel_descdraw"></a>
<a id="panel_descextension_point"></a>
<a id="panel_descopenbydefault"></a>
<a id="panel_desctitle"></a>
<a id="panel_descuserdata"></a>
<a id="panel_draw_fn"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
