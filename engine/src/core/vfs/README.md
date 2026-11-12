# core/vfs

The virtual file system: one interface (`file_system`) over several backends, a scheme-based
mount table and lexical path helpers. Public headers live in `conjure/core/vfs/`; `vfs.h` is the
umbrella. The binding contract is `docs/design/10_core.md` section 2.9.

## Types

| Type | Header | Purpose |
|---|---|---|
| `file_stat`, `enumerate_flags`, `file`, `file_system` | `file_system.h` | the backend interface: `read_file`, `read_text_file`, `write_file`, `write_file_atomic`, `write_text_file`, `stat`, `exists`, `enumerate`, `rename`, `remove`, `remove_all`, `create_directories`, `open_read`, `is_read_only` |
| `native_file_system` | `native_file_system.h` | a directory on disk; `root()`, `to_native()`; rejects root escapes |
| `memory_file_system` | `memory_file_system.h` | an in-memory tree; `set_read_only()`, `write_counter()` |
| `mount_table` | `mount_table.h` | routes `scheme://path` to backends; `mount`, `unmount`, `find_mount`, `schemes`, `split` |
| `path_utils` | `path_utils.h` | `normalize`, `join`, `parent`, `filename`, `stem`, `extension`, `has_extension`, `is_absolute`, `relative_to` |

## Path and error contract

Paths are UTF-8 and `/`-separated; backslashes and a leading `/` are accepted and normalised, and
`""` names the backend root. Anything that would leave the root (`../x`, `C:/x`, `//server/x`) is
`PERMISSION_DENIED`. Every backend returns the same generic `core::error_code`s, documented at the
top of `file_system.h`: `NOT_FOUND`, `INVALID_ARGUMENT` (wrong kind of entry, the root where an
entry is required), `ALREADY_EXISTS`, `INVALID_STATE` (non-empty directory), `PERMISSION_DENIED`
(read-only backend or escape), `OUT_OF_RANGE` (`read_at` past the end) and `IO_FAILURE` (anything the
operating system refused; the message carries its text). `enumerate` reports entries in ascending
path order on every backend so results are deterministic; its paths are root-relative, or full
URIs when enumerating through a `mount_table`.

## Implementation notes

- `native_file_system` uses only the `std::error_code` overloads of `std::filesystem` for
  directory work; nothing throws. Reading goes through `native_platform.h`: `open_read_handle`
  (`CreateFileW` with `FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE` on Windows,
  `open(O_RDONLY | O_CLOEXEC)` on POSIX) and positional `read_at` (`ReadFile` with an `OVERLAPPED`
  offset, `pread`). The delete sharing is the point: a `std::ifstream` reader would make every
  rename over or removal of the file fail on Windows for as long as it is open. `write_file_atomic`
  writes `<path>.tmp-<uuid>` next to the target (the temporary goes through `extend_native_path` on
  its own: it is 41 characters longer than the target, so a target just under the classic Windows
  limit has a temporary beyond it) and swaps it in with `internal::replace_file`:
  `SetFileInformationByHandle(FileRenameInfoEx)` with `FILE_RENAME_FLAG_POSIX_SEMANTICS` (falling
  back to `MoveFileExW(MOVEFILE_REPLACE_EXISTING)` on volumes without it, recognised by the codes in
  `internal::rename_falls_back_to_move`: `ERROR_INVALID_FUNCTION` from SMB/NAS redirectors and some
  FAT/exFAT drivers, `ERROR_INVALID_PARAMETER`, `ERROR_NOT_SUPPORTED`; never `ERROR_ACCESS_DENIED`,
  which also means an ACL refusal that `MoveFileExW` would hit too) in
  `native_platform_windows.cpp`, `std::filesystem::rename` in `native_platform_posix.cpp` (CMake
  picks the file). `rename` onto an existing file goes through the same helper; a rename onto
  another spelling of the same entry (a case-only rename on a case-insensitive volume) is handed
  to `std::filesystem::rename` directly. While a file's last name is being removed and a reader
  still holds it open, Windows answers lookups with "access denied" for an instant; `stat` and
  `open_read` recognise that state (a zero-access probe open fails too) and report `NOT_FOUND`, so
  readers racing a `remove` see the same codes as on POSIX. UTF-8 paths are converted with
  `std::u8string` so Windows receives wide paths.
