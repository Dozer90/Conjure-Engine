---
kind: header
---

[Engine](../../engine.md) → [Engine](../engine.md)

# layer_stack.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/engine/layer_stack.h` |
| API ↗ | [The frame, layers and time](../../../api/engine/frame-layers-and-time.md), [Engine: the application and the frame](../../../api/engine/index.md) |

<p>The layer stack: owns the layers below <code>mOverlayBegin</code> and the overlays above it. While the stack is attached (after <code>attach_all</code>) or iterating, pushes and pops are deferred to <code>apply_pending()</code>, which the application calls at frame step 13; before <code>attach_all</code> they take effect at once so executables build the stack before <code>initialise()</code>.</p>



## Classes

| File | Description |
|---|---|
| [`layer_stack`](layer-stack/layer-stack.md) |  |

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
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="layer_stack"></a>
<a id="layer_stackapply_pending"></a>
<a id="layer_stackattach_all"></a>
<a id="layer_stackdetach_all"></a>
<a id="layer_stackdispatch_event"></a>
<a id="layer_stackerase"></a>
<a id="layer_stackfor_each_bottom_up"></a>
<a id="layer_stackforget_pending_push"></a>
<a id="layer_stackframe_begin"></a>
<a id="layer_stackframe_end"></a>
<a id="layer_stackimgui"></a>
<a id="layer_stackimgui_end"></a>
<a id="layer_stackinsert"></a>
<a id="layer_stackis_attached"></a>
<a id="layer_stacklayer_stack"></a>
<a id="layer_stacklayer_stack_1"></a>
<a id="layer_stacklayers"></a>
<a id="layer_stackmattached"></a>
<a id="layer_stackmiterating"></a>
<a id="layer_stackmlayers"></a>
<a id="layer_stackmoverlaybegin"></a>
<a id="layer_stackmpendingpop"></a>
<a id="layer_stackmpendingpush"></a>
<a id="layer_stackmpendingpushisoverlay"></a>
<a id="layer_stackoperator"></a>
<a id="layer_stackoverlay_count"></a>
<a id="layer_stackpending_count"></a>
<a id="layer_stackpop"></a>
<a id="layer_stackpush"></a>
<a id="layer_stackpush_layer"></a>
<a id="layer_stackpush_overlay"></a>
<a id="layer_stackupdate"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
