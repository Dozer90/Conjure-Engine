# core

Foundation module. Everything else links `conjure::core`; `core` links nothing but glm (public)
and yyjson (private).

## Contents

| Area | Headers | Notes |
|---|---|---|
| Platform / types | `platform_detect.h`, `types.h` | the only place compiler macros are inspected |
| Errors | `error.h`, `result.h` | `result<T>`, `status`, `CONJURE_TRY`, `CONJURE_TRY_ASSIGN` |
| Assertions | `assert.h` | replaceable handler, compiled out in Distribution |
| Logging | `log.h` | categories, levels, sinks (console/file/callback) |
| Identity | `uuid.h` | RFC 4122 v4 |
| Helpers | `enum_flags.h`, `non_copyable.h` | |

## Threading model

- Logging is thread-safe; sinks are called under the logger mutex and never concurrently.
- `uuid::generate()` is thread-safe (per-thread generator).
- The assert handler is an atomic function pointer.

## Testing

`tests/core` - `conjure_core_tests`. Everything is headless.