- Paths beyond the classic Windows limit: single-entry calls go through `extend_native_path`, which
  switches to the extended-length form (`\\?\`) from 248 characters on. Directory walks
  (`enumerate`, `remove_all`) append components below their root as they descend, so they start
  from `always_extended_native_path` instead, whatever the root's length: one asset nested deeper
  than the limit below a short project root cannot abort the whole scan. POSIX returns both paths
  unchanged.
- `file_time_to_nanoseconds` converts with `clock_cast` (MSVC, libstdc++) or, on libc++ whose
  file clock counts from the Unix epoch, with `file_clock::to_sys`. Neither reads a clock, so an
  unchanged file reports the same write time on every `stat` and `enumerate`; consumers compare
  write times for equality.
- `memory_file_system` stores a tree of `node`s. File contents are immutable
  `shared_ptr<const vector<byte>>`s: a write swaps the pointer, so files opened with `open_read`
  keep reading the snapshot they were opened on. Write times are the value of a per-instance
  counter incremented on every successful mutation.
- `mount_table` validates schemes (letters, digits, `-`, `_`, `.`, `+`), accepts `assets` and
  `assets://` interchangeably for `mount`/`unmount`/`find_mount`, refuses to mount a mount table
  that already contains it (directly or through other tables), and moves files between mounts by
  copy-and-remove (directories are `UNSUPPORTED`). One backend may sit behind several schemes,
  directly or through nested mount tables. Before deciding, `rename` follows both URIs down to the
  backends that store them (`route_to_storage`), because a nested table hides its backends behind
  URIs; it then decides "same backend" by identity, not by scheme, so such a rename is a real
  rename and a file renamed onto itself through two routes is never copied and removed.
- The cross-backend `rename` keeps the base contract and checks it, against those storing
  backends, before copying: the root of a backend at either end is `INVALID_ARGUMENT` (`stat`
  reports a root as a directory), a read-only source is `PERMISSION_DENIED` (otherwise the copy
  would land and only the removal fail; a nested table's own `is_read_only()` would be false while
  any of its mounts is writable), a directory at `to` is `ALREADY_EXISTS` and a missing parent of
  `to` is `NOT_FOUND` (`write_file_atomic` would answer `INVALID_ARGUMENT` and create the parent).
  The parent is taken from the normalised path, because `path_utils::parent` cannot take a URI
  apart. The copy goes through `write_file_atomic`. When removing the source fails, a copy that
  created a new file is removed again (a `WARN` on `LOG_CORE` if that fails too); a replaced file
  is not restored.
- `path_utils::parent` keeps a root's separator: `"/a"` -> `"/"`, `"C:/a"` -> `"C:/"` (the bare
  `"C:"` would mean "the current directory of drive C").

## Threading

- `native_file_system` holds no mutable state; one instance can be used from any thread. `file`
  handles returned by `open_read` read positionally and need no lock. An open handle never blocks
  `write_file_atomic`, `rename` or `remove` of the same file on any platform (the handle keeps the
  old contents readable).
- `memory_file_system` guards every operation with one mutex; `enumerate` collects entries under
  the lock and invokes the callback after releasing it, so callbacks may call back into the file
  system. `set_read_only` is an atomic flag.
- `mount_table` guards its map with a mutex and copies the backend `shared_ptr` out before
  forwarding, so an operation in flight survives a concurrent `unmount`; no backend method is ever
  called while the mutex is held (`is_read_only` snapshots the backends first). `find_mount`
  returns a raw pointer that is valid only until the scheme is unmounted.

## Testing

`tests/core/vfs/` builds `conjure_core_vfs_tests`:

```
python tools/build.py --build-dir build/core_vfs --target conjure_core_vfs_tests --test -R core_vfs
```

`test_file_system_conformance.cpp` runs every behavioural case against both backends through
`TEST_CASE_TEMPLATE`, including readers holding `open_read` handles while the file is replaced,
renamed over and removed, and case-only renames; the other files cover the interface defaults, the
native specifics (root escapes, UTF-8 names, atomic writes leaving no temporary, write times), the
memory specifics (counter semantics, snapshots, concurrent writers and readers) and the mount
table (routing, error pass-through, cross-mount rename with its error codes, nested tables and the
undo of a copy whose source cannot be removed, one backend under two schemes or reached through a
nested table, mount cycles, concurrent mount changes).
