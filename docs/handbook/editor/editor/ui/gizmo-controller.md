---
kind: header
---

[Editor](../../../editor.md) → [User Interface](../../user-interface.md)

# gizmo_controller.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/ui/gizmo_controller.h` |
| API ↗ | [The workspace UI](../../../../api/editor/workspace-ui.md) |

<p>Transform gizmos over ImGuizmo: the first manipulated frame opens a transaction and caches the selected entities&#x27; world matrices; every frame with a delta dispatches <code>component.set_many</code> on the local transforms (through the parents&#x27; inverse world matrices, pivoting at the centroid) with the drag session as coalesce key; the release ends the transaction. Descendants of selected entities follow their ancestor instead of receiving the delta themselves.</p>

## Structs

| File | Description |
|---|---|
| [`ui::gizmo_controller::dragged_entity`](gizmo-controller/ui-gizmo-controller-dragged-entity.md) |  |

## Classes

| File | Description |
|---|---|
| [`ui::gizmo_controller`](gizmo-controller/ui-gizmo-controller.md) |  |

## Enumerations

### `ui::gizmo_operation` {#symbol-ui-gizmo-operation}

```cpp
enum class gizmo_operation : u8 {
    TRANSLATE = 0,
    ROTATE,
    SCALE,
    UNIVERSAL
};
```

### `ui::gizmo_space` {#symbol-ui-gizmo-space}

```cpp
enum class gizmo_space : u8 {
    LOCAL = 0,
    WORLD
};
```

## Functions

### Public

#### `ui::to_string` {#function-ui-to-string}

<div class="function-declaration" id="function-ui-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(<a href="gizmo-controller.html#symbol-ui-gizmo-operation">gizmo_operation</a> operation) <span class="k">noexcept</span>;</code></pre></div>

<p>The wire name of an operation (&quot;translate&quot;).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="gizmo_controller"></a>
<a id="gizmo_controllerapply_delta"></a>
<a id="gizmo_controllerbegin_drag"></a>
<a id="gizmo_controllerdragged_entity"></a>
<a id="gizmo_controllerdragged_entityguid"></a>
<a id="gizmo_controllerdragged_entityworldmatrix"></a>
<a id="gizmo_controllerdraw"></a>
<a id="gizmo_controllerend_drag"></a>
<a id="gizmo_controllergather_selection"></a>
<a id="gizmo_controllergizmo_controller"></a>
<a id="gizmo_controllergizmo_controller_1"></a>
<a id="gizmo_controlleris_dragging"></a>
<a id="gizmo_controlleris_over"></a>
<a id="gizmo_controllermanipulation_count"></a>
<a id="gizmo_controllermdragged"></a>
<a id="gizmo_controllermdragging"></a>
<a id="gizmo_controllermdragsession"></a>
<a id="gizmo_controllermmanipulations"></a>
<a id="gizmo_controllermoperation"></a>
<a id="gizmo_controllermoverhandle"></a>
<a id="gizmo_controllermpivot"></a>
<a id="gizmo_controllermsnaprotatedegrees"></a>
<a id="gizmo_controllermsnapscale"></a>
<a id="gizmo_controllermsnaptranslate"></a>
<a id="gizmo_controllermspace"></a>
<a id="gizmo_controllermusesnap"></a>
<a id="gizmo_controlleroperation"></a>
<a id="gizmo_controlleroperator"></a>
<a id="gizmo_controllerset_operation"></a>
<a id="gizmo_controllerset_snap"></a>
<a id="gizmo_controllerset_space"></a>
<a id="gizmo_controllersnap_enabled"></a>
<a id="gizmo_controllersnap_rotate_degrees"></a>
<a id="gizmo_controllersnap_scale"></a>
<a id="gizmo_controllersnap_translate"></a>
<a id="gizmo_controllerspace"></a>
<a id="gizmo_operation"></a>
<a id="gizmo_operationrotate"></a>
<a id="gizmo_operationscale"></a>
<a id="gizmo_operationtranslate"></a>
<a id="gizmo_operationuniversal"></a>
<a id="gizmo_space"></a>
<a id="gizmo_spacelocal"></a>
<a id="gizmo_spaceworld"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="uigizmo_controller"></a>
<a id="uigizmo_controllerapply_delta"></a>
<a id="uigizmo_controllerbegin_drag"></a>
<a id="uigizmo_controllerdragged_entity"></a>
<a id="uigizmo_controllerdragged_entityguid"></a>
<a id="uigizmo_controllerdragged_entityworldmatrix"></a>
<a id="uigizmo_controllerdraw"></a>
<a id="uigizmo_controllerend_drag"></a>
<a id="uigizmo_controllergather_selection"></a>
<a id="uigizmo_controllergizmo_controller"></a>
<a id="uigizmo_controllergizmo_controller_1"></a>
<a id="uigizmo_controlleris_dragging"></a>
<a id="uigizmo_controlleris_over"></a>
<a id="uigizmo_controllermanipulation_count"></a>
<a id="uigizmo_controllermdragged"></a>
<a id="uigizmo_controllermdragging"></a>
<a id="uigizmo_controllermdragsession"></a>
<a id="uigizmo_controllermmanipulations"></a>
<a id="uigizmo_controllermoperation"></a>
<a id="uigizmo_controllermoverhandle"></a>
<a id="uigizmo_controllermpivot"></a>
<a id="uigizmo_controllermsnaprotatedegrees"></a>
<a id="uigizmo_controllermsnapscale"></a>
<a id="uigizmo_controllermsnaptranslate"></a>
<a id="uigizmo_controllermspace"></a>
<a id="uigizmo_controllermusesnap"></a>
<a id="uigizmo_controlleroperation"></a>
<a id="uigizmo_controlleroperator"></a>
<a id="uigizmo_controllerset_operation"></a>
<a id="uigizmo_controllerset_snap"></a>
<a id="uigizmo_controllerset_space"></a>
<a id="uigizmo_controllersnap_enabled"></a>
<a id="uigizmo_controllersnap_rotate_degrees"></a>
<a id="uigizmo_controllersnap_scale"></a>
<a id="uigizmo_controllersnap_translate"></a>
<a id="uigizmo_controllerspace"></a>
<a id="uigizmo_operation"></a>
<a id="uigizmo_operationrotate"></a>
<a id="uigizmo_operationscale"></a>
<a id="uigizmo_operationtranslate"></a>
<a id="uigizmo_operationuniversal"></a>
<a id="uigizmo_space"></a>
<a id="uigizmo_spacelocal"></a>
<a id="uigizmo_spaceworld"></a>
<a id="uito_string"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
