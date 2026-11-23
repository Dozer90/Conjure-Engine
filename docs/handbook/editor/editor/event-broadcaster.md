---
kind: header
---

[Editor](../../editor.md) → [Protocol](../protocol.md)

# event_broadcaster.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/event_broadcaster.h` |
| API ↗ | [Headless mode and the command model](../../../api/editor/headless-and-commands.md), [The editor](../../../api/editor/index.md) |

<p>Events: one monotonic sequence shared by every client, a subscription bitmask per client (default everything except <code>log</code> and <code>command.executed</code>) and a sink per client that receives the <code><a href="protocol/protocol-message/protocol-event.html">protocol_event</a></code>. The protocol server registers its remote clients here; the local UI and the headless runner register with in-process sinks.</p>

## Structs

| File | Description |
|---|---|
| [`event_broadcaster::client`](event-broadcaster/event-broadcaster-client.md) |  |

## Classes

| File | Description |
|---|---|
| [`event_broadcaster`](event-broadcaster/event-broadcaster.md) |  |

## Enumerations

### `editor_event` {#symbol-editor-event}

```cpp
enum class editor_event : u8 {
    LOG = 0,
    SELECTION_CHANGED,
    PLAY_STATE,
    SCENE_CHANGED,
    SCENE_LOADED,
    PROGRESS,
    SCRIPT_COMPILED,
    SCRIPT_RELOADED,
    ASSET_IMPORTED,
    MODULE_STATE,
    COMMAND_EXECUTED,
    CLIENT_CONNECTED,
    CLIENT_DISCONNECTED,
    EVENTS_DROPPED,
    WARNING,
    COUNT
};
```

## Type Aliases

### `event_sink` {#symbol-event-sink}

```cpp
using event_sink = std::function<void(const protocol_event &event)>;
```

Receives one event for one client.

## Functions

### Public

#### `default_subscriptions` {#function-default-subscriptions}

<div class="function-declaration" id="function-default-subscriptions-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../../engine/core/types.html#symbol-u64">u64</a> default_subscriptions() <span class="k">noexcept</span>;</code></pre></div>

<p>Every event except <code>LOG</code> and <code>COMMAND_EXECUTED</code>.</p>

#### `event_bit` {#function-event-bit}

<div class="function-declaration" id="function-event-bit-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../../engine/core/types.html#symbol-u64">u64</a> event_bit(
    <a href="event-broadcaster.html#symbol-editor-event">editor_event</a> event) <span class="k">noexcept</span>;</code></pre></div>

<p>The bit of an event inside a subscription mask.</p>

#### `parse_editor_event` {#function-parse-editor-event}

<div class="function-declaration" id="function-parse-editor-event-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::optional&lt;<a href="event-broadcaster.html#symbol-editor-event">editor_event</a>&gt; parse_editor_event(
    std::string_view name) <span class="k">noexcept</span>;</code></pre></div>

<p>Parses a wire name; nullopt for unknown names.</p>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="event-broadcaster.html#symbol-editor-event">editor_event</a> event) <span class="k">noexcept</span>;</code></pre></div>

<p>The wire name of an event (&quot;<code>selection.changed</code>&quot;); &quot;&quot; for <code>COUNT</code>.</p>

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
<a id="default_subscriptions"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="editor_event"></a>
<a id="editor_eventasset_imported"></a>
<a id="editor_eventclient_connected"></a>
<a id="editor_eventclient_disconnected"></a>
<a id="editor_eventcommand_executed"></a>
<a id="editor_eventcount"></a>
<a id="editor_eventevents_dropped"></a>
<a id="editor_eventlog"></a>
<a id="editor_eventmodule_state"></a>
<a id="editor_eventplay_state"></a>
<a id="editor_eventprogress"></a>
<a id="editor_eventscene_changed"></a>
<a id="editor_eventscene_loaded"></a>
<a id="editor_eventscript_compiled"></a>
<a id="editor_eventscript_reloaded"></a>
<a id="editor_eventselection_changed"></a>
<a id="editor_eventwarning"></a>
<a id="event_bit"></a>
<a id="event_broadcaster"></a>
<a id="event_broadcasterclient"></a>
<a id="event_broadcasterclient_count"></a>
<a id="event_broadcasterclient_ids"></a>
<a id="event_broadcasterclientid"></a>
<a id="event_broadcasterclientminloglevel"></a>
<a id="event_broadcasterclientsink"></a>
<a id="event_broadcasterclientsubscriptions"></a>
<a id="event_broadcasteremit"></a>
<a id="event_broadcasteremit_log"></a>
<a id="event_broadcasteremit_to"></a>
<a id="event_broadcasteremitted_count"></a>
<a id="event_broadcasterevent_broadcaster"></a>
<a id="event_broadcasterevent_broadcaster_1"></a>
<a id="event_broadcasterfind"></a>
<a id="event_broadcasterframe"></a>
<a id="event_broadcasterhas_client"></a>
<a id="event_broadcastermake_event"></a>
<a id="event_broadcastermclients"></a>
<a id="event_broadcastermemitted"></a>
<a id="event_broadcastermframe"></a>
<a id="event_broadcastermin_log_level"></a>
<a id="event_broadcastermnextclientid"></a>
<a id="event_broadcastermnextseq"></a>
<a id="event_broadcasternext_seq"></a>
<a id="event_broadcasteroperator"></a>
<a id="event_broadcasterregister_client"></a>
<a id="event_broadcasterset_frame"></a>
<a id="event_broadcasterset_min_log_level"></a>
<a id="event_broadcasterset_subscriptions"></a>
<a id="event_broadcastersubscriptions"></a>
<a id="event_broadcasterunregister_client"></a>
<a id="event_sink"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="parse_editor_event"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
