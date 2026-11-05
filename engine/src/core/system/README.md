# core/system

Operating-system services that the rest of the engine must not touch directly: child processes,
shared libraries and a small RGBA8 image type with PNG support. Design: `docs/design/10_core.md`
section 2.10. Target `conjure::core_system`; depends on `conjure::core_foundation` publicly and on
`conjure::thirdparty::stb` privately.

| Header | Types | Purpose |
|---|---|---|
| `conjure/core/process.h` | `process`, `process_desc`, `process_result` | Spawn a program with an argument array (no shell), capture stdout/stderr, wait with an optional timeout, kill the whole process tree. `run()` blocks; `start()` / `is_running()` / `read_output()` / `wait()` drive it asynchronously. |
| `conjure/core/dynamic_library.h` | `dynamic_library` | RAII `LoadLibraryExW` / `dlopen(RTLD_NOW \| RTLD_LOCAL)`, `symbol()` / `symbol_as<Fn>()`, `decorate_name("conjure_foo")` -> platform file name. |
| `conjure/core/image.h` | `image`, `rgba8` | Row-major RGBA8 pixels, `load_png` / `load_file` / `encode_png` / `save_png` (stb_image, stb_image_write), `flip_vertical`, `compare` (mean absolute difference for golden tests). |

## Layout

```
process.cpp                  run(): start + wait
process_windows.cpp          CreateProcessW, anonymous pipes, job object, two std::thread readers
process_posix.cpp            posix_spawnp, pipes, own process group, one poll() reader thread
dynamic_library.cpp          construction, moves, decorate_name
dynamic_library_windows.cpp  LoadLibraryExW / LoadLibraryW / GetProcAddress / FreeLibrary
dynamic_library_posix.cpp    dlopen / dlsym / dlclose
image.cpp                    pixel access and the PNG codec
system_internal.{h,cpp}      description validation, timeout clamping, thread-safe output buffers,
                             reader completion latch, path -> UTF-8
win32_utils.{h,cpp}          UTF-8 <-> UTF-16, FormatMessageW, Win32 -> error_code mapping
```

The platform files are selected in `CMakeLists.txt`; `#if CONJURE_PLATFORM_*` appears only inside
the POSIX backends (Linux vs macOS details) and in the test helper `conjure_process_child`, which
needs the operating system directly for a few of its modes.

## Behaviour worth knowing

### process

- Arguments are passed as an array. On Windows they are quoted following the MSVCRT rules so the
  child's `argv` is identical to `process_desc::arguments`; `argv[0]` is the quoted executable and
  `CreateProcessW` resolves bare names through the parent's directory and `PATH` (appending
  `.exe`). On POSIX `posix_spawnp` searches `PATH` for a bare name; an executable containing a
  `/` is made absolute before the spawn, because the `chdir` file action runs in the child before
  `exec` and would otherwise resolve it against `workingDirectory` instead of the parent's current
  directory as the header promises. `argv[0]` stays as the caller wrote it.
- `environment` entries are merged into the inherited environment (case-insensitively on Windows
  through `CompareStringOrdinal`, block sorted as the platform documents); an empty value removes
  the variable. An empty name or one containing `=` is `INVALID_ARGUMENT`. Removing every variable
  yields a valid empty block.
- `timeoutSeconds` must be finite and not negative (`INVALID_ARGUMENT` otherwise); values above
  `process::MAX_TIMEOUT_SECONDS` (1e9) are clamped so the deadline arithmetic cannot overflow.
- Standard input is the null device. Uncaptured streams inherit the parent's handles. Captured
  streams are drained while the child runs, so arbitrarily large output on both streams cannot
  deadlock. `mergeStderr` gives the child one pipe for both streams. A zero-length write by the
  child does not end the capture.
- Only the three standard handles are inherited (`PROC_THREAD_ATTRIBUTE_HANDLE_LIST`); without
  this the pipe of one child would leak into another child started concurrently and its reader
  would never see end-of-file. POSIX pipes are close-on-exec from creation (`pipe2(O_CLOEXEC)`
  on Linux; on macOS `POSIX_SPAWN_CLOEXEC_DEFAULT` lets only the descriptors named in the file
  actions through), so a `start()` on another thread cannot inherit them either.
- POSIX children start with an empty signal mask and default dispositions
  (`POSIX_SPAWN_SETSIGMASK` / `POSIX_SPAWN_SETSIGDEF`), whatever the spawning thread blocks or
  the engine ignores.
- `kill()` and the timeout terminate the whole tree: a job object with
  `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE` on Windows (the tree also dies if the engine process does),
  `kill(-pid, SIGKILL)` on a child placed in its own process group on POSIX. Both still reach
  descendants after the child itself exited. A killed child reports `process::KILLED_EXIT_CODE`
  (-1); a POSIX child killed by another signal reports `-signal`.
