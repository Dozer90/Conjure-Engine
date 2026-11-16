# Core areas: time to reflect

🛠 **Engineering handbook** · 📘 API: [Time, events and random](../../../api/core/time-and-events.md), [Jobs and threading](../../../api/core/jobs.md), [JSON](../../../api/core/json.md), [Files and the virtual file system](../../../api/core/files.md), [Processes, libraries and images](../../../api/core/system.md), [Reflection](../../../api/core/reflection.md)

The remaining six areas of `conjure::core`: what each is for, its public headers, the algorithms worth knowing
before changing them, what is and is not thread-safe, where the area can be extended and how it is tested.
The shape of the module, the shared conventions and the build and test commands are on the parent page
[Core module](../core.md); the first five areas are on
[Core areas: foundation to memory](foundation-to-memory.md). `C<n>` references point at the conventions table
of [`10_core.md`](../../../design/10_core.md).

## Contents
- [1. time](#1-time)
- [2. jobs](#2-jobs)
- [3. json](#3-json)
- [4. vfs](#4-vfs)
- [5. system](#5-system)
- [6. reflect](#6-reflect)
- [Related](#related)

## 1. time

Purpose: clocks, randomness, events, delegates and the engine-wide event bus (`conjure::core_time`).

### 1.1 Headers

| Header | Contents |
|---|---|
| `conjure/core/clock.h` | `clock` (virtual `now_seconds`, `sleep_until`), `steady_clock` (sleeps until 2 ms before the deadline, then spins), `manual_clock` (`advance`, `set`), `default_clock()`, `ticks_now` / `ticks_to_seconds` / `ticks_per_second`, `stopwatch` (`start`, `elapsed_seconds`, `restart`) |
| `conjure/core/random.h` | `random::engine` (= `std::mt19937_64`), `default_engine()` (thread-local), `seed` / `current_seed` (`DEFAULT_SEED = 0x5EED`), `uniform` (integers inclusive, floats half-open), `chance`, `unit_vector3`, `random_engine` |
| `conjure/core/event.h` | `event<Args...>` (multicast, subscription order, reentrancy-safe), `event_subscription` (RAII, move-only, inert after the event dies) |
| `conjure/core/delegate.h` | `delegate<Return(Args...)>`: two pointers of inline storage, binds free functions, member functions and trivially copyable callables (`CAN_BIND`); `operator==` compares the invoker and the raw storage, so it is exact except for capturing lambdas whose closure has padding |
| `conjure/core/event_bus.h` | `event_bus`: `subscribe<Event>`, synchronous `publish`, thread-safe `enqueue`, `dispatch_queued()` (drained by the engine each frame), `queued_count`, `clear_queue`, `subscriber_count` |

### 1.2 Algorithms

The random helpers implement their own bounded-integer and unit-float draws so a seed replays identically on
every platform (`test_random.cpp` pins the first outputs for `DEFAULT_SEED`; `unit_vector3`'s x and y excepted:
they use libm `cos`/`sin`, and the golden test compares them with a tolerance, while its two engine draws and its
z stay exact). `event` handlers may subscribe, unsubscribe themselves, emit recursively or destroy the event
during an emit.

### 1.3 Threading

Clocks, events, delegates and the subscribe/publish half of `event_bus` are main-thread objects;
`steady_clock::now_seconds` and the tick functions are stateless; `event_bus::enqueue` is mutex-guarded and the
queue is swapped out before dispatch (the emptied buffer is handed back afterwards when nothing was enqueued
meanwhile, so steady enqueueing does not reallocate); `random::default_engine()` is `thread_local`.

### 1.4 Tests

`tests/core/time/`, driven by `manual_clock` and fixed seeds.

## 2. jobs

Purpose: the engine's threading primitives, and the only area that creates `std::thread` / `std::jthread`
(C4). Every other thread is a `dedicated_thread`, the `system::process` pipe readers included.

### 2.1 Headers

| Header | Contents |
|---|---|
| `conjure/core/jobs/job_system.h` | `job_priority {HIGH, NORMAL, LOW}`, `job_system_desc {workerCount, maxQueuedJobs, threadNamePrefix}`, `job_system_stats`, `job_system`: `start`, `stop`, `is_running`, `worker_count`, `schedule(fn)` / `schedule(span)`, `wait`, `wait_idle`, `parallel_for(count, grain, body)` / `parallel_for(count, body)`, static `thread_index()`, `current()`, `stats()` |
| `conjure/core/jobs/job_handle.h` | `job_handle`: shared 64-bit completion counter, `valid`, `is_complete`, `pending`, `reset` |
| `conjure/core/jobs/parallel_for.h` | `default_grain`, `range_count`, `parallel_for_range`, `parallel_for_each` (any callable, no `std::function` at the call site) |
| `conjure/core/jobs/main_thread.h` | `is_main_thread`, `set_main_thread`, `reset_main_thread`, `main_thread_id`, `post_to_main_thread`, `drain_main_thread_queue`, `main_thread_queue_size` |
| `conjure/core/jobs/dedicated_thread.h` | `dedicated_thread(name, fn(std::stop_token))`: named `std::jthread`, destruction requests a stop and joins |
| `conjure/core/jobs/jobs.h` | umbrella |

### 2.2 Algorithms

- **Queue.** `job_system` is a facade over a private `state`. One lock-guarded queue holds three fixed-capacity
  ring lanes (one per priority) of `job_block` pointers taken from a pool that starts at 16 blocks and doubles
  up to `maxQueuedJobs`; popping moves the block onto the executing thread's stack inside the same critical
  section. The lock is a spinlock that parks after a bounded spin (a test-and-test-and-set word with
  backoff; the FIFO ticket lock the performance round first used stalled every later waiter behind a
  preempted one and was replaced after review), the hot counters sit on cache lines of their own and the
  statistics are per thread, because with 16 workers draining one dispatch every queued job is a
  serialisation point (a parked `std::mutex` waiter cost about 1.7 us per job; `core.job_parallel_for_1m`
  went from 0.100 to 0.042 ms with the ticket lock and measures 0.052 with the current parking lock). Workers are named
  `<prefix>-<index>` from names `start()` prepares in the OS encoding before it creates the threads, re-check
  the queue for a bounded time (50 us, a `pause` per check and a `yield()` every 64th) and then sleep on a
  condition variable; a thread in `wait()` spins the same window before it parks. The window hides the
  wake-up for dispatches less than 50 us apart (consecutive dispatched transform levels); a stage boundary
  that runs main-thread systems or flushes between dispatches still parks the workers, and every awake worker
  spins the window once the queue runs dry (16 x 50 us of CPU per dispatch). `start()` returns only after
  every worker has named itself and entered the queue loop (a counter under the queue lock plus a condition
  variable), so a worker allocates nothing after `start()`
  returns and an allocation gate opened right after it is never charged a worker prologue. With zero workers,
  before `start()`, after `stop()` or when the queue is full the scheduling thread runs the job itself before
  `schedule` returns.
- **Waiting helps.** `wait()` and `wait_idle()` pop and execute queued jobs until the counter reaches zero and
  only then sleep on a process-wide completion monitor that is signalled only when somebody is blocked, so the
  hot path never locks it; waiting from a worker never deadlocks, even on a single worker. `wait_idle()`
  inside a job publishes the caller's own execution frames so two in-flight jobs both calling it return
  instead of waiting for each other (only a call outside every job guarantees a completely idle system).
  Finishing a job wakes the monitor when a counter or the active count reaches zero, or when the active
  count drops to the published idle-waiter depth, because the last sibling of a parked waiter's own batch
  drops neither to zero.
- **`parallel_for`.** Splits `[0, count)` into `ceil(count / grain)` ranges, queues them under one lock
  acquisition, runs range 0 plus every range that did not fit on the caller and helps until a stack-allocated
  64-bit counter hits zero; the body is referenced through a `(fn pointer, context)` pair, so steady-state
  dispatch allocates nothing (`stats().poolGrowths` is the gate). `stats().stolen` counts jobs executed by a
  worker other than the thread that queued them.
- **Main thread.** The first caller of `set_main_thread()` or the constructor of the first `job_system`;
  absent both, the first thread to call `is_main_thread()`. `drain_main_thread_queue` swaps the vector out
  before running and hands the emptied buffer back when nothing was posted meanwhile (so steady posting does
  not reallocate), asserts and runs nothing off the main thread, and never claims the designation.
- **Errors.** `start()` returns `INVALID_STATE` when running and `INVALID_ARGUMENT` for a zero or
  over-`MAX_QUEUED_JOBS` (2^20) queue, more than `MAX_WORKER_COUNT` (256) workers or an empty prefix.

### 2.3 Threading

`schedule`, `wait`, `wait_idle`, `parallel_for` and `stats` are safe from any thread, including inside jobs;
`stop()` must not be called from one of the system's own workers. Toolchain: `dedicated_thread` needs
`<stop_token>` (libstdc++ 10+, MSVC 19.28+, libc++ 18+); `engine/src/core/jobs/CMakeLists.txt` checks at
configure time.

### 2.4 Tests

`tests/core/jobs/`: every behavioural case runs with 0 and 8 workers through `CONJURE_JOBS_WORKER_SUBCASES`
(`[parallel]`); coverage includes `wait()` from inside jobs, nested `parallel_for`, two jobs calling
`wait_idle()` at once on 1 and 8 workers, a `wait_idle()` inside a batch job and inside a `parallel_for` range
whose sibling of the same batch finishes after the waiter has parked, a worker of one system scheduling a batch
into another (counted in the external statistics slot), and `stop()`/`start()` cycling under four producers.

## 3. json

Purpose: JSON for configuration, project files, scene text formats, protocol messages and reflection,
wrapping yyjson 0.10 privately behind three types (`namespace conjure::core::json`).

### 3.1 Headers

| Header | Contents |
|---|---|
| `conjure/core/json/value.h` | `json_type`, `value` (non-owning view: `is_*`, `as_bool/i64/u64/f64/f32/string(fallback)`, `size`, `operator[]`, `contains`, `get`, iteration, `members()`, `to<T>()`, `is_valid`), `json_converter<T>` customisation point |
| `conjure/core/json/document.h` | `document` (owning: `object()`, `array()`, `from_value`, `parse`, `parse_file`, `root`, `set`/`set_null`/`erase`, `push_back`, `clear`, `at_pointer`/`set_pointer`, `merge_patch`, `to_string(pretty)`, `to_canonical_string`, `write_file`, `operator==`), builders `json::object(...)` / `json::array(...)`, `MAX_NESTING_DEPTH = 1024` |
| `conjure/core/json/writer.h` | `writer`: `begin_object/end_object`, `begin_array/end_array`, `key`, `value(...)`, `write_raw_json`, `finish`, `reset`, `depth`, `is_complete`, `text` |
| `conjure/core/json/json.h` | umbrella plus free `parse()` and `to_string(value, canonical)` |

### 3.2 Algorithms

- `value` never asserts; a failed lookup yields an invalid value whose accessors return their fallback
  (`is_null()` is true for null *and* invalid, `is_valid()` tells them apart).
- **Canonical JSON** (`to_canonical_string`) builds a sorted deep copy in a scratch yyjson document (members by
  UTF-8 byte order, stable for duplicates) and writes it minified with yyjson's shortest round-trip reals
  (`0.1`, `1.0`, `-0.0`, `1e21`) and plain-digit integers, so the text is identical on Windows, Linux and macOS
  and can be hashed; `writer` output is byte-identical when keys are written in sorted order.
- Equality is structural and mirrors the canonical text (order-independent objects, reals bit for bit, an
  integer never equals a real).
- Parsing is strict RFC 8259 with `PARSE_FAILURE` messages `parse error at line L, column C: reason`; because
  yyjson's deep copy and merge patch recurse, `parse()` walks the tree with an explicit stack and rejects
  nesting deeper than `MAX_NESTING_DEPTH`.
- Mutable documents are pool allocators: erasing never shrinks a document.

### 3.3 Threading

Distinct documents are independent; one document may be read from many threads; mutation while anyone reads
is a data race; `writer` is not shared.

### 3.4 Extension points

Specialise `json_converter<T>` with `static result<T> from_json(const value &)` (the reflect area does so for
`string_id` and the math types).

### 3.5 Tests

`tests/core/json/`, including canonical stability tables, writer/canonical byte identity, RFC 7386 and RFC 6901
examples, nesting at and past the limit and multi-threaded use.

## 4. vfs

Purpose: one `file_system` interface over several backends, a scheme-based mount table and lexical path
helpers (`namespace conjure::core::vfs`).

### 4.1 Headers

| Header | Contents |
|---|---|
| `conjure/core/vfs/file_system.h` | `file_stat`, `enumerate_flags`, `file` (`read_at`, `size`), `file_system`: `read_file`, `read_text_file`, `write_file`, `write_file_atomic`, `write_text_file`, `stat`, `exists`, `enumerate`, `rename`, `remove`, `remove_all`, `create_directories`, `open_read`, `is_read_only` |
| `conjure/core/vfs/native_file_system.h` | a directory on disk: `root()`, `to_native()` (a `result<path>`, checked like the file methods), rejects root escapes |
| `conjure/core/vfs/memory_file_system.h` | an in-memory tree: `set_read_only`, `write_counter` |
| `conjure/core/vfs/mount_table.h` | routes `scheme://path`: `mount`, `unmount`, `find_mount`, `schemes`, static `split` |
| `conjure/core/vfs/path_utils.h` | `normalize`, `join`, `parent`, `filename`, `stem`, `extension` (lower-cased), `has_extension`, `is_absolute`, `relative_to`, `is_portable_name` |
| `conjure/core/vfs/vfs.h` | umbrella |

### 4.2 Contract and algorithms

- Paths are UTF-8, `/`-separated, relative to the backend root (`""` is the root); escapes are
  `PERMISSION_DENIED`; every backend returns the same codes (`NOT_FOUND`, `INVALID_ARGUMENT`, `ALREADY_EXISTS`,
  `INVALID_STATE` for a non-empty directory, `OUT_OF_RANGE` for `read_at` past the end, `IO_FAILURE` with the
  OS text) and enumerates in ascending path order.
- Names of new entries are portable (code sweep F753). `write_file`, `write_file_atomic`, `create_directories`
  and the destination of `rename` pass their normalised path to the one shared `internal::validate_new_path`
  (`vfs_internal.h`), in the native and the memory backend alike and on every platform; the mount table's
  cross-backend `rename` calls it for the destination before it looks up either file. Every segment must pass
  `internal::portable_name_problem` (public as `path_utils::is_portable_name`): no control character and none
  of `: * ? " < > |`, no trailing dot or space, no Windows device name (`CON`, `PRN`, `AUX`, `NUL`, `COM1`-`COM9`,
  `LPT1`-`LPT9`; the part before the first dot, trailing spaces dropped, case ignored). A refusal is
  `INVALID_ARGUMENT` naming the path and the segment, ranked after the escape and read-only checks and before
  any lookup, and it is lexical: a segment that exists already is judged too. `read_file`, `open_read`, `stat`,
  `enumerate`, `remove`, `remove_all` and the source of `rename` never call the validator, so an entry that
  carries such a name stays readable and removable. On Windows the native backend's `native_path_of` hands a
  path with such a name to Win32 in the extended-length form whatever its length, because the classic form
  drops a trailing dot or space (`report.` would address `report`) and, before Windows 11, opens the device
  for a device name in any directory.
  Before, a `:` in a later segment wrote an NTFS alternate
  data stream and left a 0-byte file, and a trailing dot or space was dropped by Win32.
- `native_file_system::to_native` is checked like every file method (it returns the same `resolve(path)`), so
  the on-disk path it hands out is the root or below it and an escape is `PERMISSION_DENIED`; the unchecked
  mapping is the private `native_path_of`, fed only paths that passed `internal::resolve_backend_path`
  (code sweep F241).
- The native backend reads through handles opened with delete sharing (`CreateFileW` with `FILE_SHARE_DELETE`,
  `pread` on POSIX) so an open reader never blocks `write_file_atomic`, `rename` or `remove`; atomic writes go
  to `<path>.tmp-<uuid>` and swap in with `FileRenameInfoEx` + `POSIX_SEMANTICS` (fallback `MoveFileExW` for the
  codes `internal::rename_falls_back_to_move` lists, `ERROR_INVALID_FUNCTION` from network redirectors among
  them) or `std::filesystem::rename`.
- `write_file_atomic` is durable on the native backend (code sweep F243): the temporary is written by
  `internal::write_file_flushed` (`CreateFileW` + `WriteFile` + `FlushFileBuffers`; `open` + `write` + `fsync`
  on every POSIX platform; macOS gets `fsync` too, not `F_FULLFSYNC`, decision C11) and only then renamed over
  the target, so after a power cut the path holds the old contents or the new ones, never a file whose name
  reached the disk before its data. The directory entry is not flushed: a lost rename leaves the old file,
  one of the two allowed outcomes, and possibly the temporary (the asset scan never takes a `*.tmp-<uuid>`
  for a source). The cost is one device flush per call, milliseconds each, so `write_file` stays unflushed
  for output that can be made again; `benchmark.assets.first_scan_100` measures what a scan that creates one
  `.meta` per source pays. A flush refused with a code in `internal::flush_is_unsupported` (a driver without
  a flush routine) keeps the file without the guarantee and logs one WARN per process; any other flush error
  fails the write and the target is untouched. `internal::flushed_file_count()` counts the flushed files of
  the process for tests and benchmarks.
- Recursive `enumerate` and `remove_all` start their walk from the
  extended-length form on Windows, so an entry nested past the classic path limit below a short root is still
  reached; write times convert without reading a clock, so an unchanged file always reports the same value.
- The memory backend stores immutable `shared_ptr<const vector<byte>>` contents, so open handles keep reading
  their snapshot; write times come from a per-instance counter.
- The mount table validates schemes, accepts `assets` and `assets://` interchangeably, refuses to mount itself
  (directly or through other tables) and moves files across backends by copy-and-remove. `rename` first
  follows both URIs through nested tables to the backends that store them, so one backend reached by two
  routes gets a real rename. The move keeps the rename contract against those backends and in the order of a
  backend's own rename (an escape or a read-only backend at either end, a backend root at either end and a
  destination name that is not portable are refused before either file is looked up; then a missing source, a
  directory at the destination and a missing destination parent, all before anything is copied), writes the
  copy atomically and removes a new copy again when the source cannot
  be removed. Engine mounts (R24): `project://`, `assets://`, `engine://`, `cooked://`, `modules://<name>/`.

### 4.3 Threading

`native_file_system` is stateless apart from its root (the flush statistics of `native_platform.cpp` are two
process-wide atomics); `memory_file_system` takes one mutex per operation and
invokes enumerate callbacks after releasing it; `mount_table` copies the backend `shared_ptr` out before
forwarding so an operation survives a concurrent `unmount`.

### 4.4 Extension points

Implement `file_system` (the asset pack reader will).

### 4.5 Tests

`tests/core/vfs/`, a `TEST_CASE_TEMPLATE` conformance suite run against both backends (the names of new
entries included) plus native, memory and mount specifics; the native file covers the flush of an atomic
write through `internal::flushed_file_count()` and reads, moves and removes entries whose names could not
be created.

## 5. system

Purpose: operating-system services the rest of the engine must not touch directly (`conjure::core_system`).

### 5.1 Headers

| Header | Contents |
|---|---|
| `conjure/core/process.h` | `process_desc` (executable, argument array, working directory, environment pairs, capture flags, `timeoutSeconds`), `process_result` (`exitCode`, `standardOutput`, `standardError`, `timedOut`, `elapsedSeconds`), `process`: `run`, `start`, `is_running`, `wait`, `kill`, `read_output`, `read_error`, `process_id`; `KILLED_EXIT_CODE = -1` |
| `conjure/core/dynamic_library.h` | `dynamic_library`: `load`, `unload`, `is_loaded`, `path`, `symbol`, `symbol_as<Fn>`, static `decorate_name`, `extension`, `prefix` |
| `conjure/core/image.h` | `rgba8`, `image`: `pixels`, `pixel`/`set_pixel`, `fill`, `load_png`, `load_file`, `encode_png`, `save_png`, `flip_vertical`, `compare` (mean absolute difference 0..1), `MAX_DIMENSION = 2^24` |

### 5.2 Algorithms

- Arguments are passed as an array (Windows applies the MSVCRT quoting rules so the child's `argv` matches, and
  refuses a `.bat` / `.cmd` executable because `cmd.exe` would re-parse them; a NUL anywhere in the description
  is `INVALID_ARGUMENT`); children inherit only the standard streams (handle list, `POSIX_SPAWN_CLOEXEC_DEFAULT`,
  `posix_spawn_file_actions_addclosefrom_np` on glibc 2.34+); captured streams are drained while the child runs
  so large output never deadlocks; `kill()` and the timeout terminate the whole tree (a job object on Windows,
  `kill(-pid)` on a child in its own process group); destroying an un-waited `process` kills the tree first; a
  status lost to `ECHILD` reports `KILLED_EXIT_CODE`.
- `dynamic_library::load` uses `LoadLibraryExW` with `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
  LOAD_LIBRARY_SEARCH_DEFAULT_DIRS` for absolute paths (a plugin finds a dependency shipped next to it) and
  `dlopen(RTLD_NOW | RTLD_LOCAL)` on POSIX; `symbol_as` copies the address bit-wise because object-to-function
  pointer casts are only conditionally supported.
- `image` is RGBA8 only; `load_png` expands grey, grey+alpha, RGB, palette and 16-bit files.

### 5.3 Threading

`read_output`, `read_error`, `is_running` and `kill` may be called while another thread blocks in `wait()`. The
pipe readers are `jobs::dedicated_thread`s (C4; `system` links `jobs` privately, C2 as amended by code sweep
F233), started by both backends through `detail::pipe_reader` in `system_internal.h`. Debuggers and profilers
show them as `process-stdout` and `process-stderr` on Windows and `process-pipes` on POSIX. A reader ends when
the last writer of its pipe is gone, not on a stop request (a blocking read cannot be interrupted), so the
destructor kills the process tree before it joins, and `wait()` does once its timeout has passed.

### 5.4 Tests

`tests/core/system/` plus the helpers `conjure_process_child`, `conjure_test_shared_library` and
`conjure_test_dependent_library` built in the test tree.

## 6. reflect

Purpose: runtime type information for plain data structs, driving the inspector, the scene format and the C#
mirror (`namespace conjure::core::reflect`).

### 6.1 Headers

| Header | Contents |
|---|---|
| `conjure/core/reflect/type_id.h` | `type_id` (FNV-1a 64 of the registered name, `from_name` `constexpr`), `reflect_traits<T>` / `reflect_kind_override<T>` customisation points, `type_id_of<T>()`, `reflect_name_of<T>()` |
| `conjure/core/reflect/field_info.h` | `field_kind` (BOOL .. TRANSFORM, ENUM, STRUCT, ARRAY, ENTITY, ASSET_REF), `kind_size` / `kind_alignment`, `field_attributes`, `field_info`, `enum_info`, `kind_of<T>()`, `reflectable` |
| `conjure/core/reflect/type_info.h` | `type_info` (`find_field`, `layout_hash`, `has_same_layout`), `validate_layout`, `build_type_info` (dynamic C# components), `describe_struct<T>()` / `describe_enum<T>()` |
| `conjure/core/reflect/type_registry.h` | `type_registry` (`register_type`, `register_enum`, lock-free `find` / `find_enum`, `types`, `enums`, `on_registered`, `on_unregistered`, `unregister_type/enum`), `registry()`, `type_info_of<T>()`, `enum_info_of<T>()` |
| `conjure/core/reflect/json_conversion.h` | `json_converter` for `string_id` and the math types, `to_json` / `from_json` / `to_json_document`, `field_ref`, `resolve_path("a.b[2].c")`, `set_from_json` / `get_as_json` / `write_json` |
| `conjure/core/reflect/reflect.h` | umbrella plus `CONJURE_REFLECT_STRUCT`, `CONJURE_FIELD`, `CONJURE_FIELD_HIDDEN`, `CONJURE_FIELD_READ_ONLY`, `CONJURE_REFLECT_ENUM`, `CONJURE_REFLECT_FLAGS` |

### 6.2 Algorithms

- The macros specialise `reflect_traits<T>` at global scope; offsets come from `offsetof`, so reflected structs
  are plain data (polymorphism is a compile error).
- `type_info_of<T>()` registers lazily and re-validates its cached pointer against `registry().find()` on
  every call (one lock-free lookup), so a type registers again after a conflict is resolved or after module
  unload.
- `layout_hash()` is FNV-1a 64 over `(name bytes, kind byte, offset u32 LE, size u32 LE)` per field in order
  and is the single mirror-hash definition shared with C# (R61); `test_type_info.cpp` pins
  `0xfe13925eca48303a` for `test.inner`.
- The registry validates layouts, returns the existing description for an identical re-registration
  (`has_same_layout`: size, alignment and every field, including an array's element kind and count and the
  type a `STRUCT` or `ENUM` value refers to, which `layout_hash()` does not cover) and `ALREADY_EXISTS`
  otherwise, refuses to unregister a type another live struct still references (`INVALID_STATE`), and keeps
  retired descriptions allocated so a lock-free reader racing a removal never touches freed memory. The id
  index (`engine/src/core/reflect/pointer_index.h`) is a chain of grow-only open-addressing tables with
  release/acquire publication.
- JSON reads are lenient (missing members keep their value, unknown members are reported, `3.0` is accepted
  for an integer) and stop at the first failing field with a message naming the path.
- Floats without a JSON number have their own spelling, in `write_float` / `read_float` of
  `json_conversion.cpp`, which every float of every kind goes through (scalars, the components of vectors,
  quaternions, matrices and transforms): +infinity is the string `"inf"`, -infinity is `"-inf"`, NaN is `null`,
  and each reads back as the value written (decision C15, code sweep F250). Before that an infinity was
  written as `null` and came back as NaN. Reading takes exactly the two spellings, a number or `null`; any
  other string is `INVALID_ARGUMENT`. Old data with `null` still loads (as NaN); data holding one of the
  strings does not load in a build from before the change. `json::writer` itself still asserts on a
  non-finite real, so code that writes floats without reflection must apply the same encoding itself.

### 6.3 Threading

`register_*` / `unregister_*` take a mutex; lookups are lock-free; the lock order is event mutex -> registry
mutex and handlers may register types; `types()` / `enums()` views are invalidated by the next mutation.

### 6.4 Extension points

`reflect_kind_override<T>` for foreign handles (`ENTITY`, `ASSET_REF`, specialised by `ecs` and `assets`),
`json_converter<T>`, the `on_registered` / `on_unregistered` events.

### 6.5 Tests

`tests/core/reflect/` with `reflect_test_types.h` (a struct with a field of every kind and fake handle types);
every case passes under `--order-by=rand`.

## Related
- parent page: [Core module](../core.md); sibling: [Core areas: foundation to memory](foundation-to-memory.md)
- design records: [`../../../design/10_core.md`](../../../design/10_core.md) 2.6-2.11,
  [`../../../design/09_cross_area_resolutions.md`](../../../design/09_cross_area_resolutions.md) R24, R38, R61,
  [`../../../design/06_engine_loop.md`](../../../design/06_engine_loop.md) 5 (threads)
- area READMEs: `engine/src/core/{time,jobs,json,vfs,system,reflect}/README.md`
- handbook: [Architecture](../../architecture.md) 6 (threading model),
  [Data-oriented design](../../data-oriented-design.md) 6 (job-parallel systems),
  [Module system](../../module-system/index.md) (what `dynamic_library` and reflection are used for)
