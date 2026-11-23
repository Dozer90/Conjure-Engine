---
kind: header
---

[Engine](../../engine.md) → [Physics](../physics.md)

# contact_events.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/physics/contact_events.h` |
| API ↗ | [Physics](../../../api/physics/index.md), [The physics world and queries](../../../api/physics/physics-world-and-queries.md) |

<p>Contact events. <code>physics_world::contact_events()</code> returns the events of the last step sorted by ascending (bodyIdA, bodyIdB), then kind: per pair per step at most one <code>ENTER</code> / <code>STAY</code> (or <code>TRIGGER_ENTER</code>) and at most one <code>EXIT</code> (or <code>TRIGGER_EXIT</code>), the exit after the enter. Consumers reporting to b flip the normal. Destroying a body in contact synthesises its <code>EXIT</code> / <code>TRIGGER_EXIT</code>, reported by the next step and naming the body&#x27;s entity. When the body went because that entity was destroyed, the entity is no longer alive: check <code>world::is_alive</code> before touching a participant. The entity&#x27;s generation keeps the dead id from matching a reused slot, so no later event references it.</p>

## Structs

| File | Description |
|---|---|
| [`contact_event`](contact-events/contact-event.md) | One contact event. 48 bytes, trivially copyable. |

## Enumerations

### `contact_kind` {#symbol-contact-kind}

```cpp
enum class contact_kind : u8 {
    ENTER,
    STAY,
    EXIT,
    TRIGGER_ENTER,
    TRIGGER_EXIT
};
```

What happened to a body pair in the step. Trigger pairs report only the <code>TRIGGER_</code> kinds.

## Functions

### Public

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="contact-events.html#symbol-contact-kind">contact_kind</a> kind) <span class="k">noexcept</span>;</code></pre></div>

<p>The upper-case enumerator name of a kind (&quot;<code>ENTER</code>&quot;); &quot;<code>UNKNOWN</code>&quot; for out-of-range values.</p>

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
<a id="contact_event"></a>
<a id="contact_eventa"></a>
<a id="contact_eventb"></a>
<a id="contact_eventcontactcount"></a>
<a id="contact_eventimpulse"></a>
<a id="contact_eventkind"></a>
<a id="contact_eventlayera"></a>
<a id="contact_eventlayerb"></a>
<a id="contact_eventnormal"></a>
<a id="contact_eventoperator"></a>
<a id="contact_eventpoint"></a>
<a id="contact_kind"></a>
<a id="contact_kindenter"></a>
<a id="contact_kindexit"></a>
<a id="contact_kindstay"></a>
<a id="contact_kindtrigger_enter"></a>
<a id="contact_kindtrigger_exit"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
