---
kind: header
---

[Engine](../../engine.md) → [Core](../core.md)

# file_lock.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/file_lock.h` |
| API ↗ | [Processes, libraries and images](../../../api/core/system.md) |

<p>An exclusive cross-process lock on one file. Move-only; destroying a held lock releases it. Threading: one object belongs to one thread at a time; distinct objects may be used from any thread.</p>



## Classes

| File | Description |
|---|---|
| [`file_lock`](file-lock/file-lock.md) | An exclusive cross-process lock on one file. Move-only; destroying a held lock releases it. Threading: one object belongs to one thread at a time; distinct objects may be used from any thread. |

## Enumerations

### `file_lock_open` {#symbol-file-lock-open}

```cpp
enum class file_lock_open : u8 {
    CREATE,     // create an empty file when missing; an existing file is never truncated
    EXISTING    // NOT_FOUND when missing; nothing is created
};
```

Whether <code>try_acquire</code> creates the file.

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->
