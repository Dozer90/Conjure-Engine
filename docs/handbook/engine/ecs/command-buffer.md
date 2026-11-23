---
kind: header
---

[Engine](../../engine.md) → [ECS](../ecs.md)

# command_buffer.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/ecs/command_buffer.h` |
| API ↗ | [Command buffers and observers](../../../api/ecs/command-buffers-and-observers.md) |

<p>Deferred structural changes.</p>

## Structs

| File | Description |
|---|---|
| [`command_buffer::instantiate_entry`](command-buffer/command-buffer-instantiate-entry.md) |  |
| [`command_buffer::record`](command-buffer/command-buffer-record.md) |  |
| [`flush_report`](command-buffer/flush-report.md) | Outcome of <code>world::flush</code>. |

## Classes

| File | Description |
|---|---|
| [`command_buffer`](command-buffer/command-buffer.md) |  |

## Enumerations

### `command_buffer::opcode` {#symbol-command-buffer-opcode}

```cpp
enum class opcode : u8 {
    CREATE,
    DESTROY,
    ADD,
    SET,
    REMOVE,
    INSTANTIATE
};
```

## Forward Declarations

### `entity_archive` {#symbol-entity-archive}

```cpp
class entity_archive;
```

### `world` {#symbol-world}

```cpp
class world;
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
<a id="command_buffer"></a>
<a id="command_bufferadd"></a>
<a id="command_bufferadd_raw"></a>
<a id="command_bufferappend"></a>
<a id="command_bufferclear"></a>
<a id="command_buffercommand_buffer"></a>
<a id="command_buffercommand_buffer_1"></a>
<a id="command_buffercommand_count"></a>
<a id="command_buffercreate_entity"></a>
<a id="command_bufferdestroy_entity"></a>
<a id="command_bufferdestroy_payloads"></a>
<a id="command_bufferempty"></a>
<a id="command_bufferinstantiate"></a>
<a id="command_bufferinstantiate_entry"></a>
<a id="command_bufferinstantiate_entryarchive"></a>
<a id="command_bufferinstantiate_entrycopies"></a>
<a id="command_bufferinstantiate_entryids"></a>
<a id="command_buffermarena"></a>
<a id="command_buffermcount"></a>
<a id="command_buffermfirst"></a>
<a id="command_bufferminstantiates"></a>
<a id="command_buffermlast"></a>
<a id="command_buffermregistry"></a>
<a id="command_buffermreserved"></a>
<a id="command_buffermreservedcount"></a>
<a id="command_buffermworld"></a>
<a id="command_bufferopcode"></a>
<a id="command_bufferopcodeadd"></a>
<a id="command_bufferopcodecreate"></a>
<a id="command_bufferopcodedestroy"></a>
<a id="command_bufferopcodeinstantiate"></a>
<a id="command_bufferopcoderemove"></a>
<a id="command_bufferopcodeset"></a>
<a id="command_bufferoperator"></a>
<a id="command_bufferpayload_size"></a>
<a id="command_bufferpush_instantiate"></a>
<a id="command_bufferpush_plain_record"></a>
<a id="command_bufferpush_record"></a>
<a id="command_bufferrecord"></a>
<a id="command_bufferrecordid"></a>
<a id="command_bufferrecordinstantiateindex"></a>
<a id="command_bufferrecordnext"></a>
<a id="command_bufferrecordop"></a>
<a id="command_bufferrecordpayload"></a>
<a id="command_bufferrecordpayloadlive"></a>
<a id="command_bufferrecordpayloadsize"></a>
<a id="command_bufferrecordtarget"></a>
<a id="command_bufferrecycle_arena"></a>
<a id="command_bufferrefill_reserved"></a>
<a id="command_bufferremove"></a>
<a id="command_bufferremove_raw"></a>
<a id="command_bufferreserve_block"></a>
<a id="command_bufferreserved_count"></a>
<a id="command_bufferreturn_reserved"></a>
<a id="command_bufferset"></a>
<a id="command_bufferset_raw"></a>
<a id="command_buffershrink_to_fit"></a>
<a id="command_buffertarget"></a>
<a id="command_bufferworld"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="entity_archive"></a>
<a id="extension-points"></a>
<a id="flush_report"></a>
<a id="flush_reportapplied"></a>
<a id="flush_reportskipped"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<a id="world"></a>
<!-- doc-legacy-anchors:end -->