- `wait()` returns the output not yet consumed by `read_output()` / `read_error()`, so incremental
  readers can concatenate without duplicates. `wait()` may be called once; `INVALID_STATE` after.
- `wait()` joins the readers, which finish when the last writer closes. A grandchild that
  inherited the pipe and outlives the child therefore delays `wait()` only until the timeout: past
  the deadline the tree is killed, `timedOut` is set and the child's own exit code is kept.
  Without a timeout `wait()` blocks until the descendant closes the pipe (or `kill()` is called
  from another thread). A descendant that escaped the group with `setsid()` on POSIX is out of
  reach; Windows jobs do not allow breakaway.
- Destroying (or move-assigning over) a `process` that was not waited for kills the whole tree
  first, so the destructor never waits on a descendant either.
- Error codes: `INVALID_ARGUMENT` for an inconsistent description, `NOT_FOUND` for the executable
  or working directory, `PERMISSION_DENIED` / `BACKEND_FAILURE` from the OS with its message text.
  A non-zero exit code is a result, not an error.
- POSIX reaping: `is_running()` and `wait()` both call `waitpid`; a mutex held across the call
  serialises them (`is_running()` uses `try_lock` and reports the published state when a blocking
  waiter owns the child), so the exit status can never be lost to an `ECHILD` loser.

### dynamic_library

- An absolute path is loaded with `LoadLibraryExW(LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
  LOAD_LIBRARY_SEARCH_DEFAULT_DIRS)`, so a plugin finds a dependency shipped next to it regardless
  of the engine's current directory (the current directory and `PATH` are not searched for
  dependencies in that mode). Bare and relative names use `LoadLibraryW`'s default search. This
  deviates from the spec's literal `LoadLibraryW`; see the report.
- `NOT_FOUND` when the file does not exist (Win32 `ERROR_MOD_NOT_FOUND` / `ERROR_FILE_NOT_FOUND`
  for a path that is absent; on POSIX by checking the path, or the loader's "no such file" /
  "not found" text for bare names), `BACKEND_FAILURE` with the loader's message for anything else
  (wrong format, or a dependency missing while the file itself exists).
- `symbol_as<Fn>` copies the address bit-wise into the pointer type because object-to-function
  pointer casts are only conditionally supported.
- `decorate_name` leaves a name that already ends with the platform extension unchanged and
  inserts the prefix after the last path separator (`modules/conjure_foo` ->
  `modules/libconjure_foo.so`).

### image

- RGBA8 only. `load_png` requires the PNG signature and expands grey, grey+alpha, RGB, palette and
  16-bit files to RGBA8 (alpha 255 when absent, 16-bit channels keep their high byte). Dimensions
  are limited to `MAX_DIMENSION` (2^24), stb's own limit.
- `encode_png` refuses an image whose pixel bytes plus one filter byte per row exceed
  `MAX_ENCODED_BYTES` (INT_MAX) with `UNSUPPORTED`: stb_image_write works in `int`.
- `load_file` reports `IO_FAILURE` when the file size cannot be read (the file vanished between
  the existence check and the read).
- `compare` returns the mean absolute byte difference normalised to 0..1; different dimensions
  compare as 1.

## Threading

- `process`: `read_output()`, `read_error()`, `is_running()` and `kill()` may be called from any
  thread while another thread blocks in `wait()`; the output buffers are mutex-guarded
  (`detail::process_output`), the readers signal completion through `detail::completion_latch`.
  `start()`, `wait()` and destruction belong to one thread at a time. The pipe readers are
  `std::thread`s inside the backends, the documented exception to "no `std::thread` outside
  `core::jobs`" (10_core.md C4): C2 limits `system`'s dependencies to `foundation` and stb, so
  `jobs::dedicated_thread` (which does exist) cannot be used here; C4 names it anyway, a conflict
  for the architect to settle (`jobs` does not depend on `system`, so a dependency would not cycle).
- `dynamic_library`: no shared state; the OS loader serialises loading. One instance is not meant
  to be used from several threads at once.
- `image`: a value type; `stb_image`'s failure reason is a process-wide string, so a decode error
  message may be inaccurate when two threads decode invalid data at the same moment.

## Testing

`tests/core/system/` builds `conjure_core_system_tests` plus three helpers: `conjure_process_child`
(echo / stderr / both / exit / sleep / args / env / envlist / cwd / flood / trickle / stdin / pid /
signals / zero-write / spawn-sleeper modes) in the bin directory, and in `bin/core_system_libraries/`
(outside every loader's default search) `conjure_test_shared_library` (exports `conjure_test_add`
and `conjure_test_constant`) and `conjure_test_dependent_library` (imports the former, exports
`conjure_test_add_twice`). The tests receive the helper paths as compile definitions.

```
python tools/build.py --build-dir build/core_system --target conjure_core_system_tests --test -R core_system
build/core_system/bin/conjure_core_system_tests.exe -ts="core.process"
```
