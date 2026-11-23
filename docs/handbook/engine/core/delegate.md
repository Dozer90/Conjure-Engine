---
kind: header
---

[Engine](../../engine.md) → [Core](../core.md)

# delegate.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/delegate.h` |
| API ↗ | [Time, events and random](../../../api/core/time-and-events.md) |

<p>Single-target callbacks without allocation.</p>



## Classes

| File | Description |
|---|---|
| [`delegate<Return(Args...)>`](delegate/delegate-return-args.md) |  |

## Type Aliases

### `delegate<Return(Args...)>::function_pointer` {#symbol-delegate-return-args-function-pointer}

```cpp
using function_pointer = Return (*)(Args...);
```

### `delegate<Return(Args...)>::invoker_type` {#symbol-delegate-return-args-invoker-type}

```cpp
using invoker_type = Return (*)(const void *storage, Args...);
```

## Forward Declarations

### `delegate` {#symbol-delegate}

```cpp
template <typename Signature> class delegate;
```

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
<a id="delegate"></a>
<a id="delegate_1"></a>
<a id="delegatebind"></a>
<a id="delegatecan_bind"></a>
<a id="delegatedelegate"></a>
<a id="delegatefunction_pointer"></a>
<a id="delegateinvoke_callable"></a>
<a id="delegateinvoke_function_pointer"></a>
<a id="delegateinvoke_member"></a>
<a id="delegateinvoke_static"></a>
<a id="delegateinvoker_type"></a>
<a id="delegateis_bound"></a>
<a id="delegateminvoke"></a>
<a id="delegateoperator"></a>
<a id="delegateoperator-bool"></a>
<a id="delegateoperator_1"></a>
<a id="delegatereset"></a>
<a id="delegatereturnargs"></a>
<a id="delegatereturnargsbind"></a>
<a id="delegatereturnargscan_bind"></a>
<a id="delegatereturnargsdelegate"></a>
<a id="delegatereturnargsfunction_pointer"></a>
<a id="delegatereturnargsinvoke_callable"></a>
<a id="delegatereturnargsinvoke_function_pointer"></a>
<a id="delegatereturnargsinvoke_member"></a>
<a id="delegatereturnargsinvoke_static"></a>
<a id="delegatereturnargsinvoker_type"></a>
<a id="delegatereturnargsis_bound"></a>
<a id="delegatereturnargsminvoke"></a>
<a id="delegatereturnargsoperator"></a>
<a id="delegatereturnargsoperator-bool"></a>
<a id="delegatereturnargsoperator_1"></a>
<a id="delegatereturnargsreset"></a>
<a id="delegatereturnargsstorage_bytes"></a>
<a id="delegatereturnargsvoid"></a>
<a id="delegatestorage_bytes"></a>
<a id="delegatevoid"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
