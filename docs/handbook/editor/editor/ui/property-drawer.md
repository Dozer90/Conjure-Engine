---
kind: header
---

[Editor](../../../editor.md) → [User Interface](../../user-interface.md)

# property_drawer.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/ui/property_drawer.h` |
| API ↗ | [The workspace UI](../../../../api/editor/workspace-ui.md) |

<p>Property drawers: one ImGui widget per reflection field kind. The inspector reads a component as JSON, hands each field&#x27;s value to the drawer for its kind and, when the widget reports a change, writes the new value back through <code>component.set</code>. Modules may replace the drawer of a kind for their own tooling.</p>

## Structs

| File | Description |
|---|---|
| [`ui::property_draw_context`](property-drawer/ui-property-draw-context.md) | What a drawer receives: the field description (attributes drive ranges, units and tooltips) and the value. |

## Classes

| File | Description |
|---|---|
| [`ui::property_drawer_registry`](property-drawer/ui-property-drawer-registry.md) |  |

## Type Aliases

### `ui::draw_fn` {#symbol-ui-draw-fn}

```cpp
using draw_fn = std::function<bool(const property_draw_context &context, core::json::document &value)>;
```

Draws one value. Returns <code>true</code> when the user changed it; <code>value</code> then holds the new JSON.

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
<a id="draw_fn"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="property_draw_context"></a>
<a id="property_draw_contextfield"></a>
<a id="property_draw_contextlabel"></a>
<a id="property_draw_contextreadonly"></a>
<a id="property_drawer_registry"></a>
<a id="property_drawer_registrydraw"></a>
<a id="property_drawer_registrydraw_json"></a>
<a id="property_drawer_registrydraw_object"></a>
<a id="property_drawer_registrydrawer"></a>
<a id="property_drawer_registryinstall_defaults"></a>
<a id="property_drawer_registrymdrawers"></a>
<a id="property_drawer_registrymfallback"></a>
<a id="property_drawer_registryoperator"></a>
<a id="property_drawer_registryproperty_drawer_registry"></a>
<a id="property_drawer_registryproperty_drawer_registry_1"></a>
<a id="property_drawer_registryset"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="uidraw_fn"></a>
<a id="uiproperty_draw_context"></a>
<a id="uiproperty_draw_contextfield"></a>
<a id="uiproperty_draw_contextlabel"></a>
<a id="uiproperty_draw_contextreadonly"></a>
<a id="uiproperty_drawer_registry"></a>
<a id="uiproperty_drawer_registrydraw"></a>
<a id="uiproperty_drawer_registrydraw_json"></a>
<a id="uiproperty_drawer_registrydraw_object"></a>
<a id="uiproperty_drawer_registrydrawer"></a>
<a id="uiproperty_drawer_registryinstall_defaults"></a>
<a id="uiproperty_drawer_registrymdrawers"></a>
<a id="uiproperty_drawer_registrymfallback"></a>
<a id="uiproperty_drawer_registryoperator"></a>
<a id="uiproperty_drawer_registryproperty_drawer_registry"></a>
<a id="uiproperty_drawer_registryproperty_drawer_registry_1"></a>
<a id="uiproperty_drawer_registryset"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
